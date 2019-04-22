/*
 * Ryan Paulos
 * CS 360 Final Project
 *
 * Note: outgoing writes depend on arg string ending with a \n\0.
 * However, only the \n terminator is sent
 *
 * Note: if a data connection has already been established and another data connection request is made prior to the first being used,
 * then the initial connection is closed by the server
 * 
 *
 *
 *
 * Note: will really need to take the time to change printf statements to fprint/conditional upon debug mode
 */


#define MY_PORT_NUMBER 49999

/* error definitions */
#define INIT_ERROR 1  // failure initializing server for accept state
#define CONN_ERROR 2  // failure establishing initial connection with client
#define COMM_ERROR 3  // This should be changed to RD_ERRORfatal error reading from TCP client connection 
#define WRT_ERROR 4   // error writing to 

#define DEBUG 0
#define ARG_MAX_LEN 4096  // longest possible transmission across connections

#include<fcntl.h>
#include<errno.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<arpa/inet.h>
#include<sys/stat.h>

void controlLoop(int);
void remoteToLocal(int, int, char*);
void localToRemote(int, int, char*);
void cwd(int, char *);
void listDir(int);
void buildDataConnection(int *, int);
void readConnection(char *, char [], int);
void acknowledgeError(int, char []);
void acknowledgeSuccess(int, char[]);
int writeWrapper(int, char*, int);


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
    int data_fd = -1;  // tracks existence of open data connection; -1 means no data connection is open
    char cmd;
    char client_arg[ARG_MAX_LEN] = {'\0'};

    while (1) {

        readConnection(&cmd, client_arg, connectfd);
        /* now here is where we fall into the switch statement */
        /* handling the command value as needed */
        switch (cmd) {
            case 'D':       // establish data connection
                printf("child %d: Server received command D\n", process_id);
                if (data_fd != -1) {
                    printf("child %d: tearing down unused data connection and building a new one\n", process_id);
                    close(data_fd);
                    data_fd = -1;
                }
                buildDataConnection(&data_fd, connectfd);
                printf("child %d: back in control loop. data_fd is %d\n", process_id, data_fd);
                break;
            case 'C':       // rwd
                printf("child %d: Server received command C\n", process_id);
                printf("child %d: Received pathname: %s\n", process_id, client_arg);
                cwd(connectfd, client_arg);
                printf("child %d: server has returned from cwd call\n", process_id);
                break;
            case 'L':       // rls
                printf("child %d: Server received comman L\n", process_id);
                if (data_fd == -1) {  // no data connection
                    acknowledgeError(connectfd, "Command 'L' requires a data connection\n");
                    printf("child %d: Client must first request a data connection\n", process_id);
                }
                else {
                    printf("child %d: about to fork ls -l command\n", process_id);
                    acknowledgeSuccess(connectfd, NULL);
                    listDir(data_fd);     // exec ls -l. closes data_fd
                    data_fd = -1;         // set no connectin flag
                    printf("child %d: data connection file descriptor has been closed\n", process_id);
                }
                printf("child %d: rls command complete\n", process_id);
                break;
            case 'G':      
                printf("child %d: Server received command G\n", process_id);
                if (data_fd == -1) {
                    acknowledgeError(connectfd, "Command 'G' requires a data connection\n");
                    fprintf(stderr, "child %d: Command 'G' requires a data connection\n", process_id);
                }
                else {
                    localToRemote(connectfd, data_fd, client_arg);
                    data_fd = -1;
                    printf("child %d: completed localToRemote\n", process_id);
                }
                break;
            case 'P':     
                printf("child %d:Server received command P\n", process_id);
                printf("child %d:received pathname: %s\n", process_id, client_arg);
                if (data_fd == -1) {
                    acknowledgeError(connectfd, "Command 'P' requires a data connection\n");
                    printf("Child %d: Command 'P' requires client to establish data connection\n", process_id);
                }
                else {
                    /* localFromRemote() */  // the data connection will be closed in this function

                    // actually like remoteToLocal() better
                    remoteToLocal(connectfd, data_fd, client_arg);
                    data_fd = -1;
                    printf("child %d: The data connection has been closed\n", process_id); 
                    printf("Child %d: put command complete\n", process_id);
                }
                break;
            case 'Q':    // quit
                printf("child %d:Server received command Q\n", process_id);
                if (data_fd != -1) close(data_fd);
                acknowledgeSuccess(connectfd, NULL);
                exit(0);
                break;
            default:     // generic invalid command error. Untested
                printf("child %d:Server received invalid command: %c\n", process_id, cmd);
                char error_response[ARG_MAX_LEN] = {'\0'};          // build error response string
                error_response[0] = cmd;
                strcat(error_response, ": not a valid command\n");
                acknowledgeError(connectfd, error_response);        // then send it
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
        printf("child %d: unexpected EOF reading command..terminating\n", process_id); 
        //perror("Unexpected EOF reading command...terminating\n");
        exit(COMM_ERROR);
    }
    printf("child %d: Read %c from the control connection\n", process_id, *cmd);
    // command has been read. Now for the argument, if any    
    int i = 0;              // index for path
    while (i < ARG_MAX_LEN - 1) {      // longest possibl argument accepted from client

        // read next character
        if (read(connectfd, &client_arg[i], 1) == 0) {
            printf("child %d: unexpected EOF reading argumet from client..terminating\n", process_id);
            //perror("Unexepcted EOF reading argument from client..terminating\n");
            exit(COMM_ERROR);
        }
        if (client_arg[i] == '\n') {      // check for command termination
            client_arg[i] = '\0';         // place real terminator if so
            printf("child %d: Argument from control connection: <%s>\n", process_id, client_arg);
            return;
        }
        i++;                        // otherwise increment and continue
    }
    client_arg[ARG_MAX_LEN-1] = '\0';
}

