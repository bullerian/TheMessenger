#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>

#define NAME_LEN            30
#define DEFAULT_SRV_PRT     1313
#define DEFAULT_SRV_IP_STR  "127.0.0.1"
#define SERVICE_CODE        0xFAFAFAFA

enum status_e {ONLINE, BUSY, AWAY, OFFLINE};
enum contentType_e {PEER_LIST, MESSAGE, STAT, FILE_TRANS};

typedef struct peerParam{
    char name[NAME_LEN];
    enum status_e status;
    struct peerParam* next;
} peerParam_t;


typedef struct {
    uint32_t serviceCode;
    int8_t type;
    uint32_t size;
} messageHeader_t;


//////////// Client Variables///////////////////////////////////////////
char* msgIn;
char* msgOut;

int TCPfd;
struct sockaddr_in serveraddr;
peerParam_t myParams;
char* StatusStrings[]={"Online", "Busy", "Away", "Offline"};
/////////////////////////////////////////////////////////////////////////

int parseAddrStr(struct sockaddr_in* socketAddress, char *addrString);

//////////////////////////PROGRAM////////////////////////////////////////

int Init(void){
    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(DEFAULT_SRV_PRT);
    inet_pton(AF_INET, DEFAULT_SRV_IP_STR, (void*) &serveraddr.sin_addr.s_addr);

    openlog("TheMessenger", LOG_PID | LOG_CONS, LOG_USER);

    if ((TCPfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        syslog(LOG_ERR, "socket creation error. Error %d\n", errno);
    }

    return 0;
}

int main(int argc,char **argv)
{
    int opt;
    char * socketStr;
    char * statusStr;

    // parsing command-line arguments
    while ((opt = getopt(argc, argv, "a:s:")) != -1) {
        switch (opt) {
        case 'a':
            socketStr=optarg;
            break;
        case 's':
            statusStr=optarg;
            break;
        case '?':
          if (optopt == 'a')
          {
            fprintf (stderr, "Option -%s requires an argument in form serverIP:port .\n", optopt);
          }
          else if (optopt == 's')
          {
            fprintf (stderr, "Option -%s requires one of next arguments on, bu, aw, off.\n", optopt);
          }
          else
          {
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
          }
          syslog(LOG_DEBUG, "Command-line arguments parsing failed\n");
          return 1;
        default:
            return 1;
        }
    }

    strncpy(&myParams.name, &argv[optind], NAME_LEN-1);

    if (parseAddrStr(&serveraddr, socketStr) == -1){
        syslog(LOG_ERR, "Can't parse address string \"%s\"\n", socketStr);
    }


    return 0;
}

int parseAddrStr(struct sockaddr_in* socketAddress, char* addrString){
    int port;
    in_addr_t ip;
    char * delimiter;

    if ((delimiter=strchr(addrString, ':'))==NULL){
        syslog(LOG_DEBUG, "Invalid socket string argument (: is missing)\n");
        return -1;
    }

    port=atoi((delimiter+1));

    *delimiter='\0';

    if (inet_pton(AF_INET, addrString, (void*) &ip) <= 0) {
        syslog(LOG_DEBUG, "Can't convert from string to IP.\n");
        return -1;
    }

    memset(socketAddress, '\0', sizeof(*socketAddress));

    socketAddress->sin_family=AF_INET;
    socketAddress->sin_port=htons(port);
    socketAddress->sin_addr.s_addr=ip;

    return 0;
}


int connToSrv(){
    int retVal=0;

    if (connect(TCPfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1){
        syslog(LOG_ERR, "Connection to server failed. Error %d\n", errno);
        retVal=-1;
    }

    return retVal;
}

void * getpeerArrMsg(void* args){
    ssize_t transferSize=0;
    peerParam_t* peerBufferp;
    char* buff_ptr, endRange;


    // request to server for peerArray
    messageHeader_t peerArrMsg;
    peerArrMsg.serviceCode=SERVICE_CODE;
    peerArrMsg.type=(int8_t) PEER_LIST;
    peerArrMsg.size=0;

    connToSrv();

    while(transferSize != sizeof(peerArrMsg)){
        transferSize=send(TCPfd, &peerArrMsg, sizeof(peerArrMsg), 0);
    }

    transferSize=0;

    // recv reply from server with message header
    while(transferSize != sizeof(peerArrMsg)){
        transferSize=recv(TCPfd, &peers, sizeof(peerArrMsg), 0);
    }

    //allocate memory for all of peerParam structs
    peerBufferp=malloc(peerArrMsg.size);
    buff_ptr=peerBufferp;

    transferSize=0;

    while(buff_ptr != (peerBufferp+peerArrMsg.size)){
        transferSize=recv(TCPfd, buff_ptr, sizeof(peerArrMsg), 0);
        buff_ptr+=transferSize;
    }


}

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

//closelog();


