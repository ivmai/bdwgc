# Copyright (c) 1999-2001 by Red Hat, Inc. All rights reserved.
# 
# THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
# OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
# 
# Permission is hereby granted to use or copy this program
# for any purpose,  provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is granted,
# provided the above notices are retained, and a notice that the code was
# modified is included with the above copyright notice.
#
# Original author: Tom Tromey
# Modified by: Grzegorz Jakacki <jakacki at acm dot org>

dnl Process this file with autoconf to produce configure.

# Initialization
# ==============

AC_INIT(gc,7.2alpha3,Hans.Boehm@hp.com) 
    ## version must conform to [0-9]+[.][0-9]+(alpha[0-9]+)?
AC_CONFIG_SRCDIR(gcj_mlc.c)
AC_CANONICAL_TARGET 
AC_PREREQ(2.53)
AC_REVISION($Revision: 1.51 $)
GC_SET_VERSION
AM_INIT_AUTOMAKE([foreign dist-bzip2 subdir-objects nostdinc])
AM_CONFIG_HEADER([include/private/config.h])
AM_MAINTAINER_MODE

AC_SUBST(PACKAGE)
AC_SUBST(GC_VERSION)

AM_PROG_CC_C_O
AC_PROG_CXX

AM_PROG_AS
## FIXME: really needed? (AC_LIBTOOL already provides this)
AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(RANLIB, ranlib, :)  # :)

AC_PROG_INSTALL

. ${srcdir}/configure.host

GC_CFLAGS=${gc_cflags}
AC_SUBST(GC_CFLAGS)

AC_ARG_ENABLE(threads,
  [AC_HELP_STRING([--enable-threads=TYPE], [choose threading package])],
  THREADS=$enableval,
  [ AC_MSG_CHECKING([for thread model used by GCC])
    THREADS=`$CC -v 2>&1 | sed -n 's/^Thread model: //p'`
    if test -z "$THREADS"; then
      THREADS=no
    fi
    AC_MSG_RESULT([$THREADS]) ])

AC_ARG_ENABLE(parallel-mark,
   [AC_HELP_STRING([--enable-parallel-mark],
	[parallelize marking and free list construction])],
   [case "$THREADS" in
      no | none | single)
	AC_MSG_ERROR([Parallel mark requires --enable-threads=x spec])
	;;
    esac ]
)

AC_ARG_ENABLE(cplusplus,
    [AC_HELP_STRING([--enable-cplusplus], [install C++ support])])

dnl Features which may be selected in the following thread-detection switch.
AH_TEMPLATE([PARALLEL_MARK], [Define to enable parallel marking.])
AH_TEMPLATE([THREAD_LOCAL_ALLOC],
	    [Define to enable thread-local allocation optimisation.])
AH_TEMPLATE([USE_COMPILER_TLS],
	    [Define to use of compiler-support for thread-local variables.])

dnl Thread selection macros.
AH_TEMPLATE([GC_THREADS],	    [Define to support threads.])
AH_TEMPLATE([GC_AIX_THREADS],       [Define to support IBM AIX threads.])
AH_TEMPLATE([GC_DARWIN_THREADS],    [Define to support Darwin pthreads.])
AH_TEMPLATE([GC_FREEBSD_THREADS],   [Define to support FreeBSD pthreads.])
AH_TEMPLATE([GC_GNU_THREADS],       [Define to support GNU pthreads.])
AH_TEMPLATE([GC_HPUX_THREADS],      [Define to support HP/UX 11 pthreads.])
AH_TEMPLATE([GC_IRIX_THREADS],      [Define to support Irix pthreads.])
AH_TEMPLATE([GC_LINUX_THREADS],     [Define to support pthreads on Linux.])
AH_TEMPLATE([GC_NETBSD_THREADS],    [Define to support NetBSD pthreads.])
AH_TEMPLATE([GC_OSF1_THREADS],      [Define to support Tru64 pthreads.])
AH_TEMPLATE([GC_SOLARIS_THREADS],   [Define to support Solaris pthreads.])
AH_TEMPLATE([GC_WIN32_THREADS],     [Define to support win32 threads.])

dnl System header feature requests.
AH_TEMPLATE([_POSIX_C_SOURCE], [The POSIX feature macro.])
AH_TEMPLATE([_PTHREADS], [Indicates the use of pthreads (NetBSD).])

