/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2010 by Hewlett-Packard Development Company.
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

/* Included from gc.h only.  Included only if GC_PTHREADS.              */
#if defined(GC_H) && defined(GC_PTHREADS)

/* We need to intercept calls to many of the threads primitives, so     */
/* that we can locate thread stacks and stop the world.                 */
/* Note also that the collector cannot always see thread specific data. */
/* Thread specific data should generally consist of pointers to         */
/* uncollectable objects (allocated with GC_malloc_uncollectable,       */
/* not the system malloc), which are deallocated using the destructor   */
/* facility in thr_keycreate.  Alternatively, keep a redundant pointer  */
/* to thread specific data on the thread stack.                         */

#include <pthread.h>

#if !defined(GC_DARWIN_THREADS) && !defined(GC_WIN32_PTHREADS)
# include <signal.h>
# include <dlfcn.h>

# ifndef GC_OPENBSD_THREADS
    GC_API int GC_pthread_sigmask(int /* how */, const sigset_t *,
                                  sigset_t * /* oset */);
# endif
  GC_API void *GC_dlopen(const char * /* path */, int /* mode */);
#endif

GC_API int GC_pthread_create(pthread_t *, const pthread_attr_t *,
                             void *(*)(void *), void * /* arg */);
GC_API int GC_pthread_join(pthread_t, void ** /* retval */);
GC_API int GC_pthread_detach(pthread_t);

#if !defined(GC_PTHREAD_EXIT_ATTRIBUTE) \
        && (defined(GC_LINUX_THREADS) || defined(GC_SOLARIS_THREADS))
  /* Intercept pthread_cancel and pthread_exit on Linux and Solaris.    */
# if defined(__GNUC__) /* since GCC v2.7 */
#   define GC_PTHREAD_EXIT_ATTRIBUTE __attribute__((__noreturn__))
# elif defined(__NORETURN) /* used in Solaris */
#   define GC_PTHREAD_EXIT_ATTRIBUTE __NORETURN
# else
#   define GC_PTHREAD_EXIT_ATTRIBUTE /* empty */
# endif
#endif

#ifdef GC_PTHREAD_EXIT_ATTRIBUTE
  GC_API int GC_pthread_cancel(pthread_t);
  GC_API void GC_pthread_exit(void *) GC_PTHREAD_EXIT_ATTRIBUTE;
#endif

#if !defined(GC_NO_THREAD_REDIRECTS) && !defined(GC_USE_LD_WRAP)
  /* Unless the compiler supports #pragma extern_prefix, the Tru64      */
  /* UNIX <pthread.h> redefines some POSIX thread functions to use      */
  /* mangled names.  Anyway, it's safe to undef them before redefining. */
# undef pthread_create
# undef pthread_join
# undef pthread_detach

# define pthread_create GC_pthread_create
# define pthread_join GC_pthread_join
# define pthread_detach GC_pthread_detach

# if !defined(GC_DARWIN_THREADS) && !defined(GC_WIN32_PTHREADS)
#   ifndef GC_OPENBSD_THREADS
#     undef pthread_sigmask
#     define pthread_sigmask GC_pthread_sigmask
#   endif
#   undef dlopen
#   define dlopen GC_dlopen
# endif

# ifdef GC_PTHREAD_EXIT_ATTRIBUTE
#   undef pthread_cancel
#   define pthread_cancel GC_pthread_cancel
#   undef pthread_exit
#   define pthread_exit GC_pthread_exit
# endif
#endif /* !GC_NO_THREAD_REDIRECTS */

#endif /* GC_PTHREADS */
