/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2009 by Hewlett-Packard Development Company.
 * All rights reserved.
 * Copyright (c) 2009-2022 Ivan Maidanski
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

/* Private declarations for threads support.    */

#ifndef GC_PTHREAD_SUPPORT_H
#define GC_PTHREAD_SUPPORT_H

#include "gc_priv.h"

#ifdef THREADS

#if defined(GC_PTHREADS) || defined(GC_PTHREADS_PARAMARK)
# include <pthread.h>
#endif

#ifdef GC_DARWIN_THREADS
# include <mach/mach.h>
# include <mach/thread_act.h>
#endif

#ifdef THREAD_LOCAL_ALLOC
# include "thread_local_alloc.h"
#endif

#ifdef THREAD_SANITIZER
# include "dbg_mlc.h" /* for oh type */
#endif

EXTERN_C_BEGIN

typedef struct GC_StackContext_Rep {
# if defined(THREAD_SANITIZER) && defined(SIGNAL_BASED_STOP_WORLD)
    char dummy[sizeof(oh)];     /* A dummy field to avoid TSan false    */
                                /* positive about the race between      */
                                /* GC_has_other_debug_info and          */
                                /* GC_suspend_handler_inner (which      */
                                /* sets stack_ptr).                     */
# endif

# if !defined(GC_NO_THREADS_DISCOVERY) && defined(GC_WIN32_THREADS)
    volatile
# endif
  ptr_t stack_end;              /* Cold end of the stack (except for    */
                                /* main thread on non-Windows).         */
                                /* On Windows: 0 means entry invalid;   */
                                /* not in_use implies stack_end is 0.   */

  ptr_t stack_ptr;      /* Valid only in some platform-specific states. */

# ifdef GC_WIN32_THREADS
#   define ADDR_LIMIT ((ptr_t)GC_WORD_MAX)
    ptr_t last_stack_min;       /* Last known minimum (hottest) address */
                                /* in stack or ADDR_LIMIT if unset.     */
#   ifdef I386
      ptr_t initial_stack_base; /* The cold end of the stack saved by   */
                                /* GC_record_stack_base (never modified */
                                /* by GC_set_stackbottom); used for the */
                                /* old way of the coroutines support.   */
#   endif
# elif defined(GC_DARWIN_THREADS) && !defined(DARWIN_DONT_PARSE_STACK)
    ptr_t topOfStack;           /* Result of GC_FindTopOfStack(0);      */
                                /* valid only if the thread is blocked; */
                                /* non-NULL value means already set.    */
# endif

# if defined(E2K) || defined(IA64)
    ptr_t backing_store_end;    /* Note: may reference data in GC heap. */
    ptr_t backing_store_ptr;
# endif

# ifdef GC_WIN32_THREADS
    /* For now, alt-stack is not implemented for Win32. */
# else
    ptr_t altstack;             /* The start of the alt-stack if there  */
                                /* is one, NULL otherwise.              */
    word altstack_size;         /* The size of the alt-stack if exists. */
    ptr_t normstack;            /* The start and size of the "normal"   */
                                /* stack (set by GC_register_altstack). */
    word normstack_size;
# endif

# ifndef GC_NO_FINALIZATION
    unsigned char finalizer_nested;
    char fnlz_pad[1];           /* Explicit alignment (for some rare    */
                                /* compilers such as bcc32 and wcc32).  */
    unsigned short finalizer_skipped;
                                /* Used by GC_check_finalizer_nested()  */
                                /* to minimize the level of recursion   */
                                /* when a client finalizer allocates    */
                                /* memory (initially both are 0).       */
# endif

  struct GC_traced_stack_sect_s *traced_stack_sect;
                                /* Points to the "frame" data held in   */
                                /* stack by the innermost               */
                                /* GC_call_with_gc_active() of this     */
                                /* stack (thread); may be NULL.         */

} *GC_stack_context_t;

#ifdef GC_WIN32_THREADS
  typedef DWORD thread_id_t;
# define thread_id_self() GetCurrentThreadId()
# define THREAD_ID_EQUAL(id1, id2) ((id1) == (id2))
#else
  typedef pthread_t thread_id_t;
# define thread_id_self() pthread_self()
# define THREAD_ID_EQUAL(id1, id2) THREAD_EQUAL(id1, id2)
#endif

