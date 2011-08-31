/*
 * Copyright (C) 2011 Ludovic Courtes
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED. ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/* Make sure 'GC_INIT' can be called from threads other than the initial
 * thread.
 */

#ifndef GC_THREADS
# define GC_THREADS
#endif

#define GC_NO_THREAD_REDIRECTS 1
                /* Do not redirect thread creation and join calls.      */

#include "gc.h"

#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>

static void *thread(void *arg)
{
  GC_INIT();
  (void)GC_MALLOC(123);
  (void)GC_MALLOC(12345);
  return arg;
}

#include "private/gcconfig.h"

int main(void)
{
  int code;
  pthread_t t;
# if !(defined(BEOS) || defined(MSWIN32) || defined(MSWINCE) \
       || defined(CYGWIN32) || defined(GC_OPENBSD_THREADS) \
       || (defined(DARWIN) && !defined(NO_PTHREAD_GET_STACKADDR_NP)) \
       || (defined(LINUX) && !defined(NACL)) \
       || (defined(GC_SOLARIS_THREADS) && !defined(_STRICT_STDC)) \
       || (!defined(STACKBOTTOM) && (defined(HEURISTIC1) \
          || (!defined(LINUX_STACKBOTTOM) && !defined(FREEBSD_STACKBOTTOM)))))
    /* GC_INIT() must be called from main thread only. */
    GC_INIT();
# endif
  if ((code = pthread_create (&t, NULL, thread, NULL)) != 0) {
    printf("Thread creation failed %d\n", code);
    return 1;
  }
  if ((code = pthread_join (t, NULL)) != 0) {
    printf("Thread join failed %d\n", code);
    return 1;
  }
  return 0;
}
