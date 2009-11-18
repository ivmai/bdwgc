/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1999-2001 by Hewlett-Packard Company. All rights reserved.
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

#include "private/gc_pmark.h"

#include <stdio.h>
#include <limits.h>
#include <stdarg.h>

#ifndef MSWINCE
# include <signal.h>
#endif

#ifdef GC_SOLARIS_THREADS
# include <sys/syscall.h>
#endif
#if defined(MSWIN32) || defined(MSWINCE)
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
# endif
# define NOSERVICE
# include <windows.h>
#endif

#if defined(UNIX_LIKE) || defined(CYGWIN32)
# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif

#ifdef NONSTOP
# include <floss.h>
#endif

#if defined(THREADS) && defined(PCR)
# include "il/PCR_IL.h"
  GC_INNER PCR_Th_ML GC_allocate_ml;
#endif
/* For other platforms with threads, the lock and possibly              */
/* GC_lock_holder variables are defined in the thread support code.     */

#ifdef DYNAMIC_LOADING
  /* We need to register the main data segment.  Returns  TRUE unless   */
  /* this is done implicitly as part of dynamic library registration.   */
  GC_INNER GC_bool GC_register_main_static_data(void);
# define GC_REGISTER_MAIN_STATIC_DATA() GC_register_main_static_data()
#else
  /* Don't unnecessarily call GC_register_main_static_data() in case    */
  /* dyn_load.c isn't linked in.                                        */
# define GC_REGISTER_MAIN_STATIC_DATA() TRUE
#endif

#ifdef NEED_CANCEL_DISABLE_COUNT
  __thread unsigned char GC_cancel_disable_count = 0;
#endif

GC_FAR struct _GC_arrays GC_arrays /* = { 0 } */;

GC_INNER GC_bool GC_debugging_started = FALSE;
        /* defined here so we don't have to load debug_malloc.o */

GC_INNER void (*GC_check_heap)(void) = 0;
GC_INNER void (*GC_print_all_smashed)(void) = 0;

ptr_t GC_stackbottom = 0;

#ifdef IA64
  ptr_t GC_register_stackbottom = 0;
#endif

GC_bool GC_dont_gc = 0;

GC_bool GC_dont_precollect = 0;

GC_bool GC_quiet = 0; /* used also in pcr_interface.c */

#ifndef SMALL_CONFIG
  GC_bool GC_print_stats = 0;
#endif

GC_INNER GC_bool GC_print_back_height = 0;

#ifndef NO_DEBUGGING
  GC_INNER GC_bool GC_dump_regularly = 0;
                                /* Generate regular debugging dumps. */
#endif

#ifdef KEEP_BACK_PTRS
  GC_INNER long GC_backtraces = 0;
                /* Number of random backtraces to generate for each GC. */
#endif

#ifdef FIND_LEAK
  int GC_find_leak = 1;
#else
  int GC_find_leak = 0;
#endif

#ifdef ALL_INTERIOR_POINTERS
  int GC_all_interior_pointers = 1;
#else
  int GC_all_interior_pointers = 0;
#endif

#ifdef GC_FORCE_UNMAP_ON_GCOLLECT
  /* Has no effect unless USE_MUNMAP.                           */
  /* Has no effect on implicitly-initiated garbage collections. */
  GC_INNER GC_bool GC_force_unmap_on_gcollect = TRUE;
#else
  GC_INNER GC_bool GC_force_unmap_on_gcollect = FALSE;
#endif

#ifndef GC_LARGE_ALLOC_WARN_INTERVAL
# define GC_LARGE_ALLOC_WARN_INTERVAL 5
#endif
GC_INNER long GC_large_alloc_warn_interval = GC_LARGE_ALLOC_WARN_INTERVAL;
                        /* Interval between unsuppressed warnings.      */

/*ARGSUSED*/
STATIC void * GC_CALLBACK GC_default_oom_fn(size_t bytes_requested)
{
    return(0);
}

/* All accesses to it should be synchronized to avoid data races.       */
GC_oom_func GC_oom_fn = GC_default_oom_fn;

/* Set things up so that GC_size_map[i] >= granules(i),         */
/* but not too much bigger                                              */
/* and so that size_map contains relatively few distinct entries        */
/* This was originally stolen from Russ Atkinson's Cedar                */
/* quantization algorithm (but we precompute it).                       */
STATIC void GC_init_size_map(void)
{
    int i;

    /* Map size 0 to something bigger.                  */
    /* This avoids problems at lower levels.            */
      GC_size_map[0] = 1;
    for (i = 1; i <= GRANULES_TO_BYTES(TINY_FREELISTS-1) - EXTRA_BYTES; i++) {
        GC_size_map[i] = ROUNDED_UP_GRANULES(i);
#       ifndef _MSC_VER
          GC_ASSERT(GC_size_map[i] < TINY_FREELISTS);
          /* Seems to tickle bug in VC++ 2008 for AMD64 */
#       endif
    }
    /* We leave the rest of the array to be filled in on demand. */
}

/* Fill in additional entries in GC_size_map, including the ith one */
/* We assume the ith entry is currently 0.                              */
/* Note that a filled in section of the array ending at n always    */
/* has length at least n/4.                                             */
GC_INNER void GC_extend_size_map(size_t i)
{
    size_t orig_granule_sz = ROUNDED_UP_GRANULES(i);
    size_t granule_sz = orig_granule_sz;
    size_t byte_sz = GRANULES_TO_BYTES(granule_sz);
                        /* The size we try to preserve.         */
                        /* Close to i, unless this would        */
                        /* introduce too many distinct sizes.   */
    size_t smaller_than_i = byte_sz - (byte_sz >> 3);
    size_t much_smaller_than_i = byte_sz - (byte_sz >> 2);
    size_t low_limit;   /* The lowest indexed entry we  */
                        /* initialize.                  */
    size_t j;

    if (GC_size_map[smaller_than_i] == 0) {
        low_limit = much_smaller_than_i;
        while (GC_size_map[low_limit] != 0) low_limit++;
    } else {
        low_limit = smaller_than_i + 1;
        while (GC_size_map[low_limit] != 0) low_limit++;
        granule_sz = ROUNDED_UP_GRANULES(low_limit);
        granule_sz += granule_sz >> 3;
        if (granule_sz < orig_granule_sz) granule_sz = orig_granule_sz;
    }
    /* For these larger sizes, we use an even number of granules.       */
    /* This makes it easier to, for example, construct a 16byte-aligned */
    /* allocator even if GRANULE_BYTES is 8.                            */
        granule_sz += 1;
        granule_sz &= ~1;
    if (granule_sz > MAXOBJGRANULES) {
        granule_sz = MAXOBJGRANULES;
    }
    /* If we can fit the same number of larger objects in a block,      */
    /* do so.                                                   */
    {
        size_t number_of_objs = HBLK_GRANULES/granule_sz;
        granule_sz = HBLK_GRANULES/number_of_objs;
        granule_sz &= ~1;
    }
    byte_sz = GRANULES_TO_BYTES(granule_sz);
    /* We may need one extra byte;                      */
    /* don't always fill in GC_size_map[byte_sz]        */
    byte_sz -= EXTRA_BYTES;

    for (j = low_limit; j <= byte_sz; j++) GC_size_map[j] = granule_sz;
}


/*
 * The following is a gross hack to deal with a problem that can occur
 * on machines that are sloppy about stack frame sizes, notably SPARC.
 * Bogus pointers may be written to the stack and not cleared for
 * a LONG time, because they always fall into holes in stack frames
 * that are not written.  We partially address this by clearing
 * sections of the stack whenever we get control.
 */