typedef struct GC_Thread_Rep {
  union {
#   if !defined(GC_NO_THREADS_DISCOVERY) && defined(GC_WIN32_THREADS)
      volatile AO_t in_use;     /* Updated without lock.  We assert     */
                                /* that each unused entry has invalid   */
                                /* id of zero and zero stack_end.       */
                                /* Used only with GC_win32_dll_threads. */
      LONG long_in_use;         /* The same but of the type that        */
                                /* matches the first argument of        */
                                /* InterlockedExchange(); volatile is   */
                                /* omitted because the ancient version  */
                                /* of the prototype lacked the          */
                                /* qualifier.                           */
#   endif
    struct GC_Thread_Rep *next; /* Hash table link without              */
                                /* GC_win32_dll_threads.                */
                                /* More recently allocated threads      */
                                /* with a given pthread id come         */
                                /* first.  (All but the first are       */
                                /* guaranteed to be dead, but we may    */
                                /* not yet have registered the join.)   */
  } tm; /* table_management */

  GC_stack_context_t crtn;

  thread_id_t id; /* hash table key */
# ifdef GC_DARWIN_THREADS
    mach_port_t mach_thread;
# elif defined(GC_WIN32_THREADS) && defined(GC_PTHREADS)
    pthread_t pthread_id;
# elif defined(USE_TKILL_ON_ANDROID)
    pid_t kernel_id;
# endif

# ifdef MSWINCE
    /* According to MSDN specs for WinCE targets:                       */
    /* - DuplicateHandle() is not applicable to thread handles; and     */
    /* - the value returned by GetCurrentThreadId() could be used as    */
    /* a "real" thread handle (for SuspendThread(), ResumeThread()      */
    /* and GetThreadContext()).                                         */
#   define THREAD_HANDLE(p) ((HANDLE)(word)(p) -> id)
# elif defined(GC_WIN32_THREADS)
    HANDLE handle;
#   define THREAD_HANDLE(p) ((p) -> handle)
# endif /* GC_WIN32_THREADS && !MSWINCE */

  unsigned char flags;          /* Protected by GC lock.                */
# define FINISHED       0x1     /* Thread has exited (pthreads only).   */
# ifndef GC_PTHREADS
#   define KNOWN_FINISHED(p) FALSE
# else
#   define KNOWN_FINISHED(p) (((p) -> flags & FINISHED) != 0)
#   define DETACHED     0x2     /* Thread is treated as detached.       */
                                /* Thread may really be detached, or    */
                                /* it may have been explicitly          */
                                /* registered, in which case we can     */
                                /* deallocate its GC_Thread_Rep once    */
                                /* it unregisters itself, since it      */
                                /* may not return a GC pointer.         */
# endif
# if (defined(GC_HAVE_PTHREAD_EXIT) || !defined(GC_NO_PTHREAD_CANCEL)) \
     && defined(GC_PTHREADS)
#   define DISABLED_GC  0x10    /* Collections are disabled while the   */
                                /* thread is exiting.                   */
# endif
# define DO_BLOCKING    0x20    /* Thread is in the do-blocking state.  */
                                /* If set, thread will acquire GC lock  */
                                /* before any pointer manipulation, and */
                                /* has set its SP value.  Thus, it does */
                                /* not need a signal sent to stop it.   */
# ifdef GC_WIN32_THREADS
#   define IS_SUSPENDED 0x40    /* Thread is suspended by SuspendThread. */
# endif

  char flags_pad[sizeof(word) - 1 /* sizeof(flags) */];
                                /* Explicit alignment (for some rare    */
                                /* compilers such as bcc32 and wcc32).  */

# ifdef SIGNAL_BASED_STOP_WORLD
    volatile AO_t last_stop_count;
                                /* The value of GC_stop_count when the  */
                                /* thread last successfully handled     */
                                /* a suspend signal.                    */
#   ifdef GC_ENABLE_SUSPEND_THREAD
      volatile AO_t ext_suspend_cnt;
                                /* An odd value means thread was        */
                                /* suspended externally; incremented on */
                                /* every call of GC_suspend_thread()    */
                                /* and GC_resume_thread(); updated with */
                                /* the GC lock held, but could be read  */
                                /* from a signal handler.               */
#   endif
# endif

# ifdef GC_PTHREADS
    void *status;               /* The value returned from the thread.  */
                                /* Used only to avoid premature         */
                                /* reclamation of any data it might     */
                                /* reference.                           */
                                /* This is unfortunately also the       */
                                /* reason we need to intercept join     */
                                /* and detach.                          */
# endif

# ifdef THREAD_LOCAL_ALLOC
    struct thread_local_freelists tlfs GC_ATTR_WORD_ALIGNED;
# endif

# ifdef NACL
    /* Grab NACL_GC_REG_STORAGE_SIZE pointers off the stack when        */
    /* going into a syscall.  20 is more than we need, but it's an      */
    /* overestimate in case the instrumented function uses any callee   */
    /* saved registers, they may be pushed to the stack much earlier.   */
    /* Also, on x64 'push' puts 8 bytes on the stack even though        */
    /* our pointers are 4 bytes.                                        */
#   ifdef ARM32
      /* Space for r4-r8, r10-r12, r14.       */
#     define NACL_GC_REG_STORAGE_SIZE 9
#   else
#     define NACL_GC_REG_STORAGE_SIZE 20
#   endif
    ptr_t reg_storage[NACL_GC_REG_STORAGE_SIZE];
# elif defined(PLATFORM_HAVE_GC_REG_STORAGE_SIZE)
    word registers[PLATFORM_GC_REG_STORAGE_SIZE]; /* used externally */
# endif

# if defined(WOW64_THREAD_CONTEXT_WORKAROUND) && defined(MSWINRT_FLAVOR)
    PNT_TIB tib;
# endif

# ifdef RETRY_GET_THREAD_CONTEXT /* && GC_WIN32_THREADS */
    ptr_t context_sp;
    word context_regs[PUSHED_REGS_COUNT];
                                /* Populated as part of GC_suspend() as */
                                /* resume/suspend loop may be needed    */
                                /* for GetThreadContext() to succeed.   */
# endif
} * GC_thread;

