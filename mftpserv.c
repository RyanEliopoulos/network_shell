#define MY_PORT_NUMBER 49999

/* error definitions */
#define INIT_ERROR 1  // failure initializing server for accept state
#define CONN_ERROR 2  // failure establishing initial connection with client
#define COMM_ERROR 3  // This should be changed to RD_ERRORfatal error reading from TCP client connection 
#define WRT_ERROR 4   // error writing to 

#define DEBUG 0
#define ARG_MAX_LEN 4096  // longest possible pathname accepted from the client

#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<arpa/inet.h>

void controlLoop(int);
void readConnection(char *, char [], int);
void acknowledgeError(int, char []);
void acknowledgeSuccess(int, char[]);
void writeWrapper(int, char*, int);



pid_t process_id;  // used by forked children to print useful information



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

    process_id = getpid();    // can make this a global var for easy printf debug access

    // prepare variales for readConnection call
    int data_conn_flag = 0;  // tracks existence of open data connection
    char cmd;
    char client_arg[ARG_MAX_LEN] = {'\0'};

    while (1) {
        readConnection(&cmd, client_arg, connectfd);
    
        /* now here is where we fall into the switch statement */
        /* handling the command value as needed */
        switch (cmd) {
            case 'D':
                printf("child %d: Server received command D\n", process_id);
                // data_conn_flag = 1;
                // testing acknowledgeSuccess
                acknowledgeSuccess(connectfd, "61111"); // claiming a data port is open here
                break;
            case 'C':
                // if (!data_conn_flag) return error about data connection needing to exist 
                printf("child %d: Server received command C\n", process_id);
                printf("child %d: Received pathname: %s\n", process_id, client_arg);
                break;
            case 'L':
                printf("child %d: Server received comman L\n", process_id);
                break;
            case 'G':
                printf("child %d: Server received command G\n", process_id);
                break;
            case 'P':
                printf("child %d:Server received command P\n", process_id);
                printf("child %d:received pathname: %s\n", process_id, client_arg);
                break;
            case 'Q':
                printf("child %d:Server received command Q\n", process_id);
                printf("child %d:received pathname: %s\n", process_id, client_arg);
                // exit(0);
                break;
            default:
                printf("child %d:Server received invalid command: %c\n", process_id, cmd);
                // respond with error
        }

        /* re-zero out client_arg for next pass */
        for (int i = 0; i < ARG_MAX_LEN; i++) {
            client_arg[i] = '\0';
        }
    }
}


// responsible for read() calls on the TCP connection
// relays this info back to controlLoop
void readConnection (char *cmd, char client_arg[], int connectfd) {

    // First get the command
    if (read(connectfd, cmd, 1) == 0) {
        perror("Unexpected EOF reading command...terminating\n");
        exit(COMM_ERROR);
    }
    printf("Read %c from the control connection\n", *cmd);
    // command has been read. Now for the argument, if any    
    int i = 0;              // index for path
    while (i < ARG_MAX_LEN - 1) {      // longest possibl argument accepted from client

        // read next character
        if (read(connectfd, &client_arg[i], 1) == 0) {
            perror("Unexepcted EOF reading argument from client..terminating\n");
            exit(COMM_ERROR);
        }
        if (client_arg[i] == '\n') {      // check for command termination
            client_arg[i] = '\0';         // place real terminator if so
            printf("Argument from control connection: %s\n", client_arg);
            return;
        }
        i++;                        // otherwise increment and continue
    }
    client_arg[ARG_MAX_LEN-1] = '\0';
}

// response over control connection indicating previos command failed
void acknowledgeError(int connectfd, char errorMsg[]) {

}

// sends A response to client. 
// data_port is null if no port number is being sent
void acknowledgeSuccess(int connectfd, char *data_port) {

    char response[8] = {'\0'};                                      // build response string
    response[0] = 'A';
    if (data_port != NULL) {
        strcat( (response + 1), data_port);     //  append port number, if applicable. Expect to be terminated string
        response[6] = '\n';
    }
    else {
        response[1] = '\n';
    }
    printf("the acknowledgeSuccess message: %s\n", response);
    // write response string to the client
    int i = 0;
    while (response[i] != '\0') {
        writeWrapper(connectfd, &(response[i]), 1);
        i++;
   } 
}

void writeWrapper(int fd, char *msg, int write_bytes) {

    if ( (write(fd, msg, write_bytes)) == -1) {
        printf("child %d: Error writing to descriptor %d\n", process_id, fd);
        // perform any necessary cleanup
        exit(WRT_ERROR);
    }
}