# ifdef THREADS
#   define BIG_CLEAR_SIZE 2048  /* Clear this much now and then.        */
#   define SMALL_CLEAR_SIZE 256 /* Clear this much every time.          */
# else
  STATIC word GC_stack_last_cleared = 0; /* GC_no when we last did this */
  STATIC ptr_t GC_min_sp = NULL;
                        /* Coolest stack pointer value from which       */
                        /* we've already cleared the stack.             */
  STATIC ptr_t GC_high_water = NULL;
                        /* "hottest" stack pointer value we have seen   */
                        /* recently.  Degrades over time.               */
  STATIC word GC_bytes_allocd_at_reset = 0;
#   define DEGRADE_RATE 50
# endif

# define CLEAR_SIZE 213  /* Granularity for GC_clear_stack_inner */

#if defined(ASM_CLEAR_CODE)
  void *GC_clear_stack_inner(void *, ptr_t);
#else
  /* Clear the stack up to about limit.  Return arg.  This function is  */
  /* not static because it could also be errorneously defined in .S     */
  /* file, so this error would be caught by the linker.                 */
  /*ARGSUSED*/
  void * GC_clear_stack_inner(void *arg, ptr_t limit)
  {
    word dummy[CLEAR_SIZE];

    BZERO(dummy, CLEAR_SIZE*sizeof(word));
    if ((ptr_t)(dummy) COOLER_THAN limit) {
        (void) GC_clear_stack_inner(arg, limit);
    }
    /* Make sure the recursive call is not a tail call, and the bzero   */
    /* call is not recognized as dead code.                             */
    GC_noop1((word)dummy);
    return(arg);
  }
#endif

/* Clear some of the inaccessible part of the stack.  Returns its       */
/* argument, so it can be used in a tail call position, hence clearing  */
/* another frame.                                                       */
GC_INNER void * GC_clear_stack(void *arg)
{
    ptr_t sp = GC_approx_sp();  /* Hotter than actual sp */
#   ifdef THREADS
        word dummy[SMALL_CLEAR_SIZE];
        static unsigned random_no = 0;
                                 /* Should be more random than it is ... */
                                 /* Used to occasionally clear a bigger  */
                                 /* chunk.                               */
#   endif
    ptr_t limit;

#   define SLOP 400
        /* Extra bytes we clear every time.  This clears our own        */
        /* activation record, and should cause more frequent            */
        /* clearing near the cold end of the stack, a good thing.       */
#   define GC_SLOP 4000
        /* We make GC_high_water this much hotter than we really saw    */
        /* saw it, to cover for GC noise etc. above our current frame.  */
#   define CLEAR_THRESHOLD 100000
        /* We restart the clearing process after this many bytes of     */
        /* allocation.  Otherwise very heavily recursive programs       */
        /* with sparse stacks may result in heaps that grow almost      */
        /* without bounds.  As the heap gets larger, collection         */
        /* frequency decreases, thus clearing frequency would decrease, */
        /* thus more junk remains accessible, thus the heap gets        */
        /* larger ...                                                   */
# ifdef THREADS
    if (++random_no % 13 == 0) {
        limit = sp;
        MAKE_HOTTER(limit, BIG_CLEAR_SIZE*sizeof(word));
        limit = (ptr_t)((word)limit & ~0xf);
                        /* Make it sufficiently aligned for assembly    */
                        /* implementations of GC_clear_stack_inner.     */
        return GC_clear_stack_inner(arg, limit);
    } else {
        BZERO(dummy, SMALL_CLEAR_SIZE*sizeof(word));
        return arg;
    }
# else
    if (GC_gc_no > GC_stack_last_cleared) {
        /* Start things over, so we clear the entire stack again */
        if (GC_stack_last_cleared == 0) GC_high_water = (ptr_t)GC_stackbottom;
        GC_min_sp = GC_high_water;
        GC_stack_last_cleared = GC_gc_no;
        GC_bytes_allocd_at_reset = GC_bytes_allocd;
    }
    /* Adjust GC_high_water */
        MAKE_COOLER(GC_high_water, WORDS_TO_BYTES(DEGRADE_RATE) + GC_SLOP);
        if (sp HOTTER_THAN GC_high_water) {
            GC_high_water = sp;
        }
        MAKE_HOTTER(GC_high_water, GC_SLOP);
    limit = GC_min_sp;
    MAKE_HOTTER(limit, SLOP);
    if (sp COOLER_THAN limit) {
        limit = (ptr_t)((word)limit & ~0xf);
                        /* Make it sufficiently aligned for assembly    */
                        /* implementations of GC_clear_stack_inner.     */
        GC_min_sp = sp;
        return(GC_clear_stack_inner(arg, limit));
    } else if (GC_bytes_allocd - GC_bytes_allocd_at_reset > CLEAR_THRESHOLD) {
        /* Restart clearing process, but limit how much clearing we do. */
        GC_min_sp = sp;
        MAKE_HOTTER(GC_min_sp, CLEAR_THRESHOLD/4);
        if (GC_min_sp HOTTER_THAN GC_high_water) GC_min_sp = GC_high_water;
        GC_bytes_allocd_at_reset = GC_bytes_allocd;
    }
    return(arg);
# endif
}


/* Return a pointer to the base address of p, given a pointer to a      */
/* an address within an object.  Return 0 o.w.                          */
GC_API void * GC_CALL GC_base(void * p)
{
    ptr_t r;
    struct hblk *h;
    bottom_index *bi;
    hdr *candidate_hdr;
    ptr_t limit;

    r = p;
    if (!GC_is_initialized) return 0;
    h = HBLKPTR(r);
    GET_BI(r, bi);
    candidate_hdr = HDR_FROM_BI(bi, r);
    if (candidate_hdr == 0) return(0);
    /* If it's a pointer to the middle of a large object, move it       */
    /* to the beginning.                                                */
        while (IS_FORWARDING_ADDR_OR_NIL(candidate_hdr)) {
           h = FORWARDED_ADDR(h,candidate_hdr);
           r = (ptr_t)h;
           candidate_hdr = HDR(h);
        }
    if (HBLK_IS_FREE(candidate_hdr)) return(0);
    /* Make sure r points to the beginning of the object */
        r = (ptr_t)((word)r & ~(WORDS_TO_BYTES(1) - 1));
        {
            size_t offset = HBLKDISPL(r);
            signed_word sz = candidate_hdr -> hb_sz;
            size_t obj_displ = offset % sz;

            r -= obj_displ;
            limit = r + sz;
            if (limit > (ptr_t)(h + 1) && sz <= HBLKSIZE) {
                return(0);
            }
            if ((ptr_t)p >= limit) return(0);
        }
    return((void *)r);
}


/* Return the size of an object, given a pointer to its base.           */
/* (For small objects this also happens to work from interior pointers, */
/* but that shouldn't be relied upon.)                                  */
GC_API size_t GC_CALL GC_size(const void * p)
{
    hdr * hhdr = HDR(p);

    return hhdr -> hb_sz;
}

GC_API size_t GC_CALL GC_get_heap_size(void)
{
    size_t value;
    DCL_LOCK_STATE;
    LOCK();
    /* ignore the memory space returned to OS (i.e. count only the      */
    /* space owned by the garbage collector)                            */
    value = (size_t)(GC_heapsize - GC_unmapped_bytes);
    UNLOCK();
    return value;
}

