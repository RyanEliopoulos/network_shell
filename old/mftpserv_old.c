/*
 *
 * TODO: add return values to command fnxs to meet spec need of printing command outcome
 * TODO: clean up this fucking mess
 * TODO: test rogue command inquiries once our client is up to it
 * TODO: add semaphore release function. We short-circuited the relase in remoteToLocal by adding all those return statements
 *
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
 * Note: will really need to take the time to change printf statements to fprint/conditional upon debug mode
 * 
 *
 *
 * Note: Originally all command functions were void and fell through. But I decided they should return
 *       their outcome status instead. Now they look stupid. And they violate DRY pretty hard now. ugh.
 */


#define MY_PORT_NUMBER 49999

/* error definitions */
#define INIT_ERROR 1  // failure initializing server for accept state
#define CONN_ERROR 2  // failure establishing initial connection with client
#define COMM_ERROR 3  // This should be changed to RD_ERRORfatal error reading from TCP client connection 
#define WRT_ERROR 4   // error writing to 
#define SEM_ERROR 5   // error manipulating the file-write semaphore. Fatal.

#define DEBUG 0
#define ARG_MAX_LEN 4096  // longest possible transmission across connections
#define SEM_MAX_TRIES 3   // Number of times the process attempts to get the file-writing semaphore before responding with an error

#include<sys/sem.h>
#include<sys/ipc.h>
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
int remoteToLocal(int, int, char*);
int localToRemote(int, int, char*);
int cwd(int, char *);
int listDir(int);
int buildDataConnection(int *, int);
void readConnection(char *, char [], int);
void acknowledgeError(int, char []);
void acknowledgeSuccess(int, char[]);
int writeWrapper(int, char*, int);
int takeSemaphore(int, struct sembuf *, int);
void releaseSemaphore(int, struct sembuf *, int);


// required for semaphore use
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};  

pid_t process_id;   // used by forked children to print useful information
int semaphore_id;   // semid for file-writing semaphore. 