// "puts" the client's file on the server
// relies upon a "write file" mutex that ensures only one
// client attempts to create a file at a time.
void remoteToLocal (int control_fd, int data_fd, char *client_arg) {


    // get finary semaphore controlling file writes
    char response[ARG_MAX_LEN];
    if (access(client_arg, F_OK)) {
        int tempno = errno;
        if (tempno == ENOENT) {  // file name not in use
            int new_fd;
            if ( (new_fd = open(client_arg, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP)) == -1) {  // attempt to create the file
                strcat(response, strerror(errno));
                strcat(response, "\n");
                acknowledgeError(control_fd, response);
                fprintf(stderr, "Child %d: error openig new file locally: %s\n", process_id, strerror(errno));         
            }
            else {   // now beginning data transfer
                //
                //
                // NOTE: release the binary semaphore here (i think that's as soon as possible)
                //
                acknowledgeSuccess(control_fd, NULL);
                int read_bytes;
                char temp_data[512];
                while ( (read_bytes = read(data_fd, temp_data, 512)) > 0) {
                    if ( writeWrapper(new_fd, temp_data, 512) == -1) {
                        strcat(response, strerror(errno));
                        strcat(response, "\n");
                        fprintf(stderr, "child %d: error writing to local file: %s\n", process_id, strerror(errno));
                        break;
                    }
                }
                if (read_bytes < 0) {
                    strcat(response, strerror(errno));
                    strcat(response, "\n");
                    fprintf(stderr, "child %d: error reading from data connection: %s\n", process_id, strerror(errno));
                }
            }
        }
        else {  // there was potentially an error with access itself
            fprintf(stderr, "child %d: access call error: %s\n", process_id, strerror(errno));
            strcat(response, strerror(errno));
            strcat(response, "\n");
        }
    }
    else {  // file already exists
        fprintf(stderr, "child %d: File we were asked to create already exists\n", process_id);
        strcat(response, "That file already exists on the server\n");
        acknowledgeError(control_fd, response);
    }
    close(data_fd);

//    char response[ARG_MAX_LEN] = {'\0'};        // error response string
//    /* while (get write semaphore loop */
//    int access_ret = access(client_arg, F_OK);
//    if (access_ret) {             // checking if the file exists
//        strcat(response, strerror(errno));          
//        strcat(response, "\n");                    
//        acknowledgeError(control_fd, response);
//        fprintf(stderr, "child %d: error checking access locally: %s\n", process_id, strerror(errno));
//    }
//    else {                                      // file doesn't exist
//        int new_fd;
//        if ( (new_fd = open(client_arg, O_CREAT)) == -1) {  // attempt to create the file
//            strcat(response, strerror(errno));
//            strcat(response, "\n");
//            acknowledgeError(control_fd, response);
//            fprintf(stderr, "Child %d: error openig new file locally: %s\n", process_id, strerror(errno));         
//        }
//        else {   // now beginning data transfer
//            int read_bytes;
//            char temp_data[512];
//            while ( (read_bytes = read(data_fd, temp_data, 512)) > 0) {
//                if ( writeWrapper(new_fd, temp_data, 512) == -1) {
//                    strcat(response, strerror(errno));
//                    strcat(response, "\n");
//                    fprintf(stderr, "child %d: error writing to local file: %s\n", process_id, strerror(errno));
//                    break;
//                }
//            }
//            if (read_bytes < 0) {
//                strcat(response, strerror(errno));
//                strcat(response, "\n");
//                fprintf(stderr, "child %d: error reading from data connection: %s\n", process_id, strerror(errno));
//            }
//        }
//    } 
//    close(data_fd);
}

