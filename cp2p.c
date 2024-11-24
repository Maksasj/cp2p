#define SOCK_IMPLEMENETATION
#include "sock.h"

#define HAUL_IMPLEMENTATION
#include "haul/haul.h"

#include "config.h"

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

typedef struct {
    sock_t listen;

    vector_t sockets;
} node_state_t;

void accept_callback(sock_t* listen, duplex_sock_t* sock) {
    node_state_t* state = (node_state_t*) listen->user_ptr;

    vector_push(&state->sockets, sock);

    printf("Node accepted new duplex connection\n");
}

int main(int argc, char *argv[]) {
    node_config_t config;
    load_node_config(&config, argv[1]);

    if(config.type == NODE_MASTER)
        PLUM_LOG(PLUM_INFO, "Starting node in MASTER mode");
    else
        PLUM_LOG(PLUM_INFO, "Starting node in SLAVE mode");

    node_state_t state;
    sock_create_listen_ptr(&state.listen, config.port, &state);
    create_vector(&state.sockets, 100);
    state.listen.accept_callback = accept_callback;

    if(config.type == NODE_SLAVE) {
        duplex_sock_t* sock = (duplex_sock_t*) malloc(sizeof(duplex_sock_t));
        sock_start_duplex_sock(&state.listen, sock, config.connect);
        vector_push(&state.sockets, sock);  
    }

    for (;;) {
        PLUM_LOG(PLUM_TRACE, "Pulling sock events");
        sock_pull(&state.listen, state.sockets.items, state.sockets.stored);

        for(int i = 0; i < state.sockets.stored; ++i) {
            PLUM_LOG(PLUM_TRACE, "Sending message with duplex connection");
            duplex_sock_t* duplex = state.sockets.items[i];
            
            const char* message = "This is a test message";
            unsigned int length = strlen(message);

            sock_duplex_send(duplex, message, length);
        }
    }

    return 0;
}
