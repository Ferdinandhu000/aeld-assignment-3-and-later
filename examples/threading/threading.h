#ifndef THREADING_H
#define THREADING_H

#include <stdbool.h>
#include <pthread.h>

/**
 * Thread arguments allocated by start_thread_obtaining_mutex() and returned
 * by threadfunc() so the caller that joins the thread can free them.
 */
struct thread_data {
    pthread_mutex_t *mutex;
    int wait_to_obtain_ms;
    int wait_to_release_ms;
    bool thread_complete_success;
};

/**
 * Start a thread which waits to obtain a mutex, holds it for the requested
 * duration, then releases it.  The caller must join the returned thread and
 * free the returned struct thread_data pointer.
 */
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms,
                                  int wait_to_release_ms);

#endif
