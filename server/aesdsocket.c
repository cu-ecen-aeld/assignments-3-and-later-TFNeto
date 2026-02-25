#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

/*Defines*/
#define PORT "9000"
#define tmp_file "/var/tmp/aesdsocketdata"



// Define a linked list to store client connections
struct client_thread {
    pthread_t thread_id;
    int clientfd;
    bool thread_complete_success;
    SLIST_ENTRY(client_thread) nodes;
};

SLIST_HEAD(client_thread_head, client_thread);

//thread data structure for passing mutexes to threads
typedef struct thread_data {
    bool thread_complete_success;
    struct sockaddr client_addr;
    struct client_thread *client_thread_node;
    int clientfd;
} thread_data_t;

/***Local Data***/

static bool keep_running = true;
static const size_t buffer_size = 1024; 
static char *buffer;
static int server_socket;

static pthread_mutex_t file_mutex;
static pthread_mutex_t linked_list_mutex;

//linked list head for client threads
struct client_thread_head client_threads_head;

/*Function Prototypes*/
void* message_handler_thread (void* thread_data);
void check_completed_threads(void);
void set_thread_complete_success(struct client_thread* node, bool success);
thread_data_t* create_thread_data(void);
void clean_threads(void);
void cleanup(); 

/*This function handles messages from a client thread*/
void* message_handler_thread (void* thread_data) {

    thread_data_t *data = (thread_data_t*) thread_data;

    int mutex_lock_result = pthread_mutex_lock(&file_mutex);
    if (mutex_lock_result != 0) {
        syslog(LOG_ERR, "Failed to lock file mutex: %d", mutex_lock_result);
        data->thread_complete_success = false;
        set_thread_complete_success(data->client_thread_node, true);
        return thread_data;
    }
    
    //open aesdsocketdata file and write to it
    FILE *transfered_data_file = fopen(tmp_file, "a+");
    if (transfered_data_file == NULL) {
        syslog(LOG_ERR, "Failed to open file %s", tmp_file);
        cleanup();
        data->thread_complete_success = false;
        set_thread_complete_success(data->client_thread_node, true);
        return thread_data;
    }
    ssize_t bytes_received = 0;
    while((bytes_received = recv(data->clientfd, buffer, buffer_size, 0)) > 0) {
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
        send(data->clientfd, buffer, strlen(buffer), 0);
    }
    fclose(transfered_data_file);

    int mutex_unlock_result = pthread_mutex_unlock(&file_mutex);
    if (mutex_unlock_result != 0) {
        syslog(LOG_ERR, "Failed to unlock file mutex: %d", mutex_unlock_result);
        data->thread_complete_success = false;
        set_thread_complete_success(data->client_thread_node, true);
        return thread_data;
    }

    data->thread_complete_success = true;
    set_thread_complete_success(data->client_thread_node, true);

    return thread_data;
}

void cleanup() {
    
    if(SLIST_EMPTY(&client_threads_head) == false) {
        syslog(LOG_INFO, "Waiting for threads to complete...");
        check_completed_threads();
    }
    clean_threads();
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
    
    syslog(LOG_INFO, "Caught signal %d, exiting", signum);
    keep_running = false;

    return ;
}

void check_completed_threads() {
    struct client_thread *current_node;

    //aquire lock to access linked list of threads
    int mutex_lock_result = pthread_mutex_lock(&linked_list_mutex);
    if (mutex_lock_result != 0) {
        syslog(LOG_ERR, "Failed to lock linked list mutex: %d", mutex_lock_result);
        return;
    }

    // Iterate through the linked list and join threads that have completed
    SLIST_FOREACH(current_node, &client_threads_head, nodes) 
    {
        if (current_node->thread_complete_success) {
            thread_data_t *temp_thread_data;

            int join_result = pthread_join(current_node->thread_id, (void**)&temp_thread_data);
            if (join_result != 0) {
                syslog(LOG_ERR, "Failed to join thread: %d", join_result);
                //continue; // Skip this thread and continue with the next one
            }
            SLIST_REMOVE(&client_threads_head, current_node, client_thread, nodes);
            free(current_node);
            close(temp_thread_data->clientfd);
            syslog(LOG_INFO, "Closed connection from %s", temp_thread_data->client_addr.sa_data);
            free(temp_thread_data);
        }
    }
    //release lock
    int mutex_unlock_result = pthread_mutex_unlock(&linked_list_mutex);
    if (mutex_unlock_result != 0) {
        syslog(LOG_ERR, "Failed to unlock linked list mutex: %d", mutex_unlock_result);
        return; 
    }
}

