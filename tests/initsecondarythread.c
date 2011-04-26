/*
 * Copyright (C) 2011 Ludovic Courtes <ludo@gnu.org>
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

#include "gc.h"

#include <pthread.h>
#include <stdlib.h>

static void *thread(void *arg)
{
  GC_INIT();
  GC_MALLOC(123);
  GC_MALLOC(12345);
  return NULL;
}

#include "private/gcconfig.h"

int main(void)
{
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
  pthread_create (&t, NULL, thread, NULL);
  pthread_join (t, NULL);
  return 0;
}