int main (int argc, char* argv[]) {

    // initialize global file-write semaphore
    semaphore_id = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR);
    union semun ctl_args;
    unsigned short val[1] = {1};
    ctl_args.array = val;
    
    if (semctl(semaphore_id, 0, SETALL, ctl_args) == -1) {
        fprintf(stderr, "failed to set semaphore values\n");
        exit(INIT_ERROR);
    }   
    printf("server semid: %d\n", semaphore_id);

    // prepare address info variables 
    struct sockaddr_in servAddr;
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
                printf("child %d: Server received command D with argument <%s>\n", process_id, client_arg);
                if (data_fd != -1) {
                    printf("child %d: tearing down unused data connection and building a new one\n", process_id);  // DEBUG print statement
                    close(data_fd);
                    data_fd = -1;
                }
                if (buildDataConnection(&data_fd, connectfd) == -1) {
                    printf("child %d: failed to establish data connection\n", process_id);
                }
                else {
                    printf("child %d: data connection successfully established\n", process_id);
                }
                printf("child %d: back in control loop. data_fd is %d\n", process_id, data_fd);  // DEBUG
                break;
            case 'C':       // rwd
                printf("child %d: Server received command C with argument <%s>\n", process_id, client_arg);
                if (cwd(connectfd, client_arg) == -1) {
                    printf("child %d: failed to change directory\n", process_id);
                }
                else {
                    printf("child %d: successfully changed directory\n", process_id);
                }
                break;
            case 'L':       // rls
                printf("child %d: Server received command L with argument <%s>\n", process_id, client_arg);
                if (data_fd == -1) {  // no data connection
                    acknowledgeError(connectfd, "Command 'L' requires a data connection\n");
                    printf("child %d: Client must first request a data connection\n", process_id);  // DEBUG
                }
                else {
                    printf("child %d: about to fork ls -l command\n", process_id); // DEBUG
                    acknowledgeSuccess(connectfd, NULL);
                    if (listDir(data_fd) == -1) {  // exec ls -l. closes data_fd
                        printf("child %d: error occurred in rls child process\n", process_id);
                    }   
                    else {
                        printf("child %d: successfully executed command 'L'\n", process_id);
                    }
                    data_fd = -1;         // set no connectin flag
                    printf("child %d: data connection file descriptor has been closed\n", process_id);    // DEBUG
                }
                break;
            case 'G':      
                printf("child %d: Server received command G with argument <%s>\n", process_id, client_arg);
                if (data_fd == -1) {
                    acknowledgeError(connectfd, "Command 'G' requires a data connection\n");
                    fprintf(stderr, "child %d: Command 'G' requires a data connection\n", process_id);
                }
                else {
                    if (localToRemote(connectfd, data_fd, client_arg) == -1) {
                        printf("child %d: failed to transfer file to client\n", process_id);
                    }
                    else {
                        printf("child %d: successfully transferred file to client\n", process_id);
                    }
                    data_fd = -1;
                }
                break;
            case 'P':     
                printf("child %d:Server received command P with argument <%s>\n", process_id, client_arg);
                if (data_fd == -1) {
                    acknowledgeError(connectfd, "Command 'P' requires a data connection\n");
                    printf("Child %d: Command 'P' requires client to establish data connection\n", process_id);
                }
                else {
                    if (remoteToLocal(connectfd, data_fd, client_arg) == -1) {
                        printf("Child %d: failed to successfully write file from client\n", process_id);
                        //
                        //
                    }
                    else {
                        printf("Child %d: successfully wrote file from client\n", process_id);
                    }
                    data_fd = -1;
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
// relies upon a "write file" mutex to eliminate
// file check/creation race condition
int remoteToLocal (int control_fd, int data_fd, char *client_arg) {

    // init semaphore tools
    struct sembuf taker;
    taker.sem_op = -1;
    taker.sem_flg = IPC_NOWAIT;
    struct sembuf replacer;    
    replacer.sem_op = 1;
   
    //  beginning attempts for semaphore 
    int i = 0;
    while (takeSemaphore(semaphore_id, &taker, 1) == -1) {  // returns -1 upon failure, else 0
        i++;
        if (i >= SEM_MAX_TRIES) {
            acknowledgeError(control_fd, "failed to get file-writing semaphore. Please try again\n");
            fprintf(stderr, "child %d: semaphore attempt limit exceeded\n", process_id);
            close(data_fd);
            return -1;
        }
        sleep(1);
    }

    // investigate existence of given file name
    char response[ARG_MAX_LEN] = {'\0'};
    if (access(client_arg, F_OK)) {
        int tempno = errno;
        if (tempno == ENOENT) {  // file name not in use
            int new_fd;  // closes at end of the loop or before returning from a semctl error
            if ( (new_fd = open(client_arg, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP)) == -1) {  // attempt to create the file
                strcat(response, strerror(errno));
                strcat(response, "\n");
                acknowledgeError(control_fd, response);
                close(data_fd);
                fprintf(stderr, "Child %d: error openig new file locally: %s\n", process_id, strerror(errno));         
                releaseSemaphore(semaphore_id, &replacer, 1);  // process exits upon failure
                return -1;
            }
            else {   // now beginning data transfer

                // the semaphore is no longer required
                if (semop(semaphore_id, &replacer, 1) == -1) {
                    fprintf(stderr, "child %d: unexpected fatal error replacing semaphore: %s\n", process_id, strerror(errno));
                    char response[ARG_MAX_LEN] = {'\0'};
                    strcat(response, strerror(errno));
                    strcat(response, "\n");
                    acknowledgeError(control_fd, response);
                    close(new_fd); close(data_fd);
                    exit(SEM_ERROR);
                }
                // the file transfer begins 
                acknowledgeSuccess(control_fd, NULL);
                int read_bytes, written_bytes;
                char temp_data[512];
                while ( (read_bytes = read(data_fd, temp_data, 512)) > 0) {
                    printf("child %d: read %d bytes from client\n", process_id, read_bytes);
                    if ( (written_bytes = writeWrapper(new_fd, temp_data, read_bytes)) == -1) {
                        strcat(response, strerror(errno));
                        strcat(response, "\n");
                        fprintf(stderr, "child %d: error writing to local file: %s\n", process_id, strerror(errno));
                        close(data_fd); close(new_fd);
                        releaseSemaphore(semaphore_id, &replacer, 1);  // process exits upon failure
                        return -1;
                    }
                    printf("child %d: wrote %d more bytes to the new file\n", process_id, written_bytes);
                }
                // check whether EOF or an error terminated reads 
                if (read_bytes < 0) {
                    strcat(response, strerror(errno));
                    strcat(response, "\n");
                    fprintf(stderr, "child %d: error reading from data connection: %s\n", process_id, strerror(errno));
                    close(data_fd); close(new_fd);
                    releaseSemaphore(semaphore_id, &replacer, 1);  // process exits upon failure
                    return -1;
                }
                printf("child %d: reached EOF reading from data connection\n", process_id);
                close(new_fd);
            }
        }
        else {  // there was potentially an error with access itself
            fprintf(stderr, "child %d: access call error: %s\n", process_id, strerror(errno));
            strcat(response, strerror(errno));
            strcat(response, "\n");
            acknowledgeError(control_fd, response);
            close(data_fd);
            releaseSemaphore(semaphore_id, &replacer, 1);  // process exits upon failure
            return -1;
        }
    }
    else {  // file already exists
        fprintf(stderr, "child %d: File we were asked to create already exists\n", process_id);
        strcat(response, "That filslefijee already exists on the server\n");
        acknowledgeError(control_fd, response);
        close(data_fd);
        releaseSemaphore(semaphore_id, &replacer, 1);  // process exits upon failure
        return -1;
    }
    
    releaseSemaphore(semaphore_id, &replacer, 1);  // process exits upon failure
    close(data_fd);
    return 0;
}

//  writes a local file to the data connection
//  server's response to the 'G' command
int localToRemote (int control_fd, int data_fd, char *client_arg) {
   
    // init vars 
    char response[ARG_MAX_LEN] = {'\0'};  // error response string
    FILE *file;                           // stream for desired file
    struct stat file_info;               

    // check if specific file is a valid option
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

        // transfer file data from local file to data connection
        char file_chunk[512] = {'\0'};
        int read;
        while ( (read = fread(file_chunk, sizeof(char), 511, file)) == 511) {
            /* upon write failure the server will return to the control loop   */
            /* looking for further commands. If client disconnected it will be */
            /* handled there                                                   */
            if (writeWrapper(data_fd, file_chunk, 512) == -1) {
                fprintf(stderr, "child %d: Error writing file to data connection\n", process_id);
                close(data_fd);
                return -1;
            }
        }
        // Now check if there was an fread error.
        // if there was not, the last group
        // of data needs to be written
        if (feof(file)) {  
            if (writeWrapper(data_fd, file_chunk, read) == -1) {
                fprintf(stderr, "child %d: Error writing final final chunk to data connection\n", process_id);
                close(data_fd);
                return -1;
            }
            // file successfully written.
        } 
        else {   // an fread error occurred :(
            close(data_fd);
            fprintf(stderr, "child %d: Encountered fread error transfering file to client\n", process_id);
            return -1;
        }
        close(data_fd);
        return 0;
    }

    /* this covers error response for the first      */
    /* three clauses of the initial if               */
    /* any errors after opening the file are handled */
    /* in the outer else clause                      */
    acknowledgeError(control_fd, response);
    printf("acknowledging error to client\n");
    close(data_fd);
    return -1;
}

