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

int findemptyuser(int c_sockets[]){
    int i;
    for (i = 0; i <  MAXCLIENTS; i++){
        if (c_sockets[i] == -1){
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]){
    int l_socket; // Self listening socket
    int s_sockets[MAXCONNECTIONS];
    int c_sockets[MAXCLIENTS];

    // Usage
    // if (argc != 2){
    //     fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
    //     return -1;
    // }

    // Check port
    unsigned int port = atoi(argv[1]);
    if ((port < 1) || (port > 65535)){
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        return -1;
    }

    if ((l_socket = socket(AF_INET, SOCK_STREAM,0)) < 0){
        fprintf(stderr, "ERROR #2: cannot create listening socket.\n");
        return -1;
    }

    // Setup listening port
    struct sockaddr_in servaddr;
    memset(&servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port);

    if (bind(l_socket, (struct sockaddr *)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr, "ERROR #3: bind listening socket.\n");
        return -1;
    }

    if (listen(l_socket, 5) < 0){
        fprintf(stderr, "ERROR #4: error in listen().\n");
        return -1;
    }                           

    for (int i = 0; i < MAXCLIENTS; i++)
        c_sockets[i] = -1;

    for (int i = 0; i < MAXCONNECTIONS; i++)
        s_sockets[i] = -1;

    // Connect to other node if can
    if(argc == 3) {
        unsigned int thisport = atoi(argv[2]);
    
        struct sockaddr_in servaddr;
        memset(&servaddr,0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(thisport);

        if ((s_sockets[0] = socket(AF_INET, SOCK_STREAM, 0))< 0){
            fprintf(stderr,"ERROR #2: cannot create socket.\n");
            exit(1);
        }

        if (inet_aton("127.0.0.1", &servaddr.sin_addr) <= 0 ) {
            fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
            exit(1);
        }

        if (connect(s_sockets[0],(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
            fprintf(stderr,"ERROR #4: error in connect().\n");
            exit(1);
        }
    }

    int maxfd = 0;
    for (;;){
        fd_set read_set;
        FD_ZERO(&read_set);

        // Bind client sockets
        for (int i = 0; i < MAXCLIENTS; i++){
            if (c_sockets[i] != -1){
                FD_SET(c_sockets[i], &read_set);
                if (c_sockets[i] > maxfd){
                    maxfd = c_sockets[i];
                }
            }
        }

        // Bind connections sockets        
        for (int i = 0; i < MAXCONNECTIONS; i++){
            if (s_sockets[i] != -1){
                FD_SET(s_sockets[i], &read_set);
                if (s_sockets[i] > maxfd){
                    maxfd = s_sockets[i];
                }
            }
        }

        FD_SET(l_socket, &read_set);
        if (l_socket > maxfd){
            maxfd = l_socket;
        }
        
        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        if (FD_ISSET(l_socket, &read_set)){
            int client_id = findemptyuser(c_sockets);
            
            if (client_id != -1){
                struct sockaddr_in clientaddr;
                unsigned int clientaddrlen = sizeof(clientaddr);

                memset(&clientaddr, 0, clientaddrlen);
                c_sockets[client_id] = accept(l_socket, (struct sockaddr*) &clientaddr, &clientaddrlen);
                printf("Connected:  %s\n",inet_ntoa(clientaddr.sin_addr));
            }
        }


        for(int i = 0; i < MAXCONNECTIONS; i++) {
            if(s_sockets[i] == -1)
                continue;

            if (FD_ISSET(c_sockets[i], &read_set)){
                char recvbuffer[BUFFLEN] = "Poggers";

                memset(&recvbuffer,0,BUFFLEN);
                int i = read(s_sockets[i], &recvbuffer, BUFFLEN);
                printf("%s\n",recvbuffer);
            }
        }       

        for (int i = 0; i < MAXCLIENTS; i++){
            if (c_sockets[i] == -1)
                continue;

            if (FD_ISSET(c_sockets[i], &read_set)){
                // Process node
                char buffer[BUFFLEN];

                memset(&buffer,0,BUFFLEN);
                int r_len = recv(c_sockets[i],&buffer,BUFFLEN,0);

                for (int j = 0; j < MAXCLIENTS; j++){
                    if (c_sockets[j] == -1)
                        continue;

                    int w_len = send(c_sockets[j], buffer, r_len,0);

                    if (w_len <= 0){
                        close(c_sockets[j]);
                        c_sockets[j] = -1;
                    }
                }
            }
        }
    }

    return 0;
}