GC_API size_t GC_CALL GC_get_free_bytes(void)
{
    size_t value;
    DCL_LOCK_STATE;
    LOCK();
    /* ignore the memory space returned to OS */
    value = (size_t)(GC_large_free_bytes - GC_unmapped_bytes);
    UNLOCK();
    return value;
}

/* The _inner versions assume the caller holds the allocation lock.     */
/* Declared in gc_mark.h (where other public "inner" functions reside). */
GC_API size_t GC_CALL GC_get_heap_size_inner(void)
{
    return (size_t)(GC_heapsize - GC_unmapped_bytes);
}

GC_API size_t GC_CALL GC_get_free_bytes_inner(void)
{
    return (size_t)(GC_large_free_bytes - GC_unmapped_bytes);
}

GC_API size_t GC_CALL GC_get_unmapped_bytes(void)
{
# ifdef USE_MUNMAP
    size_t value;
    DCL_LOCK_STATE;
    LOCK();
    value = (size_t)GC_unmapped_bytes;
    UNLOCK();
    return value;
# else
    return 0;
# endif
}

GC_API size_t GC_CALL GC_get_bytes_since_gc(void)
{
    size_t value;
    DCL_LOCK_STATE;
    LOCK();
    value = GC_bytes_allocd;
    UNLOCK();
    return value;
}

GC_API size_t GC_CALL GC_get_total_bytes(void)
{
    size_t value;
    DCL_LOCK_STATE;
    LOCK();
    value = GC_bytes_allocd+GC_bytes_allocd_before_gc;
    UNLOCK();
    return value;
}

#ifdef THREADS
  GC_API int GC_CALL GC_get_suspend_signal(void)
  {
#   ifdef SIG_SUSPEND
      return SIG_SUSPEND;
#   else
      return -1;
#   endif
  }
#endif /* THREADS */

GC_INNER GC_bool GC_is_initialized = FALSE;

#if (defined(MSWIN32) || defined(MSWINCE)) && defined(THREADS)
    GC_INNER CRITICAL_SECTION GC_write_cs;
#endif

#ifdef MSWIN32
    GC_INNER void GC_init_win32(void);
#endif

GC_INNER void GC_setpagesize(void);

STATIC void GC_exit_check(void)
{
   GC_gcollect();
}

#ifdef SEARCH_FOR_DATA_START
  GC_INNER void GC_init_linux_data_start(void);
#endif

#ifdef UNIX_LIKE

  GC_INNER void GC_set_and_save_fault_handler(void (*handler)(int));

  static void looping_handler(int sig)
  {
    GC_err_printf("Caught signal %d: looping in handler\n", sig);
    for (;;) {}
  }

  static GC_bool installed_looping_handler = FALSE;

  static void maybe_install_looping_handler(void)
  {
    /* Install looping handler before the write fault handler, so we    */
    /* handle write faults correctly.                                   */
    if (!installed_looping_handler && 0 != GETENV("GC_LOOP_ON_ABORT")) {
      GC_set_and_save_fault_handler(looping_handler);
      installed_looping_handler = TRUE;
    }
  }

#else /* !UNIX_LIKE */
# define maybe_install_looping_handler()
#endif

#if defined(DYNAMIC_LOADING) && defined(DARWIN)
  GC_INNER void GC_init_dyld(void);
#endif

#if defined(NETBSD) && defined(__ELF__)
  GC_INNER void GC_init_netbsd_elf(void);
#endif

#if !defined(OS2) && !defined(MACOS) && !defined(MSWIN32) && !defined(MSWINCE)
  STATIC int GC_log = 2;
#endif