THREADDLLIBS=
need_atomic_ops_asm=false
## Libraries needed to support dynamic loading and/or threads.
case "$THREADS" in
 no | none | single)
    THREADS=none
    ;;
 posix | pthreads)
    THREADS=posix
    THREADDLLIBS=-lpthread
    case "$host" in
     x86-*-linux* | ia64-*-linux* | i586-*-linux* | i686-*-linux* | x86_64-*-linux* | alpha-*-linux* | sparc*-*-linux*)
	AC_DEFINE(GC_LINUX_THREADS)
	AC_DEFINE(_REENTRANT)
        if test "${enable_parallel_mark}" = yes; then
	  AC_DEFINE(PARALLEL_MARK)
	fi
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	AC_MSG_WARN("Explicit GC_INIT() calls may be required.");
	;;
     *-*-linux*)
	AC_DEFINE(GC_LINUX_THREADS)
	AC_DEFINE(_REENTRANT)
	;;
     *-*-aix*)
	AC_DEFINE(GC_AIX_THREADS)
	AC_DEFINE(_REENTRANT)
	;;
     *-*-hpux11*)
	AC_MSG_WARN("Only HP/UX 11 POSIX threads are supported.")
	AC_DEFINE(GC_HPUX_THREADS)
	AC_DEFINE(_POSIX_C_SOURCE,199506L)
	if test "${enable_parallel_mark}" = yes; then
	  AC_DEFINE(PARALLEL_MARK)
	fi
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	AC_MSG_WARN("Explicit GC_INIT() calls may be required.");
	THREADDLLIBS="-lpthread -lrt"
	# HPUX needs REENTRANT for the _r calls.
	AC_DEFINE(_REENTRANT, 1, [Required define if using POSIX threads])
	;;
     *-*-hpux10*)
	AC_MSG_WARN("Only HP-UX 11 POSIX threads are supported.")
	;;
     *-*-freebsd*)
	AC_MSG_WARN("FreeBSD does not yet fully support threads with Boehm GC.")
	AC_DEFINE(GC_FREEBSD_THREADS)
	INCLUDES="$INCLUDES -pthread"
      	;;
     *-*-kfreebsd*-gnu)
	AC_DEFINE(GC_FREEBSD_THREADS)
	INCLUDES="$INCLUDES -pthread"
	THREADDLLIBS=-pthread
	AC_DEFINE(_REENTRANT)
        if test "${enable_parallel_mark}" = yes; then
	  AC_DEFINE(PARALLEL_MARK)
	fi
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	AC_DEFINE(USE_COMPILER_TLS)
      	;;
     *-*-gnu*)
	AC_DEFINE(GC_GNU_THREADS)
	AC_DEFINE(_REENTRANT)
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	;;
     *-*-netbsd*)
	AC_MSG_WARN("Only on NetBSD 2.0 or later.")
	AC_DEFINE(GC_NETBSD_THREADS)
	AC_DEFINE(_REENTRANT)
	AC_DEFINE(_PTHREADS)
	THREADDLLIBS="-lpthread -lrt"
	;;
     *-*-solaris*)
	AC_DEFINE(GC_SOLARIS_THREADS)
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	THREADDLLIBS="-lpthread -lrt"
	if test "$GCC" != yes; then
          CFLAGS="$CFLAGS -O"
          need_atomic_ops_asm=true
        fi
	;;
     *-*-irix*)
	AC_DEFINE(GC_IRIX_THREADS)
	;;
     *-*-cygwin*)
	AC_DEFINE(GC_THREADS)
        if test "${enable_parallel_mark}" = yes; then
	  AC_DEFINE(PARALLEL_MARK)
	fi
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	# Cygwin doesn't have a real libpthread, so Libtool can't link
	# against it.
	THREADDLLIBS=""
	win32_threads=true
	;;
     *-*-darwin*)
	AC_DEFINE(GC_DARWIN_THREADS)
	AC_DEFINE(THREAD_LOCAL_ALLOC)
	AC_MSG_WARN("Explicit GC_INIT() calls may be required.");
	# Parallel-mark is currently unreliable on Darwin; ignore request
	# if test "${enable_parallel_mark}" = yes; then
	#   AC_DEFINE(PARALLEL_MARK)
	# fi
	darwin_threads=true
	;;
     *-*-osf*)
	AC_DEFINE(GC_OSF1_THREADS)
        if test "${enable_parallel_mark}" = yes; then
	  AC_DEFINE(PARALLEL_MARK)
	  AC_DEFINE(THREAD_LOCAL_ALLOC)
	  AC_MSG_WARN("Explicit GC_INIT() calls may be required.");
	  # May want to enable it in other cases, too.
	  # Measurements havent yet been done.
	fi
	INCLUDES="$INCLUDES -pthread"
	THREADDLLIBS="-lpthread -lrt"
	;;
      *)
	AC_MSG_ERROR("Pthreads not supported by the GC on this platform.")
	;;
    esac
    ;;
 win32)
    AC_DEFINE(GC_THREADS)
    if test "${enable_parallel_mark}" = yes; then
      AC_DEFINE(PARALLEL_MARK)
      AC_DEFINE(THREAD_LOCAL_ALLOC)
    fi
    win32_threads=true
    AC_DEFINE([EMPTY_GETENV_RESULTS], [1],
	      [Wine getenv may not return NULL for missing entry])
    ;;
 dgux386)
    THREADS=dgux386
    AC_MSG_RESULT($THREADDLLIBS)
    # Use pthread GCC  switch
    THREADDLLIBS=-pthread
    if test "${enable_parallel_mark}" = yes; then
        AC_DEFINE(PARALLEL_MARK)
    fi
    AC_DEFINE(THREAD_LOCAL_ALLOC)
    AC_MSG_WARN("Explicit GC_INIT() calls may be required.");
    AC_DEFINE([GC_DGUX386_THREADS], 1,
	      [Define to enable support for DB/UX threads on i386.])
    AC_DEFINE([DGUX_THREADS], 1,
	      [Define to enable support for DB/UX threads.])
    # Enable _POSIX4A_DRAFT10_SOURCE with flag -pthread
    INCLUDES="-pthread $INCLUDES"
    ;;
 aix)
    THREADS=posix
    THREADDLLIBS=-lpthread
    AC_DEFINE(GC_AIX_THREADS)
    AC_DEFINE(_REENTRANT)
    ;;
 decosf1 | irix | mach | os2 | solaris | dce | vxworks)
    AC_MSG_ERROR(thread package $THREADS not yet supported)
    ;;
 *)
    AC_MSG_ERROR($THREADS is an unknown thread package)
    ;;
