#include "includes/main.h"
#include <sys/ipc.h>
#include <sys/msg.h>

//////////// Client DEFINEs/////////////////////////////////////////////
#define CONN_RETR_COUNT     2
#define CONN_RETR_DELAY_MS  50
#define DEFAULT_SRV_PRT     50013
#define DEFAULT_SRV_IP_STR  "127.0.0.1"
#define ERROR_RETVAL        (-1)
#define SUCCESS_RETVAL      0
#define FTOK_PATH           "/tmp"
#define PROJ_ID             13

#define                     DBG
/////////////////////////////////////////////////////////////////////////
//////////////////// Client prototypes //////////////////////////////////
static int parseAddrStr(struct sockaddr_in* socketAddress, char *addrString);
static int parseStatusStr(peerParam_t* myParams, char * statusStr);
static int sendRecv_all(int fd, char *buf, int len, int direction);
int disconFromSev(void);
int connToSrv(void);
<<<<<<< HEAD
void * sendRecv_File(void * none);
void * updateStatus(void* none);
void * sendRecv_Mail(void * none);
void * getpeerArrMsg(void* none);
void printPeerList(peerArray_t* peAr);
void printLetter(letter_t* letter);

=======
void * sendRecv_File(void * fileT);
void * updateStatus(void* status);
void * sendRecv_Mail(void * arg);
void * getpeerArrMsg(void* arg);
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
/////////////////////////////////////////////////////////////////////////
//////////// Client Globals /////////////////////////////////////////////
int TCPfd;
struct sockaddr_in Serveraddr;
peerParam_t myParams;
char* StatusStrings[]={"Online", "Busy", "Away", "Offline"};
char* Stat_cl_paramStr[]={"on", "bu", "aw", "of"};
<<<<<<< HEAD
int ResultQ_id, CommandQ_id;

enum qmsgtype {UI_ACTION, NEW_LETTER, PEERS, STATUS, SW_OFF};
struct msg{
    enum qmsgtype mtype;
    void * payload;
=======
int ResultQueue_id;

enum qmsgtype {UI_ACTION, IN_MESSAGE, PEERS};
struct msg{
    enum qmsgtype type;
    void * data;
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
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
    char * socketStr=NULL;
    char * statusStr=NULL;
    key_t resultsQ;

    // clear socketAddress structure
    // fill it with default data
    memset(&Serveraddr, 0, sizeof(Serveraddr));
    Serveraddr.sin_family=AF_INET;
    Serveraddr.sin_port=htons(DEFAULT_SRV_PRT);
    //Serveraddr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    inet_pton(AF_INET, DEFAULT_SRV_IP_STR, (void*) &Serveraddr.sin_addr.s_addr);

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


    // copy entered name to current paramters structure
    // memset also sets myParams.status to 0 status
    memset(&myParams, 0, sizeof(myParams));
    strncpy(myParams.name, argv[optind], NAME_LEN);
    myParams.name[NAME_LEN]='\0';

    //parse address if it was passed as command opt
    if (socketStr &&\
            (parseAddrStr(&Serveraddr, socketStr) == ERROR_RETVAL)){
        syslog(LOG_ERR, "Can't parse address option");
    }

    //parse status if it was passed as command opt
    if (statusStr &&\
            (parseStatusStr(&myParams, statusStr) == ERROR_RETVAL)){
        syslog(LOG_ERR, "Can't parse status option");
    }