GC_API void GC_CALL GC_init(void)
{
    /* LOCK(); -- no longer does anything this early. */
#   if !defined(THREADS) && defined(GC_ASSERTIONS)
        word dummy;
#   endif

#   ifdef GC_INITIAL_HEAP_SIZE
        word initial_heap_sz = divHBLKSZ(GC_INITIAL_HEAP_SIZE);
#   else
        word initial_heap_sz = (word)MINHINCR;
#   endif
    IF_CANCEL(int cancel_state;)

    if (GC_is_initialized) return;

    DISABLE_CANCEL(cancel_state);
    /* Note that although we are nominally called with the */
    /* allocation lock held, the allocation lock is now    */
    /* only really acquired once a second thread is forked.*/
    /* And the initialization code needs to run before     */
    /* then.  Thus we really don't hold any locks, and can */
    /* in fact safely initialize them here.                */
#   ifdef THREADS
      GC_ASSERT(!GC_need_to_lock);
#   endif
#   if defined(GC_WIN32_THREADS) && !defined(GC_PTHREADS)
     {
#     ifndef MSWINCE
        BOOL (WINAPI *pfn) (LPCRITICAL_SECTION, DWORD) = NULL;
        HMODULE hK32 = GetModuleHandle(TEXT("kernel32.dll"));
        if (hK32)
          pfn = (BOOL (WINAPI *) (LPCRITICAL_SECTION, DWORD))
                GetProcAddress (hK32,
                                "InitializeCriticalSectionAndSpinCount");
        if (pfn)
            pfn(&GC_allocate_ml, 4000);
        else
#     endif /* !MSWINCE */
        /* else */ InitializeCriticalSection (&GC_allocate_ml);
     }
#   endif /* GC_WIN32_THREADS */
#   if (defined(MSWIN32) || defined(MSWINCE)) && defined(THREADS)
      InitializeCriticalSection(&GC_write_cs);
#   endif
#   ifndef SMALL_CONFIG
#     ifdef GC_PRINT_VERBOSE_STATS
        /* This is useful for debugging and profiling on platforms with */
        /* missing getenv() (like WinCE).                               */
        GC_print_stats = VERBOSE;
#     else
        if (0 != GETENV("GC_PRINT_VERBOSE_STATS")) {
          GC_print_stats = VERBOSE;
        } else if (0 != GETENV("GC_PRINT_STATS")) {
          GC_print_stats = 1;
        }
#     endif
#     if defined(UNIX_LIKE) || defined(CYGWIN32)
        {
          char * file_name = GETENV("GC_LOG_FILE");
          if (0 != file_name) {
            int log_d = open(file_name, O_CREAT|O_WRONLY|O_APPEND, 0666);
            if (log_d < 0) {
              GC_err_printf("Failed to open %s as log file\n", file_name);
            } else {
              GC_log = log_d;
            }
          }
        }
#     endif
#   endif /* !SMALL_CONFIG */
#   ifndef NO_DEBUGGING
      if (0 != GETENV("GC_DUMP_REGULARLY")) {
        GC_dump_regularly = 1;
      }
#   endif
#   ifdef KEEP_BACK_PTRS
      {
        char * backtraces_string = GETENV("GC_BACKTRACES");
        if (0 != backtraces_string) {
          GC_backtraces = atol(backtraces_string);
          if (backtraces_string[0] == '\0') GC_backtraces = 1;
        }
      }
#   endif
    if (0 != GETENV("GC_FIND_LEAK")) {
      GC_find_leak = 1;
      atexit(GC_exit_check);
    }
    if (0 != GETENV("GC_ALL_INTERIOR_POINTERS")) {
      GC_all_interior_pointers = 1;
    }
    if (0 != GETENV("GC_DONT_GC")) {
      GC_dont_gc = 1;
    }
    if (0 != GETENV("GC_PRINT_BACK_HEIGHT")) {
      GC_print_back_height = 1;
    }
    if (0 != GETENV("GC_NO_BLACKLIST_WARNING")) {
      GC_large_alloc_warn_interval = LONG_MAX;
    }
    {
      char * addr_string = GETENV("GC_TRACE");
      if (0 != addr_string) {
#       ifndef ENABLE_TRACE
          WARN("Tracing not enabled: Ignoring GC_TRACE value\n", 0);
#       else
          word addr = (word)STRTOULL(addr_string, NULL, 16);
          if (addr < 0x1000)
              WARN("Unlikely trace address: %p\n", addr);
          GC_trace_addr = (ptr_t)addr;
#       endif
      }
    }
#   ifndef GC_DISABLE_INCREMENTAL
      {
        char * time_limit_string = GETENV("GC_PAUSE_TIME_TARGET");
        if (0 != time_limit_string) {
          long time_limit = atol(time_limit_string);
          if (time_limit < 5) {
            WARN("GC_PAUSE_TIME_TARGET environment variable value too small "
                 "or bad syntax: Ignoring\n", 0);
          } else {
            GC_time_limit = time_limit;
          }
        }
      }
#   endif
#   ifndef SMALL_CONFIG
      {
        char * full_freq_string = GETENV("GC_FULL_FREQUENCY");
        if (full_freq_string != NULL) {
          int full_freq = atoi(full_freq_string);
          if (full_freq > 0)
            GC_full_freq = full_freq;
        }
      }
#   endif
    {
      char * interval_string = GETENV("GC_LARGE_ALLOC_WARN_INTERVAL");
      if (0 != interval_string) {
        long interval = atol(interval_string);
        if (interval <= 0) {
          WARN("GC_LARGE_ALLOC_WARN_INTERVAL environment variable has "
               "bad value: Ignoring\n", 0);
        } else {
          GC_large_alloc_warn_interval = interval;
        }
      }
    }
    {
        char * space_divisor_string = GETENV("GC_FREE_SPACE_DIVISOR");
        if (space_divisor_string != NULL) {
          int space_divisor = atoi(space_divisor_string);
          if (space_divisor > 0)
            GC_free_space_divisor = (GC_word)space_divisor;
        }
    }
#   ifdef USE_MUNMAP
      {
        char * string = GETENV("GC_UNMAP_THRESHOLD");
        if (string != NULL) {
          if (*string == '0' && *(string + 1) == '\0') {
            /* "0" is used to disable unmapping. */
            GC_unmap_threshold = 0;
          } else {
            int unmap_threshold = atoi(string);
            if (unmap_threshold > 0)
              GC_unmap_threshold = unmap_threshold;
          }
        }
      }
      {
        char * string = GETENV("GC_FORCE_UNMAP_ON_GCOLLECT");
        if (string != NULL) {
          if (*string == '0' && *(string + 1) == '\0') {
            /* "0" is used to turn off the mode. */
            GC_force_unmap_on_gcollect = FALSE;
          } else {
            GC_force_unmap_on_gcollect = TRUE;
          }
        }
      }
#   endif
    maybe_install_looping_handler();
    /* Adjust normal object descriptor for extra allocation.    */
    if (ALIGNMENT > GC_DS_TAGS && EXTRA_BYTES != 0) {
      GC_obj_kinds[NORMAL].ok_descriptor = ((word)(-ALIGNMENT) | GC_DS_LENGTH);
    }
    GC_setpagesize();
    GC_exclude_static_roots_inner(beginGC_arrays, endGC_arrays);
    GC_exclude_static_roots_inner(beginGC_obj_kinds, endGC_obj_kinds);
#   ifdef SEPARATE_GLOBALS
      GC_exclude_static_roots_inner(beginGC_objfreelist, endGC_objfreelist);
      GC_exclude_static_roots_inner(beginGC_aobjfreelist, endGC_aobjfreelist);
#   endif
#   ifdef MSWIN32
        GC_init_win32();
#   endif
#   if defined(USE_PROC_FOR_LIBRARIES) && defined(GC_LINUX_THREADS)
        WARN("USE_PROC_FOR_LIBRARIES + GC_LINUX_THREADS performs poorly.\n", 0);
        /* If thread stacks are cached, they tend to be scanned in      */
        /* entirety as part of the root set.  This wil grow them to     */
        /* maximum size, and is generally not desirable.                */
#   endif
#   if defined(SEARCH_FOR_DATA_START)
        GC_init_linux_data_start();
#   endif
#   if defined(NETBSD) && defined(__ELF__)
        GC_init_netbsd_elf();
#   endif
#   if !defined(THREADS) || defined(GC_PTHREADS) || defined(GC_WIN32_THREADS) \
        || defined(GC_SOLARIS_THREADS)
      if (GC_stackbottom == 0) {
        GC_stackbottom = GC_get_main_stack_base();
#       if (defined(LINUX) || defined(HPUX)) && defined(IA64)
          GC_register_stackbottom = GC_get_register_stack_base();
#       endif
      } else {
#       if (defined(LINUX) || defined(HPUX)) && defined(IA64)
          if (GC_register_stackbottom == 0) {
            WARN("GC_register_stackbottom should be set with GC_stackbottom\n", 0);
            /* The following may fail, since we may rely on             */
            /* alignment properties that may not hold with a user set   */
            /* GC_stackbottom.                                          */
            GC_register_stackbottom = GC_get_register_stack_base();
          }
#       endif
      }
#   endif
    GC_STATIC_ASSERT(sizeof (ptr_t) == sizeof(word));
    GC_STATIC_ASSERT(sizeof (signed_word) == sizeof(word));
    GC_STATIC_ASSERT(sizeof (struct hblk) == HBLKSIZE);
#   ifndef THREADS
#     ifdef STACK_GROWS_DOWN
        GC_ASSERT((word)(&dummy) <= (word)GC_stackbottom);
#     else
        GC_ASSERT((word)(&dummy) >= (word)GC_stackbottom);
#     endif
#   endif
#   if !defined(_AUX_SOURCE) || defined(__GNUC__)
      GC_STATIC_ASSERT((word)(-1) > (word)0);
      /* word should be unsigned */
#   endif
#   if !defined(__BORLANDC__) /* Workaround for Borland C */
        GC_STATIC_ASSERT((ptr_t)(word)(-1) > (ptr_t)0);
        /* Ptr_t comparisons should behave as unsigned comparisons.     */
#   endif
    GC_STATIC_ASSERT((signed_word)(-1) < (signed_word)0);
#   ifndef GC_DISABLE_INCREMENTAL
      if (GC_incremental || 0 != GETENV("GC_ENABLE_INCREMENTAL")) {
        /* For GWW_MPROTECT on Win32, this needs to happen before any   */
        /* heap memory is allocated.                                    */
        GC_dirty_init();
        GC_ASSERT(GC_bytes_allocd == 0)
        GC_incremental = TRUE;
      }
#   endif

    /* Add initial guess of root sets.  Do this first, since sbrk(0)    */
    /* might be used.                                                   */
      if (GC_REGISTER_MAIN_STATIC_DATA()) GC_register_data_segments();
    GC_init_headers();
    GC_bl_init();
    GC_mark_init();
    {
        char * sz_str = GETENV("GC_INITIAL_HEAP_SIZE");
        if (sz_str != NULL) {
          initial_heap_sz = (word)STRTOULL(sz_str, NULL, 10);
          if (initial_heap_sz <= MINHINCR * HBLKSIZE) {
            WARN("Bad initial heap size %s - ignoring it.\n",
                 sz_str);
          }
          initial_heap_sz = divHBLKSZ(initial_heap_sz);
        }
    }
    {
        char * sz_str = GETENV("GC_MAXIMUM_HEAP_SIZE");
        if (sz_str != NULL) {
          word max_heap_sz = (word)STRTOULL(sz_str, NULL, 10);
          if (max_heap_sz < initial_heap_sz * HBLKSIZE) {
            WARN("Bad maximum heap size %s - ignoring it.\n",
                 sz_str);
          }
          if (0 == GC_max_retries) GC_max_retries = 2;
          GC_set_max_heap_size(max_heap_sz);
        }
    }
    if (!GC_expand_hp_inner(initial_heap_sz)) {
        GC_err_printf("Can't start up: not enough memory\n");
        EXIT();
    }
    GC_initialize_offsets();
    GC_register_displacement_inner(0L);
#   if defined(GC_LINUX_THREADS) && defined(REDIRECT_MALLOC)
      if (!GC_all_interior_pointers) {
        /* TLS ABI uses pointer-sized offsets for dtv. */
        GC_register_displacement_inner(sizeof(void *));
      }
#   endif
    GC_init_size_map();
#   ifdef PCR
      if (PCR_IL_Lock(PCR_Bool_false, PCR_allSigsBlocked, PCR_waitForever)
          != PCR_ERes_okay) {
          ABORT("Can't lock load state\n");
      } else if (PCR_IL_Unlock() != PCR_ERes_okay) {
          ABORT("Can't unlock load state\n");
      }
      PCR_IL_Unlock();
      GC_pcr_install();
#   endif
    GC_is_initialized = TRUE;
#   if defined(GC_PTHREADS) || defined(GC_WIN32_THREADS)
        GC_thr_init();
#   endif
    COND_DUMP;
    /* Get black list set up and/or incremental GC started */
      if (!GC_dont_precollect || GC_incremental) GC_gcollect_inner();
#   ifdef STUBBORN_ALLOC
        GC_stubborn_init();
#   endif
    /* Convince lint that some things are used */
#   ifdef LINT
      {
          extern char * const GC_copyright[];
          GC_noop(GC_copyright, GC_find_header, GC_push_one,
                  GC_call_with_alloc_lock, GC_dont_expand,
#                 ifndef NO_DEBUGGING
                    GC_dump,
#                 endif
                  GC_register_finalizer_no_order);
      }
#   endif

    /* The rest of this again assumes we don't really hold      */
    /* the allocation lock.                                     */
#   if defined(PARALLEL_MARK) || defined(THREAD_LOCAL_ALLOC)
        /* Make sure marker threads are started and thread local */
        /* allocation is initialized, in case we didn't get      */
        /* called from GC_init_parallel.                         */
        GC_init_parallel();
#   endif /* PARALLEL_MARK || THREAD_LOCAL_ALLOC */

#   if defined(DYNAMIC_LOADING) && defined(DARWIN)
        /* This must be called WITHOUT the allocation lock held */
        /* and before any threads are created.                  */
        GC_init_dyld();
#   endif
    RESTORE_CANCEL(cancel_state);
}

