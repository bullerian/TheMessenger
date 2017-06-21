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
enum contentType_e {PEER_LIST, LETTER, STAT, FILE_TRANS, NOP};

typedef struct peerParam{
    char name[NAME_LEN];
    enum status_e status;
} peerParam_t;

typedef struct {
    uint32_t serviceCode;
    int8_t type;
    uint32_t size;
} messageHeader_t;

typedef struct {
    char from[NAME_LEN];
    uint32_t textSize;
    char* text;
} letter_t;

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

int Init(int argc, char **argv){
    int opt;
    char * socketStr;
    char * statusStr;

    // clear socketAddress structure
    // fill it with default data
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(DEFAULT_SRV_PRT);
    inet_pton(AF_INET, DEFAULT_SRV_IP_STR, (void*) &serveraddr.sin_addr.s_addr);

    // open logger
    openlog("TheMessenger", LOG_PID | LOG_CONS, LOG_USER);

    // Usage nickname [-a serverIP:port] [-s status]
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
            fprintf (stderr, "Option -%c requires an argument in form serverIP:port .\n", optopt);
          }
          else if (optopt == 's')
          {
            fprintf (stderr, "Option -%c requires one of next arguments on, bu, aw, off.\n", optopt);
          }
          else
          {
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
          }
          syslog(LOG_DEBUG, "Command-line arguments parsing failed\n");
          exit(EXIT_FAILURE);
        default:
            exit(EXIT_FAILURE);
        }
    }

    // copy entered name to parametres structure
    strncpy(&myParams.name, &argv[optind], NAME_LEN-1);

    //parse entered parameteres
    if (parseAddrStr(&serveraddr, socketStr) == -1){
        syslog(LOG_ERR, "Can't parse address string \"%s\"\n", socketStr);
    }

    // connect to socket
    if ((TCPfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        syslog(LOG_ERR, "socket creation error. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }

    return SUCCESS_RETVAL;
}

int main(int argc,char **argv)
{
    Init(argc, argv);

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

// sends request to server for new set of peers
// arg -- struct peerArray_s
// return: NULL on fail, struct peerArray_s pointer on success
// Warning: newPeerArray->peerArr_p must be freed
void * getpeerArrMsg(void* arg){
    struct peerArray_s* newPeerArray=(struct peerArray_s*)arg;
    messageHeader_t peerArrMsg;
    unsigned int errorNumber=0;
    char* errorString;

    peerArrMsg.serviceCode=SERVICE_CODE;
    peerArrMsg.type=(int8_t) PEER_LIST;
    peerArrMsg.size=0;

    connToSrv();

    // request to server to get all connected peers
    if(sendRecv_all(TCPfd, (char*)&peerArrMsg, sizeof(peerArrMsg), DIR_SEND) == ERROR_RETVAL)
    {
        errorNumber=errno;
        errorString="Request to server for peerArray failed. Error %d\n";
    }
    // recieve reply from server with peerArr message header
    else if(sendRecv_all(TCPfd, (char*)&peerArrMsg, sizeof(peerArrMsg), DIR_RECV) == ERROR_RETVAL)
    {
        errorNumber=errno;
        errorString="Recv reply from server with message header failed. Error %d\n";
    }
    //processing recieved header
    else if(peerArrMsg.serviceCode == SERVICE_CODE && peerArrMsg.type == PEER_LIST)
    {
        //allocate memory for all of peerParam structs
        newPeerArray->peerArr_p=malloc(peerArrMsg.size);
        newPeerArray->size=peerArrMsg.size;

        // recv peerArray
        if(sendRecv_all(TCPfd, (char*) newPeerArray->peerArr_p, peerArrMsg.size, DIR_RECV) == ERROR_RETVAL){
            errorNumber=errno;
            errorString="Recv reply from server with message header failed. Error %d\n";
            free(newPeerArray->peerArr_p);
        }
    }

    disconFromSev();

    if(errorNumber != 0){
        syslog(LOG_ERR, errorString, errorNumber);
        return NULL;
    }

    return (void*) newPeerArray;
}

// sends peerStatus to server
void * updatePresence(void* status){
    messageHeader_t statusMsg;
    peerParam_t* newStatus=(peerParam_t*) status;

    // fill header struct
    statusMsg.serviceCode=SERVICE_CODE;
    statusMsg.type=(int8_t) STAT;
    statusMsg.size=sizeof(peerParam_t);

    connToSrv();

    // send message with status
    if(!sendRecv_all(TCPfd, (char*)&statusMsg, sizeof(statusMsg), DIR_SEND) &&\
       !sendRecv_all(TCPfd, (char*)&newStatus, sizeof(peerParam_t), DIR_SEND))
    {
        syslog(LOG_ERR, "Send message of type STATus failed. Error %d\n", errno);
        disconFromSev();
        return NULL;
    }

    disconFromSev();

    return (void*) status;
}


// call this func periodicaly to ask server for pressence of new messages
// if arg is != NULL than sends message to server
// return:
void * sendRecv_Mail(void * arg){
    messageHeader_t pollMsg;
    letter_t* newLetter;
    int isSend=arg ? 1:0;

    pollMsg.serviceCode=SERVICE_CODE;
    pollMsg.type=LETTER;

    connToSrv();

    // if func was called with non NULL arg then send message
    // else just poll server for new mail
    if (isSend){
        newLetter=(letter_t *) arg;
        pollMsg.size=sizeof(letter_t);

        if(sendRecv_all(TCPfd, (char*)&pollMsg, sizeof(pollMsg), DIR_SEND) == ERROR_RETVAL)
        {

        }
        else if (sendRecv_all(TCPfd, (char*) newLetter, sizeof(letter_t), DIR_SEND) == ERROR_RETVAL)
        {

        }
        else if (sendRecv_all(TCPfd, newLetter->text, newLetter->textSize, DIR_SEND) == ERROR_RETVAL)
        {

        }


    }else{
        pollMsg.size=0;

        if(sendRecv_all(TCPfd, (char*)&pollMsg, sizeof(pollMsg), DIR_SEND) == ERROR_RETVAL)
        {

        }

        if (pollMsg.size)
        {
            if (sendRecv_all(TCPfd, (char*)newLetter, sizeof(letter_t), DIR_RECV) == ERROR_RETVAL){

            } else if()
        }
    }




}


// send or recieve the whole buf
// direction -- DIR_SEND or DIR_RECV
// retval -- ERROR_RETVAL or SUCCESS_RETVAL
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