//  writes a local file to the data connection
//  server's response to the 'G' command
void localToRemote (int control_fd, int data_fd, char *client_arg) {
   
    // if (client_arg not in restricted list)  
    
    // init vars 
    char response[ARG_MAX_LEN] = {'\0'};  // error response string
    FILE *file;                           // stream for desired file
    struct stat file_info;               
    if (stat(client_arg, &file_info) == -1) {
        strcat(response, strerror(errno));    
        strcat(response, "\n");             // strerror doesn't supply a newline
        fprintf(stderr, "Child %d: stat failed in localToRemote: %s\n", process_id, strerror(errno)); 
    }
    else if (!S_ISREG(file_info.st_mode)) {
        strcat(response, "path is not a regular file\n");
        fprintf(stderr, "Child %d: given path is not a regular file\n", process_id);
    }   
    else if ( (file = fopen(client_arg, "r")) == NULL) {
        strcat(response, strerror(errno));
        strcat(response, "\n");
        fprintf(stderr, "Child %d: unable to open the file\n%s\n", process_id, strerror(errno));
    }
    else {  // at this point the file has been opened successfully
        acknowledgeSuccess(control_fd, NULL); 

        char file_chunk[512] = {'\0'};
        int read;
        while ( (read = fread(file_chunk, sizeof(char), 511, file)) == 511) {
            /* upon write failure the server will return to the control loop   */
            /* looking for further commands. If client disconnected it will be */
            /* handled there                                                   */
            if (writeWrapper(data_fd, file_chunk, 512) == -1) {
                fprintf(stderr, "child %d: Error writing file to data connection\n", process_id);
                close(data_fd);
                return;
            }
        }
        // Now check if there was an fread error.
        // if there was not, the last group
        // of data needs to be written
        if (feof(file)) {  
            if (writeWrapper(data_fd, file_chunk, read) == -1) {
                fprintf(stderr, "child %d: Error writing final final chunk to data connection\n", process_id);
            }
            // file successfully written.
        } 
        else {   // an fread error occurred :(
            fprintf(stderr, "child %d: Encountered fread error transfering file to client\n", process_id);
        }
        close(data_fd);
        return;
    }

    /* this covers error response for the first      */
    /* three clauses of the initial if               */
    /* any errors after opening the file are handled */
    /* in the outer else clause                      */
    acknowledgeError(control_fd, response);
    printf("acknowledging error to client\n");
    close(data_fd);
}

// attempts to change server's working diretory to client_arg
void cwd (int control_connection, char *client_arg) {

    int ret;
    if ( (ret = chdir(client_arg)) == -1) {
        char response[ARG_MAX_LEN] = {'\0'};
        acknowledgeError(control_connection, "directory doesn't fucking exist yo\n");
        printf("child %d: failed to change directory\n%s\n", process_id, strerror(errno));
    }
    else {
        acknowledgeSuccess(control_connection, NULL);
        printf("child %d: successfully changed directories\n", process_id);
    }
}


// forks call to exec(ls -l)
// remember to close the data_fd after fork
void listDir(int data_fd) {
    if (!fork()) {
        close(1);   // make room for data_fd
        dup(data_fd);
        close(data_fd);
        execlp("ls", "ls", "-l", (char *) NULL);
    }
    else {
        close(data_fd);
    }
}

