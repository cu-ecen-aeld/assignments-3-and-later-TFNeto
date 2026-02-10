#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* data = (struct thread_data*) thread_param;

    // Wait for the specified time before obtaining the mutex
    usleep(data->wait_to_obtain_ms * 1000); // Convert ms to microseconds

    // attempt to lock the mutex
    int rc = pthread_mutex_lock(data->mutex);
    if (rc != 0) {
        ERROR_LOG("Failed to lock mutex: %d", rc);
        return thread_param;
    }
    // Wait for the specified time while holding the mutex
    usleep(data->wait_to_release_ms * 1000); // Convert ms to microseconds
    // attempt to unlock the mutex
    rc = pthread_mutex_unlock(data->mutex);
    if (rc != 0) {
        ERROR_LOG("Failed to unlock mutex: %d", rc);
        return thread_param;
    }
    // Everything ok -> set the thread_complete_success flag to true before exiting
    data->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data* new_data = (struct thread_data*) malloc(sizeof(struct thread_data));

    if (new_data == NULL) {
        ERROR_LOG("Failed to allocate memory for thread_data");
        return false;
    }
    // Initialize the mutex in the thread_data structure
    new_data->mutex = mutex;
    new_data->wait_to_obtain_ms = wait_to_obtain_ms;
    new_data->wait_to_release_ms = wait_to_release_ms;

    // create the thread
    int rc = pthread_create(thread,NULL,threadfunc,new_data);
    if (rc != 0) {
        ERROR_LOG("Failed to create thread: %d", rc);
        free(new_data); // Free the allocated memory if thread creation fails
        return false;
    }
    
    return true;
}

