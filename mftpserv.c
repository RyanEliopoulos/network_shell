#define MY_PORT_NUMBER 49999

/* error definitions */
#define INIT_ERROR 1
#define CONN_ERROR 2

#define DEBUG 0

#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<arpa/inet.h>

void acknowledge(int);
void controlLoop(int);


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
    printf("listenfd is set to %d\n", listenfd);
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
        if (DEBUG) printf("connectfd is set to %d\n", connectfd);
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
            // first get string representation of IPv4 dot notation
            char *name;
            name = inet_ntoa(clientAddr.sin_addr);
            printf("Established connection with: %s\n", name);
            // now get the hostname from addr
            struct hostent* hostEntry;
            char *hostname;
            hostEntry = gethostbyaddr(&(clientAddr.sin_addr), 
                                        sizeof(struct in_addr), AF_INET);
            if (hostEntry == NULL) {
                printf("failed to get host by addr\n"); 
            }
            else {
                if (DEBUG) printf("got the fucking thing\n");
                hostname = hostEntry->h_name;
                printf(" the hostname: %s\n", hostname);
            }
            /* enter primary serverLoop now with connectfd */
            // clientID(clientAddr); // This will print client hostname/IP address to stdout. Doing it messy for the rough draft for now
            // controlLoop(connectfd);
            controlLoop(connectfd);
        }
    }
}


/* connection with the client has been established */
/* this is the primary interface the client has with the server */
void controlLoop(int connectfd) {

    acknowledge(connectfd);
    

}

// spruce this up later. This will handle error checking stuff
void acknowledge(int connectfd) {

    write(connectfd, "A\n", 2);
    
}
