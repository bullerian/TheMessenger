#ifndef MAIN_H
#define MAIN_H

#define _GNU_SOURCE
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
#include <time.h>
#include <signal.h>
#include <pthread.h>

#define NAME_LEN            30
#define SERVICE_CODE        0xFAFAFAFA
#define DIR_SEND            0
#define DIR_RECV            1
#define STATUS_STATES       4
#define FALSE               0
#define TRUE                (!FALSE)
#define TEXT_MAX_LEN        251

enum status_e {ONLINE, BUSY, AWAY, OFFLINE};
enum contentType_e {PEER_LIST, LETTER, STAT, FILE_TRANS, NOP};

typedef struct peerParam{
    uint8_t name[NAME_LEN];
    uint8_t status;
} peerParam_t;

typedef struct {
    uint32_t serviceCode;
    int8_t type;
    uint32_t size;
} messageHeader_t;

typedef struct {
    uint8_t from[NAME_LEN];
    uint32_t textSize;
    uint8_t* text;
} letter_t;

typedef struct {
    uint8_t recipientName[NAME_LEN];
    uint8_t fileName[NAME_LEN];
    uint32_t fsize;
    uint8_t * data;
} file_t;


#endif





