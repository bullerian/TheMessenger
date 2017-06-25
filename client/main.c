#include "includes/main.h"

//////////// Client DEFINEs/////////////////////////////////////////////
#define CONN_RETR_COUNT     2
#define CONN_RETR_DELAY_MS  50
#define DEFAULT_SRV_PRT     1313
#define DEFAULT_SRV_IP_STR  "127.0.0.1"
#define ERROR_RETVAL        (-1)
#define SUCCESS_RETVAL      0
/////////////////////////////////////////////////////////////////////////
//////////////////// Client prototypes //////////////////////////////////
static int parseAddrStr(struct sockaddr_in* socketAddress, char *addrString);
static int parseStatusStr(peerParam_t* myParams, char * statusStr);
static int sendRecv_all(int fd, char *buf, int len, int direction);
int disconFromSev(void);
int connToSrv(void);
void * sendRecv_File(void * fileT);
void * updateStatus(void* status);
void * sendRecv_Mail(void * arg);
void * getpeerArrMsg(void* arg);
/////////////////////////////////////////////////////////////////////////
//////////// Client Globals /////////////////////////////////////////////
int TCPfd;
struct sockaddr_in serveraddr;
peerParam_t myParams;
char* StatusStrings[]={"Online", "Busy", "Away", "Offline"};
char* Stat_cl_paramStr[]={"on", "bu", "aw", "of"};

struct peerArray_s{
    peerParam_t* peerArr_p;
    size_t size;
};
/////////////////////////////////////////////////////////////////////////
//////////////////////////PROGRAM////////////////////////////////////////