esac
AC_SUBST(THREADDLLIBS)
AM_CONDITIONAL(THREADS, test x$THREADS != xnone)
AM_CONDITIONAL(PTHREADS, test x$THREADS = xposix)
AM_CONDITIONAL(DARWIN_THREADS, test x$darwin_threads = xtrue)
AM_CONDITIONAL(WIN32_THREADS, test x$win32_threads = xtrue)

case "$host" in 
   powerpc-*-darwin*)
      powerpc_darwin=true
      ;;
esac

# Darwin needs a few extra special tests to deal with variation in the
# system headers.
case "$host" in
  powerpc*-*-darwin*)
    AC_CHECK_MEMBER(ppc_thread_state_t.r0,
      AC_DEFINE(HAS_PPC_THREAD_STATE_R0,1,
	[ppc_thread_state_t has field r0]),,
      [#include <mach/thread_status.h>])
    AC_CHECK_MEMBER(ppc_thread_state_t.__r0,
      AC_DEFINE(HAS_PPC_THREAD_STATE___R0,1,dnl
	[ppc_thread_state_t has field __r0]),,
      [#include <mach/thread_status.h>])
    AC_CHECK_MEMBER(ppc_thread_state64_t.r0,
      AC_DEFINE(HAS_PPC_THREAD_STATE64_R0,1,dnl
	[ppc_thread_state64_t has field r0]),,
      [#include <mach/thread_status.h>])
    AC_CHECK_MEMBER(ppc_thread_state64_t.__r0,
      AC_DEFINE(HAS_PPC_THREAD_STATE64___R0,1,dnl
	[ppc_thread_state64_t has field __r0]),,
      [#include <mach/thread_status.h>])
    ;;
  i?86*-*-darwin*)
    AC_CHECK_MEMBER(x86_thread_state32_t.eax,
      AC_DEFINE(HAS_X86_THREAD_STATE32_EAX,1,dnl
	[x86_thread_state32_t has field eax]),,
      [#include <sys/cdefs.h>
      #include <mach/thread_status.h>])
    AC_CHECK_MEMBER(x86_thread_state32_t.__eax,
      AC_DEFINE(HAS_X86_THREAD_STATE32___EAX,1,dnl
	[x86_thread_state32_t has field __eax]),,
      [#include <sys/cdefs.h>
      #include <mach/thread_status.h>])
    ;;
  x86_64-*-darwin*)
    AC_CHECK_MEMBER(x86_thread_state64_t.rax,
      AC_DEFINE(HAS_X86_THREAD_STATE64_RAX,1,dnl
	[x86_thread_state64_t has field rax]),,
      [#include <sys/cdefs.h>
      #include <mach/thread_status.h>])
    AC_CHECK_MEMBER(x86_thread_state64_t.__rax,
      AC_DEFINE(HAS_X86_THREAD_STATE64___RAX,1,dnl
	[x86_thread_state64_t has field __rax]),,
      [#include <sys/cdefs.h>
      #include <mach/thread_status.h>])
     ;;
  *) ;;
esac

AC_MSG_CHECKING(for xlc)
AC_TRY_COMPILE([],[
 #ifndef __xlC__
 # error
 #endif
], [compiler_xlc=yes], [compiler_xlc=no])
AC_MSG_RESULT($compiler_xlc)
AM_CONDITIONAL(COMPILER_XLC,test $compiler_xlc = yes)
if test $compiler_xlc = yes -a "$powerpc_darwin" = true; then
  # the darwin stack-frame-walking code is completely broken on xlc
  AC_DEFINE([DARWIN_DONT_PARSE_STACK], 1, [See Makefile.direct])
fi

# We never want libdl on darwin. It is a fake libdl that just ends up making
# dyld calls anyway.  The same applies to Cygwin.
case "$host" in
  *-*-darwin*) ;;
  *-*-cygwin*) ;;
  *)
    AC_CHECK_LIB(dl, dlopen, THREADDLLIBS="$THREADDLLIBS -ldl")
    ;;
esac

case "$host" in
  *-*-hpux*)
    avoid_cpp_lib=yes;;
  *)
    avoid_cpp_lib=no;
    ;;
esac
AM_CONDITIONAL(AVOID_CPP_LIB,test $avoid_cpp_lib = yes)

# extra LD Flags which are required for targets
case "${host}" in
  *-*-darwin*)
    extra_ldflags_libgc=-Wl,-single_module
    ;;
esac
AC_SUBST(extra_ldflags_libgc)

AC_SUBST(EXTRA_TEST_LIBS)

target_all=libgc.la
AC_SUBST(target_all)

dnl If the target is an eCos system, use the appropriate eCos
dnl I/O routines.
dnl FIXME: this should not be a local option but a global target
dnl system; at present there is no eCos target.
TARGET_ECOS="no"
AC_ARG_WITH(ecos,
[  --with-ecos             enable runtime eCos target support],
TARGET_ECOS="$with_ecos"
)

addobjs=
addlibs=
CXXINCLUDES=
case "$TARGET_ECOS" in
   no)
      ;;
   *)
      AC_DEFINE([ECOS], 1, [Define to enable eCos target support.])
      CXXINCLUDES="-I${TARGET_ECOS}/include"
      addobjs="$addobjs ecos.lo"
      ;;
esac

AM_CONDITIONAL(CPLUSPLUS, test "${enable_cplusplus}" = yes)

AC_SUBST(CXX)

AC_SUBST(INCLUDES)
AC_SUBST(CXXINCLUDES)

# Configuration of shared libraries
#
AC_MSG_CHECKING(whether to build shared libraries)
AC_ENABLE_SHARED

case "$host" in
 alpha-*-openbsd*)
     enable_shared=no
     ;;
 *)
     ;;
esac

AC_MSG_RESULT($enable_shared)

# Configuration of machine-dependent code
#
AC_MSG_CHECKING(which machine-dependent code should be used) 
machdep=
case "$host" in
 alpha-*-openbsd*)
    machdep="mach_dep.lo"
    if test x"${ac_cv_lib_dl_dlopen}" != xyes ; then
       AC_MSG_WARN(OpenBSD/Alpha without dlopen(). Shared library support is disabled)
    fi
    ;;
 alpha*-*-linux*)
    machdep="mach_dep.lo"
    ;;
 i?86-*-solaris2.[[89]] | i?86-*-solaris2.1?)
    AC_DEFINE([SOLARIS25_PROC_VDB_BUG_FIXED], 1, [See gcconfig.h])
    ;;
 mipstx39-*-elf*)
    machdep="mach_dep.lo"
    ;;
 mips-dec-ultrix*)
    machdep="mach-dep.lo"
    ;;
 mips-nec-sysv*|mips-unknown-sysv*)
    ;;
 mips*-*-linux*) 
    ;; 
 mips-*-*)
    machdep="mach_dep.lo"
    dnl AC_DEFINE(NO_EXECUTE_PERMISSION)
    dnl This is now redundant, but it is also important for incremental GC
    dnl performance under Irix.
    ;;
 sparc*-*-linux*)
    machdep="mach_dep.lo sparc_mach_dep.lo"
    ;;
 sparc-*-netbsd*)
    machdep="mach_dep.lo sparc_netbsd_mach_dep.lo"
    ;;
 sparc64-*-netbsd*)
    machdep="mach_dep.lo sparc_mach_dep.lo"
    ;;
 sparc-sun-solaris2.3)
    machdep="mach_dep.lo sparc_mach_dep.lo"
    AC_DEFINE(SUNOS53_SHARED_LIB, 1,
	      [Define to work around a Solaris 5.3 bug (see dyn_load.c).])
    ;;
 sparc*-sun-solaris2*)
    machdep="mach_dep.lo sparc_mach_dep.lo"
    ;;
 ia64-*-*)
    machdep="mach_dep.lo ia64_save_regs_in_stack.lo"
    ;;
