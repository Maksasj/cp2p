#include <pthread.h>

#include <stdio.h>

#define HAUL_IMPLEMENTATION
#include "haul/haul.h"

#include "config.h"
#include "packet.h"
#include "socket.h"

typedef struct {
    unsigned char running;

    node_config_t config;

    vector_t send_sink;
    vector_t recv_sink;
} node_state_t;

void receiving_thread_callback(void* ptr) {
    node_state_t* state = (node_state_t*) ptr;

    // Create listening socket
    socket_t listen_sock;

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(state->config.port);

    if ((listen_sock = socket(AF_INET, SOCK_STREAM,0)) < 0) {
        PLUM_LOG(PLUM_ERROR, "Cannot create listening socket");
        exit(1);
    }

    PLUM_LOG(PLUM_TRACE, "Created socket with port %d", state->config.port);

    if (bind(listen_sock, (struct sockaddr *)&servaddr,sizeof(servaddr)) < 0) {
        PLUM_LOG(PLUM_ERROR, "Failed to bind listening socket");
        exit(1);
    }

    if (listen(listen_sock, 5) < 0) {
        PLUM_LOG(PLUM_ERROR, "Failed to listen()");
        exit(1);
    }

    PLUM_LOG(PLUM_TRACE, "Created listening socket");
    while(state->running) {
        int maxfd = 0;
    
        fd_set read_set;
        FD_ZERO(&read_set);
        
        for(int i = 0; i < vector_size(&state->recv_sink); ++i) {
            recv_sock_t* sock = vector_get(&state->recv_sink, i);

            if(sock->socket != -1) {
                FD_SET(sock->socket, &read_set);

                if (sock->socket > maxfd)
                    maxfd = sock->socket;
            }
        }

        FD_SET(listen_sock, &read_set);
        if (listen_sock > maxfd)
            maxfd = listen_sock;
        
        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        // handle incoming request
        if (FD_ISSET(listen_sock, &read_set)) {
            recv_sock_t* sock = (recv_sock_t*) malloc(sizeof(recv_sock_t));

            struct sockaddr_in clientaddr;
            unsigned int clientaddrlen = sizeof(clientaddr);
            memset(&clientaddr, 0, clientaddrlen);

            sock->socket = accept(listen_sock, (struct sockaddr*) &clientaddr, &clientaddrlen);
            PLUM_LOG(PLUM_INFO, "Accepting connection from %s", inet_ntoa(clientaddr.sin_addr));

            vector_push(&state->recv_sink, sock);
        }

        for(int i = 0; i < vector_size(&state->recv_sink); ++i) {
            recv_sock_t* sock = vector_get(&state->recv_sink, i);

            if(sock->socket != -1) {
                if (FD_ISSET(sock->socket, &read_set)) {
                    char buffer[1024];

                    memset(&buffer, 0, 1024);
                    int r_len = recv(sock->socket, &buffer, 1024,0);

                    PLUM_LOG(PLUM_EXPERIMENTAL, "Received message from duplex port thing: '%s'", buffer);
                }
            }
        }
    }
}

void create_send_sock(node_state_t* state, const char* adress, port_t port) {
    send_sock_t* send = (send_sock_t*) malloc(sizeof(send_sock_t));

    if ((send->socket = socket(AF_INET, SOCK_STREAM, 0))< 0){
        fprintf(stderr,"ERROR #2: cannot create socket.\n");
        exit(1);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_aton(adress, &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    if (connect(send->socket, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    vector_push(&state->send_sink, send);
}

void sending_thread_callback(void* ptr) {
    node_state_t* state = (node_state_t*) ptr;

    // Create sending socket
    if(state->config.type == NODE_SLAVE)
        create_send_sock(state, "127.0.0.1", state->config.connect);

    // 1 Second timer
    int timer1s = timerfd_create(CLOCK_REALTIME,  0);
    struct itimerspec spec1s = { { 1, 0 }, { 1, 0 } };
    timerfd_settime(timer1s, 0, &spec1s, NULL);

    while(state->running) {
        int maxfd = 0;
    
        fd_set read_set;
        FD_ZERO(&read_set);
        
        FD_SET(timer1s, &read_set);
        if(timer1s > maxfd)
            maxfd = timer1s;

        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        // Broadcast PING message 
        if(FD_ISSET(timer1s, &read_set)) {
            PLUM_LOG(PLUM_INFO, "Broadcasting ping message");

            for(int i = 0; i < vector_size(&state->send_sink); ++i) {
                send_sock_t* sock = vector_get(&state->send_sink, i);

                char buffer[] = "Ping message";

                packet_t packet = (packet_t) { .type = PING_PACKET };
                write(sock->socket, &packet, sizeof(packet));
            }

            timerfd_settime(timer1s, 0, &spec1s, NULL);
        }
    }
} 

int main(int argc, char *argv[]) {
    node_config_t config;
    load_node_config(&config, argv[1]);

    if(config.type == NODE_MASTER)
        PLUM_LOG(PLUM_INFO, "Starting node in MASTER mode");
    else
        PLUM_LOG(PLUM_INFO, "Starting node in SLAVE mode");

    node_state_t state;
    state.running = 1;
    state.config = config;

    create_vector(&state.send_sink, 100);
    create_vector(&state.recv_sink, 100);

    pthread_t listening_thread, receiving_thread;
    pthread_create(&listening_thread, NULL, receiving_thread_callback, (void*) &state);
    pthread_create(&receiving_thread, NULL, sending_thread_callback, (void*) &state);

    pthread_join( listening_thread, NULL);
    pthread_join( receiving_thread, NULL); 

    return 0;
}
