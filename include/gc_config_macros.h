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

/*
 * This should never be included directly.  It is included only from gc.h.
 * We separate it only to make gc.h more suitable as documentation.
 *
 * Some tests for old macros.  These violate our namespace rules and will
 * disappear shortly.  Use the GC_ names.
 */
#if defined(SOLARIS_THREADS) || defined(_SOLARIS_THREADS) \
    || defined(_SOLARIS_PTHREADS) || defined(GC_SOLARIS_PTHREADS)
  /* We no longer support old style Solaris threads.            */
  /* GC_SOLARIS_THREADS now means pthreads.                     */
# ifndef GC_SOLARIS_THREADS
#   define GC_SOLARIS_THREADS
# endif
#endif
#if defined(IRIX_THREADS)
# define GC_IRIX_THREADS
#endif
#if defined(DGUX_THREADS) && !defined(GC_DGUX386_THREADS)
# define GC_DGUX386_THREADS
#endif
#if defined(AIX_THREADS)
# define GC_AIX_THREADS
#endif
#if defined(HPUX_THREADS)
# define GC_HPUX_THREADS
#endif
#if defined(OSF1_THREADS)
# define GC_OSF1_THREADS
#endif
#if defined(LINUX_THREADS)
# define GC_LINUX_THREADS
#endif
#if defined(WIN32_THREADS)
# define GC_WIN32_THREADS
#endif
#if defined(USE_LD_WRAP)
# define GC_USE_LD_WRAP
#endif

#if defined(GC_WIN32_PTHREADS) && !defined(GC_WIN32_THREADS)
  /* Using pthreads-w32 library. */
# define GC_WIN32_THREADS
#endif

#if defined(GC_AIX_THREADS) || defined(GC_DARWIN_THREADS) \
    || defined(GC_DGUX386_THREADS) || defined(GC_FREEBSD_THREADS) \
    || defined(GC_GNU_THREADS) || defined(GC_HPUX_THREADS) \
    || defined(GC_IRIX_THREADS) || defined(GC_LINUX_THREADS) \
    || defined(GC_NETBSD_THREADS) || defined(GC_OPENBSD_THREADS) \
    || defined(GC_OSF1_THREADS) || defined(GC_SOLARIS_THREADS) \
    || defined(GC_WIN32_THREADS)
# ifndef GC_THREADS
#   define GC_THREADS
# endif
#elif defined(GC_THREADS)
# if defined(__linux__)
#   define GC_LINUX_THREADS
# endif
# if !defined(__linux__) && (defined(_PA_RISC1_1) || defined(_PA_RISC2_0) \
                             || defined(hppa) || defined(__HPPA)) \
     || (defined(__ia64) && defined(_HPUX_SOURCE))
#   define GC_HPUX_THREADS
# endif
# if !defined(__linux__) && (defined(__alpha) || defined(__alpha__))
#   define GC_OSF1_THREADS
# endif
# if defined(__mips) && !defined(__linux__)
#   define GC_IRIX_THREADS
# endif
# if defined(__sparc) && !defined(__linux__) \
     || defined(sun) && (defined(i386) || defined(__i386__) \
                         || defined(__amd64__))
#   define GC_SOLARIS_THREADS
# elif defined(__APPLE__) && defined(__MACH__)
#   define GC_DARWIN_THREADS
# elif defined(__OpenBSD__)
#   define GC_OPENBSD_THREADS
# elif !defined(GC_LINUX_THREADS) && !defined(GC_HPUX_THREADS) \
       && !defined(GC_OSF1_THREADS) && !defined(GC_IRIX_THREADS)
    /* FIXME: Should we really need for FreeBSD and NetBSD to check     */
    /* that no other GC_xxx_THREADS macro is set?                       */
#   if defined(__FreeBSD__) || defined(__DragonFly__)
#     define GC_FREEBSD_THREADS
#   elif defined(__NetBSD__)
#     define GC_NETBSD_THREADS
#   endif
# endif
# if defined(DGUX) && (defined(i386) || defined(__i386__))
#   define GC_DGUX386_THREADS
# endif
# if defined(_AIX)
#   define GC_AIX_THREADS
# endif
# if (defined(_WIN32) || defined(_MSC_VER) || defined(__BORLANDC__) \
      || defined(__CYGWIN32__) || defined(__CYGWIN__) || defined(__CEGCC__) \
      || defined(_WIN32_WCE) || defined(__MINGW32__)) \
     && !defined(GC_WIN32_THREADS)
    /* Either posix or native Win32 threads. */