GC_API void GC_CALL GC_enable_incremental(void)
{
# if !defined(GC_DISABLE_INCREMENTAL) && !defined(KEEP_BACK_PTRS)
    DCL_LOCK_STATE;
    /* If we are keeping back pointers, the GC itself dirties all */
    /* pages on which objects have been marked, making            */
    /* incremental GC pointless.                                  */
    if (!GC_find_leak && 0 == GETENV("GC_DISABLE_INCREMENTAL")) {
      LOCK();
      if (!GC_incremental) {
        GC_setpagesize();
        /* if (GC_no_win32_dlls) goto out; Should be win32S test? */
        maybe_install_looping_handler(); /* Before write fault handler! */
        GC_incremental = TRUE;
        if (!GC_is_initialized) {
          GC_init();
        } else {
          GC_dirty_init();
        }
        if (GC_dirty_maintained && !GC_dont_gc) {
                                /* Can't easily do it if GC_dont_gc.    */
          if (GC_bytes_allocd > 0) {
            /* There may be unmarked reachable objects. */
            GC_gcollect_inner();
          }
            /* else we're OK in assuming everything's   */
            /* clean since nothing can point to an      */
            /* unmarked object.                         */
          GC_read_dirty();
        }
      }
      UNLOCK();
      return;
    }
# endif
  GC_init();
}


#if defined(MSWIN32) || defined(MSWINCE)

# if defined(_MSC_VER) && defined(_DEBUG) && !defined(MSWINCE)
#   include <crtdbg.h>
# endif

  STATIC HANDLE GC_stdout = 0;

  void GC_deinit(void)
  {
#   ifdef THREADS
      if (GC_is_initialized) {
        DeleteCriticalSection(&GC_write_cs);
      }
#   endif
  }

#ifdef THREADS
# ifdef PARALLEL_MARK
#   define IF_NEED_TO_LOCK(x) if (GC_parallel || GC_need_to_lock) x
# else
#   define IF_NEED_TO_LOCK(x) if (GC_need_to_lock) x
# endif
#else
# define IF_NEED_TO_LOCK(x)
#endif

#ifndef _MAX_PATH
# define _MAX_PATH MAX_PATH
#endif

  STATIC HANDLE GC_CreateLogFile(void)
  {
#   if !defined(NO_GETENV_WIN32) || !defined(OLD_WIN32_LOG_FILE)
      TCHAR logPath[_MAX_PATH + 0x10]; /* buffer for path + ext */
#   endif
    /* Use GetEnvironmentVariable instead of GETENV() for unicode support. */
#   ifndef NO_GETENV_WIN32
      if (GetEnvironmentVariable(TEXT("GC_LOG_FILE"), logPath,
                                 _MAX_PATH + 1) - 1U >= (DWORD)_MAX_PATH)
#   endif
    {
      /* Env var not found or its value too long.       */
#     ifdef OLD_WIN32_LOG_FILE
        return CreateFile(TEXT("gc.log"), GENERIC_WRITE, FILE_SHARE_READ,
                          NULL /* lpSecurityAttributes */, CREATE_ALWAYS,
                          FILE_FLAG_WRITE_THROUGH, NULL /* hTemplateFile */);
#     else
        int len = (int)GetModuleFileName(NULL /* hModule */, logPath,
                                         _MAX_PATH + 1);
        /* If GetModuleFileName() has failed then len is 0. */
        if (len > 4 && logPath[len - 4] == (TCHAR)'.') {
          len -= 4; /* strip executable file extension */
        }
        /* strcat/wcscat() are deprecated on WinCE, so use memcpy()     */
        memcpy(&logPath[len], TEXT(".gc.log"), sizeof(TEXT(".gc.log")));
#     endif
    }
#   if !defined(NO_GETENV_WIN32) || !defined(OLD_WIN32_LOG_FILE)
      return CreateFile(logPath, GENERIC_WRITE, FILE_SHARE_READ,
                        NULL /* lpSecurityAttributes */, CREATE_ALWAYS,
                        GC_print_stats == VERBOSE ? FILE_ATTRIBUTE_NORMAL :
                            /* immediately flush writes unless very verbose */
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                        NULL /* hTemplateFile */);
#   endif
  }

  STATIC int GC_write(const char *buf, size_t len)
  {
      BOOL tmp;
      DWORD written;
      if (len == 0)
          return 0;
      IF_NEED_TO_LOCK(EnterCriticalSection(&GC_write_cs));
#     ifdef THREADS
        GC_ASSERT(!GC_write_disabled);
#     endif
      if (GC_stdout == INVALID_HANDLE_VALUE) {
          IF_NEED_TO_LOCK(LeaveCriticalSection(&GC_write_cs));
          return -1;
      } else if (GC_stdout == 0) {
        GC_stdout = GC_CreateLogFile();
        /* Ignore open log failure if the collector is built with       */
        /* print_stats always set on.                                   */
#       ifndef GC_PRINT_VERBOSE_STATS
          if (GC_stdout == INVALID_HANDLE_VALUE)
            ABORT("Open of log file failed");
#       endif
      }
      tmp = WriteFile(GC_stdout, buf, (DWORD)len, &written, NULL);
      if (!tmp)
          DebugBreak();
#     if defined(_MSC_VER) && defined(_DEBUG)
#         ifdef MSWINCE
              /* There is no CrtDbgReport() in WinCE */
              {
                  WCHAR wbuf[1024];
                  /* Always use Unicode variant of OutputDebugString() */
                  wbuf[MultiByteToWideChar(CP_ACP, 0 /* dwFlags */,
                                buf, len, wbuf,
                                sizeof(wbuf) / sizeof(wbuf[0]) - 1)] = 0;
                  OutputDebugStringW(wbuf);
              }
#         else
              _CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "%.*s", len, buf);
