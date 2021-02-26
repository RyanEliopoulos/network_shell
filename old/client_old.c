
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

void intakeInput (char *, char *);
void interfaceLoop (int);

int main (int argc, char *argv[]) {

    /* create connection socket */
    int socketfd;
    socketfd = socket(AF_INET, SOCK_STREAM, 0);

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
        printf("gethostbyname success\n");
        pptr = (struct in_addr **) hostEntry->h_addr_list;
        memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
    }
   
    /* server information gathered. Connecting */ 
    if (connect(socketfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        printf("Error: connection failed\n");
        close(socketfd);
        exit(CONN_ERROR);
    }

    printf("Are we here?\n");
    // ok, a control connection has been established. Now we would drop into our interface loop    

    interfaceLoop(socketfd);

    char response[300] = {'\0'};
    read(socketfd, response, 299);
    printf("%s", response);
    close(socketfd);
}


// primary control loop.
// inifinite loops parsing user input and translating
// to inputs for the server
void interfaceLoop (int control_fd) {

    char command[100] = {'\0'};     // e.g. put
    char arg[1024] = {'\0'};        // e.g. ./mobydick.txt

    while (1) {
        // prompt user for input and parse this somehow
        printf("MFTB>");
        intakeInput(command, arg); 
    } 

}

//
// intended to be called in a switch statement
// returns a single character corresponding to
// a server command in the switch block
// command and arg will exist in scope of call
char inputToCommand (char *command, char *arg) {



}


void intakeInput (char *command, char *arg) {

    char buf[4096];
    fgets(buf, 4096, stdin);
    command = strtok(buf, " ");
    arg = strtok(NULL, " ");
   
    printf("user command is <%s>\n", command);
    printf("user arg is <%s>\n", arg); 

}
