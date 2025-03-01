#ifndef PACKET_H
#define PACKET_H

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/timerfd.h>

#include <string.h>
#include <unistd.h>

#include "socket.h"

typedef struct {
    struct {
        char address[32];
        unsigned int port;
    } source;
    
    enum {
        PING_PACKET,
        BROADCAST_CONNECT_REQUEST,
    } type;
    
    union {
        struct { } ping;
        struct { 
            int life; 
        } broadcast_connect_request;
    } as;
} packet_t;

#endif