esac
if test x"$machdep" = x; then
AC_MSG_RESULT($machdep)
   machdep="mach_dep.lo"
fi
addobjs="$addobjs $machdep"
AC_SUBST(addobjs)
AC_SUBST(addlibs)

AC_PROG_LIBTOOL

#
# Check for AViiON Machines running DGUX
# FIXME: Should this be moved down to below the gc-debug processing?
#
ac_is_dgux=no
AC_CHECK_HEADER(sys/dg_sys_info.h,
[ac_is_dgux=yes;])

    ## :GOTCHA: we do not check anything but sys/dg_sys_info.h
if test $ac_is_dgux = yes; then
    if test "$enable_gc_debug" = "yes"; then
      CFLAGS="-g -mstandard -DDGUX -D_DGUX_SOURCE -Di386 -mno-legend -O2"
      CXXFLAGS="-g -mstandard -DDGUX -D_DGUX_SOURCE -Di386 -mno-legend -O2"
    else
      CFLAGS="-DDGUX -D_DGUX_SOURCE -Di386 -mno-legend -O2"
      CXXFLAGS="-DDGUX -D_DGUX_SOURCE -Di386 -mno-legend -O2"
    fi
    AC_SUBST(CFLAGS)
    AC_SUBST(CXXFLAGS)