#ifndef THREAD_TABLE_SZ
# define THREAD_TABLE_SZ 256    /* Power of 2 (for speed). */
#endif

#ifdef GC_WIN32_THREADS
# define THREAD_TABLE_INDEX(id) /* id is of DWORD type */ \
                (int)((((id) >> 8) ^ (id)) % THREAD_TABLE_SZ)
#elif CPP_WORDSZ == 64
# define THREAD_TABLE_INDEX(id) \
    (int)(((((NUMERIC_THREAD_ID(id) >> 8) ^ NUMERIC_THREAD_ID(id)) >> 16) \
          ^ ((NUMERIC_THREAD_ID(id) >> 8) ^ NUMERIC_THREAD_ID(id))) \
         % THREAD_TABLE_SZ)
#else
# define THREAD_TABLE_INDEX(id) \
                (int)(((NUMERIC_THREAD_ID(id) >> 16) \
                       ^ (NUMERIC_THREAD_ID(id) >> 8) \
                       ^ NUMERIC_THREAD_ID(id)) % THREAD_TABLE_SZ)
#endif

/* The set of all known threads.  We intercept thread creation and      */
/* join/detach.  Protected by the allocation lock.                      */
GC_EXTERN GC_thread GC_threads[THREAD_TABLE_SZ];

#ifndef MAX_MARKERS
# define MAX_MARKERS 16
#endif

#ifdef GC_ASSERTIONS
  GC_EXTERN GC_bool GC_thr_initialized;
#endif

#ifdef GC_WIN32_THREADS

# ifdef GC_NO_THREADS_DISCOVERY
#   define GC_win32_dll_threads FALSE
# elif defined(GC_DISCOVER_TASK_THREADS)
#   define GC_win32_dll_threads TRUE
# else
    GC_EXTERN GC_bool GC_win32_dll_threads;
# endif

# ifdef PARALLEL_MARK
    GC_EXTERN int GC_available_markers_m1;
    GC_EXTERN unsigned GC_required_markers_cnt;
    GC_EXTERN ptr_t GC_marker_sp[MAX_MARKERS - 1];
    GC_EXTERN ptr_t GC_marker_last_stack_min[MAX_MARKERS - 1];
#   ifndef GC_PTHREADS_PARAMARK
      GC_EXTERN thread_id_t GC_marker_Id[MAX_MARKERS - 1];
#   endif
#   if !defined(HAVE_PTHREAD_SETNAME_NP_WITH_TID) && !defined(MSWINCE)
      GC_INNER void GC_init_win32_thread_naming(HMODULE hK32);
#   endif
#   ifdef GC_PTHREADS_PARAMARK
      GC_INNER void *GC_mark_thread(void *);
#   elif defined(MSWINCE)
      GC_INNER DWORD WINAPI GC_mark_thread(LPVOID);
#   else
      GC_INNER unsigned __stdcall GC_mark_thread(void *);
#   endif
# endif /* PARALLEL_MARK */

  GC_INNER GC_thread GC_new_thread(thread_id_t);
  GC_INNER void GC_record_stack_base(GC_stack_context_t crtn,
                                     const struct GC_stack_base *sb);
  GC_INNER GC_thread GC_register_my_thread_inner(
                                        const struct GC_stack_base *sb,
                                        thread_id_t self_id);

