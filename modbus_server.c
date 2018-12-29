#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <pthread.h>
#include <signal.h>


typedef struct{
    int state;
    char *value;
} argument;

pthread_t thread;
unsigned short server_a, server_n;
unsigned char *memory;
struct sockaddr_in address, cli_addr;
int print=0;

void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);
     printf("\nOUCH, ou hit Ctrl-C!\n");
     exit(0);
     signal(SIGINT, INThandler);
     getchar(); // Get new line character
}

int Receive_Modbus_request (int fd, unsigned char **APDU, unsigned short *TI)
{
	int length_APDU = 0, i=0;
	unsigned char PDU[2048];
    struct timeval timeout;
    fd_set fds;

	*TI = 0;

    timeout.tv_sec = 3; 
    timeout.tv_usec = 0; 

    FD_ZERO(&fds); 
    FD_SET(fd,&fds); 

    int retval = select(fd+1, &fds, NULL, NULL, &timeout); 

    if(retval== 0)
    {
        printf("Error - connection timeout\n");
        return -1;
    }


	// ESPERA PELA RESPOSTA DO CLIENTE
	while (recv(fd, PDU, 2048, 0))
    {
	
        //EXTRAI-SE O NUMERO DA TRANSIÇÃO E O CUMPRIMENTO DO APDU
		*TI = (PDU[0] << 8) + PDU[1];   
		length_APDU = (PDU[4] << 8) + PDU[5] - 1;
		*APDU = malloc(sizeof(unsigned char)*length_APDU);

		//REMOVE O CABEÇALHO
		for (i = 0; i < length_APDU; i++)
			(*APDU)[i] = PDU[i+7];

		return length_APDU;

	}
}

int Send_Modbus_response(int fd, unsigned char *APDU_R, unsigned short length_APDU_R, unsigned short TI)
{
	int i=0;
	unsigned char *PDU_R;

	PDU_R = malloc(sizeof(unsigned char)*(length_APDU_R + 7));
			
	//CONSTRU^ÇÃO DA RESPOSTA
	PDU_R[0] = (TI >> 8) & 0xFF;
	PDU_R[1] = TI & 0xFF;
	PDU_R[2] = 0;
	PDU_R[3] = 0;
	PDU_R[4] = ((length_APDU_R+1) >> 8) & 0xFF;
	PDU_R[5] = (length_APDU_R+1) & 0xFF;
	PDU_R[6] = 0;
	
	for (i = 0; i < length_APDU_R; i++)
		PDU_R[i+7] = APDU_R[i];

    //usleep(100000000); TESTE PARA O TIMEOUT!

	return send(fd, PDU_R, 7 + length_APDU_R, 0);

}

int R_coils (unsigned short st_c, unsigned short n_c, unsigned char **val)
{
    int n_bytes, n_bits=0, j, i;
    unsigned char bit, byte;

    if(n_c%8==0)
        n_bytes = (n_c/8);
    else 
        n_bytes = (n_c/8)+1;
	
    *val=malloc(sizeof(unsigned char)*(n_bytes));

    for (j = 0; j < n_bytes; j++) {
		byte = 0;

        //ALGORÍTIMO PARA LEITURA DE COILS
		for (i = 0; i < 8; i++) {
			if (j == n_bytes - 1 && (n_c % 8) == i && i != 0)
				break;
			bit = ((memory[(st_c+i-server_a)/8+j]) >> (7 - (st_c+i-server_a) % 8)) & 0x01;
		
			byte += bit << i;
			n_bits++;
		}
		
		(*val)[j] = byte;	
	}

	return n_bits;
}

int W_coils (unsigned short st_c, unsigned short n_c, unsigned char *val)
{

	int n_bytes, n_bits = 0, j , i;
	unsigned char bit, byte;

	if(n_c%8==0)
        n_bytes = (n_c/8);
    else 
        n_bytes = (n_c/8)+1;

    //ALGORÍTIMO PARA ESCRITA DE COILS
	for (j = 0; j < n_bytes; j++) {
		for (i = 0; i < 8; i++) {
			if (j == n_bytes - 1 && (n_c % 8) == i && i != 0)
				break;

			bit = (val[j] >> i) & 0x01;

			if (bit == 1)
				memory[(st_c+i-server_a)/8 + j] |= 1 << (7 - (st_c+i-server_a) % 8);
			else 
				memory[(st_c+i-server_a)/8 + j] &= ~(1 << (7 - (st_c+i-server_a) % 8));
			
			n_bits++;
		}
		
	}

	return n_bits;
}

