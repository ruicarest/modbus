#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
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

static unsigned short TI=1;
int print=0;

unsigned char hex2byte(char *hexValue) { 
    unsigned char byte = 0;  
    int i;      
    for (i = 1; i >= 0; i--) { 
        if (hexValue[i] >= '0' && hexValue[i] <= '9') 
            byte += (hexValue[i] - '0')*pow(16, 1 - i); 
        else if (hexValue[i] >= 'a' && hexValue[i] <= 'f') 
            byte += (hexValue[i] - 'a' + 10)*pow(16, 1 - i);         
    } 
  
    return byte; 
}

int send_Modbus_request(int fd, unsigned char *APDU, unsigned char **APDU_R, unsigned short length_APDU, unsigned short length_APDU_R)
{
    unsigned char *PDU, *PDU_R;
    int i, aux=1, n_lidos=0, length=0, n_escritos=0;
    struct timeval timeout;
    fd_set fds;

    PDU = malloc(sizeof(unsigned char)*(length_APDU+7));

    PDU_R = malloc(sizeof(unsigned char)*(length_APDU_R+7));

    //CONSTRUÇÃO DO CABEÇALHO DO APDU
    PDU[0] = (TI >> 8) & 0xFF;
    PDU[1] = TI & 0xFF;
    PDU[2] = 0;
    PDU[3] = 0;
    PDU[4] = ((length_APDU+1) >> 8) & 0xFF;
    PDU[5] = (length_APDU+1) & 0xFF;
    PDU[6] = 0;   

    TI++;

    for (i=0; i<length_APDU; i++)
    {
        PDU[i+7] = APDU[i];
    }

    do{
        n_escritos += write(fd, PDU, length_APDU+7);
    }while(n_escritos!=length_APDU+7);


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


    n_lidos = read(fd, PDU_R, (length_APDU_R+7));

    for(i=0; i<n_lidos-7; i++)
    {
        (*APDU_R)[i] = PDU_R[i+7];
    }


    free(PDU);
    free(PDU_R);

    return 0;
}

int write_multiple_coils (int fd, unsigned short st_c, unsigned short n_c, unsigned char *val)
{
    unsigned char *APDU, *APDU_R;    
    unsigned char high_addr, low_addr, byte_count;
    unsigned char high_num, low_num;
    int i=0, length_APDU_R=5, length_APDU;


    high_addr = (st_c >> 8) & 0xFF;
    low_addr = st_c & 0xFF;

    high_num = (n_c >> 8) & 0xFF;
    low_num = n_c & 0xFF;

    if(n_c%8==0)
        byte_count = n_c/8;
    else
        byte_count = n_c/8 + 1;

    length_APDU = 6+byte_count;

    APDU = malloc(sizeof(unsigned char)*length_APDU);
    APDU_R = malloc(sizeof(unsigned char)*length_APDU_R);

    //CONSTRUÇÃO DO APDU
    APDU[0] = 0x0F;
    APDU[1] = high_addr;
    APDU[2] = low_addr;
    APDU[3] = high_num;
    APDU[4] = low_num;
    APDU[5] = byte_count & 0xFF;

    for (i = 6; i < length_APDU; ++i)
    {
        APDU[i] = val[i-6];
    }

    int var = send_Modbus_request (fd, APDU, &APDU_R, length_APDU, length_APDU_R);

    if(var==-1)
    {
        printf("ERROR - connection timeout\n");
        return -1;
    }


     //EXCEPTION ERROR
    if((int)APDU_R[0]>=128)
    {
         switch((int)APDU_R[1]){
            case 0x01: printf("ILLEGAL FUNCTION\n"); break;
            case 0x02: printf("ILLEGAL DATA ADDRESS\n"); break;
            case 0x03: printf("ILLEGAL DATA VALUE\n"); break;
            case 0x04: printf("SERVER DEVICE FAILURE\n"); break;
            case 0x05: printf("ACKNOWLEDGE\n"); break;
            case 0x06: printf("SERVER DEVICE BUSY \n"); break;
            case 0x08: printf("MEMORY PARITY ERROR\n"); break;
            case 0x0a: printf("GATEWAY PATH UNAVAILABLE \n"); break;
            case 0x0b: printf("GATEWAY TARGET DEVICE FAILED TO RESPOND \n"); break;
        }
    }
    else if(((int)APDU_R[0]>0) && print == 1)
    {
        printf("SUCCESS: %d : ", TI-1);
        for(i=6; i<length_APDU; i++)
        {
            printf("%x ",(int)APDU[i]);
        }
        printf("\n");
    }
    else if ((int)APDU_R[0]==0)
    {
        printf("Error - connection timeout\n");
    }
    return byte_count;
}

