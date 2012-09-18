
#ifdef HAVE_CONFIG_H
  /* For PARALLEL_MARK */
# include "config.h"
#endif

#ifndef GC_THREADS
# define GC_THREADS
#endif
#include "gc.h"

#ifdef PARALLEL_MARK
# define AO_REQUIRE_CAS
#endif
#include "atomic_ops.h"

#include <stdio.h>

#ifdef AO_HAVE_fetch_and_add

#ifdef GC_PTHREADS
# include <pthread.h>
#else
# include <windows.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifndef MAX_SUBTHREAD_DEPTH
# define INITIAL_THREAD_COUNT 31
# define MAX_ALIVE_THREAD_COUNT 55
# define MAX_SUBTHREAD_DEPTH 7
# define MAX_SUBTHREAD_COUNT 200
#endif

#ifndef DECAY_NUMER
# define DECAY_NUMER 15
# define DECAY_DENOM 16
#endif

volatile AO_t thread_created_cnt = 0;
volatile AO_t thread_ended_cnt = 0;

#ifdef GC_PTHREADS
  void *entry(void *arg)
#else
  DWORD WINAPI entry(LPVOID arg)
#endif
{
    int thread_num = AO_fetch_and_add(&thread_created_cnt, 1);
    GC_word my_depth = (GC_word)arg + 1;

    if (my_depth <= MAX_SUBTHREAD_DEPTH
            && thread_num < MAX_SUBTHREAD_COUNT
            && (thread_num % DECAY_DENOM) < DECAY_NUMER
            && (int)(thread_num - AO_load(&thread_ended_cnt))
                <= MAX_ALIVE_THREAD_COUNT) {
# ifdef GC_PTHREADS
        int err;
        pthread_t th;
        err = pthread_create(&th, NULL, entry, (void *)my_depth);
        if (err) {
            fprintf(stderr, "Thread #%d creation failed: %s", thread_num,
                    strerror(err));
            exit(2);
        }
# else
        HANDLE th;
        DWORD thread_id;
        th = CreateThread(NULL, 0, entry, (LPVOID)my_depth, 0, &thread_id);
        if (th == NULL) {
            fprintf(stderr, "Thread #%d creation failed: %d\n", thread_num,
                   (int)GetLastError());
            exit(2);
        }
        CloseHandle(th);
# endif
    }

    AO_fetch_and_add(&thread_ended_cnt, 1);
    return 0;
}

int main(void)
{
    int i;
# ifdef GC_PTHREADS
    int err;
    pthread_t th[INITIAL_THREAD_COUNT];
# else
    HANDLE th[INITIAL_THREAD_COUNT];
# endif

    GC_INIT();
    for (i = 0; i < INITIAL_THREAD_COUNT; ++i) {
#     ifdef GC_PTHREADS
        err = pthread_create(&th[i], NULL, entry, 0);
        if (err) {
            fprintf(stderr, "Thread creation failed: %s", strerror(err));
            exit(1);
        }
#     else
        DWORD thread_id;
        th[i] = CreateThread(NULL, 0, entry, 0, 0, &thread_id);
        if (th[i] == NULL) {
            fprintf(stderr, "Thread creation failed: %d\n",
                    (int)GetLastError());
            exit(1);
        }
#     endif
    }

    for (i = 0; i < INITIAL_THREAD_COUNT; ++i) {
#     ifdef GC_PTHREADS
        void *res;
        err = pthread_join(th[i], &res);
        if (err) {
            fprintf(stderr, "Failed to join thread: %s", strerror(err));
            exit(1);
        }
#     else
        if (WaitForSingleObject(th[i], INFINITE) != WAIT_OBJECT_0) {
            fprintf(stderr, "Failed to join thread: %d\n",
                    (int)GetLastError());
            CloseHandle(th[i]);
            exit(1);
        }
        CloseHandle(th[i]);
#     endif
    }
  printf("subthread_create: created %d threads (%d ended)\n",
         (int)AO_load(&thread_created_cnt), (int)AO_load(&thread_ended_cnt));
  return 0;
}

#else

int main(void)
{
  printf("subthread_create test skipped\n");
  return 0;
}

#endif /* !AO_HAVE_fetch_and_add */