// attempts to change server's working diretory to client_arg
int cwd (int control_connection, char *client_arg) {

    int ret;
    if ( (ret = chdir(client_arg)) == -1) {
        acknowledgeError(control_connection, "unable to change directory\n");
        printf("child %d: failed to change directory\n%s\n", process_id, strerror(errno));  // DEBUG
        return -1;
    }
    else {
        acknowledgeSuccess(control_connection, NULL);
        printf("child %d: successfully changed directories\n", process_id);  // DEBUG
        return 0;
    }
}


// forks call to exec(ls -l)
int listDir(int data_fd) {
    if (!fork()) {
        close(1);   // make room for data_fd
        dup(data_fd);
        close(data_fd);
        execlp("ls", "ls", "-l", (char *) NULL);
    }
    else {
        int child_return_status;
        wait(&child_return_status);
        close(data_fd);
        if (WIFEXITED(child_return_status)) {
            return 0;
        }
        return -1;
    }
}

// establishes data connection data_fd with client
int buildDataConnection (int *data_fd, int control_fd) {

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
        return -1;
    }
    printf("child %d: created new data socket on descriptor %d\n", process_id, listenfd); 

    if ( bind(listenfd, (struct sockaddr *) &data_addr, sizeof(data_addr)) < 0) {
        fprintf(stderr, "child %d: Error binding the data connection socket\n", process_id);
        fprintf(stderr, "child %d: %s\n", process_id, strerror(errno));
        acknowledgeError(control_fd, "Could not bind data socket\n");
        return -1;
    } 

    if (listen(listenfd, 0) == -1) {
        fprintf(stderr, "child %d: listen call for data connection failed\n", process_id);
        fprintf(stderr, "child %d: %s\n", process_id, strerror(errno));
        acknowledgeError(control_fd, "Could not listen on data socket\n");
        return -1; 
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
        return -1;
    }
    printf("child %d: accepted new port fd: %d\n", process_id, tempfd);

    // cleanup listener
    close(listenfd);
    // push the data connection fd down the stack
    *data_fd = tempfd;
    return 0;
}