fi

dnl We use these options to decide which functions to include.
AC_ARG_WITH(target-subdir,
[  --with-target-subdir=SUBDIR
                          configuring with a cross compiler])
AC_ARG_WITH(cross-host,
[  --with-cross-host=HOST  configuring with a cross compiler])

# automake wants to see AC_EXEEXT.  But we don't need it.  And having
# it is actually a problem, because the compiler we're passed can't
# necessarily do a full link.  So we fool automake here.
if false; then
  # autoconf 2.50 runs AC_EXEEXT by default, and the macro expands
  # to nothing, so nothing would remain between `then' and `fi' if it
  # were not for the `:' below.
  :
  AC_EXEEXT
fi

dnl As of 4.13a2, the collector will not properly work on Solaris when
dnl built with gcc and -O.  So we remove -O in the appropriate case.
dnl Not needed anymore on Solaris.
AC_MSG_CHECKING(whether Solaris gcc optimization fix is necessary)
case "$host" in
 *aix*)
    if test "$GCC" = yes; then
       AC_MSG_RESULT(yes)
       new_CFLAGS=
       for i in $CFLAGS; do
	  case "$i" in
	   -O*)
	      ;;
	   *)
	      new_CFLAGS="$new_CFLAGS $i"
	      ;;
	  esac
       done
       CFLAGS="$new_CFLAGS"
    else
       AC_MSG_RESULT(no)
    fi
    ;;
 *) AC_MSG_RESULT(no) ;;
esac

dnl Include defines that have become de facto standard.
dnl ALL_INTERIOR_POINTERS can be overridden in startup code.
AC_DEFINE([NO_EXECUTE_PERMISSION], [1],
	  [Define to allow the collector to disable execute permission of heap objects.])