    // connect to socket
    if ((TCPfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        syslog(LOG_ERR, "socket creation error. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // create ipc queue
    if ((resultsQ = ftok(FTOK_PATH, PROJ_ID)) == ERROR_RETVAL){
        syslog(LOG_ERR, "Queue tocken generation failed. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }
<<<<<<< HEAD
    else if((ResultQ_id = msgget(resultsQ, IPC_CREAT | 0666)) == ERROR_RETVAL){
=======
    else if((ResultQueue_id = msgget(resultsQ, IPC_CREAT | 0666)) == ERROR_RETVAL){
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
        syslog(LOG_ERR, "Queue creation failed. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //register sig handler
    signal(SIGINT, &Exit_handler);

    return SUCCESS_RETVAL;
}

int main(int argc,char **argv)
{
<<<<<<< HEAD
    //pthread_t uiThread;
    pthread_t srvCmdThread;
    pthread_attr_t detachedAttr;

    pthread_attr_init(&detachedAttr);
    pthread_attr_setdetachstate(&detachedAttr, PTHREAD_CREATE_DETACHED);
=======
    pthread_t uiThread, srvCmdThread;
    letter_t letter;
    void * threadRetval;
    peerArray_t peersOnline;
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b

    Init(argc, argv);
    struct msg res_msg, comm_msg;

<<<<<<< HEAD
    //msgsnd(ResultQ_id, &sentmsg, sizeof(sentmsg), 0);

    // create ui thread
    // thread can send commands to CommandQ
    /*
    if(pthread_create(&uiThread, NULL, &uiThreadFunc, NULL) == ERROR_RETVAL){
        syslog(LOG_ERR, "UI thread creation failed. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }
    */

    // send status to server
    if(pthread_create(&srvCmdThread, &detachedAttr, &updateStatus, NULL) == ERROR_RETVAL){
        syslog(LOG_ERR, "Update status thread creation failed. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // getPeer array
    if(pthread_create(&srvCmdThread, &detachedAttr, &getpeerArrMsg, NULL) == ERROR_RETVAL){
        syslog(LOG_ERR, "Update status thread creation failed. Error %d\n", errno);
=======

    getpeerArrMsg(&peersOnline);

    while(1);

    // create message queue


    int received;
    struct msg sentmsg, rcvmsg;
    sentmsg.type = 2;
    sentmsg.data = "This is text";

    msgsnd(ResultQueue_id, &sentmsg, sizeof(sentmsg), 0);

    received = msgrcv(ResultQueue_id, &rcvmsg, sizeof(rcvmsg), 0, 0);

    printf("%s\n", rcvmsg.data);

    // create ui thread
    /*
    if(pthread_create(&uiThread, NULL, &uiThreadFunc, NULL) == ERROR_RETVAL){
        syslog(LOG_ERR, "UI thread creation failed. Error %d\n", errno);
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
        exit(EXIT_FAILURE);
    }
    */

    while(1)
    {
<<<<<<< HEAD
        if(msgrcv(ResultQ_id, &res_msg, sizeof(res_msg), 0, 0))
        {
            switch (res_msg.mtype) {
            case NEW_LETTER:
                printLetter((letter_t*) res_msg.payload);
                break;
            case PEERS:
                printPeerList((peerArray_t*) res_msg.payload);
                break;
            case SW_OFF:
                printf("Shutting down!\n");
                raise(SIGINT);
            default:
                printf("Type of result %d\n", res_msg.mtype);
                break;
            }
        }

        if(msgrcv(CommandQ_id, &comm_msg, sizeof(comm_msg), 0, 0)){
            switch (comm_msg.mtype) {
            case NEW_LETTER:
                // getPeer array
                if(pthread_create(&srvCmdThread, &detachedAttr, &getpeerArrMsg, NULL) == ERROR_RETVAL){
                    syslog(LOG_ERR, "Update status thread creation failed. Error %d\n", errno);
                    exit(EXIT_FAILURE);
                }
                break;
            case PEERS:

                break;
            case STATUS:

                break;
            default:
                printf("Type of command %d\n", res_msg.mtype);
                break;
            }
        }

    }

    exit(EXIT_SUCCESS);
}
=======
        // create mail check thread
        if(pthread_create(&srvCmdThread, NULL, &sendRecv_Mail, &letter) == ERROR_RETVAL){
            syslog(LOG_ERR, "UI thread creation failed. Error %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pthread_tryjoin_np(srvCmdThread, &threadRetval);
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b

void printPeerList(peerArray_t* peAr){
    unsigned int i;

    printf("Total peers online: %d\n", peAr->count);

    for(i=0; i < peAr->count; i++){
        printf("\tName: %s\t\tStatus: %d\n",\
               (peAr->peerArr + i)->name,\
               (peAr->peerArr + i)->status);
        fflush(stdout);
    }

    free(peAr->peerArr);
    free(peAr);
}

void printLetter(letter_t* letter){

    printf("%s:\n", letter->from);
    printf("%s\n", letter->text);
    fflush(stdout);

<<<<<<< HEAD
    free(letter->text);
    free(letter);
=======
    exit(EXIT_SUCCESS);
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
}

static int
parseAddrStr(struct sockaddr_in* socketAddress, char* addrString){
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
    int connectVal=0;

    while(retryCount--){
<<<<<<< HEAD
        if (connect(TCPfd, (struct sockaddr *) &Serveraddr,\
                    sizeof(Serveraddr)) == SUCCESS_RETVAL){
            return SUCCESS_RETVAL;
        }
        syslog(LOG_ERR, "Connection to server failed. Retry count %d. Error: %s",\
               retryCount, strerror(errno));
=======
        if ((connectVal = connect(TCPfd, (struct sockaddr *) &Serveraddr,\
                    sizeof(Serveraddr))) == SUCCESS_RETVAL){
            return SUCCESS_RETVAL;
        }

        syslog(LOG_ERR, "Connection to server failed. Retry count %d. Error: %s",\
               retryCount, strerror(errno));

>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
        usleep(retryDelayUs);
    }

    return ERROR_RETVAL;
}

int disconFromSev(void){
    int retVal=SUCCESS_RETVAL;

    if (close(TCPfd) == ERROR_RETVAL){
        syslog(LOG_ERR, "Socket closing failed. Error %d\n", errno);
        retVal= ERROR_RETVAL;
    }

    return retVal;
}

// sends request to server for new set of peers
// arg -- struct peerArray_s
// return: NULL on fail, struct peerArray_s pointer on success
// Warning: newPeerArray->peerArr_p must be freed
<<<<<<< HEAD
void * getpeerArrMsg(void* none){
    peerArray_t * newPeerArray;
    struct msg qMsg;
=======
void * getpeerArrMsg(void* arg){
    peerArray_t * newPeerArray=(peerArray_t *) arg;
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
    messageHeader_t peerArrMsg;
    unsigned int errorNumber=0;
    char* errorString;
    //supress gcc warnings
    (void)none;

    // allocate memory for peerArray_t
    if(!(newPeerArray=malloc(sizeof(peerArray_t)))){
        syslog(LOG_ERR, "Can't allocate memory for peerArray_t");
        pthread_exit(NULL);
    }

    peerArrMsg.serviceCode=SERVICE_CODE;
    peerArrMsg.type=(int8_t) PEER_LIST;
    peerArrMsg.size=0;

    connToSrv();

    // request to server to get all connected peers
    if(sendRecv_all(TCPfd, (char*)&peerArrMsg, sizeof(peerArrMsg), DIR_SEND) == ERROR_RETVAL)
    {
        errorNumber=errno;
        errorString="Request to server for peerArray failed. Error %d";
    }
    // recv peer array struct
    else if(sendRecv_all(TCPfd, (char*)newPeerArray, sizeof(peerArray_t), DIR_RECV) == ERROR_RETVAL){
        errorNumber=errno;
<<<<<<< HEAD
        errorString="Recv peer array struct failed. Error %d";
=======
        errorString="Recv peer array struct failed. Error %d\n";
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
    }
    //processing recieved header
    else if(newPeerArray->count)
    {
        //allocate memory for all of peerParam structs
        size_t peerArrayByteSize=sizeof(peerParam_t) * newPeerArray->count;
        newPeerArray->peerArr=malloc(peerArrayByteSize);

        // recv peerArray
        if(sendRecv_all(TCPfd, (char*)newPeerArray->peerArr, peerArrayByteSize, DIR_RECV) == ERROR_RETVAL){
            errorNumber=errno;
<<<<<<< HEAD
            errorString="Recv reply from server with message header failed. Error %d";
            free(newPeerArray->peerArr);
        }

#ifdef DBG
        unsigned int i;

        printf("Total peers online %d\n", newPeerArray->count);

        for(i=0; i < newPeerArray->count; i++){
            printf("\tName: %s\t\tStatus: %d\n",\
                   (newPeerArray->peerArr + i)->name,\
                   (newPeerArray->peerArr + i)->status);
            fflush(stdout);
        }
=======
            errorString="Recv reply from server with message header failed. Error %d\n";
            free(newPeerArray->peerArr);
        }

#ifdef DBG
        unsigned int i;

        printf("Total peers online %d\n", newPeerArray->count);

        char * cp=(newPeerArray->peerArr + 1)->name;
        uint8_t s =(newPeerArray->peerArr + 1)->status;

        for(i=0; i < newPeerArray->count; i++){
            printf("\tName: %s\t\tStatus: %d\n",\
                   (newPeerArray->peerArr + i)->name,\
                   (newPeerArray->peerArr + i)->status);
            fflush(stdout);
        }
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
#endif
    }



    disconFromSev();

    if(errorNumber != 0){
        syslog(LOG_ERR, errorString, errorNumber);
        free(newPeerArray);
        pthread_exit(NULL);
    }

    qMsg.mtype=PEERS;
    qMsg.payload=newPeerArray;
    if(msgsnd(ResultQ_id, &qMsg, sizeof(qMsg), 0) == ERROR_RETVAL){
        free(newPeerArray);
        syslog(LOG_ERR, "Failed to send peerArr message to result queue. Error %d", errno);
    }

    pthread_exit(NULL);
}

// send peerStatus to server
<<<<<<< HEAD
void * updateStatus(void* none){
    messageHeader_t statusMsg;
    peerParam_t* newStatus;
    struct msg commandParam;

    //supress gcc warning
    (void)none;

    if(msgrcv(CommandQ_id,\
              &commandParam,\
              sizeof(commandParam),\
              STATUS, 0)==ERROR_RETVAL)
    {
        syslog(LOG_ERR,\
               "Message not found in command queue. Error %s",\
               strerror(errno));
        pthread_exit(NULL);
    }

    newStatus=(peerParam_t*) commandParam.payload;
=======
void * updateStatus(void* status){
    messageHeader_t statusMsg;
    peerParam_t* newStatus=(peerParam_t*) status;
    void * retVal = status;
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b

    // fill header struct
    statusMsg.serviceCode=SERVICE_CODE;
    statusMsg.type=(int8_t) STAT;
    statusMsg.size=sizeof(peerParam_t);

    connToSrv();

    printf("Name %s stat %d", newStatus->name, newStatus->status);
    fflush(stdout);

    // send message with status
    if(!sendRecv_all(TCPfd, (char*)&statusMsg, sizeof(statusMsg), DIR_SEND) &&\
       !sendRecv_all(TCPfd, (char*)newStatus, sizeof(peerParam_t), DIR_SEND))
    {
        syslog(LOG_ERR, "Send message of type STATus failed. Error %s", strerror(errno));
<<<<<<< HEAD
=======
        retVal = NULL;
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
    }

    disconFromSev();

<<<<<<< HEAD
    free(newStatus);
=======
    return retVal;
}
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b

    pthread_exit(NULL);
}

// call this func to ask server for pressence of new messages
// if passed param of type letter_t has newLetter->textSize > 0
// than sends message else receives
// return: NULL when failes and arg ptr when succedes
<<<<<<< HEAD
void * sendRecv_Mail(void * none){
=======
void * sendRecv_Mail(void * arg){
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
    messageHeader_t pollMsg;
    letter_t* newLetter;
    struct msg qMsg;
    int dirSndRcv=DIR_RECV;
    unsigned int errorNumber=0;
    char* errorString;
    //supress gcc warnings
    (void)none;

    pollMsg.serviceCode=SERVICE_CODE;
    pollMsg.type=LETTER;

<<<<<<< HEAD
    if(msgrcv(CommandQ_id, &qMsg, sizeof(qMsg),\
              NEW_LETTER, 0) != ERROR_RETVAL)
    {
        printf("Letter to send found\n");
        dirSndRcv=DIR_SEND;

        newLetter=(letter_t*) qMsg.payload;
    }
    // no letter to send or error. allocate space for
    // newLetter to check for new mail
    else if(!(newLetter=malloc(sizeof(letter_t))))
    {
        syslog(LOG_ERR, "Cant't allocate memory for letter_t");
        pthread_exit(NULL);
    }
=======
    if(!arg){
        printf("sendRecv_Mail func. Passed param eqv null!\n");
        return ERROR_RETVAL;
    }

    connToSrv();
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b

    connToSrv();

<<<<<<< HEAD
    if (!dirSndRcv){
=======

    if (isSend){
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
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
#ifndef DBG
            free(newLetter->text);
<<<<<<< HEAD
            free(newLetter);
=======
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
#endif
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
        // recv letter struct
        else if (sendRecv_all(TCPfd, (char*)newLetter, sizeof(letter_t), DIR_RECV) == ERROR_RETVAL)
        {
            errorNumber=errno;
            errorString="Recieving letter from server error. Reason %d.\n";
        }
        // if recieved message header size is greater that 0
        // than there is new letter
        else if (newLetter->textSize > 0)
        {
            // recv letter text
            newLetter->text=malloc(newLetter->textSize);

            //recv letter text
            if(sendRecv_all(TCPfd, (char*)newLetter->text, newLetter->textSize, DIR_RECV) == ERROR_RETVAL)
<<<<<<< HEAD
            {
                errorNumber=errno;
                errorString="Recieving letter text from server. Reason %d.\n";
            }
            // letter received and can be added to queue
            else
            {
                qMsg.mtype=NEW_LETTER;
                qMsg.payload=newLetter;
                if(msgsnd(ResultQ_id, &qMsg, sizeof(qMsg), 0) == ERROR_RETVAL){
                    errorNumber=errno;
                    errorString="Failed to send newLetter message to result queue. Reason %d.";
                }
=======
            {
                free(newLetter->text);
                errorNumber=errno;
                errorString="Recieving letter text from server. Reason %d.\n";
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
            }
        }
    }

    disconFromSev();

    if(errorNumber){
        syslog(LOG_ERR, errorString, errorNumber);

        free(newLetter->text);
        free(newLetter);
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

<<<<<<< HEAD
void * sendRecv_File(void * none){
    //supress gcc warnings
    (void)none;

=======
void * sendRecv_File(void * fileT){
>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
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
<<<<<<< HEAD
=======


>>>>>>> e1024852260d6eb2780281e70076ca1581ac7b9b
