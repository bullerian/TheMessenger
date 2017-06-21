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
#define DIR_SEND            0
#define DIR_RECV            1
#define ERROR_RETVAL        -1
#define SUCCESS_RETVAL      0

enum status_e {ONLINE, BUSY, AWAY, OFFLINE};
enum contentType_e {PEER_LIST, MESSAGE, STAT, FILE_TRANS};

typedef struct peerParam{
    char name[NAME_LEN];
    enum status_e status;
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

struct peerArray_s{
    peerParam_t* peerArr_p;
    size_t size;
};

/////////////////////////////////////////////////////////////////////////

static int parseAddrStr(struct sockaddr_in* socketAddress, char *addrString);
static int sendRecv_all(int fd, char *buf, int len, int direction);
int disconFromSev(void);
int connToSrv(void);


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

static int parseAddrStr(struct sockaddr_in* socketAddress, char* addrString){
    int port;
    in_addr_t ip;
    char * delimiter;

    if ((delimiter=strchr(addrString, ':'))==NULL){
        syslog(LOG_DEBUG, "Invalid socket string argument (: is missing)\n");
        return ERROR_RETVAL;
    }

    port=atoi((delimiter+1));

    *delimiter='\0';

    if (inet_pton(AF_INET, addrString, (void*) &ip) <= 0) {
        syslog(LOG_DEBUG, "Can't convert from string to IP.\n");
        return ERROR_RETVAL;
    }

    memset(socketAddress, '\0', sizeof(*socketAddress));

    socketAddress->sin_family=AF_INET;
    socketAddress->sin_port=htons(port);
    socketAddress->sin_addr.s_addr=ip;

    return SUCCESS_RETVAL;
}

int connToSrv(void){
    if (connect(TCPfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) == -1){
        syslog(LOG_ERR, "Connection to server failed. Error %d\n", errno);
        return -1;
    }

    return 0;
}

int disconFromSev(void){
    if (close(TCPfd) == -1){
        syslog(LOG_ERR, "Socket closing failed. Error %d\n", errno);
        return -1;
    }

    return 0;
}

void * getpeerArrMsg(void* args){
    struct peerArray_s* newPeerArray=(struct peerArray_s*)args;

    // request to server to get all connected peers
    messageHeader_t peerArrMsg;
    peerArrMsg.serviceCode=SERVICE_CODE;
    peerArrMsg.type=(int8_t) PEER_LIST;
    peerArrMsg.size=0;

    connToSrv();

    if(sendRecv_all(TCPfd, (char*)&peerArrMsg, sizeof(peerArrMsg), DIR_SEND) == -1){
        syslog(LOG_ERR, "Request to server for peerArray failed. Error %d\n", errno);
        disconFromSev();
        return NULL;
    }

    // recv reply from server with peerArr message header
    if(sendRecv_all(TCPfd, (char*)&peerArrMsg, sizeof(peerArrMsg), DIR_RECV) == -1){
        syslog(LOG_ERR, "Recv reply from server with message header failed. Error %d\n", errno);
        disconFromSev();
        return NULL;
    }

    //process recieved header
    if (peerArrMsg.serviceCode != SERVICE_CODE || peerArrMsg.type != PEER_LIST){
        syslog(LOG_ERR, "Invalid reply from server. Expected peerList.");
        disconFromSev();
        return NULL;
    }

    //allocate memory for all of peerParam structs
    newPeerArray->peerArr_p=malloc(peerArrMsg.size);
    newPeerArray->size=peerArrMsg.size;

    // recv peerArray
    if(sendRecv_all(TCPfd, (char*) newPeerArray->peerArr_p, peerArrMsg.size, DIR_RECV) == -1){
        syslog(LOG_ERR, "Recv reply from server with message header failed. Error %d\n", errno);
        disconFromSev();
        free(newPeerArray->peerArr_p);
        return NULL;
    }

    return (void*) newPeerArray;
}

// send or recieve the whole buf
// direction -- DIR_SEND or DIR_RECV
static int sendRecv_all(int fd, char *buf, int len, int direction)
{
    int total = 0;        // sent bytes
    int bytesleft = len; // left to send
    int n;

    while(total < len) {
        n = direction ? (recv(fd, buf+total, bytesleft, 0)):(send(fd, buf+total, bytesleft, 0));
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    len = total; // return number actually sent here

    return n==-1?ERROR_RETVAL:SUCCESS_RETVAL; // return -1 on failure, 0 on success
}

//closelog();