#         endif
#     endif
      IF_NEED_TO_LOCK(LeaveCriticalSection(&GC_write_cs));
      return tmp ? (int)written : -1;
  }

#endif /* MSWIN32 */

#if defined(OS2) || defined(MACOS)
  STATIC FILE * GC_stdout = NULL;
  STATIC FILE * GC_stderr = NULL;
  STATIC FILE * GC_log = NULL;

  STATIC void GC_set_files(void)
  {
      if (GC_stdout == NULL) {
        GC_stdout = stdout;
    }
    if (GC_stderr == NULL) {
        GC_stderr = stderr;
    }
    if (GC_log == NULL) {
        GC_log = stderr;
    }
  }
#endif

#if !defined(OS2) && !defined(MACOS) && !defined(MSWIN32) && !defined(MSWINCE)
  STATIC int GC_stdout = 1;
  STATIC int GC_stderr = 2;
# if !defined(AMIGA)
#   include <unistd.h>
# endif
#endif

#if !defined(MSWIN32) && !defined(MSWINCE) && !defined(OS2) \
    && !defined(MACOS)  && !defined(ECOS) && !defined(NOSYS)
STATIC int GC_write(int fd, const char *buf, size_t len)
{
     int bytes_written = 0;
     int result;
     IF_CANCEL(int cancel_state;)

     DISABLE_CANCEL(cancel_state);
     while (bytes_written < len) {
#       ifdef GC_SOLARIS_THREADS
            result = syscall(SYS_write, fd, buf + bytes_written,
                                            len - bytes_written);
#       else
            result = write(fd, buf + bytes_written, len - bytes_written);
#       endif
        if (-1 == result) {
            RESTORE_CANCEL(cancel_state);
            return(result);
        }
        bytes_written += result;
    }
    RESTORE_CANCEL(cancel_state);
    return(bytes_written);
}
#endif /* UN*X */

#ifdef ECOS
  STATIC int GC_write(int fd, const char *buf, size_t len)
  {
    /* FIXME: This seems to be defined nowhere at present. */
    /* _Jv_diag_write(buf, len); */
    return len;
  }
#endif

#ifdef NOSYS
  STATIC int GC_write(int fd, const char *buf, size_t len)
  {
    /* No writing.  */
    return len;
  }
#endif


#if defined(MSWIN32) || defined(MSWINCE)
    /* FIXME: This is pretty ugly ... */
#   define WRITE(f, buf, len) GC_write(buf, len)
#else
#   if defined(OS2) || defined(MACOS)
    static int fwrite_gc_res; /* Should really be local ... */
#   define WRITE(f, buf, len) (GC_set_files(), \
                               fwrite_gc_res = fwrite((buf), 1, (len), (f)), \
                               fflush(f), fwrite_gc_res)
#   else
#     define WRITE(f, buf, len) GC_write((f), (buf), (len))
#   endif
#endif

#define BUFSZ 1024
#ifdef _MSC_VER
# ifdef MSWINCE
    /* _vsnprintf is deprecated in WinCE */
#   define vsnprintf StringCchVPrintfA
# else
#   define vsnprintf _vsnprintf
# endif
#endif
/* A version of printf that is unlikely to call malloc, and is thus safer */
/* to call from the collector in case malloc has been bound to GC_malloc. */
/* Floating point arguments and formats should be avoided, since fp       */
/* conversion is more likely to allocate.                                 */
/* Assumes that no more than BUFSZ-1 characters are written at once.      */
void GC_printf(const char *format, ...)
{
    va_list args;
    char buf[BUFSZ+1];

    va_start(args, format);
    if (GC_quiet) return;
    buf[BUFSZ] = 0x15;
    (void) vsnprintf(buf, BUFSZ, format, args);
    va_end(args);
    if (buf[BUFSZ] != 0x15) ABORT("GC_printf clobbered stack");
    if (WRITE(GC_stdout, buf, strlen(buf)) < 0)
      ABORT("write to stdout failed");
}

void GC_err_printf(const char *format, ...)
{
    va_list args;
    char buf[BUFSZ+1];

    va_start(args, format);
    buf[BUFSZ] = 0x15;
    (void) vsnprintf(buf, BUFSZ, format, args);
    va_end(args);
    if (buf[BUFSZ] != 0x15) ABORT("GC_printf clobbered stack");
    if (WRITE(GC_stderr, buf, strlen(buf)) < 0)
      ABORT("write to stderr failed");
}

void GC_log_printf(const char *format, ...)
{
    va_list args;
    char buf[BUFSZ+1];

    va_start(args, format);
    buf[BUFSZ] = 0x15;
    (void) vsnprintf(buf, BUFSZ, format, args);
    va_end(args);
    if (buf[BUFSZ] != 0x15) ABORT("GC_printf clobbered stack");
    if (WRITE(GC_log, buf, strlen(buf)) < 0)
      ABORT("write to log failed");
}

void GC_err_puts(const char *s)
{
    if (WRITE(GC_stderr, s, strlen(s)) < 0) ABORT("write to stderr failed");
}

#if defined(LINUX) && !defined(SMALL_CONFIG)
  GC_INNER void GC_err_write(const char *buf, size_t len)
  {
    if (WRITE(GC_stderr, buf, len) < 0) ABORT("write to stderr failed");
  }
#endif

STATIC void GC_CALLBACK GC_default_warn_proc(char *msg, GC_word arg)
{
    GC_err_printf(msg, arg);
}

GC_INNER GC_warn_proc GC_current_warn_proc = GC_default_warn_proc;

/* This is recommended for production code (release). */
GC_API void GC_CALLBACK GC_ignore_warn_proc(char *msg, GC_word arg)
{
    if (GC_print_stats) {
      /* Don't ignore warnings if stats printing is on. */
      GC_default_warn_proc(msg, arg);
    }
}

GC_API void GC_CALL GC_set_warn_proc(GC_warn_proc p)
{
    GC_ASSERT(p != 0);
#   ifdef GC_WIN32_THREADS
#     ifdef CYGWIN32
        /* Need explicit GC_INIT call */
        GC_ASSERT(GC_is_initialized);
#     else
        if (!GC_is_initialized) GC_init();
#     endif
#   endif
    LOCK();
    GC_current_warn_proc = p;
    UNLOCK();
}

GC_API GC_warn_proc GC_CALL GC_get_warn_proc(void)
{
    GC_warn_proc result;
    LOCK();
    result = GC_current_warn_proc;
    UNLOCK();
    return(result);
}

