#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <unistd.h>

static bool keep_running = true;

void end_program_singal(int signum) {
    
    syslog(LOG_INFO, "Caught signal, exiting");

    return ;
}

int main(int argc, char *argv[]) {

    // Setup syslog with LOG_USER facility
    openlog("aesdsocket", 0, LOG_USER);
    syslog(LOG_INFO, "AESD Socket application started.");

    //Setup signal handlers to gracefully exit the program
    struct sigaction new_action;
    memset(&new_action,0,sizeof(struct sigaction));
    new_action.sa_handler = end_program_singal;
    
    if(sigaction(SIGINT,&new_action,NULL) !=0){
        syslog(LOG_ERR,"Failed to set signal SIGINT");
        closelog();
        return -1;
    }
    if(sigaction(SIGTERM, &new_action,NULL) !=0){
        syslog(LOG_ERR,"Failed to set signal SIGTERM");
        closelog();
        return -1;
    }
    
    //get heap size limit
    struct rlimit limits;
    memset(&limits, 0, sizeof(struct rlimit));

    if (getrlimit(RLIMIT_AS, &limits) != 0) {
        syslog(LOG_ERR, "Failed to get heap size limit.");
        closelog();
        return -1;
    }
    rlim_t heap_size = limits.rlim_cur ;// leave some space for the program itself
    printf("Heap size limit: %lu bytes\n", heap_size);
    heap_size = (1 * 1024 * 1024); // leave 10MB for the program itself

    //create buffer for messages
    char *buffer = malloc(heap_size);
        if (buffer == NULL) {
            closelog();
            free(buffer);
            return -1;
        }

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "Failed to create socket.");
        closelog();
        free(buffer);
        return -1;
    }

    // Resolve the server address and port
    struct addrinfo *res;
    if(getaddrinfo("localhost", "9000", NULL, &res) != 0) {
        syslog(LOG_ERR, "Failed to resolve address.");
        closelog();
        free(buffer);
        return -1;
    }
   
    // bind the socket to the address and port
    if(bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        syslog(LOG_ERR, "Failed to bind socket.");
        closelog();
        free(buffer);
        return -1;
    }

    freeaddrinfo(res);
    
    // Listen for incoming connections
    int result = listen(sockfd, 1);
    if (result < 0) {
        syslog(LOG_ERR, "Failed to listen on socket.");
        closelog();
        free(buffer);
        return -1;
    }

    while(keep_running){

        // Accept a connection
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int clientfd = accept(sockfd, &client_addr, &client_addr_len);
        if (clientfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection.");
            closelog();
            free(buffer);
            return -1;
        }

        syslog(LOG_INFO,"Accepted connection from %s",client_addr.sa_data);

        //open aesdsocketdata file and write to it
        FILE *transfered_data_file = fopen("/var/tmp/aesdsocketdata", "a");
        if (transfered_data_file == NULL) {
            syslog(LOG_ERR, "Failed to open file /var/tmp/aesdsocketdata");
            closelog();
            free(buffer);
            return -1;
        }

        int bytes_received = recv(clientfd, buffer, heap_size, 0);
        if (bytes_received > 0) {
            fwrite(buffer, 1, bytes_received, transfered_data_file);
        }
        fclose(transfered_data_file);
        memset(buffer, 0, heap_size);

        transfered_data_file = fopen("/var/tmp/aesdsocketdata", "r");
        if (transfered_data_file == NULL) {
            closelog();
            free(buffer);
            return -1;
        }

        while (fgets(buffer, 1024, transfered_data_file) != NULL) {
            send(clientfd, buffer, strlen(buffer), 0);
        }
        fclose(transfered_data_file);
        
        
        syslog(LOG_INFO, "Closing connection from %s", client_addr.sa_data);
        close(clientfd);

    }

    //close connections and cleanup
    free(buffer);
    //remove tmp file
    if (remove("/var/tmp/aesdsocketdata") != 0) {
        syslog(LOG_ERR, "Failed to remove file /var/tmp/aesdsocketdata");
    }

    closelog();

    return 0;
}