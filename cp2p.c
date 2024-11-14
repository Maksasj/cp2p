#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>

#define BUFFLEN 1024

#define MAXCLIENTS 10
#define MAXCONNECTIONS 10

unsigned int get_port(char* port_string) {
    unsigned int port = atoi(port_string);

    if ((port < 1) || (port > 65535)){
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        exit(1);
    }

    return port;
}

typedef unsigned int socket_t;

typedef struct {
    socket_t recv;
    socket_t send;
} duplex_socket_t;

socket_t setup_listening_socket(unsigned int port) {
    socket_t l_socket; // Self listening socket

    // Setup listening socket
    struct sockaddr_in servaddr;
    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port);

    if ((l_socket = socket(AF_INET, SOCK_STREAM,0)) < 0){
        fprintf(stderr, "ERROR #2: cannot create listening socket.\n");
        return -1;
    }

    if (bind(l_socket, (struct sockaddr *)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr, "ERROR #3: bind listening socket.\n");
        return -1;
    }

    if (listen(l_socket, 5) < 0){
        fprintf(stderr, "ERROR #4: error in listen().\n");
        return -1;
    }     

    return l_socket;
}

duplex_socket_t initialize_duplex_connection(socket_t, unsigned int target_port) {
    duplex_socket_t duplex;
    duplex.recv = -1;
    duplex.send = -1;

    struct sockaddr_in servaddr;
    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(target_port);

    if ((duplex.send = socket(AF_INET, SOCK_STREAM, 0))< 0){
        fprintf(stderr,"ERROR #2: cannot create socket.\n");
        exit(1);
    }

    if (inet_aton("127.0.0.1", &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    if (connect(duplex.send, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    struct sockaddr_in clientaddr;
    unsigned int clientaddrlen = sizeof(clientaddr);
    memset(&clientaddr, 0, clientaddrlen);
    duplex.recv = accept(l_socket, (struct sockaddr*) &clientaddr, &clientaddrlen);

    return duplex;
}


duplex_socket_t handle_incoming_duplex_connection(socket_t l_socket) {
    duplex_socket_t duplex;
    duplex.recv = -1;
    duplex.send = -1;

    // Accept listening and setup as duplex recv
    struct sockaddr_in clientaddr;
    unsigned int clientaddrlen = sizeof(clientaddr);
    memset(&clientaddr, 0, clientaddrlen);

    duplex.recv = accept(l_socket, (struct sockaddr*) &clientaddr, &clientaddrlen);
    printf("Accepting connection from:  %s\n", inet_ntoa(clientaddr.sin_addr));
    printf("Created recv duplex\n");

    struct sockaddr_in servaddr;
    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(11111);

    // Create send socket
    if ((duplex.send = socket(AF_INET, SOCK_STREAM, 0))< 0) {
        fprintf(stderr,"ERROR #2: cannot create socket.\n");
        exit(1);
    }

    if (inet_aton(inet_ntoa(clientaddr.sin_addr), &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    if (connect(duplex.send, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    printf("Created send duplex\n");

    return duplex;
}

int main(int argc, char *argv[]){
    unsigned int port = get_port(argv[1]);
    socket_t l_socket = setup_listening_socket(port);

    duplex_socket_t duplex;
    duplex.recv = -1;
    duplex.send = -1;

    // establish connection request
    if(argc == 3)
       duplex = initialize_duplex_connection(l_socket, get_port(argv[2]));

    int maxfd = 0;
    for (;;){
        fd_set read_set;
        FD_ZERO(&read_set);

        if(duplex.recv != -1) {
            FD_SET(duplex.recv, &read_set);
            if (duplex.recv > maxfd){
                maxfd = duplex.recv;
            }
        }

        FD_SET(l_socket, &read_set);
        if (l_socket > maxfd){
            maxfd = l_socket;
        }
        
        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        // handle incoming request
        if(duplex.recv == -1 && duplex.send == -1) {
            if (FD_ISSET(l_socket, &read_set)){
                duplex = handle_incoming_duplex_connection(l_socket);
            }
        }

        // accept message from duplex
        if(duplex.recv != -1) {
            if (FD_ISSET(duplex.recv, &read_set)) {
                char buffer[BUFFLEN];

                memset(&buffer,0,BUFFLEN);
                int r_len = recv(duplex.recv, &buffer,BUFFLEN,0);

                printf("Recived message from duplex port thing: %s\n", buffer);
            }
        }

        // send message to duplex
        if(duplex.send != -1) {
            char buffer[100] = { '\0' };
            sprintf(buffer, "This is message from %s", argv[1]);

            int w_len = send(duplex.send, buffer, sizeof(buffer), 0);

            if (w_len <= 0){
                printf("Failed to send");
                exit(1);
            }
        }
    }

    return 0;
}