#if !defined(PCR) && !defined(SMALL_CONFIG)
  /* Abort the program with a message. msg must not be NULL. */
  void GC_abort(const char *msg)
  {
#   if defined(MSWIN32)
#     ifndef DONT_USE_USER32_DLL
        /* Use static binding to "user32.dll".  */
        (void)MessageBoxA(NULL, msg, "Fatal error in GC", MB_ICONERROR|MB_OK);
#     else
        /* This simplifies linking - resolve "MessageBoxA" at run-time. */
        HINSTANCE hU32 = LoadLibrary(TEXT("user32.dll"));
        if (hU32) {
          FARPROC pfn = GetProcAddress(hU32, "MessageBoxA");
          if (pfn)
            (void)(*(int (WINAPI *)(HWND, LPCSTR, LPCSTR, UINT))pfn)(
                                NULL /* hWnd */, msg, "Fatal error in GC",
                                MB_ICONERROR | MB_OK);
          (void)FreeLibrary(hU32);
        }
#     endif
      /* Also duplicate msg to GC log file.     */
#   endif
      /* Avoid calling GC_err_printf() here, as GC_abort() could be     */
      /* called from it.  Note 1: this is not an atomic output.         */
      /* Note 2: possible write errors are ignored.                     */
      if (WRITE(GC_stderr, (void *)msg, strlen(msg)) >= 0)
        (void)WRITE(GC_stderr, (void *)("\n"), 1);

    if (GETENV("GC_LOOP_ON_ABORT") != NULL) {
            /* In many cases it's easier to debug a running process.    */
            /* It's arguably nicer to sleep, but that makes it harder   */
            /* to look at the thread if the debugger doesn't know much  */
            /* about threads.                                           */
            for(;;) {}
    }
#   if defined(MSWIN32) && defined(NO_DEBUGGING)
      /* A more user-friendly abort after showing fatal message.        */
      if (msg) /* to suppress compiler warnings in ABORT callers. */
        _exit(-1); /* exit on error without running "at-exit" callbacks */
#   elif defined(MSWIN32) || defined(MSWINCE)
        DebugBreak();
#   else
        (void) abort();
#   endif
  }
#endif /* !SMALL_CONFIG */

GC_API void GC_CALL GC_enable(void)
{
    LOCK();
    GC_dont_gc--;
    UNLOCK();
}

GC_API void GC_CALL GC_disable(void)
{
    LOCK();
    GC_dont_gc++;
    UNLOCK();
}

/* Helper procedures for new kind creation.     */
GC_API void ** GC_CALL GC_new_free_list_inner(void)
{
    void *result = GC_INTERNAL_MALLOC((MAXOBJGRANULES+1)*sizeof(ptr_t),
                                      PTRFREE);
    if (result == 0) ABORT("Failed to allocate freelist for new kind");
    BZERO(result, (MAXOBJGRANULES+1)*sizeof(ptr_t));
    return result;
}

GC_API void ** GC_CALL GC_new_free_list(void)
{
    void *result;
    LOCK();
    result = GC_new_free_list_inner();
    UNLOCK();
    return result;
}

GC_API unsigned GC_CALL GC_new_kind_inner(void **fl, GC_word descr,
                                        int adjust, int clear)
{
    unsigned result = GC_n_kinds++;

    if (GC_n_kinds > MAXOBJKINDS) ABORT("Too many kinds");
    GC_obj_kinds[result].ok_freelist = fl;
    GC_obj_kinds[result].ok_reclaim_list = 0;
    GC_obj_kinds[result].ok_descriptor = descr;
    GC_obj_kinds[result].ok_relocate_descr = adjust;
    GC_obj_kinds[result].ok_init = clear;
    return result;
}

GC_API unsigned GC_CALL GC_new_kind(void **fl, GC_word descr, int adjust,
                                    int clear)
{
    unsigned result;
    LOCK();
    result = GC_new_kind_inner(fl, descr, adjust, clear);
    UNLOCK();
    return result;
}

GC_API unsigned GC_CALL GC_new_proc_inner(GC_mark_proc proc)
{
    unsigned result = GC_n_mark_procs++;

    if (GC_n_mark_procs > MAX_MARK_PROCS) ABORT("Too many mark procedures");
    GC_mark_procs[result] = proc;
    return result;
}

GC_API unsigned GC_CALL GC_new_proc(GC_mark_proc proc)
{
    unsigned result;
    LOCK();
    result = GC_new_proc_inner(proc);
    UNLOCK();
    return result;
}

GC_API void * GC_CALL GC_call_with_stack_base(GC_stack_base_func fn, void *arg)
{
    int dummy;
    struct GC_stack_base base;

    base.mem_base = (void *)&dummy;
#   ifdef IA64
      base.reg_base = (void *)GC_save_regs_in_stack();
      /* Unnecessarily flushes register stack,          */
      /* but that probably doesn't hurt.                */
#   endif
    return fn(&base, arg);
}

#ifdef THREADS

  /* Defined in pthread_support.c or win32_threads.c.     */
  GC_INNER void GC_do_blocking_inner(ptr_t data, void * context);

#else

GC_INNER ptr_t GC_blocked_sp = NULL;
        /* NULL value means we are not inside GC_do_blocking() call. */
# ifdef IA64
    STATIC ptr_t GC_blocked_register_sp = NULL;
# endif

GC_INNER struct GC_activation_frame_s *GC_activation_frame = NULL;

/* This is nearly the same as in win32_threads.c        */
GC_API void * GC_CALL GC_call_with_gc_active(GC_fn_type fn,
                                             void * client_data)
{
    struct GC_activation_frame_s frame;
    GC_ASSERT(GC_is_initialized);

    /* Adjust our stack base value (this could happen if        */
    /* GC_get_main_stack_base() is unimplemented or broken for  */
    /* the platform).                                           */
    if (GC_stackbottom HOTTER_THAN (ptr_t)(&frame))
      GC_stackbottom = (ptr_t)(&frame);

    if (GC_blocked_sp == NULL) {
      /* We are not inside GC_do_blocking() - do nothing more.  */
      return fn(client_data);
    }

    /* Setup new "frame".       */
    frame.saved_stack_ptr = GC_blocked_sp;
#   ifdef IA64
      /* This is the same as in GC_call_with_stack_base().      */
      frame.backing_store_end = GC_save_regs_in_stack();
      /* Unnecessarily flushes register stack,          */
      /* but that probably doesn't hurt.                */
      frame.saved_backing_store_ptr = GC_blocked_register_sp;
#   endif
    frame.prev = GC_activation_frame;
    GC_blocked_sp = NULL;
    GC_activation_frame = &frame;

    client_data = fn(client_data);
    GC_ASSERT(GC_blocked_sp == NULL);
    GC_ASSERT(GC_activation_frame == &frame);

    /* Restore original "frame".        */
    GC_activation_frame = frame.prev;
#   ifdef IA64
      GC_blocked_register_sp = frame.saved_backing_store_ptr;
#   endif
    GC_blocked_sp = frame.saved_stack_ptr;

    return client_data; /* result */
}

