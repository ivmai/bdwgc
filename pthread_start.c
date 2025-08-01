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
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/*
 * We want to make sure that `GC_thread_exit_proc()` is unconditionally
 * invoked, even if the client is not compiled with `-fexceptions` option,
 * but the collector is.  The workaround is to put `GC_pthread_start_inner()`
 * in its own file (this one), and undefine `__EXCEPTIONS` in the gcc case
 * at the top of the file.
 * FIXME: It is still unclear whether this will actually cause the
 * `exit` handler to be invoked last when `thread_exit` is called (and
 * if `-fexceptions` option is used).
 */

#if !defined(DONT_UNDEF_EXCEPTIONS) && defined(__GNUC__) && defined(__linux__)
/*
 * We undefine `__EXCEPTIONS` to avoid using gcc `__cleanup__` attribute.
 * The current NPTL implementation of `pthread_cleanup_push` uses
 * `__cleanup__` attribute when `__EXCEPTIONS` is defined (`-fexceptions`
 * option is given).  The stack unwinding and cleanup with `__cleanup__`
 * attribute work correctly when everything is compiled with `-fexceptions`
 * option, but it is not the requirement for this library clients to use
 * `-fexceptions` option everywhere.  With `__EXCEPTIONS` undefined, the
 * cleanup routines are registered with `__pthread_register_cancel` thus
 * should work anyway.
 */
#  undef __EXCEPTIONS
#endif

#include "private/pthread_support.h"

#if defined(GC_PTHREADS) && !defined(PLATFORM_THREADS) \
    && !defined(SN_TARGET_PSP2)

/* Invoked from `GC_pthread_start()`. */
GC_INNER_PTHRSTART void *GC_CALLBACK
GC_pthread_start_inner(struct GC_stack_base *sb, void *arg)
{
  void *(*start)(void *);
  void *start_arg;
  void *result;
  volatile GC_thread me
      = GC_start_rtn_prepare_thread(&start, &start_arg, sb, arg);

#  ifndef NACL
  pthread_cleanup_push(GC_thread_exit_proc, (/* no volatile */ void *)me);
#  endif
  result = (*start)(start_arg);
#  if defined(DEBUG_THREADS) && !defined(GC_PTHREAD_START_STANDALONE)
  GC_log_printf("Finishing thread %p\n", PTHREAD_TO_VPTR(pthread_self()));
#  endif
  me->status = result;
  /* Note: we cannot use `GC_dirty()` instead. */
  GC_end_stubborn_change(me);

  /*
   * Cleanup acquires the allocator lock, ensuring that we cannot exit
   * while a collection that thinks we are alive is trying to stop us.
   */
#  ifdef NACL
  GC_thread_exit_proc((/* no volatile */ void *)me);
#  else
  pthread_cleanup_pop(1);
#  endif
  return result;
}

#endif /* GC_PTHREADS */
