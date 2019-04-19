#define MY_PORT_NUMBER 49999
#define START_ERROR 1
#define CONN_ERROR 2


#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
///////////////
#include<string.h>  // need for memset
#include<stdio.h>   
#include<unistd.h>  // read/write
#include<stdlib.h>
#include<time.h>

int main (int argc, char* argv[]) {

    /* create connection socket */
    int socketfd;
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    printf("value of socketfd: %d\n", socketfd);

    /* prepare address info of the server */
    struct sockaddr_in servAddr;
    struct hostent* hostEntry;
    struct in_addr **pptr;

    /* set initial values */
    memset( &servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);

    /* Attempting to get the required connection/address info */
    /* first try using hostname */  
    if ( (hostEntry = gethostbyname(argv[1])) == NULL) {
        herror("invalid host name..attempting to use arg as IP address\n");

        /* didn't work. Now check if its an IPv4 address */
        if (inet_pton(AF_INET, argv[1], &servAddr.sin_addr)<=0) {
            printf("both hostname and IP address failed\n");
            memset( &servAddr, 0, sizeof(servAddr));
            close(socketfd);
            exit(START_ERROR);
        }
    }
    else { /* hostname sucess..copy data to appropriate structure */
        pptr = (struct in_addr **) hostEntry->h_addr_list;
        memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
    }
   
    /* server information gathered. Connecting */ 
    if (connect(socketfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        printf("Error: connection failed\n");
        close(socketfd);
        exit(CONN_ERROR);
    }
    printf("socketfd value is set to %d\n", socketfd);
    char response[300] = {'\0'};
    read(socketfd, response, 299);
    printf("%s", response);
    sleep(20);
    close(socketfd);
}


