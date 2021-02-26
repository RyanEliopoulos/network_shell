//
// TODO: cleanup remoteListDir() and then finish everything else !
// TODO: stop repeating close(data_fd) everywhere. Maybe closing it upon return to control loop is best
// TODO: Remove all logic closing data_fd upon command failure. Server maintains the connection if it isn't consumed || This might not be true..hold on.

#define MY_PORT_NUMBER 49999
#define START_ERROR 1
#define CONN_ERROR 2

#include"mftp.h"
#include<sys/types.h>
#include<sys/stat.h>
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
#include<sys/wait.h>

void intakeInput (char *, char *);
void interfaceLoop (int);
char inputToCommand(char *);
int remoteToLocal(int, int, char*);
int localToRemote (int, int, char *);
int localcd (char *);
int ls ();
int remotecd (int, int, char *);
int remoteListDir (int, int);
int show (int, int, char *);
void readConnection(char *, char [], int);
int buildDataConnection (int *, int);

char *arg1;

int main (int argc, char *argv[]) {
    arg1 = argv[1];
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

    // ok, a control connection has been established. Now we would drop into our interface loop    
    interfaceLoop(socketfd); // primary logic
}


// primary control loop.
// inifinite loops parsing user input and translating
// to inputs for the server
void interfaceLoop (int control_fd) {

    // we will assume a data connection is consumed immediately after creation
    int data_fd;
    char command[2000] = {'\0'};
    char arg[2000] = {'\0'};

    while (1) {
        printf("MFTB>");                // prompt and parse user input
        intakeInput(command, arg); 

        switch(inputToCommand(command)) {   
            case 'X':
                if (DEBUG) printf("user entered exit command..\n"); 
                exit(0);
                break;
            case 'C':
                if (DEBUG) printf("user entered cd command\n");
                if (localcd(arg) == -1) {
                    fprintf(stderr, "encountered error changing directory: %s\n", strerror(errno));
                }
                else {
                    printf("successfully changed local directory\n");
                }
                break;
            case 'R':
                if (DEBUG) printf("user entered rcd command\n");
                if (remotecd(control_fd, data_fd, arg) == -1) {
                    printf("call to rcd failed\n");
                }
                else {
                    printf("call to rcd successful\n");
                }
                break;
            case 'L':
                if (DEBUG) printf("user entered ls command\n");
                if (ls() == -1) {
                    printf("call to ls was unsuccessful\n");
                } 
                else {
                    printf("ls call successful\n");
                }
                break;
            case 'S':
                if (buildDataConnection(&data_fd, control_fd) == -1) {
                    printf("rls failed: data connection couldn't be established\n");
                }
                else if (remoteListDir(control_fd, data_fd) == -1) {
                    printf("rls failed: fnx call didn't work\n");
                }
                else {
                    printf("rls completed successfully\n"); 
                }
                data_fd = -1;
                break; 
            case 'G':
                if (DEBUG) printf("user entered get command\n"); 
                if (buildDataConnection(&data_fd, control_fd) == -1) {
                    printf("get failed. Couldn't establish data connection\n");
                }
                else if (remoteToLocal(control_fd, data_fd, arg) == -1) {
                    printf("get failed\n");
                }
                else {
                    printf("get succeeded\n");
                }
                data_fd = -1;
                break;
            case 'P':
                if (DEBUG) printf("user entered put command\n");
                if (buildDataConnection(&data_fd, control_fd) == -1) {
                    printf("put failed: couldn't establish data connection\n");
                }
                else if (localToRemote(control_fd, data_fd, arg) == -1) {
                    printf("put call failed\n");
                }
                else {
                    printf("put call succeeded\n");
                }
                break;
            case 'O':   // show
                if (DEBUG) printf("call to 'show' with parameter '%s'\n", arg);
                if (buildDataConnection(&data_fd, control_fd) == -1) {
                    printf("show failed: data connection couldn't be established\n");
                } 
                else if (show(control_fd, data_fd, arg) == -1) {
                    printf("show failed: fnx call didn't work\n");
                }
                else {
                    printf("show call successfully completed\n");
                }
                data_fd = -1;
                break;
            default:
                printf("<%s> is not a recognized command\n", command);
        }

        // DEBUG printing
        if (DEBUG) printf("user command is <%s>\n", command);
        if (DEBUG) printf("user arg is <%s>\n", arg); 

        // now reset the input arrays for continued use
        for (int i = 0; i < 2000; i++) {
            command[i] = '\0';
            arg[i] = '\0';
        } 
    } 
}