# ifdef GC_PTHREADS
    GC_INNER void GC_win32_cache_self_pthread(thread_id_t);
# else
    GC_INNER void GC_delete_thread(GC_thread);
# endif

# ifdef CAN_HANDLE_FORK
    GC_INNER void GC_setup_atfork(void);
# endif

# ifndef GC_NO_THREADS_DISCOVERY
#   ifdef GC_ASSERTIONS
      GC_EXTERN thread_id_t GC_main_thread_id;
#   endif
    GC_INNER GC_thread GC_win32_dll_lookup_thread(thread_id_t);
# endif

# ifdef MPROTECT_VDB
    /* Make sure given thread descriptor is not protected by the VDB    */
    /* implementation.  Used to prevent write faults when the world     */
    /* is (partially) stopped, since it may have been stopped with      */
    /* a system lock held, and that lock may be required for fault      */
    /* handling.                                                        */
    GC_INNER void GC_win32_unprotect_thread(GC_thread);
# else
#   define GC_win32_unprotect_thread(t) (void)(t)
# endif /* !MPROTECT_VDB */

#else
# define GC_win32_dll_threads FALSE
#endif /* !GC_WIN32_THREADS */

#ifdef GC_PTHREADS
# if defined(GC_WIN32_THREADS) && !defined(CYGWIN32) \
     && (defined(GC_WIN32_PTHREADS) || defined(GC_PTHREADS_PARAMARK)) \
     && !defined(__WINPTHREADS_VERSION_MAJOR)
#   define GC_PTHREAD_PTRVAL(pthread_id) pthread_id.p
# else
#   define GC_PTHREAD_PTRVAL(pthread_id) pthread_id
# endif /* !GC_WIN32_THREADS || CYGWIN32 */
# ifdef GC_WIN32_THREADS
    GC_INNER GC_thread GC_lookup_by_pthread(pthread_t);
# else
#   define GC_lookup_by_pthread(t) GC_lookup_thread(t)
# endif
#endif /* GC_PTHREADS */

GC_INNER GC_thread GC_lookup_thread(thread_id_t);
#define GC_self_thread_inner() GC_lookup_thread(thread_id_self())

GC_INNER void GC_wait_for_gc_completion(GC_bool);

#ifdef NACL
  GC_INNER void GC_nacl_initialize_gc_thread(GC_thread);
  GC_INNER void GC_nacl_shutdown_gc_thread(void);
#endif

#ifdef GC_EXPLICIT_SIGNALS_UNBLOCK
  GC_INNER void GC_unblock_gc_signals(void);
#endif

#if defined(GC_ENABLE_SUSPEND_THREAD) && defined(SIGNAL_BASED_STOP_WORLD)
  GC_INNER void GC_suspend_self_inner(GC_thread me, word suspend_cnt);

  GC_INNER void GC_suspend_self_blocked(ptr_t thread_me, void *context);
                                /* Wrapper over GC_suspend_self_inner.  */
#endif

#if defined(GC_PTHREADS) \
    && !defined(SN_TARGET_ORBIS) && !defined(SN_TARGET_PSP2)

# ifdef GC_PTHREAD_START_STANDALONE
#   define GC_INNER_PTHRSTART /* empty */
# else
#   define GC_INNER_PTHRSTART GC_INNER
# endif

  GC_INNER_PTHRSTART void *GC_CALLBACK GC_pthread_start_inner(
                                        struct GC_stack_base *sb, void *arg);
  GC_INNER_PTHRSTART GC_thread GC_start_rtn_prepare_thread(
                                        void *(**pstart)(void *),
                                        void **pstart_arg,
                                        struct GC_stack_base *sb, void *arg);
  GC_INNER_PTHRSTART void GC_thread_exit_proc(void *);
#endif /* GC_PTHREADS */

#ifdef GC_DARWIN_THREADS
# ifndef DARWIN_DONT_PARSE_STACK
    GC_INNER ptr_t GC_FindTopOfStack(unsigned long);
# endif
# if defined(PARALLEL_MARK) && !defined(GC_NO_THREADS_DISCOVERY)
    GC_INNER GC_bool GC_is_mach_marker(thread_act_t);
# endif
#endif /* GC_DARWIN_THREADS */

#ifdef PTHREAD_STOP_WORLD_IMPL
  GC_INNER void GC_stop_init(void);
#endif

EXTERN_C_END

#endif /* THREADS */

#endif /* GC_PTHREAD_SUPPORT_H */
