#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFLEN 1024

int main(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr,"USAGE: %s <port>\n",argv[0]);
        exit(1);
    }

    unsigned int port = atoi(argv[1]);

    if ((port < 1) || (port > 65535)){
        printf("ERROR #1: invalid port specified.\n");
        exit(1);
    }

    int s_socket;
    if ((s_socket = socket(AF_INET, SOCK_STREAM, 0))< 0){
        fprintf(stderr,"ERROR #2: cannot create socket.\n");
        exit(1);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);

    if (inet_aton("127.0.0.1", &servaddr.sin_addr) <= 0 ) {
        fprintf(stderr,"ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    if (connect(s_socket,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0){
        fprintf(stderr,"ERROR #4: error in connect().\n");
        exit(1);
    }

    char sendbuffer[BUFFLEN];
    memset(&sendbuffer,0,BUFFLEN);

    fd_set read_set;
    fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);

    while (1){
        FD_ZERO(&read_set);

        FD_SET(s_socket,&read_set);
        FD_SET(0,&read_set);

        select(s_socket + 1, &read_set, NULL, NULL, NULL);

        if (FD_ISSET(s_socket, &read_set)){
            char recvbuffer[BUFFLEN];

            memset(&recvbuffer,0,BUFFLEN);
            int i = read(s_socket, &recvbuffer, BUFFLEN);
            printf("%s\n",recvbuffer);
        }

        if (FD_ISSET(0, &read_set)) {
            int i = read(0, &sendbuffer, 1);
            write(s_socket, sendbuffer, i);
        }
    }

    close(s_socket);

    return 0;
}