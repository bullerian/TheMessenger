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
#define PROJ_ID_1           13
#define PROJ_ID_2           31


#define                     DBG
/////////////////////////////////////////////////////////////////////////
//////////// Client Globals /////////////////////////////////////////////
int TCPfd;
struct sockaddr_in Serveraddr;
peerParam_t myParams;
char* StatusStrings[]={"Online", "Busy", "Away", "Offline"};
char* Stat_cl_paramStr[]={"on", "bu", "aw", "of"};
int ResultQ_id, CommandQ_id;

enum qmsgtype {UI_ACTION, NEW_LETTER, PEERS, STATUS, SW_OFF};
struct msg{
    enum qmsgtype mtype;
    void * payload;
};
/////////////////////////////////////////////////////////////////////////
//////////////////// Client prototypes //////////////////////////////////
// init routine func
static int parseAddrStr(struct sockaddr_in* socketAddress, char *addrString);
static int parseStatusStr(peerParam_t* myParams, char * statusStr);
static int sendRecv_all(int fd, char *buf, int len, int direction);
static int createQ(int proj_id, char * ftok_path, char * descr);
// thread spawner
int addCommand(struct msg* command);
// server communication thread func
void * sendRecv_File(void * none);
void * updateStatus(void* none);
void * sendRecv_Mail(void * none);
void * getpeerArrMsg(void* none);
int disconFromServ(void);
int connToSrv(void);
// stubs
void printPeerList(peerArray_t* peAr);
void printLetter(letter_t* letter);
/////////////////////////////////////////////////////////////////////////
//////////////////////////PROGRAM////////////////////////////////////////

void Exit_handler(int signum){
    close(TCPfd);
    syslog(LOG_DEBUG, "Terminated by signal %d\n", signum);
    closelog();
    exit(EXIT_FAILURE);
}

