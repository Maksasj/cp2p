#ifndef SOCK_H
#define SOCK_H

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>

struct sock_t;
struct duplex_sock_t;

typedef void (*sock_accept_callback_t)(struct sock_t* listen, struct duplex_sock_t* sock);

typedef struct {
    unsigned int socket;

    void* user_ptr;
    sock_accept_callback_t accept_callback;
} sock_t;

typedef struct {
    sock_t recv;
    sock_t send;
    
    void* user_ptr;
} duplex_sock_t;

typedef enum {
    SOCK_OK,
    SOCK_ERROR,
    SOCK_EMPTY
} sock_result_code_t;

typedef struct {
    sock_result_code_t code;
    const char* message;
} sock_result_t;

typedef unsigned int port_t;

sock_result_t sock_create_listen(sock_t* sock, port_t port);
sock_result_t sock_create_listen_ptr(sock_t* sock, port_t port, void* user_ptr);

sock_result_t sock_pull(sock_t* listen, duplex_sock_t** pool, unsigned int count);

#ifdef SOCK_IMPLEMENETATION

sock_result_t sock_start_duplex_sock(sock_t* sock, duplex_sock_t* duplex, unsigned int port) {
    struct sockaddr_in servaddr;
    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if ((duplex->send.socket = socket(AF_INET, SOCK_STREAM, 0))< 0){
        fprintf(stderr,"ERROR #2: cannot create socket.\n");
        exit(1);
    }

    if (inet_aton("127.0.0.1", &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    if (connect(duplex->send.socket, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    struct sockaddr_in clientaddr;
    unsigned int clientaddrlen = sizeof(clientaddr);
    memset(&clientaddr, 0, clientaddrlen);
    duplex->recv.socket = accept(sock->socket, (struct sockaddr*) &clientaddr, &clientaddrlen);

    return (sock_result_t){ .code = SOCK_OK };
}

sock_result_t sock_accept_duplex_sock(sock_t* sock, duplex_sock_t* duplex) {
    // Accept listening and setup as duplex recv
    struct sockaddr_in clientaddr;
    unsigned int clientaddrlen = sizeof(clientaddr);
    memset(&clientaddr, 0, clientaddrlen);

    duplex->recv.socket = accept(sock->socket, (struct sockaddr*) &clientaddr, &clientaddrlen);
    printf("Accepting connection from:  %s\n", inet_ntoa(clientaddr.sin_addr));
    printf("Created recv duplex\n");

    struct sockaddr_in servaddr;
    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(44444); // Todo

    // Create send socket
    if ((duplex->send.socket = socket(AF_INET, SOCK_STREAM, 0))< 0) {
        fprintf(stderr,"ERROR #2: cannot create socket.\n");
        exit(1);
    }

    if (inet_aton(inet_ntoa(clientaddr.sin_addr), &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    if (connect(duplex->send.socket, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    printf("Created send duplex\n");

    return (sock_result_t){ .code = SOCK_OK };
}

sock_result_t sock_create_listen(sock_t* sock, port_t port) {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port);

    if ((sock->socket = socket(AF_INET, SOCK_STREAM,0)) < 0)
        return (sock_result_t) { .code = SOCK_ERROR, .message = "Cannot create listening socket" };

    if (bind(sock->socket, (struct sockaddr *)&servaddr,sizeof(servaddr)) < 0)
        return (sock_result_t) { .code = SOCK_ERROR, .message = "Failed to bind listening socket" };

    if (listen(sock->socket, 5) < 0)
        return (sock_result_t){ .code = SOCK_ERROR, .message = "Failed to listen()"};

    return (sock_result_t){ .code = SOCK_OK };
}

sock_result_t sock_create_listen_ptr(sock_t* sock, port_t port, void* user_ptr) {
    sock_result_t result = sock_create_listen(sock, port); 

    sock->user_ptr = user_ptr;

    return result;
}

sock_result_t sock_duplex_send(duplex_sock_t* sock, const void* data, unsigned int size) {
    if(sock->recv.socket != -1) {
        int len = send(sock->recv.socket, data, size, 0);

        if (len <= 0)
            return (sock_result_t){ .code = SOCK_ERROR };
    }

    return (sock_result_t){ .code = SOCK_OK };
}

sock_result_t sock_pull(sock_t* listen, duplex_sock_t** pool, unsigned int count) {
    int maxfd = 0;
    
    fd_set read_set;
    FD_ZERO(&read_set);
    
    for(int i = 0; i < count; ++i) {
        duplex_sock_t* duplex = pool[i];

        if(duplex->recv.socket != -1) {
            FD_SET(duplex->recv.socket, &read_set);

            if (duplex->recv.socket > maxfd)
                maxfd = duplex->recv.socket;
        }
    }
   
    FD_SET(listen->socket, &read_set);

    if (listen->socket > maxfd)
        maxfd = listen->socket;
    
    select(maxfd + 1, &read_set, NULL, NULL, NULL);

    // handle incoming request
    if (FD_ISSET(listen->socket, &read_set)){
        duplex_sock_t* duplex = (duplex_sock_t*) malloc(sizeof(duplex_sock_t));
        sock_accept_duplex_sock(listen, duplex);

        listen->accept_callback(listen, duplex);
    }

    for(int i = 0; i < count; ++i) {
        duplex_sock_t* duplex = pool[i];

        if(duplex->recv.socket != -1) {
            if (FD_ISSET(duplex->recv.socket, &read_set)) {
                char buffer[1024];

                memset(&buffer, 0, 1024);
                int r_len = recv(duplex->recv.socket, &buffer, 1024,0);

                printf("Recived message from duplex port thing: %s\n", buffer);
            }
        }
    }
}

#endif

#endif