#define MY_PORT_NUMBER 49999

/* error definitions */
#define INIT_ERROR 1
#define CONN_ERROR 2

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<arpa/inet.h>

int main (int argc, char* argv[]) {

    struct sockaddr_in servAddr;

    // prepare address info variables 
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // prepare for accepting an incoming connection
    int listenfd, connectfd, addr_len;
    struct sockaddr_in clientAddr; 
    addr_len = sizeof(servAddr);

    // create and bind the socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        perror("bind error\n");
        exit(INIT_ERROR);
    }
    // listen for incoming connections
    if (listen(listenfd, 4) == -1) {
        perror("call to listen failed\n");
        exit(INIT_ERROR);
    }

    while (1) {     // enter primary service loop
        // wait for incoming connection
        if ( (connectfd = accept(listenfd, (struct sockaddr *)&clientAddr, &addr_len)) == -1)  {
            perror("error accepting incoming connections\n");
            exit(CONN_ERROR);
        }

        // then fork the primary logic
        if (fork()) {
            if (close(connectfd) == -1) {
                perror("server error closing old port connection\n");
                exit(CONN_ERROR);
            }
            // quick zombie cleanup
            for (int i = 0; i < 4; i++) {
                waitpid(-1, NULL, WNOHANG);
            }
        }
        // child process begins here
        else {
            // establish control connection with client on a new port
        }
    }
}