AC_DEFINE([ALL_INTERIOR_POINTERS], [1],
	  [Define to recognise all pointers to the interior of objects.])


dnl Interface Selection
dnl -------------------
dnl
dnl By default, make the library as general as possible.
dnl enable_gcj_support=no
AC_ARG_ENABLE(gcj-support,
    [AC_HELP_STRING([--disable-gcj-support],
	[Disable support for gcj.])])
AM_CONDITIONAL(ENABLE_GCJ_SUPPORT,
    [test x"$enable_gcj_support" != xno])
if test x"$enable_gcj_support" != xno; then
    AC_DEFINE(GC_GCJ_SUPPORT, 1, [Define to include support for gcj])
fi

dnl Debugging
dnl ---------

AH_TEMPLATE([GC_HAVE_BUILTIN_BACKTRACE],
	    [Define if backtrace information is supported.])
AH_TEMPLATE([MAKE_BACK_GRAPH], [See Makefile.direct.])
AH_TEMPLATE([SAVE_CALL_COUNT],
	    [The number of caller frames saved when allocating with the
	     debugging API.])
UNWINDLIBS=
AC_ARG_ENABLE(gc-debug,
[AC_HELP_STRING([--enable-gc-debug],
    [include full support for pointer backtracing etc.])],
[ if test "$enable_gc_debug" = "yes"; then
    AC_MSG_WARN("Should define GC_DEBUG and use debug alloc. in clients.")
    AC_DEFINE([KEEP_BACK_PTRS], 1,
	      [Define to save back-pointers in debugging headers.])
    keep_back_ptrs=true
    AC_DEFINE([DBG_HDRS_ALL], 1,
	      [Define to force debug headers on all objects.])
    case $host in
      ia64-*-linux* )
	AC_DEFINE(MAKE_BACK_GRAPH)
	AC_DEFINE(SAVE_CALL_COUNT, 8)
        AC_CHECK_LIB(unwind, backtrace, [
	  AC_DEFINE(GC_HAVE_BUILTIN_BACKTRACE)
	  UNWINDLIBS=-lunwind
	  AC_MSG_WARN("Client code may need to link against libunwind.")
	])
      ;;
      x86-*-linux* | i586-*-linux* | i686-*-linux* | x86_64-*-linux* )
	AC_DEFINE(MAKE_BACK_GRAPH)
	AC_MSG_WARN("Client must not use -fomit-frame-pointer.")
	AC_DEFINE(SAVE_CALL_COUNT, 8)
      ;;
      i[3456]86-*-dgux*)
	AC_DEFINE(MAKE_BACK_GRAPH)
      ;;
    esac ]
  fi)
AM_CONDITIONAL([KEEP_BACK_PTRS], [test x"$keep_back_ptrs" = xtrue])

AC_ARG_ENABLE(java-finalization,
    [AC_HELP_STRING([--disable-java-finalization],
	[Disable support for java finalization.])])
if test x"$enable_java_finalization" != xno; then
    AC_DEFINE([JAVA_FINALIZATION], 1, [See Makefile.direct.])
fi

AC_ARG_ENABLE(atomic-uncollectable,
    [AC_HELP_STRING([--disable-atomic-uncollectible],
	[Disable support for atomic uncollectible allocation.])])
if test x"$enable_atomic_uncollectible" != x"no"; then
    AC_DEFINE(ATOMIC_UNCOLLECTABLE, 1,
	[Define to enable atomic uncollectible allocation.])
fi

AC_ARG_ENABLE(redirect-malloc,
    [AC_HELP_STRING([--enable-redirect-malloc],
	[Redirect malloc and friends to GC routines])])

if test "${enable_redirect_malloc}" = yes; then
    if test "${enable_gc_debug}" = yes; then
	AC_DEFINE([REDIRECT_MALLOC], GC_debug_malloc_replacement,
		  [If defined, redirect malloc to this function.])
	AC_DEFINE([REDIRECT_REALLOC], GC_debug_realloc_replacement,
		  [If defined, redirect GC_realloc to this function.])
	AC_DEFINE([REDIRECT_FREE], GC_debug_free,
		  [If defined, redirect free to this function.])
    else
	AC_DEFINE(REDIRECT_MALLOC, GC_malloc)
    fi
    AC_DEFINE([GC_USE_DLOPEN_WRAP], 1, [See Makefile.direct.])
fi