void *Request_Handler(void* args)
{
    int fd = *(int*)args, length_APDU_R, length_APDU, i=0, j=0;
    char ip[15];
    unsigned short TI, start_addr=0, n_coils=0, n_bits=0, n_bytes=0;
    unsigned char *APDU, *APDU_R, *PDU_R, *val;

    //GUARDAR O IP
    strcpy (ip, inet_ntoa(cli_addr.sin_addr));

    while (length_APDU = Receive_Modbus_request(fd, &APDU, &TI)) 
    {
        if(length_APDU==-1)
            break;

        // LÊ N_COILS A PARTIR DO START_ADDR
        start_addr = (APDU[1] << 8) + APDU[2];
        n_coils = (APDU[3] << 8) + APDU[4];

        //VERIFICAÇÂO DO ENDEREÇO DE DADOS
        if (start_addr < server_a || (start_addr+n_coils) > (server_a+server_n))
        {
            length_APDU_R = 2;
            APDU_R = malloc(sizeof(unsigned char)*length_APDU_R);

            APDU_R[0] = APDU[0] + 0x80;
            APDU_R[1] = 0x02;

            printf("ERROR: %s: ILLEGAL DATA ADDRESS\n", ip);
        } 
        
        //READ-COILS
        else if (APDU[0] == 0x01) {
            n_bits = R_coils(start_addr, n_coils, &val);

            if(n_bits%8==0)
                n_bytes = (n_bits/8);
            else 
                n_bytes = (n_bits/8)+1;

            length_APDU_R = 2 + n_bytes;

            // Allocate space for response
            APDU_R = malloc(sizeof(unsigned char)*length_APDU_R);
    
            APDU_R[0] = 0x01;
            APDU_R[1] = n_bytes;

            for ( i = 0; i < n_bytes; i++) 
                APDU_R[i+2] = val[i];
        }
        
        //WRITE-COILS
        else if (APDU[0] == 0x0F) {
            val = malloc(sizeof(unsigned char)*APDU[5]);

            if(n_coils%8==0)
                n_bytes = (n_coils/8);
            else 
                n_bytes = (n_coils/8)+1;


            for (i = 0; i < n_bytes; i++)
                val[i] = APDU[6+i];

            n_bits = W_coils(start_addr, n_coils, val);

            //ALOCAÇÃO DE MEMÓRIA para o APDU_R
            length_APDU_R = 5;
            APDU_R = malloc(sizeof(unsigned char)*length_APDU_R);

            APDU_R[0] = 0x0F;
            
            APDU_R[1] = (start_addr >> 8) & 0xFF;
            APDU_R[2] = start_addr & 0xFF;

            APDU_R[3] = (n_bits >> 8) & 0xFF;
            APDU_R[4] = n_bits & 0xFF; 
        }
        
        //VERIFICAÇÃO DO FUNCTION CODE
        else {
            length_APDU_R = 2;
            APDU_R = malloc(sizeof(unsigned char)*length_APDU_R);
            
            APDU_R[0] = APDU[0] + 0x80;
            APDU_R[1] = 0x01;

            printf("ERROR: %s: ILLEGAL FUNCTION\n", ip);
        }

        Send_Modbus_response(fd, APDU_R, length_APDU_R, TI);

        if (length_APDU_R > 2 && print==1)
        {
            printf("SUCCESS: %s: %d: ", ip, TI);
            unsigned char byte, bit;

            for (j = 0; j < n_bytes; j++) 
            {
                byte = 0;
                for (i = 0; i < 8; i++) 
                {
                    if (j == n_bytes - 1 && (n_coils % 8) == i && i != 0)
                        break;
                    bit = (memory[(start_addr+i-server_a)/8+j] >> (7 - (start_addr+i-server_a) % 8)) & 0x01;
                
                    byte += bit << i;
                }
                printf("%x ", (int)byte);             
            }
            printf("\n");
        }
        free(APDU);
        free(APDU_R);
    }

    close(fd);
}




int connection(int port)
{
    int sockfd, fd;
    socklen_t client_addr_length;
    /* create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd <= 0)
    {
        printf("error: cannot create socket\n");
        return -1;
    }

    int optval=1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) 
        return -1;


    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        printf("error: cannot bind socket to port %d\n", port);
        return -1;
    }

    /* listen on port */
    if (listen(sockfd, 5) < 0)
    {
        printf("error: cannot listen on port\n");
        return -1;
    }
    
    while (1)
    {
        /* accept incoming connections */
        client_addr_length = sizeof(cli_addr);
        fd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_length);
       
            /* start a new thread but do not wait for it */
            pthread_create(&thread, 0, Request_Handler, &fd);
        
    }

    close(sockfd);
    return 0;
}




int main(int argc, char ** argv)
{
    int sock = -1;
    pthread_t thread;
    unsigned short port, start, n_c, flags=0, n_bytes=0;

    argument args[4];

    //PASSAGEM DOS VALORES INDEPENDENTE DA POSIÇÃO
    int i;
    
    signal(SIGINT, INThandler);

    for (i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'p':
                    args[0].state = 1;
                    args[0].value = malloc(sizeof(char)*(strlen(argv[i+1])+1));
                    strcpy(args[0].value, argv[i+1]);
                    break;
                case 's':
                    args[1].state = 1;
                    args[1].value = malloc(sizeof(char)*(strlen(argv[i+1])+1));
                    strcpy(args[1].value, argv[i+1]);
                    break;
                case 'n':
                    args[2].state = 1;
                    args[2].value = malloc(sizeof(char)*(strlen(argv[i+1])+1));
                    strcpy(args[2].value, argv[i+1]);
                    break;
                case 'v':
                    args[3].state = 1;
                    break;
                default: 
                    printf("Unknown option - %s\n", argv[i]);
                    return -1;
            }
        }
    }
    if(argc > (7+args[3].state))
    {
        printf("Error - Too many arguments\n");
        return -1;
    }

    else if(argc < (7+args[3].state))
    {
        printf("Error - Too few arguments\n");
        return -1;
    }

    if (args[0].state + args[1].state + args[2].state != 3) {
        printf("Missing arguments!\n");
        return -1;
    }

    server_a = atoi(args[1].value);
    server_n = atoi(args[2].value);
    print = args[3].state;


    if(server_n%8==0)
        n_bytes = (server_n/8);
    else 
        n_bytes = (server_n/8)+1;

    //CRIAÇÃO DA "MEMORIA" DE COILS 
    memory = malloc(sizeof(char)*(n_bytes));

    for(i=0; i<n_bytes; i++)
    {
        memory[i]=0;
    }

    //CRIAÇÃO DO SERVIDOR
    if (connection(atoi(args[0].value)) < 0) {
        printf("Error creating server!\n");
        return -1;
    }

    return 1;
}