void clean_threads(void)
{   
    struct client_thread *current_node;
    //aquire lock to access linked list of threads
    int mutex_lock_result = pthread_mutex_lock(&linked_list_mutex);
    if (mutex_lock_result != 0) {
        syslog(LOG_ERR, "Failed to lock linked list mutex: %d", mutex_lock_result);
        return;
    }

    // Iterate through the linked list remove all threads
    
    while (!SLIST_EMPTY(&client_threads_head)) {

        current_node = SLIST_FIRST(&client_threads_head);
        thread_data_t *temp_thread_data = NULL;

        int join_result = pthread_join(current_node->thread_id, (void**)&temp_thread_data);
        if (join_result != 0) {
            syslog(LOG_ERR, "Failed to join thread: %d", join_result);
            //continue; // Skip this thread and continue with the next one
        }
        SLIST_REMOVE(&client_threads_head, current_node, client_thread, nodes);
        free(current_node);
        syslog(LOG_INFO, "Closed connection from %s", temp_thread_data->client_addr.sa_data);
        close(temp_thread_data->clientfd);
        free(temp_thread_data);
    }
    //release lock
    int mutex_unlock_result = pthread_mutex_unlock(&linked_list_mutex);
    if (mutex_unlock_result != 0) {
        syslog(LOG_ERR, "Failed to unlock linked list mutex: %d", mutex_unlock_result);
        return; 
    }
}

void set_thread_complete_success(struct client_thread* node, bool success) {

    struct client_thread *current_node = NULL;

    //aquire lock to access linked list of threads
    int mutex_lock_result = pthread_mutex_lock(&linked_list_mutex);
    if (mutex_lock_result != 0) {
        syslog(LOG_ERR, "Failed to lock linked list mutex: %d", mutex_lock_result);
        return;
    }

    // Iterate through the linked list and join threads that have completed
    SLIST_FOREACH(current_node, &client_threads_head, nodes) 
    {
        if (current_node == node) {
            node->thread_complete_success = success;
            break;
        }
    }
    //release lock
    int mutex_unlock_result = pthread_mutex_unlock(&linked_list_mutex);
    if (mutex_unlock_result != 0) {
        syslog(LOG_ERR, "Failed to unlock linked list mutex: %d", mutex_unlock_result);
        return; 
    }

    return;
}

thread_data_t* create_thread_data(void) {
    thread_data_t* data = malloc(sizeof(thread_data_t));
    if (data == NULL) {
        return NULL;
    }
    data->thread_complete_success = false;
    return data;
}

void print_timestamp(int signum) {
    struct timespec timestamp;
    char timestamp_str[50];
    int time_result = clock_gettime(CLOCK_REALTIME, &timestamp);
    if (time_result != 0) {
        syslog(LOG_ERR, "Failed to get current time");
        return;
    }
    struct tm *tm_info = localtime(&timestamp.tv_sec);
    if (tm_info == NULL) {
        syslog(LOG_ERR, "Failed to convert time to local time");
        return;
    }
    int strftime_result = strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", tm_info);
    if (strftime_result == 0) {
        syslog(LOG_ERR, "Failed to format timestamp string");
        return;
    }

    int mutex_lock_result = pthread_mutex_lock(&file_mutex);
    if (mutex_lock_result != 0) {
        syslog(LOG_ERR, "Failed to lock file mutex: %d", mutex_lock_result);
        return ;
    }

    FILE *transfered_data_file = fopen(tmp_file, "a+");
    if (transfered_data_file == NULL) {
        syslog(LOG_ERR, "Failed to open file %s", tmp_file);
        cleanup();
    }
    char timestamp_line[70];
    sprintf(timestamp_line, "timestamp:%s \n", timestamp_str);
    syslog(LOG_ERR, "Writing timestamp to file: %s", timestamp_line);

    fwrite(timestamp_line, 1, strlen(timestamp_line), transfered_data_file);

    fflush(transfered_data_file);
    fclose(transfered_data_file);

    int mutex_unlock_result = pthread_mutex_unlock(&file_mutex);
    if (mutex_unlock_result != 0) {
        syslog(LOG_ERR, "Failed to unlock file mutex: %d", mutex_unlock_result);
        return ;
    }
    return;
    
}