void Exit_handler(int signum){
    close(TCPfd);
    syslog(LOG_DEBUG, "Terminated by signal %d\n", signum);
    closelog();
    exit(EXIT_FAILURE);
}

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
    strncpy((char *) myParams.name, argv[optind], NAME_LEN-1);

    //parse address and status
    if ((parseAddrStr(&serveraddr, socketStr) == ERROR_RETVAL) ||\
            (parseStatusStr(&myParams, statusStr) == ERROR_RETVAL)){
        syslog(LOG_ERR, "Can't parse address or status string\n");
    }

    // connect to socket
    if ((TCPfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        syslog(LOG_ERR, "socket creation error. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, &Exit_handler);

    return SUCCESS_RETVAL;
}

int main(int argc,char **argv)
{
    pthread_t uiThread, srvCmdThread;
    struct peerArray_s peersArray;
    letter_t letter;
    void * threadRetval;


    Init(argc, argv);

    // create ui thread
    /*
    if(pthread_create(&uiThread, NULL, &uiThreadFunc, NULL) == ERROR_RETVAL){
        syslog(LOG_ERR, "UI thread creation failed. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }
    */

    updateStatus(&myParams);
    getpeerArrMsg(&peersArray);

    while(1)
    {
        // create mail check thread
        if(pthread_create(&srvCmdThread, NULL, &sendRecv_Mail, &letter) == ERROR_RETVAL){
            syslog(LOG_ERR, "UI thread creation failed. Error %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pthread_tryjoin_np(srvCmdThread, &threadRetval);



    }


    exit(EXIT_SUCCESS);
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


// creates connection to server
// makes CONN_RETR_COUNT connection attempts with delay CONN_RETR_DELAY_MS
int connToSrv(void){
    unsigned int retryCount=CONN_RETR_COUNT;
    unsigned int retryDelayUs=CONN_RETR_DELAY_MS * 100;

    while(retryCount--){
        if (connect(TCPfd, (struct sockaddr *) &serveraddr,\
                    sizeof(serveraddr)) == SUCCESS_RETVAL){
            return SUCCESS_RETVAL;
        }

        syslog(LOG_ERR, "Connection to server failed. Retry count %d. Error %d\n",\
               retryCount, errno);

        usleep(retryDelayUs);
    }

    return ERROR_RETVAL;
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
    // recieve reply from server with message header
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

// send peerStatus to server
void * updateStatus(void* status){
    messageHeader_t statusMsg;
    peerParam_t* newStatus=(peerParam_t*) status;
    void * retVal = status;

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
        retVal = NULL;
    }

    disconFromSev();

    return retVal;
}


// call this func periodicaly to ask server for pressence of new messages
// if arg is != NULL than sends message to server
// return:
void * sendRecv_Mail(void * arg){
    messageHeader_t pollMsg;
    letter_t* newLetter=(letter_t *) arg;
    int isSend=0;
    unsigned int errorNumber=0;
    char* errorString;

    pollMsg.serviceCode=SERVICE_CODE;
    pollMsg.type=LETTER;

    connToSrv();

    // if newLetter->textSize > 0 then there is new letter to send
    isSend=newLetter->textSize ? 1 : 0;


    if (isSend){
        // send letter
        pollMsg.size=sizeof(letter_t);

        // send header that indicates presence of new message
        if(sendRecv_all(TCPfd, (char*)&pollMsg, sizeof(pollMsg), DIR_SEND) == ERROR_RETVAL)
        {
            errorNumber=errno;
            errorString="Sending header that indicates presence of new message failed. Reason %d.\n";
        }
        //send letter structure
        else if (sendRecv_all(TCPfd, (char*) newLetter, sizeof(letter_t),\
                              DIR_SEND) == ERROR_RETVAL)
        {
            errorNumber=errno;
            errorString="Sending letter structure failed. Reason %d.\n";
        }
        // send letter text
        else if (sendRecv_all(TCPfd, newLetter->text, newLetter->textSize,\
                              DIR_SEND) == ERROR_RETVAL)
        {
            errorNumber=errno;
            errorString="Sending letter text failed. Reason %d.\n";
        }
        // letter was succesfuly send. memory can be freed
        else
        {
            free(newLetter->text);
        }
    }else{
        // check for new incomming letters
        pollMsg.size=0;

        // send header with size 0
        // this indicates that clien is polling for new letters
        if(sendRecv_all(TCPfd, (char*)&pollMsg, sizeof(pollMsg),\
                        DIR_SEND) == ERROR_RETVAL)
        {
            errorNumber=errno;
            errorString="Sending header with size 0. Reason %d.\n";
        }
        // recv message header
        else if (sendRecv_all(TCPfd, (char*)&pollMsg, sizeof(pollMsg), DIR_RECV) == ERROR_RETVAL)
        {
            errorNumber=errno;
            errorString="Recieving message header from server. Reason %d.\n";
        }
        // if recieved message header size is greater that 0
        // than there is new letter
        else if (pollMsg.size > 0)
        {
            // recv letter struct
            if (sendRecv_all(TCPfd, (char*)newLetter, sizeof(letter_t), DIR_RECV) == SUCCESS_RETVAL)
            {
                newLetter->text=malloc(newLetter->textSize);

                //recv letter text
                if(sendRecv_all(TCPfd, (char*)newLetter->text, newLetter->textSize, DIR_RECV) == ERROR_RETVAL)
                {
                    free(newLetter->text);
                    errorNumber=errno;
                    errorString="Recieving letter text from server. Reason %d.\n";
                }
            }
            else
            {
                errorNumber=errno;
                errorString="Receiving letter struct from server failed. Reason %d.\n";
            }
        }
    }

    disconFromSev();

    if(errorNumber){
        syslog(LOG_ERR, errorString, errorNumber);
        return NULL;
    }

    return (void*) arg;
}

void * sendRecv_File(void * fileT){
    return NULL;
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


// Parses entered stat argument. If argument is one of Stat_cl_paramStr items
// sets clients status.
// If statusStr is not in Stat_cl_paramStr then sets default status
// return: SUCCESS_RETVAL when statusStr is valid, ERROR_RETVAL on invalid arg.
static int parseStatusStr(peerParam_t* myParams, char * statusStr){
    int i, isNewStatSet=FALSE, retVal=ERROR_RETVAL;
    int defaultStatus=0; // default status is "Online"

    for(i=0; i < STATUS_STATES; i++){
        if(strcmp(statusStr, Stat_cl_paramStr[i])){
            myParams->status=(char) i;
            isNewStatSet=TRUE;
        }
    }

    if(!isNewStatSet){
        myParams->status=(char) defaultStatus;
        syslog(LOG_ERR, "Status argument %s can't be parsed. Default status %s set.\n",\
               statusStr, Stat_cl_paramStr[defaultStatus]);
    }else{
        retVal=SUCCESS_RETVAL;
    }

    return retVal;
}