int read_coils (int fd, unsigned short st_c, unsigned short n_c)
{
    unsigned char high_addr, low_addr;
    unsigned char high_num, low_num;
    unsigned char *APDU, *APDU_R;
    int i=0, length_APDU_R, length_APDU=5;

    APDU = malloc(sizeof(unsigned char)*length_APDU);

    high_addr = (st_c >> 8) & 0xFF;
    low_addr = st_c & 0xFF;

    high_num = (n_c >> 8) & 0xFF;
    low_num = n_c & 0xFF;

    //CONSTRUÇÃO DO APDU
    APDU[0] = 0x01;
    APDU[1] = high_addr;
    APDU[2] = low_addr;
    APDU[3] = high_num;
    APDU[4] = low_num;


    if(n_c%8==0)
        length_APDU_R = (n_c/8)+2;
    else 
        length_APDU_R = (n_c/8)+3;

    APDU_R = malloc(sizeof(unsigned char)*(length_APDU_R));

    int var = send_Modbus_request (fd, APDU, &APDU_R, length_APDU, length_APDU_R);

    if(var==-1)
    {
        printf("ERROR - connection timeout\n");
        return -1;
    }

    //EXCEPTION ERROR
    if((int)APDU_R[0]>=128)
    {
        switch((int)APDU_R[1]){
            case 0x01: printf("ILLEGAL FUNCTION\n"); break;
            case 0x02: printf("ILLEGAL DATA ADDRESS\n"); break;
            case 0x03: printf("ILLEGAL DATA VALUE\n"); break;
            case 0x04: printf("SERVER DEVICE FAILURE\n"); break;
            case 0x05: printf("ACKNOWLEDGE\n"); break;
            case 0x06: printf("SERVER DEVICE BUSY \n"); break;
            case 0x08: printf("MEMORY PARITY ERROR\n"); break;
            case 0x0a: printf("GATEWAY PATH UNAVAILABLE \n"); break;
            case 0x0b: printf("GATEWAY TARGET DEVICE FAILED TO RESPOND \n"); break;
        }
    }
    else if(((int)APDU_R[0]>0) && print == 1)
    {
        printf("SUCCESS: %d : ", TI-1);
        for(i=2; i<length_APDU_R; i++)
        {
            printf("%x ",(int)APDU_R[i]);
        }
        printf("\n");
    }
    /*else if ((int)APDU_R[0]==0)
    {
        printf("Error - connection timeout\n");
    }*/

    free(APDU_R);
    free(APDU);

return length_APDU_R;
}



int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0, length_APDU, i=0, number_cycle, j, f_r=0, f_w=0, posicao=0;
    unsigned char recvBuff[1024], F=0, addr, *val;
    unsigned short n_coils, i_address, port, n_bytes;
    char address[30];
    struct sockaddr_in serv_addr;
    clock_t t;

    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;


    // VERIFICAÇÕES DO ESCRITO NO TERMINAL

    if(argc < 11)
    {
        printf("Too few arguments!\n");
        return 0;
    }
    if(argc < 11)
    {
        printf("Too few arguments!\n");
    }



    // PASSAGEM DOS VALORES INDEPENDENTE DA POSIÇÃO, DEFINIDA OU NÃO!!!
    for (i = 0; i < argc; ++i)
    {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) 
            {
                case 's': 
                    i_address = atoi(argv[i+1]);
                    break;
                case 'n':
                    n_coils = atoi(argv[i+1]);
                    if(n_coils%8==0)
                        n_bytes = n_coils/8;
                    else
                        n_bytes = n_coils/8+1;
                    val = malloc (sizeof(char)*(n_bytes+1));
                    break;
                case 'a':
                    strcpy(address,argv[i+1]);
                    break;
                case 'p':
                    port = atoi(argv[i+1]);
                    break;
                case 'c':
                    number_cycle = atoi(argv[i+1]);
                    break;
                case 'v':
                    print = 1;
                    break;
                case 'd': 
                    posicao = i+1;
                    break;
                case 'r': 
                    f_r++;
                    break;
                case 'w': 
                    f_w++;
                    break;
                default:
                    printf("Error - Unknown option %s\n", argv[i]);
                    return 0;
            }
        }
    }

    if(((argc == (13+n_bytes+print))&&f_w==1) ||((argc == (12+print))&&f_r==1)) 
    {
        for (j = 0; j < n_bytes; ++j)
        {
            val[j] = hex2byte(argv[posicao+j]); 
        }
    }
    else if (argc > (13+n_bytes+print))
    {
        printf("Error - Too many arguments\n");
        return 0;
    }
    else
    {
        printf("Error - Too few arguments\n");
        return 0;
    }

    if(f_r+f_w==2)
    {
        printf("Error - Can't read and write\n");
        return 0;
    }

    if(inet_pton(AF_INET, address, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 

    serv_addr.sin_port = htons(port); //LEITURA DA PORTA



    //  CONECÇÂO COM O SOCKET
    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return 1;
    } 

    t = clock();
    
    //VERIFICAÇÃO DA FUNÇÃO (SE READ OU WRITE)
    switch(strcmp(argv[5], "-r")){
        case 0:
            while(1)
            {
                if(clock()-t == number_cycle*100000)
                {
                    length_APDU = read_coils(sockfd,i_address, n_coils);
                    if(length_APDU<0){
                        printf("Error receiving coils\n");
                        return 0;
                    }
                    t = clock();
                }
            }
            break;
        default: 
            while(1)
            {
                if(clock()-t == number_cycle*100000)
                {
                    length_APDU = write_multiple_coils(sockfd,i_address, n_coils, val);
                    if(length_APDU<0){
                        printf("Error writing coils\n");
                        return 0;
                    }
                    t = clock();
                }
            }
            break;
        } 


    return 0;
}
