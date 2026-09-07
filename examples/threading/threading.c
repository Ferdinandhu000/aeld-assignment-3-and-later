#include "threading.h"

#include <stdlib.h>
#include <unistd.h>

void *threadfunc(void *thread_param)
{
    struct thread_data *thread_func_args = thread_param;

    thread_func_args->thread_complete_success = false;

    if (usleep((useconds_t)thread_func_args->wait_to_obtain_ms * 1000U) != 0) {
        return thread_func_args;
    }

    if (pthread_mutex_lock(thread_func_args->mutex) != 0) {
        return thread_func_args;
    }

    if (usleep((useconds_t)thread_func_args->wait_to_release_ms * 1000U) != 0) {
        (void)pthread_mutex_unlock(thread_func_args->mutex);
        return thread_func_args;
    }

    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) {
        return thread_func_args;
    }
    thread_func_args->thread_complete_success = true;

    return thread_func_args;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms,
                                  int wait_to_release_ms)
{
    struct thread_data *thread_data;

    if (thread == NULL || mutex == NULL || wait_to_obtain_ms < 0 ||
        wait_to_release_ms < 0) {
        return false;
    }

    thread_data = malloc(sizeof(*thread_data));
    if (thread_data == NULL) {
        return false;
    }

    thread_data->mutex = mutex;
    thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data->wait_to_release_ms = wait_to_release_ms;
    thread_data->thread_complete_success = false;

    if (pthread_create(thread, NULL, threadfunc, thread_data) != 0) {
        free(thread_data);
        return false;
    }

    return true;
}