#   define GC_WIN32_THREADS
# endif
#endif /* GC_THREADS */

#undef GC_PTHREADS
#if (!defined(GC_WIN32_THREADS) || defined(GC_WIN32_PTHREADS) \
     || defined(__CYGWIN32__) || defined(__CYGWIN__)) && defined(GC_THREADS)
  /* Posix threads. */
# define GC_PTHREADS
#endif

#if !defined(_PTHREADS) && defined(GC_NETBSD_THREADS)
# define _PTHREADS
#endif

#if defined(GC_DGUX386_THREADS) && !defined(_POSIX4A_DRAFT10_SOURCE)
# define _POSIX4A_DRAFT10_SOURCE 1
#endif

#if !defined(_REENTRANT) && defined(GC_PTHREADS) && !defined(GC_WIN32_THREADS)
  /* Better late than never.  This fails if system headers that depend  */
  /* on this were previously included.                                  */
# define _REENTRANT
#endif

#define __GC
#if !defined(_WIN32_WCE) || defined(__GNUC__)
# include <stddef.h>
# if defined(__MINGW32__) && !defined(_WIN32_WCE)
#   include <stdint.h>
    /* We mention uintptr_t.                                            */
    /* Perhaps this should be included in pure msft environments        */
    /* as well?                                                         */
# endif
#else /* ! _WIN32_WCE */
  /* Yet more kludges for WinCE */
# include <stdlib.h> /* size_t is defined here */
# ifndef _PTRDIFF_T_DEFINED
    /* ptrdiff_t is not defined */
#   define _PTRDIFF_T_DEFINED
    typedef long ptrdiff_t;
# endif
#endif

#if defined(_DLL) && !defined(GC_NOT_DLL) && !defined(GC_DLL) \
        && !defined(__GNUC__)
# define GC_DLL
#endif

#if defined(GC_DLL) && !defined(GC_API)

# if defined(__MINGW32__) || defined(__CEGCC__)
#   ifdef GC_BUILD
#     define GC_API __declspec(dllexport)
#   else
#     define GC_API __declspec(dllimport)
#   endif

# elif defined(_MSC_VER) || defined(__DMC__) || defined(__BORLANDC__) \
        || defined(__CYGWIN__)
#   ifdef GC_BUILD
#     define GC_API extern __declspec(dllexport)
#   else
#     define GC_API __declspec(dllimport)
#   endif

# elif defined(__WATCOMC__)
#   ifdef GC_BUILD
#     define GC_API extern __declspec(dllexport)
#   else
#     define GC_API extern __declspec(dllimport)
#   endif

# elif defined(__GNUC__)
    /* Only matters if used in conjunction with -fvisibility=hidden option. */
#   if __GNUC__ >= 4 && defined(GC_BUILD)
#     define GC_API extern __attribute__((__visibility__("default")))
#   endif

# endif
#endif /* GC_DLL */

#ifndef GC_API
# define GC_API extern
#endif

#ifndef GC_CALL
# define GC_CALL
#endif

#ifndef GC_CALLBACK
# define GC_CALLBACK GC_CALL
#endif

#ifndef GC_ATTR_MALLOC
  /* 'malloc' attribute should be used for all malloc-like functions    */
  /* (to tell the compiler that a function may be treated as if any     */
  /* non-NULL pointer it returns cannot alias any other pointer valid   */
  /* when the function returns).  If the client code violates this rule */
  /* by using custom GC_oom_func then define GC_OOM_FUNC_RETURNS_ALIAS. */
# if !defined(GC_OOM_FUNC_RETURNS_ALIAS) && defined(__GNUC__) \
        && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#   define GC_ATTR_MALLOC __attribute__((__malloc__))
# else
#   define GC_ATTR_MALLOC
# endif
#endif

#ifndef GC_ATTR_ALLOC_SIZE
  /* 'alloc_size' attribute improves __builtin_object_size correctness. */
  /* Only single-argument form of 'alloc_size' attribute is used.       */
# if defined(__GNUC__) && (__GNUC__ > 4 \
        || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3 && !defined(__ICC)))
#   define GC_ATTR_ALLOC_SIZE(argnum) __attribute__((__alloc_size__(argnum)))
# else
#   define GC_ATTR_ALLOC_SIZE(argnum)
# endif
#endif