int set_timer(void)
{
    static struct itimerval timestamp_timer;

    // Set up the timer to trigger every 10 seconds
    timestamp_timer.it_value.tv_sec = 10;
    timestamp_timer.it_value.tv_usec = 0;
    // Set the interval for the timer to 10 seconds
    timestamp_timer.it_interval.tv_sec = 10;
    timestamp_timer.it_interval.tv_usec = 0;

    struct sigaction new_action;
    memset(&new_action,0,sizeof(struct sigaction));
    new_action.sa_handler = print_timestamp;
    if(sigaction(SIGALRM,&new_action,NULL) !=0){
        syslog(LOG_ERR, "Failed to set SIGEV_THREAD handler");
        return -1;
    }

    int res = setitimer(ITIMER_REAL, &timestamp_timer, NULL);
    if (res != 0) {
        syslog(LOG_ERR, "Failed to set timer: %d", res);
        return -1;
    }

    return 0;
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

    //create thread locks for file and linked list access
    int mutex_init_result = pthread_mutex_init(&file_mutex, NULL);
    if (mutex_init_result != 0) {
        syslog(LOG_ERR, "Failed to initialize file mutex: %d", mutex_init_result);
        closelog();
        return -1;
    }
    mutex_init_result = pthread_mutex_init(&linked_list_mutex, NULL);
    if (mutex_init_result != 0) {
        syslog(LOG_ERR, "Failed to initialize linked list mutex: %d", mutex_init_result);
        pthread_mutex_destroy(&file_mutex);
        closelog();
        return -1;
    }

    //create buffer for messages
    buffer = malloc(buffer_size);
    if (buffer == NULL) {
        closelog();
        return -1;
    }

     // Resolve the server address and port
    struct addrinfo *res;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        syslog(LOG_ERR, "Failed to resolve address.");
        cleanup();
        return -1;
    }

    // Create a socket
    server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
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
    

    int set_time_res = set_timer();
    if (set_time_res != 0) {
        cleanup();
        return -1;
    }

    // Initialize the linked list for client threads
    struct client_thread *last = NULL;
    SLIST_INIT(&client_threads_head);


    // Listen for incoming connections
    int result = listen(server_socket, 5);
    if (result < 0) {
        syslog(LOG_ERR, "Failed to listen on socket.");
        cleanup();
        return -1;
    }

    while(keep_running){

        // Create a thread data to handle the connection
        pthread_t thread_id;
        thread_data_t* thread_data = create_thread_data();
        if (thread_data == NULL) {
            syslog(LOG_ERR, "Failed to create thread data.");
            continue;
        }

        // Accept a connection
        socklen_t client_addr_len = sizeof(thread_data->client_addr);
        thread_data->clientfd = accept(server_socket, &thread_data->client_addr, &client_addr_len);
        if(keep_running == false){
            free(thread_data);
            break;
        }
        else if (thread_data->clientfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection.");
            free(thread_data);
            continue;
        }

        syslog(LOG_INFO,"Accepted connection from %s",thread_data->client_addr.sa_data);
        
        //create a thread to handle the connection
        int rc = pthread_create(&thread_id, NULL, (void *)message_handler_thread, thread_data);
        if (rc != 0) {
            syslog(LOG_ERR, "Failed to create thread: %d", rc);
            close(thread_data->clientfd);
            free(thread_data);
            continue;
        }

        // Add the thread to the linked list
        struct client_thread *new_node = malloc(sizeof(struct client_thread));
        if (new_node == NULL) {
            syslog(LOG_ERR, "Failed to allocate memory for client thread node.");
            close(thread_data->clientfd);
            free(thread_data);
            continue;
        }
        new_node->thread_id = thread_id;
        new_node->clientfd = thread_data->clientfd;
        new_node->thread_complete_success = false;

        //aquire lock to access linked list of threads
        int mutex_lock_result = pthread_mutex_lock(&linked_list_mutex);
        if (mutex_lock_result != 0) {
            syslog(LOG_ERR, "Failed to lock linked list mutex: %d", mutex_lock_result);
            close(thread_data->clientfd);
            free(thread_data);
            free(new_node);
            continue;
        }

        if(last == NULL) {
            SLIST_INSERT_HEAD(&client_threads_head, new_node, nodes);
        } else {
            SLIST_INSERT_AFTER(last, new_node, nodes);
        }
         //release lock
        int mutex_unlock_result = pthread_mutex_unlock(&linked_list_mutex);
        if (mutex_unlock_result != 0) {
            syslog(LOG_ERR, "Failed to unlock linked list mutex: %d", mutex_unlock_result);
            continue; 
        }
    
        last = new_node;

        check_completed_threads();

    }

    cleanup();

    return 0;
}