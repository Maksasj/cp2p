#ifndef CONFIG_H
#define CONFIG_H

#define TRITON_IMPLEMENTATION
#include "triton/triton.h"

#define PLUM_IMPLEMENTATION
#include "caifu/plum/plum.h"

typedef unsigned int port_t;

typedef enum {
    NODE_MASTER,
    NODE_SLAVE
} node_type_t;

typedef struct {
    node_type_t type;
    char* name;

    port_t port;
    port_t connect;
} node_config_t;

void load_node_config(node_config_t* config, const char* file_path) {
    PLUM_LOG(PLUM_INFO, "Loading configuration from '%s'", file_path);

    FILE *stream = stream = fopen(file_path, "r");

    if(stream == NULL) {
        PLUM_LOG(PLUM_ERROR, "Failed to open file '%s'", file_path);
        exit(1);
    }

    char* buffer = 0;
    long length;

    fseek (stream, 0, SEEK_END);
    length = ftell(stream);
    fseek (stream, 0, SEEK_SET);
    buffer = malloc(length);
    
    if (buffer == NULL) {
        PLUM_LOG(PLUM_ERROR, "Failed to read file '%s'", file_path);
        exit(1);
    }

    fread(buffer, 1, length, stream);

    triton_json_t json;
    triton_parse_result_t result = triton_parse_json(&json, buffer);

    if(result.code == TRITON_ERROR) {
        PLUM_LOG(PLUM_ERROR, "Failed to parse JSON config file");
        exit(1);
    }

    triton_value_entry_t* role = triton_object_get_field(&json.value.as.object, "\"role\"");
    if(role == NULL)
        PLUM_LOG(PLUM_ERROR, "Failed to read 'role' field");

    ++role->value.as.string;
    role->value.as.string[strlen(role->value.as.string) - 1] = '\0';

    triton_value_entry_t* port = triton_object_get_field(&json.value.as.object, "\"port\"");
    if(port == NULL)
        PLUM_LOG(PLUM_ERROR, "Failed to read 'port' field");

    ++port->value.as.string;
    port->value.as.string[strlen(port->value.as.string) - 1] = '\0';

    if(strcmp(role->value.as.string, "master") == 0) {
        config->type = NODE_MASTER;
    } else {
        config->type = NODE_SLAVE;
    }

    config->port = atoi(port->value.as.string);

    if ((config->port < 1) || (config->port > 65535)){
        PLUM_LOG(PLUM_ERROR, "Invalid port specified in JSON configuration");
        exit(1);
    }

    if(config->type == NODE_SLAVE) {
        triton_value_entry_t* connect = triton_object_get_field(&json.value.as.object, "\"connect\"");
        if(connect == NULL)
            PLUM_LOG(PLUM_ERROR, "Failed to read 'connect' field");

        ++connect->value.as.string;
        connect->value.as.string[strlen(connect->value.as.string) - 1] = '\0';

        config->connect = atoi(connect->value.as.string);

        if ((config->connect < 1) || (config->connect > 65535)) {
            PLUM_LOG(PLUM_ERROR, "Invalid port specified in JSON configuration");
            exit(1);
        }
    }

    free(buffer);
    fclose(stream);

    PLUM_LOG(PLUM_INFO, "Successfully loaded JSON configuration");
}


#endif