/*
CSE536 Project2 Build a communication channel that uses normal read/write system calls to communicate over IP
Main program
*/
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curses.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#define BUFFER_SIZE 256
#define DATA_SIZE 256
#define SERVER_PORT 23456
FILE *getFileDescriptor(char * mode);
FILE *filedescriptor=NULL;	
char remotemachineip[50]="127.0.0.1";
char serverip[50]="127.0.0.1";
uint32_t final_Clock=0;

char host[NI_MAXHOST];
typedef struct Message{
    uint32_t recordID;
    uint32_t finalClock;
    uint32_t origClock;
    uint32_t source;
    uint32_t destination;
    char msgData[236];
}Message;
void split(int val, unsigned char *arr) {
    memcpy(arr, &val, sizeof(int));

}

char * getlocalipaddress(){
    struct ifaddrs *ifaddr, *ifa;
    int family, s;


    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if((strcmp(ifa->ifa_name,"eth0")==0)&&(ifa->ifa_addr->sa_family==AF_INET))
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

        }
    }


    freeifaddrs(ifaddr);
    
    return host;
}

int sendToUDPServer(char *app_buf){

    struct sockaddr_in client, server;
    struct hostent *hp;
    int len, ret, n;
    int s, new_s;
    int p=0;
    bzero((char *)&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(0);

    s = socket(AF_INET, SOCK_DGRAM, 0);				//create UDP socket
    if (s < 0)
    {
        perror("simplex-talk: UDP_socket error");
        exit(1);
    }

    if ((bind(s, (struct sockaddr *)&server, sizeof(server))) < 0)
    {
        perror("simplex-talk: UDP_bind error");
        exit(1);
    }
    hp = gethostbyname(serverip);//remote ip address
    if( !hp )
    {
        fprintf(stderr, "Unknown host %s\n", "localhost");
        exit(1);
    }

    bzero( (char *)&server, sizeof(server));
    server.sin_family = AF_INET;
    bcopy( hp->h_addr, (char *)&server.sin_addr, hp->h_length );
    server.sin_port = htons(SERVER_PORT);
    n=256;
    ret = sendto(s, app_buf, n, 0,(struct sockaddr *)&server, sizeof(server));
    if( ret != n)
    {
        fprintf( stderr, "Datagram Send error %d\n", ret );
        exit(1);
    }

    return ret;


}

int main(int argc, char * argv[])
{
    printf("\n****************************\nHello !!! %s\n****************************\n",getenv("SUDO_USER")?getenv("SUDO_USER"):getenv("USER"));


    char buf[256];
    char temp[256];
    char readbuf[256];
    char packet[236];
    char in;
    size_t cnt;
    int quit = 0;
    int ch;
    int len=0;

   // uint32_t orig_clock;
   // uint32_t * finalclockaddr=&final_clock;
  //  uint32_t * origclockaddr=&orig_clock;
    volatile uint32_t orig_clock[]={0,0};
uint32_t * origclockaddr=&orig_clock;
    Message *msg=(Message*)malloc(sizeof(Message));

    /*
    By default use localhost(127.0.0.1) if no in argument is passed for remote machine to connect.
    Caution: using localhost will read and write to same device /dev/cse5361 Use for testing purpose only
    */
//    if(argc==1)
//    {
//        strcpy(remotemachineip ,"127.0.0.1");
//        printf("-----------------------------------\n");
//        printf("Remote machine address: %s\n",remotemachineip);
//        printf("-----------------------------------\n");
//        printf("If you do not want to use localhost exit the program and rerun using ip address as 1st parameter.\nDo you want to continue with localhost y/n:\n");
//        printf("-----------------------------------\n");
//        in=getchar();
//        ch = getchar();
//        while(ch!=EOF&&ch!='\n'){
//        }
//        if(in=='n'||in=='N'){
//            return 0;
//        }
//    }
//    else
//    {
        strcpy(serverip,argv[1]);
        strcpy(remotemachineip,argv[2]);

//    }
         memset(buf,0,BUFFER_SIZE);	//initialise buffer to zeros to avoid junk packet
       if(filedescriptor=getFileDescriptor("wb"))	//write packet to character device to send ip address
         {
             //fwrite(buf,1,strlen(remotemachineip)+1,filedescriptor);
             msg->recordID=0;
             msg->finalClock=0;
             msg->origClock=0;
             msg->source=inet_addr(getlocalipaddress());;
             msg->destination=inet_addr(remotemachineip);
             strncpy(msg->msgData,remotemachineip,16);
             //memcpy(buf,msg,sizeof(msg));
             memset(buf,0,256);
             memcpy(buf, &(msg->recordID), 4);
             memcpy(buf+4,&(msg->finalClock),4);
             memcpy(buf+8,&(msg->origClock), 4);
             memcpy(buf+12,&(msg->source), 4);
             memcpy(buf+16,&(msg->destination), 4);
             memcpy(buf+20, &(msg->msgData), 16);
             fwrite(buf,1,256,filedescriptor);

             fclose(filedescriptor);
         }


    memset(buf,0,256);	//initialise buffer to zeros to avoid junk packet

    while(1){
        printf("\n\nPlease select [C]hange Destination Ip or [S]end or [R]eceive or [E]xit\n");
        printf("-----------------------------------\n");
        in=getchar();
        while((ch=getchar())!=EOF&&ch!='\n'){}
        if(in=='E'||in=='e'){
            printf("Good Bye!!! %s\n",getenv("SUDO_USER")?getenv("SUDO_USER"):getenv("USER"));
            exit(0);
        }else if(in=='C'||in=='c'){
            printf("-----------------------------------\n");
            printf("Please enter the IP address of Destination:\n");
            scanf("%[^\n]",remotemachineip);//read a line from console
            while((ch=getchar())!=EOF&&ch!='\n'){
            }
            memset(buf,0,BUFFER_SIZE);	//initialise buffer to zeros to avoid junk packet
          if(filedescriptor=getFileDescriptor("wb"))	//write packet to character device to send ip address
            {
                //fwrite(buf,1,strlen(remotemachineip)+1,filedescriptor);
                msg->recordID=0;
                msg->finalClock=0;
                msg->origClock=0;
                msg->source=inet_addr(getlocalipaddress());;
                msg->destination=inet_addr(remotemachineip);
                strncpy(msg->msgData,remotemachineip,16);
                //memcpy(buf,msg,sizeof(msg));
                memset(buf,0,256);
                memcpy(buf, &(msg->recordID), 4);
                memcpy(buf+4,&(msg->finalClock),4);
                memcpy(buf+8,&(msg->origClock), 4);
                memcpy(buf+12,&(msg->source), 4);
                memcpy(buf+16,&(msg->destination), 4);
                memcpy(buf+20, &(msg->msgData), 16);
                fwrite(buf,1,256,filedescriptor);

                fclose(filedescriptor);
            }
        }else if(in=='S'||in=='s'){
            printf("-----------------------------------\n");
            printf("Please enter your msg:\n");
            memset(packet,0,236);
            scanf("%[^\n]",packet);//read a line from console and store in packet assuming not given more than 256 characters to fit into buffer
            while((ch=getchar())!=EOF&&ch!='\n'){
            }
            memset(buf,0,BUFFER_SIZE);

            if(filedescriptor=getFileDescriptor("wb"))
            {
                msg->recordID=1;
                msg->finalClock=0;
                msg->origClock=0;
                msg->source=inet_addr(getlocalipaddress());
                msg->destination=inet_addr(remotemachineip);
                packet[235]=0;
                strncpy(msg->msgData,packet,236);
                //memcpy(buf,msg,sizeof(msg));

                memset(buf,0,256);

                memcpy(buf, &(msg->recordID), 4);
                memcpy(buf+4,&origclockaddr,8);

                memcpy(buf+12,&(msg->source), 4);
                memcpy(buf+16,&(msg->destination), 4);
                memcpy(buf+20, &(msg->msgData), 236);
                fwrite(buf,1,256,filedescriptor);
                fclose(filedescriptor);
                printf("Your msg sent: %s\n",buf+20);
                memcpy(temp,buf,256);
                memcpy(temp+4,origclockaddr,4);
                memcpy(temp+8,origclockaddr+1,4);
                len = sendToUDPServer(temp);
                memset(packet,0,236);	// to clear old data
                printf("-----------------------------------\n");

            }

        }else if(in=='R'||in=='r'){
            memset(readbuf,0,256);
           // printf("You have received these messages:\n");
            printf("-----------------------------------\n");
            Message* readMsg=(Message*)malloc(sizeof(Message));

            while(1)

            {	memset(readbuf,0,256);
                memset(readMsg,0,256);

                if(filedescriptor=getFileDescriptor("rb"))
                {
                    cnt=0;
                    cnt=fread(readbuf,1,256,filedescriptor);
                    if(!cnt)break;

                    readMsg=(Message*)readbuf;
                    memcpy(&(readMsg->recordID),readMsg,4);
                    memcpy(&(readMsg->finalClock),&(readMsg)->recordID+1,4);
                    memcpy(&(readMsg->origClock),&(readMsg->recordID)+2,4);
                    memcpy(&(readMsg->source),&(readMsg->recordID)+3,4);
                    memcpy(&(readMsg->destination),&(readMsg->recordID)+4,4);
                    memcpy(&(readMsg->msgData),&(readMsg->recordID)+5,236);


                    if(readMsg->recordID==0){
                       printf("Received ACK for your message : %s\n",readMsg->msgData);
                          len = sendToUDPServer((char*)readMsg);
                    }else{
                        printf("You have recevied this message: %s\n",readMsg->msgData);
                    }

                    fclose(filedescriptor);

                }else{
                        break;
                    }
                }
                printf("-----------------------------------\n");
            }else{
                printf("-----------------------------------\n");
                printf("Not valid input please retry\n");
                printf("-----------------------------------\n");
            }
        }
        return 0;
    }
    FILE *getFileDescriptor(char * mode)
    {
        if(!(filedescriptor=fopen("/dev/cse5361",mode)))
        {	printf("-----------------------------------\n");
            printf("Error while invoking file in %s mode\n",mode);
            printf("-----------------------------------\n");
            exit(1);
        }
        return filedescriptor;
    }