// response over control connection indicating previos command failed
// needs error checking
void acknowledgeError(int control_fd, char errorMsg[]) {
    
    char response[ARG_MAX_LEN] = {'\0'};  // construct response string
    response[0] = 'E'; 
    printf("error response is now <%s>\ncatting the error message", response);
    strcat(response, errorMsg);
    response[ARG_MAX_LEN-1] = '\0';
    printf("error message is now <%s>\n", response); 
    printf("child %d: error message written to client: <%s>\n", process_id, response);
    int i = 0;
    while ( response[i] != '\0') {
        printf("child %d: Writing errors - character <%c> to control connection\n", process_id, response[i]);
        if (writeWrapper(control_fd, &(response[i]), 1) == -1) {
            fprintf(stderr, "child %d: encountered error writing to control channel. Fatal\n", process_id);
            exit(WRT_ERROR);
        }
        i++;
    }
}

// sends 'A' response to client with optional arg data_port
// data_port is null if no port number is being sent
// depends on data_port being null terminated
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

//returns -1 upon error, otherwise returns bytes_written
int writeWrapper (int fd, char *msg, int write_bytes) {
    int bytes_written;
    if ( (bytes_written = write(fd, msg, write_bytes)) == -1) {
        printf("child %d: Error writing to descriptor %d\n", process_id, fd);
        return -1;
    }
    return bytes_written;
}


// attempts to acquire the binary file-write semaphore 
// return values:    0: semaphore acquired
//                  -1: semaphore is in use
//                  -2: semaphore error. Check errno
int takeSemaphore (int semid, struct sembuf *taker, int nops) {

    if (semop(semid, taker, nops) == -1) {
        int tempno = errno;
        if (tempno == EAGAIN) { // semaphore is in use
            return -1;
        }
        else {          
            return -2;  
        }
    }
    return 0; 
}

// fatal upon error. Relies upon caller to actually have acquired the semaphore
void releaseSemaphore (int semid, struct sembuf *replacer, int nops) {
    if (semop(semid, replacer, nops) == -1) {
        fprintf(stderr, "child %d: fatal error attempting to release semaphore\n", process_id);      
        exit(SEM_ERROR);
    }
}
