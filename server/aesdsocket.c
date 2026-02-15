#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#define PORT "9000"
#define tmp_file "/var/tmp/aesdsocketdata"

static bool keep_running = true;
static char *buffer;
static int server_socket;

void cleanup() {
    //free buffer
    free(buffer);
    //remove tmp file
    if (unlink(tmp_file) != 0) {
        syslog(LOG_ERR, "Failed to remove file %s", tmp_file);
    }
    //close connections and cleanup
    close(server_socket);
    closelog();
}


void end_program_singal(int signum) {
    
    syslog(LOG_INFO, "Caught signal, exiting");
    keep_running = false;

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

    // Allocate buffer for receiving data
       size_t buffer_size = (1024); // leave 10MB for the program itself

    //create buffer for messages
    buffer = malloc(buffer_size);
    if (buffer == NULL) {
        closelog();
        return -1;
    }

    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        syslog(LOG_ERR, "Failed to create socket.");
        closelog();
        return -1;
    }
    // Set socket options to allow reuse of address and port
    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        syslog(LOG_ERR, "Failed to set socket options.");
        cleanup();
        return -1;
    }

    // Resolve the server address and port
    struct addrinfo *res;
    if(getaddrinfo("localhost", PORT, NULL, &res) != 0) {
        syslog(LOG_ERR, "Failed to resolve address.");
        cleanup();
        return -1;
    }
   
    // bind the socket to the address and port
    if(bind(server_socket, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        syslog(LOG_ERR, "Failed to bind socket.");
        cleanup();
        return -1;
    }

    freeaddrinfo(res);

    if(argc>1 && strcmp(argv[1],"-d") == 0){
        //enter daemon mode
        //create child process
        pid_t pid = fork();
        //check if fork failed
        if(pid == -1){
            syslog(LOG_ERR, "Failed to fork process.");
            cleanup();
            return -1;
        }
        else if(pid == 0){
            //child process
            int setsid_result = setsid();
            if(setsid_result == -1){
                syslog(LOG_ERR, "Failed to set session id.");
                cleanup();
                return -1;
            }
            chdir("/");
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            
        }
        else{
            //parent process
            //exit parent process
            exit(0);
        }
    }
    

    // Listen for incoming connections
    int result = listen(server_socket, 5);
    if (result < 0) {
        syslog(LOG_ERR, "Failed to listen on socket.");
        cleanup();
        return -1;
    }

    while(keep_running){

        // Accept a connection
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int clientfd = accept(server_socket, &client_addr, &client_addr_len);
        if(keep_running == false){
            break;
        }
        else if (clientfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection.");
            cleanup();
            return -1;
        }

        syslog(LOG_INFO,"Accepted connection from %s",client_addr.sa_data);

        //open aesdsocketdata file and write to it
        FILE *transfered_data_file = fopen(tmp_file, "a+");
        if (transfered_data_file == NULL) {
            syslog(LOG_ERR, "Failed to open file %s", tmp_file);
            cleanup();
            return -1;
        }
        ssize_t bytes_received = 0;
        while((bytes_received = recv(clientfd, buffer, buffer_size, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, transfered_data_file);
             // Search for newline in the received chunk
            if (memchr(buffer, '\n', bytes_received)) {
                break;
            }
        }

        fflush(transfered_data_file);
        fseek(transfered_data_file, 0, SEEK_SET);

        memset(buffer, 0, buffer_size);

        while (fgets(buffer, buffer_size, transfered_data_file) != NULL) {
            send(clientfd, buffer, strlen(buffer), 0);
        }
        fclose(transfered_data_file);
        
        
        syslog(LOG_ERR, "Closing connection from %s", client_addr.sa_data);
        close(clientfd);

    }

    cleanup();

    return 0;
}