// establishes data connection data_fd with client
void buildDataConnection (int *data_fd, int control_fd) {

    // prepare port address info variables  
    struct sockaddr_in data_addr;
    int listenfd;
    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(0);
    data_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // prepare to accept incoming connection
    int sockaddr_len = sizeof(struct sockaddr_in); 
    if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  {
        fprintf(stderr, "child %d: error establishing socket for data connection\n", process_id); // not a fatal error
        fprintf(stderr, "child %d: %s\n", process_id, strerror(errno));
        // write error to client
        acknowledgeError(control_fd, "Could not establish data socket\n");
        return;
    }
    printf("child %d: created new data socket on descriptor %d\n", process_id, listenfd); 

    if ( bind(listenfd, (struct sockaddr *) &data_addr, sizeof(data_addr)) < 0) {
        fprintf(stderr, "child %d: Error binding the data connection socket\n", process_id);
        fprintf(stderr, "child %d: %s\n", process_id, strerror(errno));
        acknowledgeError(control_fd, "Could not bind data socket\n");
        return;
    } 

    if (listen(listenfd, 0) == -1) {
        fprintf(stderr, "child %d: listen call for data connection failed\n", process_id);
        fprintf(stderr, "child %d: %s\n", process_id, strerror(errno));
        acknowledgeError(control_fd, "Could not listen on data socket\n");
        return; 
    }

    // retrieve the port number of the new socket
    struct sockaddr_in temp_addr;  
    memset(&temp_addr, 0, sizeof(temp_addr));
    getsockname(listenfd, (struct sockaddr *) &temp_addr, &sockaddr_len);
    int data_port = ntohs(temp_addr.sin_port); 
    printf("child %d: data port number: %d\n", process_id, data_port);

    // relay port number to client
    char port_string[6]; 
    snprintf(port_string, sizeof(char) * 6, "%d", data_port);
    printf("child %d: data port string: %s\n", process_id, port_string);
    acknowledgeSuccess(control_fd, port_string);

    int tempfd;
    if ( (tempfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1) {
        fprintf(stderr, "child %d: encountered error accepting data connection\n%s\n", process_id, strerror(errno));
        close(listenfd);
        return;
    }
    printf("child %d: accepted new port fd: %d\n", process_id, tempfd);

    // cleanup listener
    close(listenfd);
    // send acnowledgement to client
    //acknowledgeSuccess(control_fd, NULL);  // This is a mistake.  
    // push the data connection fd down the stack
    *data_fd = tempfd;
}

// response over control connection indicating previos command failed
// needs error checking
void acknowledgeError(int control_fd, char errorMsg[]) {
    
    char response[ARG_MAX_LEN] = {'\0'};  // construct response string
    response[0] = 'E'; 
    strncat(response+1, errorMsg, ARG_MAX_LEN-3);
    printf("child %d: error message written to client: %s\n", process_id, response);
    int i = 0;
    while ( errorMsg[i] != '\0') {
        if (writeWrapper(control_fd, &(errorMsg[i]), 1) == -1) {
            fprintf(stderr, "child %d: encountered error writing to control channel. Fatal\n", process_id);
            exit(WRT_ERROR);
        }
        i++;
    }
}

// sends 'A' response to client with optional arg data_port
// data_port is null if no port number is being sent
void acknowledgeSuccess(int connectfd, char *data_port) {
    
    // longest possible response: 'A' + 5 digit port + \n\0 - \0
    char response[8] = {'\0'};                                      // build response string
    response[0] = 'A';
    if (data_port != NULL) {
        strcat(response+1, data_port);     //  append port number, if applicable. Expect to be terminated string
        response[6] = '\n';
    }
    else {
        response[1] = '\n';
    }
    printf("child %d: the acknowledgeSuccess message: %s\n", process_id, response);
    // write response string to the client
    int i = 0;
    while (response[i] != '\0') {
        if (writeWrapper(connectfd, &(response[i]), 1) == -1) {
            fprintf(stderr, "Child %d: error writing to control stream. Fatal\n", process_id);
            exit(WRT_ERROR);
        }
        i++;
   } 
}

// process exits on error
int writeWrapper(int fd, char *msg, int write_bytes) {
    if ( (write(fd, msg, write_bytes)) == -1) {
        printf("child %d: Error writing to descriptor %d\n", process_id, fd);
        return -1;
    }
    return 0;
}
