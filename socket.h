#ifndef SOCKET_H
#define SOCKET_H

typedef unsigned int socket_t;

typedef struct {
    socket_t socket;
} send_sock_t;

typedef struct {
    socket_t socket;
} recv_sock_t;

#endif