/* This is nearly the same as in win32_threads.c        */
/*ARGSUSED*/
STATIC void GC_do_blocking_inner(ptr_t data, void * context)
{
    struct blocking_data * d = (struct blocking_data *) data;
    GC_ASSERT(GC_is_initialized);
    GC_ASSERT(GC_blocked_sp == NULL);
#   ifdef SPARC
        GC_blocked_sp = GC_save_regs_in_stack();
#   else
        GC_blocked_sp = (ptr_t) &d; /* save approx. sp */
#   endif
#   ifdef IA64
        GC_blocked_register_sp = GC_save_regs_in_stack();
#   endif

    d -> client_data = (d -> fn)(d -> client_data);

#   ifdef SPARC
        GC_ASSERT(GC_blocked_sp != NULL);
#   else
        GC_ASSERT(GC_blocked_sp == (ptr_t) &d);
#   endif
    GC_blocked_sp = NULL;
}

#endif /* !THREADS */

/* Wrapper for functions that are likely to block (or, at least, do not */
/* allocate garbage collected memory and/or manipulate pointers to the  */
/* garbage collected heap) for an appreciable length of time.           */
/* In the single threaded case, GC_do_blocking() (together              */
/* with GC_call_with_gc_active()) might be used to make stack scanning  */
/* more precise (i.e. scan only stack frames of functions that allocate */
/* garbage collected memory and/or manipulate pointers to the garbage   */
/* collected heap).                                                     */
GC_API void * GC_CALL GC_do_blocking(GC_fn_type fn, void * client_data)
{
    struct blocking_data my_data;

    my_data.fn = fn;
    my_data.client_data = client_data;
    GC_with_callee_saves_pushed(GC_do_blocking_inner, (ptr_t)(&my_data));
    return my_data.client_data; /* result */
}

#if !defined(NO_DEBUGGING)
  GC_API void GC_CALL GC_dump(void)
  {
    GC_printf("***Static roots:\n");
    GC_print_static_roots();
    GC_printf("\n***Heap sections:\n");
    GC_print_heap_sects();
    GC_printf("\n***Free blocks:\n");
    GC_print_hblkfreelist();
    GC_printf("\n***Blocks in use:\n");
    GC_print_block_list();
  }
#endif /* !NO_DEBUGGING */

/* Getter functions for the public Read-only variables.                 */

/* GC_get_gc_no() is unsynchronized and should be typically called      */
/* inside the context of GC_call_with_alloc_lock() to prevent data      */
/* races (on multiprocessors).                                          */
GC_API GC_word GC_CALL GC_get_gc_no(void)
{
    return GC_gc_no;
}

GC_API int GC_CALL GC_get_parallel(void)
{
    /* GC_parallel is initialized at start-up.  */
    return GC_parallel;
}

/* Setter and getter functions for the public R/W function variables.   */
/* These functions are synchronized (like GC_set_warn_proc() and        */
/* GC_get_warn_proc()).                                                 */

GC_API void GC_CALL GC_set_oom_fn(GC_oom_func fn)
{
    GC_ASSERT(fn != 0);
    LOCK();
    GC_oom_fn = fn;
    UNLOCK();
}

GC_API GC_oom_func GC_CALL GC_get_oom_fn(void)
{
    GC_oom_func fn;
    LOCK();
    fn = GC_oom_fn;
    UNLOCK();
    return fn;
}

GC_API void GC_CALL GC_set_finalizer_notifier(GC_finalizer_notifier_proc fn)
{
    /* fn may be 0 (means no finalizer notifier). */
    LOCK();
    GC_finalizer_notifier = fn;
    UNLOCK();
}

GC_API GC_finalizer_notifier_proc GC_CALL GC_get_finalizer_notifier(void)
{
    GC_finalizer_notifier_proc fn;
    LOCK();
    fn = GC_finalizer_notifier;
    UNLOCK();
    return fn;
}

/* Setter and getter functions for the public numeric R/W variables.    */
/* It is safe to call these functions even before GC_INIT().            */
/* These functions are unsynchronized and should be typically called    */
/* inside the context of GC_call_with_alloc_lock() (if called after     */
/* GC_INIT()) to prevent data races (unless it is guaranteed the        */
/* collector is not multi-threaded at that execution point).            */

GC_API void GC_CALL GC_set_find_leak(int value)
{
    /* value is of boolean type. */
    GC_find_leak = value;
}

GC_API int GC_CALL GC_get_find_leak(void)
{
    return GC_find_leak;
}

GC_API void GC_CALL GC_set_all_interior_pointers(int value)
{
    GC_ASSERT(!GC_is_initialized || value == GC_all_interior_pointers);
    GC_ASSERT(value == 0 || value == 1);
    GC_all_interior_pointers = value;
}

GC_API int GC_CALL GC_get_all_interior_pointers(void)
{
    return GC_all_interior_pointers;
}

GC_API void GC_CALL GC_set_finalize_on_demand(int value)
{
    GC_ASSERT(value != -1);
    /* value is of boolean type. */
    GC_finalize_on_demand = value;
}

GC_API int GC_CALL GC_get_finalize_on_demand(void)
{
    return GC_finalize_on_demand;
}

GC_API void GC_CALL GC_set_java_finalization(int value)
{
    GC_ASSERT(value != -1);
    /* value is of boolean type. */
    GC_java_finalization = value;
}

GC_API int GC_CALL GC_get_java_finalization(void)
{
    return GC_java_finalization;
}

GC_API void GC_CALL GC_set_dont_expand(int value)
{
    GC_ASSERT(value != -1);
    /* value is of boolean type. */
    GC_dont_expand = value;
}

GC_API int GC_CALL GC_get_dont_expand(void)
{
    return GC_dont_expand;
}

GC_API void GC_CALL GC_set_no_dls(int value)
{
    GC_ASSERT(value != -1);
    /* value is of boolean type. */
    GC_no_dls = value;
}

GC_API int GC_CALL GC_get_no_dls(void)
{
    return GC_no_dls;
}

GC_API void GC_CALL GC_set_non_gc_bytes(GC_word value)
{
    GC_non_gc_bytes = value;
}

GC_API GC_word GC_CALL GC_get_non_gc_bytes(void)
{
    return GC_non_gc_bytes;
}

GC_API void GC_CALL GC_set_free_space_divisor(GC_word value)
{
    GC_ASSERT(value > 0);
    GC_free_space_divisor = value;
}

GC_API GC_word GC_CALL GC_get_free_space_divisor(void)
{
    return GC_free_space_divisor;
}

GC_API void GC_CALL GC_set_max_retries(GC_word value)
{
    GC_ASSERT(value != ~(GC_word)0);
    GC_max_retries = value;
}

GC_API GC_word GC_CALL GC_get_max_retries(void)
{
    return GC_max_retries;
}

GC_API void GC_CALL GC_set_dont_precollect(int value)
{
    GC_ASSERT(value != -1);
    /* value is of boolean type. */
    GC_dont_precollect = value;
}

GC_API int GC_CALL GC_get_dont_precollect(void)
{
    return GC_dont_precollect;
}

GC_API void GC_CALL GC_set_full_freq(int value)
{
    GC_ASSERT(value >= 0);
    GC_full_freq = value;
}

GC_API int GC_CALL GC_get_full_freq(void)
{
    return GC_full_freq;
}

GC_API void GC_CALL GC_set_time_limit(unsigned long value)
{
    GC_ASSERT(value != (unsigned long)-1L);
    GC_time_limit = value;
}

GC_API unsigned long GC_CALL GC_get_time_limit(void)
{
    return GC_time_limit;
}

GC_API void GC_CALL GC_set_force_unmap_on_gcollect(int value)
{
    GC_force_unmap_on_gcollect = (GC_bool)value;
}

GC_API int GC_CALL GC_get_force_unmap_on_gcollect(void)
{
    return (int)GC_force_unmap_on_gcollect;
}
