#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int rc; 
    thread_func_args->thread_complete_success = false;
	
    // wait_to_obtain mutex
    DEBUG_LOG("wait to obtain mutex");
    rc = usleep(thread_func_args->wait_to_obtain_ms*1000);
    if(rc != 0){
	ERROR_LOG("Failed to wait_to_obtain mutex with error-code=%d, error-msg=%s", rc, strerror(rc));
 	return thread_param;
    }
	
    // obtain mutex
    DEBUG_LOG("obtain mutex");
    rc = pthread_mutex_lock(thread_func_args->mutex);
    if(rc != 0){
	ERROR_LOG("Failed to obtain mutex with error-code=%d, error-msg=%s", rc, strerror(rc));
	return thread_param;
    }
	
    // wait_to_release mutex
    DEBUG_LOG("wait to release mutex");
    rc = usleep(thread_func_args->wait_to_release_ms*1000);
    if(rc != 0){
	ERROR_LOG("Failed to wait_to_release mutex with error-code=%d, error-msg=%s", rc, strerror(rc));
	return thread_param;
    }
	
    // release mutex
    DEBUG_LOG("release mutex");
    rc = pthread_mutex_unlock(thread_func_args->mutex);
    if(rc != 0){
	ERROR_LOG("Failed to release mutex with error-code=%d, error-msg=%s", rc, strerror(rc));
	return thread_param;
    }

    thread_func_args->thread_complete_success = true;	
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
    bool status = false;
    int rc;
    if(( thread == NULL) ||( mutex == NULL)){
        ERROR_LOG("Nullpointer parameter");
        return false;
    }
    DEBUG_LOG("Allocate thread_data memory");
    struct thread_data *data = malloc(sizeof(struct thread_data));		
    if (data != NULL) {
	data->mutex = mutex;
	data->wait_to_obtain_ms = wait_to_obtain_ms;
	data->wait_to_release_ms = wait_to_release_ms;
	DEBUG_LOG("Create thread!");
	rc = pthread_create(thread, NULL, threadfunc, data);
	if(rc == 0){
            status = true;
	}else{
	    ERROR_LOG("Failed to create thread with error-code=%d, error-msg=%s", rc, strerror(rc));
	}
    }else{
	ERROR_LOG("Failed to allocate thread_data memory!");
    }
	 
    return status;
}