int Init(int argc, char **argv){
    int opt, i;
    char * socketStr=NULL;
    char * statusStr=NULL;


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
    while ((opt = getopt(argc, argv, "a:s:h")) != -1) {
        switch (opt) {
        case 'a':
            socketStr=optarg;
            break;
        case 's':
            statusStr=optarg;
            break;
        case 'h':
            printf("Usage: nickname [-a serverIP:port] [-s status]\n"
                   "Nickname will be truncated to 30 characters max\n"
                   "Default socket %s:%d, default status %s\n"
                   "The 'status' can be one of: ",\
                   DEFAULT_SRV_IP_STR, DEFAULT_SRV_PRT, StatusStrings[0]);
            for (i=0; i < STATUS_STATES; i++){
                printf("\n\t'%s' for %s\n", Stat_cl_paramStr[i], StatusStrings[i]);
            }

            exit(EXIT_SUCCESS);
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
        syslog(LOG_ERR, "socket creation error. Error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // create ipc result and command queues
    if((ResultQ_id=createQ(PROJ_ID_1, FTOK_PATH, "Result")) == ERROR_RETVAL ||\
        (CommandQ_id=createQ(PROJ_ID_2, FTOK_PATH, "Command"))== ERROR_RETVAL)
    {
        syslog(LOG_ERR, "Create ipc result or command queues failed.");
        exit(EXIT_FAILURE);
    }

    //register sig handler
    signal(SIGINT, &Exit_handler);

    return SUCCESS_RETVAL;
}

int main(int argc,char **argv)
{
    //pthread_t uiThread;
    pthread_attr_t detachedAttr;

    pthread_attr_init(&detachedAttr);
    pthread_attr_setdetachstate(&detachedAttr, PTHREAD_CREATE_DETACHED);

    Init(argc, argv);
    struct msg tmp_msg;

    // create ui thread
    // thread can send commands with addCommand(struct msg* qm)
    // UI sposed to be based on NCURSES pseudo graphics lib
    /*
    if(pthread_create(&uiThread, NULL, &uiThreadFunc, NULL) == ERROR_RETVAL){
        syslog(LOG_ERR, "UI thread creation failed. Error %d\n", errno);
        exit(EXIT_FAILURE);
    }
    */

    // update status
    tmp_msg.mtype=STATUS;
    tmp_msg.payload=&myParams;
    addCommand(&tmp_msg);

    tmp_msg.mtype=PEERS;
    tmp_msg.payload=NULL;
    addCommand(&tmp_msg);


    // the program loop
    while(1)
    {
        if(msgrcv(ResultQ_id, &tmp_msg, sizeof(tmp_msg), 0, 0))
        {
            switch (tmp_msg.mtype) {
            case NEW_LETTER:
                printLetter((letter_t*) tmp_msg.payload);
                break;
            case PEERS:
                printPeerList((peerArray_t*) tmp_msg.payload);
                break;
            case SW_OFF:
                printf("Shutting down!\n");
                raise(SIGINT);
            default:
                printf("Type of result %d\n", tmp_msg.mtype);
                break;
            }
        }

        // the UI thread calls commands with addCommand(struct msg* qm)

    }

    exit(EXIT_SUCCESS);
}


////
/// \brief Create IPC queue
/// \param proj_id  Random number
/// \param Path     Path to some existing file in the system
/// \param descr    Short text description of queue (for dbg purposes)
/// \return         New Queue id on success, ERROR_RETVAL of failure
///
static int
createQ(int proj_id, char * ftok_path, char * descr){
    key_t tmp_key;
    int retVal=ERROR_RETVAL;

    // create ipc queue
    if ((tmp_key = ftok(ftok_path, proj_id)) == ERROR_RETVAL){
        syslog(LOG_ERR, "%s queue tocken generation failed. Error %d\n", descr, errno);
    }
    else if((retVal = msgget(tmp_key, IPC_CREAT | 0666)) == ERROR_RETVAL){
        syslog(LOG_ERR, "%s queue creation failed. Error %d\n", descr, errno);
    }

    return retVal;
}

int addCommand(struct msg* command){
    pthread_t tmp_th;
    pthread_attr_t detachedAttr;
    int retVal=SUCCESS_RETVAL;

    pthread_attr_init(&detachedAttr);
    pthread_attr_setdetachstate(&detachedAttr, PTHREAD_CREATE_DETACHED);

    if(!command)
    {
        syslog(LOG_ERR, "Null queue command pointer. Error %d\n", errno);
        //return from func
    }
    else if(msgsnd(CommandQ_id, command, sizeof(struct msg), 0) == ERROR_RETVAL)
    {
        syslog(LOG_ERR, "Adding command to commQ failed. Error %d\n", errno);
        retVal=ERROR_RETVAL;
    }
    // the command was succesfuly added. new thread may be started
    else
    {
        switch (command->mtype) {
            case NEW_LETTER:
                if(pthread_create(&tmp_th, &detachedAttr, &sendRecv_Mail, NULL) == ERROR_RETVAL){
                    syslog(LOG_ERR, "Mail check thread creation failed. Error %d\n", errno);
                    retVal=ERROR_RETVAL;
                }
                break;
            case PEERS:
                if(pthread_create(&tmp_th, &detachedAttr, &getpeerArrMsg, NULL) == ERROR_RETVAL){
                    syslog(LOG_ERR, "GetPeerArray thread creation failed. Error %d\n", errno);
                    retVal=ERROR_RETVAL;
                }
                break;
            case STATUS:
                if(pthread_create(&tmp_th, &detachedAttr, &updateStatus, NULL) == ERROR_RETVAL){
                    syslog(LOG_ERR, "UpdateStatus thread creation failed. Error %d\n", errno);
                    retVal=ERROR_RETVAL;
                }
                break;
            default:
                syslog(LOG_ERR, "Unknown comm queue msg type %d.", command->mtype);
                retVal=ERROR_RETVAL;
                break;
        }
    }

    return retVal;
}

void printPeerList(peerArray_t* peAr){
    unsigned int i;

    printf("Total peers online: %d\n", peAr->count);

    for(i=0; i < peAr->count; i++){
        printf("\tName: %s\t\tStatus: %s\n",\
               (peAr->peerArr + i)->name,\
               StatusStrings[(peAr->peerArr + i)->status]);
        fflush(stdout);
    }

    free(peAr->peerArr);
    free(peAr);
}

void printLetter(letter_t* letter){

    printf("%s:\n", letter->from);
    printf("%s\n", letter->text);
    fflush(stdout);

    free(letter->text);
    free(letter);
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

    while(retryCount--){
        if (connect(TCPfd, (struct sockaddr *) &Serveraddr,\
                    sizeof(Serveraddr)) == SUCCESS_RETVAL){
            return SUCCESS_RETVAL;
        }
        syslog(LOG_ERR, "Connection to server failed. Retry count %d. Error: %s",\
               retryCount, strerror(errno));
        usleep(retryDelayUs);
    }

    return ERROR_RETVAL;
}

int disconFromServ(void){
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
void * getpeerArrMsg(void* none){
    peerArray_t * newPeerArray;
    struct msg qMsg;
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
        errorString="Recv peer array struct failed. Error %d";
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
#endif
    }

    disconFromServ();

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

    // fill header struct
    statusMsg.serviceCode=SERVICE_CODE;
    statusMsg.type=(int8_t) STAT;
    statusMsg.size=sizeof(peerParam_t);

    connToSrv();

    // send message with status
    if(!sendRecv_all(TCPfd, (char*)&statusMsg, sizeof(statusMsg), DIR_SEND) &&\
       !sendRecv_all(TCPfd, (char*)newStatus, sizeof(peerParam_t), DIR_SEND))
    {
        syslog(LOG_ERR, "Send message of type STATus failed. Error %s", strerror(errno));
    }

    disconFromServ();

    //peerParam_t* newStatus is pointing to global var.;
    //free(newStatus);

    pthread_exit(NULL);
}

// call this func to ask server for pressence of new messages
// if passed param of type letter_t has newLetter->textSize > 0
// than sends message else receives
// return: NULL when failes and arg ptr when succedes
void * sendRecv_Mail(void * none){
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

    connToSrv();

    if (!dirSndRcv){
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
            free(newLetter);
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
            }
        }
    }

    disconFromServ();

    if(errorNumber){
        syslog(LOG_ERR, errorString, errorNumber);

        free(newLetter->text);
        free(newLetter);
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

void * sendRecv_File(void * none){
    //supress gcc warnings
    (void)none;

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
