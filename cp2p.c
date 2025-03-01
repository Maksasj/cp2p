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

unsigned char send_sock_exist(node_state_t* state, char* address, unsigned int port);
void handle_packet_callback(node_state_t* state, packet_t packet);
void create_send_sock(node_state_t* state, const char* address, port_t port);
void broadcast_packet(node_state_t* state, packet_t packet);

void* receiving_thread_callback(void* ptr);
void* sending_thread_callback(void* ptr);
void* status_thread_callback(void* ptr);

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

    pthread_t listening_thread, receiving_thread, status_thread;
    pthread_create(&listening_thread, NULL, receiving_thread_callback, (void*) &state);
    pthread_create(&receiving_thread, NULL, sending_thread_callback, (void*) &state);
    pthread_create(&status_thread, NULL, status_thread_callback, (void*) &state);

    pthread_join( listening_thread, NULL);
    pthread_join( receiving_thread, NULL); 

    return 0;
}

unsigned char send_sock_exist(node_state_t* state, char* address, unsigned int port) {
    for(int i = 0; i < vector_size(&state->send_sink); ++i) {
        send_sock_t* sock = vector_get(&state->send_sink, i);

        if(sock->port != port)
            continue;
        
        if(strcmp(sock->address, address) != 0)
            continue;

        return 1;
    }

    return 0;
}

void handle_packet_callback(node_state_t* state, packet_t packet) {
    if (packet.type == PING_PACKET)
        PLUM_LOG(PLUM_INFO, "Received ping packet from %s:%d", packet.source.address, packet.source.port);

    if (packet.type == BROADCAST_CONNECT_REQUEST) {
        // PLUM_LOG(PLUM_INFO, "Received broadcast connect request packet from %s:%d with %d life level", packet.source.address, packet.source.port, packet.as.broadcast_connect_request.life);

        if(!send_sock_exist(state, packet.source.address, packet.source.port) && state->config.port != packet.source.port) {
            PLUM_LOG(PLUM_INFO, "Creating a new send connection from request, for %s:%d", packet.source.address, packet.source.port);
            create_send_sock(state, packet.source.address, packet.source.port);
        }

        if(packet.as.broadcast_connect_request.life > 0) {
            --packet.as.broadcast_connect_request.life;
            broadcast_packet(state, packet);
        }
    }
}

