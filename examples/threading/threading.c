#include "threading.h"
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
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
    struct thread_data *data = (struct thread_data *)thread_param;
    struct timespec obtain, release;
    obtain.tv_sec  = data->wait_to_obtain_ms / 1000;
    obtain.tv_nsec = (data->wait_to_obtain_ms % 1000) * 1000000L;
    release.tv_sec  = data->wait_to_release_ms / 1000;
    release.tv_nsec = (data->wait_to_release_ms % 1000) * 1000000L;
    nanosleep(&obtain, NULL);
    pthread_mutex_lock(data->mutex);
    nanosleep(&release, NULL);
    pthread_mutex_unlock(data->mutex);
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
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct thread_data *data = malloc(sizeof(struct thread_data));
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;
    int ret;
    ret = pthread_create(thread, &attr, threadfunc, data);
    if (ret == 0)
        return true;
    else {
        perror("pthread_create");
        return false;
    }
}

