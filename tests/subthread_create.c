#define GC_NO_THREAD_REDIRECTS
#ifndef GC_THREADS
#  define GC_THREADS
#endif
#include "gc.h"
#include <atomic_ops.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SUBTHREAD_DEPTH 16
#define MAX_SUBTHREAD_COUNT 2048
#define INITIAL_THREAD_COUNT 128
#define DECAY_NUMER 15
#define DECAY_DENOM 16

void *entry(void *arg)
{
    static AO_t thread_count = (AO_t)0;
    pthread_t th;
    int thread_num = AO_fetch_and_add(&thread_count, 1);
    int my_depth = *(int *)arg + 1;
    if (my_depth < MAX_SUBTHREAD_DEPTH
            && thread_num < MAX_SUBTHREAD_COUNT
            && (thread_num % DECAY_DENOM) < DECAY_NUMER)
        GC_pthread_create(&th, NULL, entry, &my_depth);
    return arg;
}

int main(void)
{
    int i, err;
    GC_INIT();
    pthread_t th[INITIAL_THREAD_COUNT];
    int my_depth = 0;

    for (i = 0; i < INITIAL_THREAD_COUNT; ++i) {
        err = GC_pthread_create(&th[i], NULL, entry, &my_depth);
        if (err) {
            fprintf(stderr, "Thread creation failed: %s", strerror(err));
            exit(69);
        }
    }
    for (i = 0; i < INITIAL_THREAD_COUNT; ++i) {
        void *res;
        err = GC_pthread_join(th[i], &res);
        if (err) {
            fprintf(stderr, "Failed to join thread: %s", strerror(err));
            exit(69);
        }
    }
    return 0;
}
