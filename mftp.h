#include<fcntl.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<stdio.h>
#include<unistd.h>



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




//returns -1 upon error, otherwise returns bytes_written
int writeWrapper (int fd, char *msg, int write_bytes) {
    int bytes_written;
    if ( (bytes_written = write(fd, msg, write_bytes)) == -1) {
        return -1;
    }
    return bytes_written;
}
