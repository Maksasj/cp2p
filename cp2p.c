#define SOCK_IMPLEMENETATION
#include "sock.h"

#define HAUL_IMPLEMENTATION
#include "haul/haul.h"

#define TRITON_IMPLEMENTATION
#include "triton/triton.h"

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

typedef enum {
    NODE_MASTER,
    NODE_SLAVE
} node_type_t;

typedef struct {
    node_type_t type;
    port_t port;

    port_t connect;
} node_config_t;

void load_node_config(node_config_t* config, const char* file_path) {
    FILE *stream = stream = fopen(file_path, "r");

    char* buffer = 0;
    long length;

    fseek (stream, 0, SEEK_END);
    length = ftell(stream);
    fseek (stream, 0, SEEK_SET);
    buffer = malloc (length);
    
    if (buffer == NULL) {
        fprintf(stderr, "Failed to read file");
        return 1;
    }

    fread(buffer, 1, length, stream);

    triton_json_t json;
    triton_parse_result_t result = triton_parse_json(&json, buffer);

    if(result.code == TRITON_ERROR) {
        printf("Failed to parse config file\n");
    }

    triton_value_entry_t* role = triton_object_get(&json.value.as.object, 0);
    ++role->value.as.string;
    role->value.as.string[strlen(role->value.as.string) - 1] = '\0';

    triton_value_entry_t* port = triton_object_get(&json.value.as.object, 1);
    ++port->value.as.string;
    port->value.as.string[strlen(port->value.as.string) - 1] = '\0';

    if(strcmp(role->value.as.string, "master") == 0) {
        config->type = NODE_MASTER;
    } else {
        config->type = NODE_SLAVE;
    }

    config->port = atoi(port->value.as.string);

    if ((config->port < 1) || (config->port > 65535)){
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        exit(1);
    }

    if(config->type == NODE_SLAVE) {
        triton_value_entry_t* connect = triton_object_get(&json.value.as.object, 2);
        ++connect->value.as.string;
        connect->value.as.string[strlen(connect->value.as.string) - 1] = '\0';

        config->connect = atoi(connect->value.as.string);

        if ((config->connect < 1) || (config->connect > 65535)){
            fprintf(stderr, "ERROR #1: invalid port specified.\n");
            exit(1);
        }
    }

    fclose (stream);
}

void accept_callback(sock_t* listen, duplex_sock_t* sock) {
    node_state_t* state = (node_state_t*) listen->user_ptr;

    vector_push(&state->sockets, sock);

    printf("Node accepted new duplex connection\n");
}

int main(int argc, char *argv[]) {
    node_config_t config;
    load_node_config(&config, argv[1]);

    node_state_t state;
    sock_create_listen_ptr(&state.listen, config.port, &state);
    create_vector(&state.sockets, 100);
    state.listen.accept_callback = accept_callback;

    if(config.type == NODE_SLAVE) {
        duplex_sock_t* sock = (duplex_sock_t*) malloc(sizeof(duplex_sock_t));
        sock_start_duplex_sock(&state.listen, sock, config.connect);
        vector_push(&state.sockets, sock);  
    }

    for (;;){
        sock_pull(&state.listen, state.sockets.items, state.sockets.stored);

        for(int i = 0; i < state.sockets.stored; ++i) {
            duplex_sock_t* duplex = state.sockets.items[i];
            
            const char* message = "This is a test message";
            unsigned int length = strlen(message);

            sock_duplex_send(duplex, message, length);
        }
    }

    return 0;
}