AC_ARG_ENABLE(large-config,
    [AC_HELP_STRING([--enable-large-config],
	[Optimize for large (> 100 MB) heap or root set])])

if test "${enable_large_config}" = yes; then
    AC_DEFINE(LARGE_CONFIG, 1, [Define to optimize for large heaps or root sets])
fi

dnl This is something of a hack.  When cross-compiling we turn off
dnl some functionality.  We also enable the "small" configuration.
dnl These is only correct when targetting an embedded system.  FIXME.
if test -n "${with_cross_host}"; then
   AC_DEFINE([NO_CLOCK], 1, [Define to not use system clock (cross compiling).])
   AC_DEFINE([SMALL_CONFIG], 1,
	     [Define to tune the collector for small heap sizes.])
   AC_DEFINE([NO_DEBUGGING], 1,
	     [Disable debugging, like GC_dump and its callees.])
fi


AC_SUBST(UNWINDLIBS)

AC_ARG_ENABLE(gc-assertions,
    [AC_HELP_STRING([--enable-gc-assertions],
	[collector-internal assertion checking])])
if test "${enable_gc_assertions}" = yes; then
    AC_DEFINE([GC_ASSERTIONS], 1, [Define to enable internal debug assertions.])
fi

AC_ARG_ENABLE(munmap,
    [AC_HELP_STRING([--enable-munmap=N],
	[return page to the os if empty for N collections])],
  MUNMAP_THRESHOLD=$enableval;
   [case "$MMAP" in
      no)
        AC_MSG_ERROR([--enable-munmap requires --enable-mmap])
        ;;
    esac]
   )
if test "${enable_munmap}" != ""; then
    AC_DEFINE([USE_MMAP], 1,
	      [Define to use mmap instead of sbrk to expand the heap.])
    AC_DEFINE([USE_MUNMAP], 1,
	      [Define to return memory to OS with munmap calls.
	       See Makefile.direct.])
    if test "${MUNMAP_THRESHOLD}" = "yes"; then
      MUNMAP_THRESHOLD=6
    fi
    AC_DEFINE_UNQUOTED([MUNMAP_THRESHOLD], [${MUNMAP_THRESHOLD}],
	[Number of GC cycles to wait before unmapping an unused block.])
fi

AM_CONDITIONAL(USE_LIBDIR, test -z "$with_cross_host")


# Atomic Ops
# ----------

# Do we want to use an external libatomic_ops?  By default use it if it's
# found.
AC_ARG_WITH([libatomic-ops],
    [AS_HELP_STRING([--with-libatomic-ops[=yes|no|check]],
		    [Use a pre-installed libatomic_ops? (default: check)])],
    [], [with_libatomic_ops=check])

# Check for an external libatomic_ops if the answer was yes or check.  If not
# found, fail on yes, and convert check to no.
AS_IF([test x"$with_libatomic_ops" != xno],
  [ AC_CHECK_HEADER([atomic_ops.h], [],
      [ AS_IF([test x"$with_libatomic_ops" != xcheck],
	      [AC_MSG_ERROR([A pre-installed libatomic_ops was not found.])])
	with_libatomic_ops=no ]) ])

# Finally, emit the definitions for bundled or pre-installed AO.
AC_MSG_CHECKING([which libatomic_ops to use])
AS_IF([test x"$with_libatomic_ops" != xno],
  [ AC_MSG_RESULT([pre-installed])
    ATOMIC_OPS_LIBS="-latomic_ops"
    AC_SUBST([ATOMIC_OPS_LIBS]) ],
  [ AC_MSG_RESULT([bundled])
    AC_CONFIG_SUBDIRS([libatomic_ops])
  ])
AM_CONDITIONAL([USE_INTERNAL_LIBATOMIC_OPS],
    [test x$with_libatomic_ops = xno -a x"$THREADS" != xnone])
AM_CONDITIONAL([NEED_ATOMIC_OPS_ASM],
    [test x$with_libatomic_ops = xno -a x$need_atomic_ops_asm = xtrue])

dnl Produce the Files
dnl -----------------

AC_CONFIG_FILES([Makefile bdw-gc.pc])

AC_CONFIG_COMMANDS([default],,
  [ srcdir=${srcdir}
    host=${host}
    CONFIG_SHELL=${CONFIG_SHELL-/bin/sh}
    CC="${CC}"
    DEFS="$DEFS" ])

AC_OUTPUT