void create_send_sock(node_state_t* state, const char* address, port_t port) {
    send_sock_t* send = (send_sock_t*) malloc(sizeof(send_sock_t));

    strcpy(send->address, address);
    send->port = port;

    if ((send->socket = socket(AF_INET, SOCK_STREAM, 0))< 0){
        PLUM_LOG(PLUM_ERROR, "Cannnot create socket");
        exit(1);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_aton(address, &servaddr.sin_addr) <= 0 ) {
        PLUM_LOG(PLUM_ERROR, "Invalid IP address");
        exit(1);
    }

    if (connect(send->socket, (struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        PLUM_LOG(PLUM_ERROR, "Error in connect()");
        exit(1);
    }

    vector_push(&state->send_sink, send);
}

void broadcast_packet(node_state_t* state, packet_t packet) {
    vector_t existing_send;
    create_vector(&existing_send, 100);

    for(int i = 0; i < vector_size(&state->send_sink); ++i) {
        recv_sock_t* sock = vector_get(&state->send_sink, i);

        int len = send(sock->socket, &packet, sizeof(packet_t), MSG_NOSIGNAL);
        
        if (len > 0) {
            vector_push(&existing_send, sock);
        } else {
            PLUM_LOG(PLUM_WARNING, "Connection is shutdown");
        }
    }

    free_vector(&state->send_sink);
    state->send_sink = existing_send;
}

void* receiving_thread_callback(void* ptr) {
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

        vector_t existing_recv;
        create_vector(&existing_recv, 100);

        for(int i = 0; i < vector_size(&state->recv_sink); ++i) {
            recv_sock_t* sock = vector_get(&state->recv_sink, i);

            if (FD_ISSET(sock->socket, &read_set)) {
                packet_t packet;

                int len = recv(sock->socket, &packet, sizeof(packet_t) ,0);
                
                if (len > 0) {
                    handle_packet_callback(state, packet);
                    vector_push(&existing_recv, sock);
                } else {
                    if(len == -1) {
                        PLUM_LOG(PLUM_WARNING, "Failed to receive");
                    } else if (len == 0) {
                        PLUM_LOG(PLUM_WARNING, "Connection is shutdown");
                    }
                }
            } else {
                vector_push(&existing_recv, sock);
            }
        }

        free_vector(&state->recv_sink);
        state->recv_sink = existing_recv;
    }

    return NULL;
}

void* sending_thread_callback(void* ptr) {
    node_state_t* state = (node_state_t*) ptr;

    // Create sending socket
    if(state->config.type == NODE_SLAVE)
        create_send_sock(state, "127.0.0.1", state->config.connect);

    // 1 Second timer
    int timer1s = timerfd_create(CLOCK_REALTIME,  0);
    struct itimerspec spec1s = { { 1, 0 }, { 1, 0 } };
    timerfd_settime(timer1s, 0, &spec1s, NULL);

    // 3 Second timer
    int timer3s = timerfd_create(CLOCK_REALTIME,  0);
    struct itimerspec spec3s = { { 3, 0 }, { 3, 0 } };
    timerfd_settime(timer3s, 0, &spec3s, NULL);

    while(state->running) {
        int maxfd = 0;
    
        fd_set read_set;
        FD_ZERO(&read_set);
        
        FD_SET(timer1s, &read_set);
        if(timer1s > maxfd)
            maxfd = timer1s;

        FD_SET(timer3s, &read_set);
        if(timer3s > maxfd)
            maxfd = timer3s;

        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        // Broadcast PING message
        if(FD_ISSET(timer1s, &read_set)) {
            PLUM_LOG(PLUM_INFO, "Broadcasting ping message");

            packet_t packet = (packet_t) { 
                .source = {
                    .address = "127.0.0.1",
                    .port = state->config.port
                },
                .type = PING_PACKET 
            };

            broadcast_packet(state, packet);

            timerfd_settime(timer1s, 0, &spec1s, NULL);
        }

        // Broadcast connect message
        if(FD_ISSET(timer3s, &read_set)) {
            PLUM_LOG(PLUM_INFO, "Broadcasting connect message");

            packet_t packet = (packet_t) { 
                .source = {
                    .address = "127.0.0.1",
                    .port = state->config.port
                },
                .type = BROADCAST_CONNECT_REQUEST,
                .as.broadcast_connect_request = {
                    .life = 3
                }
            };

            broadcast_packet(state, packet);

            timerfd_settime(timer3s, 0, &spec3s, NULL);
        }
    }

    return NULL;
} 

void* status_thread_callback(void* ptr) {
    node_state_t* state = (node_state_t*) ptr;

    // 3 Second timer
    int timer3s = timerfd_create(CLOCK_REALTIME,  0);
    struct itimerspec spec3s = { { 3, 0 }, { 3, 0 } };
    timerfd_settime(timer3s, 0, &spec3s, NULL);

    while(state->running) {
        int maxfd = 0;
    
        fd_set read_set;
        FD_ZERO(&read_set);
        
        FD_SET(timer3s, &read_set);
        if(timer3s > maxfd)
            maxfd = timer3s;

        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        // Broadcast PING message
        if(FD_ISSET(timer3s, &read_set)) {
            PLUM_LOG(PLUM_DEBUG, "Connected recv sinks: %d", state->recv_sink.stored);
            PLUM_LOG(PLUM_DEBUG, "Connected send sinks: %d", state->send_sink.stored);

            timerfd_settime(timer3s, 0, &spec3s, NULL);
        }
    }

    return NULL;
}
