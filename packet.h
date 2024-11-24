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
    enum {
        PING_PACKET
    } type;

    union {
        struct {
            
        } ping;
    } as;
} packet_t;

void send_packet(send_sock_t socket, packet_t packet) {
    write(socket.socket, &packet, sizeof(packet));
}

#endif