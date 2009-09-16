/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2009 by Hewlett-Packard Development Company.
 * All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/* Our pthread support normally needs to intercept a number of thread   */
/* calls.  We arrange to do that here, if appropriate.                  */

#ifndef GC_PTHREAD_REDIRECTS_H

#define GC_PTHREAD_REDIRECTS_H

#if !defined(GC_USE_LD_WRAP) && defined(GC_PTHREADS)
/* We need to intercept calls to many of the threads primitives, so      */
/* that we can locate thread stacks and stop the world.                  */
/* Note also that the collector cannot always see thread specific data.  */
/* Thread specific data should generally consist of pointers to          */
/* uncollectable objects (allocated with GC_malloc_uncollectable,        */
/* not the system malloc), which are deallocated using the destructor    */
/* facility in thr_keycreate.  Alternatively, keep a redundant pointer   */
/* to thread specific data on the thread stack.                          */

# include <pthread.h>
# include <signal.h>

# if !defined(GC_DARWIN_THREADS)
#   include <dlfcn.h>

    GC_API int GC_pthread_sigmask(int /* how */, const sigset_t *,
                                  sigset_t * /* oset */);
    GC_API void *GC_dlopen(const char * /* path */, int /* mode */);

#   undef pthread_sigmask
#   undef dlopen
#   define pthread_sigmask GC_pthread_sigmask
#   define dlopen GC_dlopen
# endif

GC_API int GC_pthread_create(pthread_t *, const pthread_attr_t *,
                        void *(*)(void *), void * /* arg */);
GC_API int GC_pthread_join(pthread_t, void ** /* retval */);
GC_API int GC_pthread_detach(pthread_t);

/* Unless the compiler supports #pragma extern_prefix, the Tru64 UNIX   */
/* <pthread.h> redefines some POSIX thread functions to use mangled     */
/* names.  Anyway, it's safe to undef them before redefining.           */
#undef pthread_create
#undef pthread_join
#undef pthread_detach

#define pthread_create GC_pthread_create
#define pthread_join GC_pthread_join
#define pthread_detach GC_pthread_detach

#endif /* GC_PTHREADS && !GC_USE_LD_WRAP */

#endif /* GC_PTHREAD_REDIRECTS_H */