//
// intended to be called in a switch statement
// returns a single character corresponding to
// a server command in the switch block
// command and arg will exist in scope of call
char inputToCommand (char *command) {

    if (!strcmp("exit", command)) return 'X';
    if (!strcmp("cd", command)) return 'C';
    if (!strcmp("rcd", command)) return 'R';
    if (!strcmp("ls", command)) return 'L';
    if (!strcmp("rls", command)) return 'S';
    if (!strcmp("get", command)) return 'G';
    if (!strcmp("put", command)) return 'P';
    if (!strcmp("show", command)) return 'O';

    return 'z';  // invalid input
}


int localcd (char *path) {

    if (path == NULL) {
        printf("path is null\n");
        return -1;
    }
    path[strlen(path)-1] = '\0';
    
    if (chdir(path) == -1) {
        fprintf(stderr, "error in call to chdir: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int ls () {
   
    // prepare pipe 
    int fd[2];
    if (pipe(fd) == -1) {
        fprintf(stderr, "error establishing ls | more pipe: %s\n", strerror(errno));
        return -1;
    }
    // re-org descriptors
    
    if (!fork()) { // call ls -l
        close(fd[0]);
        close(1);
        dup(fd[1]);
        close(fd[1]);
        printf("forking ls\n");
        // prep ls -l to pipe into more -20
        execlp("ls", "ls", "-l", (char *) NULL); 
    }
    if (!fork()) {  // call more
        close(fd[1]);
        close(0);
        dup(fd[0]);
        close(fd[0]);
        printf("forking more\n");
        execlp("more", "more", "-20", (char *) NULL);

    }
    close(fd[0]);
    close(fd[1]);
    if (DEBUG) printf("waiting on child 1\n");
    wait(NULL);
    if (DEBUG) printf("waiting on child 2\n");
    wait(NULL);
    if (DEBUG) printf("returning to control loop\n");
    return 0;
}

int remotecd (int control_fd, int data_fd, char *parameter) {

    if (write(control_fd, "C", 1) == -1) {
        fprintf(stderr, "Encountered error writing to controll connection: %s\n", strerror(errno));
        close(data_fd);
        return -1;
    }
    if (write(control_fd, parameter, strlen(parameter)) == -1) {
        fprintf(stderr, "encountered error writing parameter to control connection: %s\n", strerror(errno));
        close(data_fd);
        return -1;
    }
    if (DEBUG) printf("wrote to server\n");

    char ret;
    char server_message[ARG_MAX_LEN] = {'\0'};
    readConnection(&ret, server_message, control_fd);


    // check server response code
    if (ret == 'E') {
        printf("server responded with error: %s\n", server_message);
        close(data_fd);
        return -1;
    }
    if (ret != 'A') {
        printf("server response error: first character is <%c>\n", ret); 
        printf("server response message: %s\n", server_message);
        close(data_fd);
        return -1;
    }
    // positive acknowledgement
    return 0;
}

// pulls file data from server
// and pipes into more
int show (int control_fd, int data_fd, char *parameter) {

    if (write(control_fd, "G", 1) == -1) {
        fprintf(stderr, "Encountered error writing to control connection: %s\n", strerror(errno));
        close(data_fd);
        return -1; 
    }
    if (write(control_fd, parameter, strlen(parameter)) == -1) {
        fprintf(stderr, "encountered error writing pathname to control connection: %s\n", strerror(errno));
        close(data_fd);
        return -1;
    } 


    if (DEBUG) printf("wrote 'G%s\n' to server\n", parameter);    

    char ret;
    char server_message[ARG_MAX_LEN] = {'\0'};
    if (DEBUG) printf("entering read connection\n");
    readConnection(&ret, server_message, control_fd);
    if (DEBUG) printf("returned from readConnection\n");
    if (ret == 'E') {
        printf("server responded with error: %s\n", server_message);
        close(data_fd);
        return -1;
    }
    if (ret != 'A') {
        printf("server response error: reply begins with <%c>\n", ret);
        printf("server respones: %s\n", server_message);
        close(data_fd);
        return -1;
    }
    printf("about to fork!\n");

    if (fork()) {
        close(data_fd);
    } 
    else {
        close(0);
        dup(data_fd);
        close(data_fd);
        execlp("more", "more", "-20", (char *) NULL);
    }
    wait(NULL);
    return 0;
}

// ret -1 upon failure, 0 on success
int remoteListDir (int control_fd, int data_fd) {
    if (DEBUG) printf("entering remoteListDir\n");
    // send 'L' signal to server
    if (write(control_fd, "L\n", 2) == -1) {
        fprintf(stderr, "Encountered error writing to control connection: %s\n", strerror(errno));
        close(data_fd);
        return -1;
    }

    if (DEBUG) printf("wrote 'L\n' to control_fd\n");
    // check for acknowledgement
    char ret;
    char server_message[ARG_MAX_LEN] = {'\0'};
    if (DEBUG) printf("entering readConnection\n");
    readConnection(&ret, server_message, control_fd);
    if (DEBUG) printf("Returned from readConnection\n"); 
    if (ret == 'E') {
        printf("Server responded with error: %s\n", server_message);
        close(data_fd);
        return -1;
    } 
    if (ret != 'A') {
        printf("server response error: Reply begins with <%c>\n", ret);
        close(data_fd);
        return -1;
    }    
    if (DEBUG) printf("about to fork!\n");

    // server acknowledges
    pid_t child;
    if ((child = fork()) > 0) {
        close(data_fd);
    }
    else {
        close(0);
        dup(data_fd); 
        close(data_fd);
        execlp("more", "more", "-20", (char *) NULL);
    }
 
    if (DEBUG) printf("waiting\n");
    wait(NULL); 
    if (DEBUG) printf("done waiting\n");
    return 0; 
}

// fgets user input
// 1. command will never contain a trailing newline.
// 2. arg will always have a trailing newline
void intakeInput (char *command, char *arg) {

    char buf[2000];
    char *temp_cmd, *temp_arg;
    fgets(buf, 2000, stdin);
    
    temp_cmd = strtok(buf, " ");
    if (temp_cmd[0] == '\n') {
        if (DEBUG) printf("looks like user entered nothing\n");
        return;  // user entered nothing
    }
    
    //  ensure command string doesn't end with a newline character
    if (temp_cmd[strlen(temp_cmd)-1] == '\n') temp_cmd[strlen(temp_cmd)-1] = '\0';

    // user entered at least one char
    strcpy(command, temp_cmd);

    // check if there is something for arg
    temp_arg = strtok(NULL, " ");
    if (temp_arg == NULL) {
        arg[0] = '\n';
        if (DEBUG) printf("temp_arg is null\n");
        return;
    }

    // copy temp_arg down the stack
    // append trailing newline, if required
    strcpy(arg, temp_arg);
    if (arg[strlen(arg)-1] != '\n') arg[strlen(arg)] = '\n';
}

// "puts" the client's file on the server
// relies upon a "write file" mutex to eliminate
// file check/creation race condition
int remoteToLocal (int control_fd, int data_fd, char *client_arg) {

    // send command
    if (write(control_fd, "G", 1) == -1) {
        fprintf(stderr, "error writing to control connection: %s\n", strerror(errno));
        close(data_fd); 
        return -1;
    } 
    if (write(control_fd, client_arg, strlen(client_arg)) == -1) {
        fprintf(stderr, "error writing to control connection: %s\n", strerror(errno));
        close(data_fd);
        return -1;
    }
    // check response
    char ret;
    char server_response[ARG_MAX_LEN] = {'\0'};
    readConnection(&ret, server_response, control_fd);
    if (ret == 'E') {
        printf("server error: %s\n", server_response);
        close(data_fd);
        return -1;
    }
    if (ret != 'A') {
        printf("server response error: should not begin with <%c>\n", ret);
    }
    // positive acknowledgement
     
    // remove trailing newline 
    client_arg[strlen(client_arg)-1] = '\0';

    // investigate existence of given file name
    char response[ARG_MAX_LEN] = {'\0'};
    if (access(client_arg, F_OK)) {
        int tempno = errno;
        if (tempno == ENOENT) {  // file name not in use
            int new_fd;  // closes at end of the loop or before returning from a semctl error
            if ( (new_fd = open(client_arg, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP)) == -1) {  // attempt to create the file
                strcat(response, strerror(errno));
                strcat(response, "\n");
                close(data_fd);
                fprintf(stderr, "error openig new file locally: %s\n", strerror(errno));         
                return -1;
            }
            else {   // now beginning data transfer

                // the file transfer begins 
                int read_bytes, written_bytes;
                char temp_data[512];
                while ( (read_bytes = read(data_fd, temp_data, 512)) > 0) {
                    if (DEBUG) printf("read %d bytes from client\n", read_bytes);
                    if ( (written_bytes = writeWrapper(new_fd, temp_data, read_bytes)) == -1) {
                        strcat(response, strerror(errno));
                        strcat(response, "\n");
                        fprintf(stderr, "error writing to local file: %s\n", strerror(errno));
                        close(data_fd); close(new_fd);
                        return -1;
                    }
                    if (DEBUG) printf("wrote %d more bytes to the new file\n", written_bytes);
                }
                // check whether EOF or an error terminated reads 
                if (read_bytes < 0) {
                    strcat(response, strerror(errno));
                    strcat(response, "\n");
                    fprintf(stderr, "error reading from data connection: %s\n", strerror(errno));
                    close(data_fd); close(new_fd);
                    return -1;
                }
                printf("reached EOF reading from data connection\n");
                close(new_fd);
            }
        }
        else {  // there was potentially an error with access itself
            fprintf(stderr, "access call error: %s\n", strerror(errno));
            strcat(response, strerror(errno));
            strcat(response, "\n");
            close(data_fd);
            return -1;
        }
    }
    else {  // file already exists
        fprintf(stderr, "File we were asked to create already exists\n");
        strcat(response, "That filslefijee already exists on the server\n");
        close(data_fd);
        return -1;
    }
    
    close(data_fd);
    return 0;
}

//  writes a local file to the data connection
//  server's response to the 'G' command
int localToRemote (int control_fd, int data_fd, char *client_arg) {

    // command to server
    if (write(control_fd, "P", 1) == -1) {
        fprintf(stderr, "encountered error writing to control connection: %s\n", strerror(errno));
        return -1;
    }
    if (write(control_fd, client_arg, strlen(client_arg)) == -1) {
        fprintf(stderr, "encountered error writing to control connection: %s\n", strerror(errno));
        return -1;
    } 

    // check response
    char ret;
    char server_response[ARG_MAX_LEN] = {'\0'};
    readConnection(&ret, server_response, control_fd);
    if (ret == 'E') {
        printf("server error: %s\n", server_response);
        close(data_fd);
        return -1;
    }
    if (ret != 'A') {
        printf("server response error: shouldn't begin with %c\n", ret);
        close(data_fd);
        return -1;
    }
    // positive response
    
    // delete the trailing newline 
    client_arg[strlen(client_arg)-1] = '\0';

    // init vars 
    char response[ARG_MAX_LEN] = {'\0'};  // error response string
    FILE *file;                           // stream for desired file
    struct stat file_info;               

    // check if specific file is a valid option
    if (stat(client_arg, &file_info) == -1) {
        strcat(response, strerror(errno));    
        strcat(response, "\n");             // strerror doesn't supply a newline
        fprintf(stderr, "stat failed in localToRemote: %s\n", strerror(errno)); 
    }
    else if (!S_ISREG(file_info.st_mode)) {
        strcat(response, "path is not a regular file\n");
        fprintf(stderr, "given path is not a regular file\n");
    }   
    else if ( (file = fopen(client_arg, "r")) == NULL) {
        strcat(response, strerror(errno));
        strcat(response, "\n");
        fprintf(stderr, "unable to open the file\n%s\n", strerror(errno));
    }
    else {  // at this point the file has been opened successfully
        //acknowledgeSuccess(control_fd, NULL); 
        if (DEBUG) printf("successfully opened <%s>\n", client_arg);
        // transfer file data from local file to data connection
        char file_chunk[512] = {'\0'};
        int read;
        while ( (read = fread(file_chunk, sizeof(char), 511, file)) == 511) {
            /* upon write failure the server will return to the control loop   */
            /* looking for further commands. If client disconnected it will be */
            /* handled there                                                   */
            if (DEBUG) printf("read %d bytes of the file into memory\n", read);
            if (writeWrapper(data_fd, file_chunk, 511) == -1) {
                fprintf(stderr, "Error writing file to data connection\n");
                //close(data_fd);
                return -1;
            }
            if (DEBUG) printf("wrote another chunk to the server\n");
        }
        // Now check if there was an fread error.
        // if there was not, the last group
        // of data needs to be written
        if (feof(file)) {  
            if (writeWrapper(data_fd, file_chunk, read) == -1) {
                fprintf(stderr, "Error writing final final chunk to data connection\n");
                //close(data_fd);
                return -1;
            }
            // file successfully written.
        } 
        else {   // an fread error occurred :(
            //close(data_fd);
            fprintf(stderr, "Encountered fread error transfering file to client\n");
            return -1;
        }
        printf("writing complete. closing data connection\n");
        close(data_fd);
        return 0;
    }

    /* this covers error response for the first      */
    /* three clauses of the initial if               */
    /* any errors after opening the file are handled */
    /* in the outer else clause                      */
    close(data_fd);
    return -1;
}



// responsible for read() calls on the TCP connection
// relays this info back to controlLoop
// copied from server so var names are nonsense
void readConnection (char *cmd, char client_arg[], int connectfd) {

    if (DEBUG) printf("inside readConnection\n");
    // First get the command
    if (read(connectfd, cmd, 1) == 0) {
        fprintf(stderr, "unexpected EOF reading command..terminating\n");
        exit(COMM_ERROR);
    }

    if (DEBUG) printf("read %c from the control connection\n", *cmd);
    // command has been read. Now for the argument, if any    
    int i = 0;              // index for path
    while (i < ARG_MAX_LEN - 1) {      // longest possibl argument accepted from client

        // read next character
        if (read(connectfd, &client_arg[i], 1) == 0) {
            fprintf(stderr, "unexpected EOF reading argument..terminating\n");
            exit(COMM_ERROR);
        }
        if (client_arg[i] == '\n') {      // check for command termination
            client_arg[i] = '\0';         // place real terminator if so
            printf("message from server: <%s>\n", client_arg);
            return;
        }
        i++;                        // otherwise increment and continue
    }
    client_arg[ARG_MAX_LEN-1] = '\0';
    if (DEBUG) printf("read <%s> from server\n", client_arg);
}


// sloppy piece of shit. Really will need to come back to this
int buildDataConnection (int *data_fd, int control_fd) {

    // initiatie contact with server
    if (write(control_fd, "D\n", 2) == -1) {
        fprintf(stderr, "Error attempting write to server\n");
        return -1;
    }

    // examine response
    char cmd;
    char server_response[ARG_MAX_LEN] = {'\0'};
    readConnection(&cmd, server_response, control_fd);     
    
    if (cmd == 'E') {
        fprintf(stderr, "Server error: <%s>\n", server_response);
        return -1;
    }
    else if (cmd != 'A') {
        fprintf(stderr, "Server response error: First letter should not be '%c'\n", cmd);
        return -1;
    }
   
    // server response is 'A' 
    // extract port number
    //
    int data_port = atoi(server_response); 
    printf("The new data port is %d\n", data_port);

 
    *data_fd = socket(AF_INET, SOCK_STREAM, 0);
        // prepare address info of the server */
    struct sockaddr_in servAddr;
    struct hostent* hostEntry;
    struct in_addr **pptr;

    // set initial values */
    memset( &servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(data_port);


    if ( (hostEntry = gethostbyname(arg1)) == NULL) {
        herror("invalid host name..attempting to use arg as IP address\n");

        // didn't work. Now check if its an IPv4 address */
        if (inet_pton(AF_INET, arg1, &servAddr.sin_addr)<=0) {
            printf("both hostname and IP address failed\n");
            memset( &servAddr, 0, sizeof(servAddr));
            close(*data_fd);
            return -1;
        }
    }
    else { // hostname sucess..copy data to appropriate structure */
        printf("gethostbyname success\n");
        pptr = (struct in_addr **) hostEntry->h_addr_list;
        memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
    }

     // server information gathered. Connecting */ 
    if (connect(*data_fd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        fprintf(stderr, "Error: connection failed\n");
        close(*data_fd);
        return -1;
    }
    return 0;
}

