/* 
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2008 by Hewlett-Packard Development Company.
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

#include "private/gc_priv.h"

#if defined(GC_WIN32_THREADS)

#include <windows.h>

#ifdef THREAD_LOCAL_ALLOC
# include "private/thread_local_alloc.h"
#endif /* THREAD_LOCAL_ALLOC */

/* Allocation lock declarations.	*/
#if !defined(USE_PTHREAD_LOCKS)
# if defined(GC_DLL)
    __declspec(dllexport) CRITICAL_SECTION GC_allocate_ml;
# else
    CRITICAL_SECTION GC_allocate_ml;
# endif
  DWORD GC_lock_holder = NO_THREAD;
  	/* Thread id for current holder of allocation lock */
#else
  pthread_mutex_t GC_allocate_ml = PTHREAD_MUTEX_INITIALIZER;
  unsigned long GC_lock_holder = NO_THREAD;
#endif

#ifdef GC_PTHREADS
# include <errno.h>

 /* Cygwin-specific forward decls */
# undef pthread_create 
# undef pthread_sigmask 
# undef pthread_join 
# undef pthread_detach
# undef dlopen 

# ifdef DEBUG_THREADS
#   ifdef CYGWIN32
#     define DEBUG_CYGWIN_THREADS 1
#     define DEBUG_WIN32_PTHREADS 0
#   else
#     define DEBUG_WIN32_PTHREADS 1
#     define DEBUG_CYGWIN_THREADS 0
#   endif
# else
#   define DEBUG_CYGWIN_THREADS 0
#   define DEBUG_WIN32_PTHREADS 0
# endif

  STATIC void * GC_pthread_start(void * arg);
  STATIC void GC_thread_exit_proc(void *arg);

# include <pthread.h>

#else

# ifdef DEBUG_THREADS
#   define DEBUG_WIN32_THREADS 1
# else
#   define DEBUG_WIN32_THREADS 0
# endif

# undef CreateThread
# undef ExitThread
# undef _beginthreadex
# undef _endthreadex
# undef _beginthread
# ifdef DEBUG_THREADS
#   define DEBUG_WIN32_THREADS 1
# else
#   define DEBUG_WIN32_THREADS 0
# endif

# ifndef MSWINCE
#   include <process.h>  /* For _beginthreadex, _endthreadex */
# endif

#endif

/* DllMain-based thread registration is currently incompatible	*/
/* with thread-local allocation, pthreads and WinCE.		*/
#if defined(GC_DLL) && !defined(MSWINCE) \
	&& !defined(THREAD_LOCAL_ALLOC) && !defined(GC_PTHREADS)
  static GC_bool GC_win32_dll_threads = FALSE;
  /* This code operates in two distinct modes, depending on	*/
  /* the setting of GC_win32_dll_threads.  If			*/
  /* GC_win32_dll_threads is set, all threads in the process	*/
  /* are implicitly registered with the GC by DllMain. 		*/
  /* No explicit registration is required, and attempts at	*/
  /* explicit registration are ignored.  This mode is		*/
  /* very different from the Posix operation of the collector.	*/
  /* In this mode access to the thread table is lock-free.	*/
  /* Hence there is a static limit on the number of threads.	*/
  
  /* If GC_win32_dll_threads is FALSE, or the collector is	*/
  /* built without GC_DLL defined, things operate in a way	*/
  /* that is very similar to Posix platforms, and new threads	*/
  /* must be registered with the collector, e.g. by using	*/
  /* preprocessor-based interception of the thread primitives.	*/
  /* In this case, we use a real data structure for the thread	*/
  /* table.  Note that there is no equivalent of linker-based	*/
  /* call interception, since we don't have ELF-like 		*/
  /* facilities.  The Windows analog appears to be "API		*/
  /* hooking", which really seems to be a standard way to 	*/
  /* do minor binary rewriting (?).  I'd prefer not to have	*/
  /* the basic collector rely on such facilities, but an	*/
  /* optional package that intercepts thread calls this way	*/
  /* would probably be nice.					*/

  /* GC_win32_dll_threads must be set at initialization time,	*/
  /* i.e. before any collector or thread calls.  We make it a	*/
  /* "dynamic" option only to avoid multiple library versions.	*/
#else
# define GC_win32_dll_threads FALSE
# undef MAX_THREADS
# define MAX_THREADS 1 /* dll_thread_table[] is always empty.	*/
#endif

/* We have two versions of the thread table.  Which one	*/
/* we us depends on whether or not GC_win32_dll_threads */
/* is set.  Note that before initialization, we don't 	*/
/* add any entries to either table, even if DllMain is	*/
/* called.  The main thread will be added on		*/
/* initialization.					*/

/* The type of the first argument to InterlockedExchange.	*/
/* Documented to be LONG volatile *, but at least gcc likes 	*/
/* this better.							*/
typedef LONG * IE_t;

GC_bool GC_thr_initialized = FALSE;

GC_bool GC_need_to_lock = FALSE;

static GC_bool parallel_initialized = FALSE;

void GC_init_parallel(void);

/* GC_use_DllMain() is currently incompatible with pthreads.		    */
/* It might be possible to get GC_DLL and DllMain-based thread registration */
/* to work with Cygwin, but if you try, you are on your own.		    */
#if defined(GC_DLL) && !defined(GC_PTHREADS)
  /* Turn on GC_win32_dll_threads	*/
  GC_API void GC_CALL GC_use_DllMain(void)
  {
#     ifdef THREAD_LOCAL_ALLOC
	  ABORT("Cannot use thread local allocation with DllMain-based "
		"thread registration.");
	  /* Thread-local allocation really wants to lock at thread	*/
	  /* entry and exit.						*/
#     else
	  GC_ASSERT(!parallel_initialized);
	  GC_win32_dll_threads = TRUE;
	  GC_init_parallel();
#     endif
  }
#else
  GC_API void GC_CALL GC_use_DllMain(void)
  {
      ABORT("GC not configured as DLL");
  }
#endif

STATIC DWORD GC_main_thread = 0;

#define ADDR_LIMIT ((ptr_t)(word)-1)

struct GC_Thread_Rep {
  union {
    AO_t tm_in_use; 	/* Updated without lock.		*/
  			/* We assert that unused 		*/
  			/* entries have invalid ids of		*/
  			/* zero and zero stack fields.  	*/
    			/* Used only with GC_win32_dll_threads. */
    struct GC_Thread_Rep * tm_next;
    			/* Hash table link without 		*/
    			/* GC_win32_dll_threads.		*/
    			/* More recently allocated threads	*/
			/* with a given pthread id come 	*/
			/* first.  (All but the first are	*/
			/* guaranteed to be dead, but we may    */
			/* not yet have registered the join.)   */
  } table_management;
# define in_use table_management.tm_in_use
# define next table_management.tm_next
  DWORD id;
  HANDLE handle;
  ptr_t stack_base;	/* The cold end of the stack.   */
			/* 0 ==> entry not valid.	*/
			/* !in_use ==> stack_base == 0	*/
  ptr_t last_stack_min;	/* Last known minimum (hottest) address */
  			/* in stack or ADDR_LIMIT if unset	*/
# ifdef IA64
    ptr_t backing_store_end;
    ptr_t backing_store_ptr;
# endif

  GC_bool suspended;

# ifdef GC_PTHREADS
    void *status; /* hold exit value until join in case it's a pointer */
    pthread_t pthread_id;
    short flags;		/* Protected by GC lock.	*/
#	define FINISHED 1   	/* Thread has exited.	*/
#	define DETACHED 2	/* Thread is intended to be detached.	*/
#   define KNOWN_FINISHED(t) (((t) -> flags) & FINISHED)
# else
#   define KNOWN_FINISHED(t) 0
# endif
# ifdef THREAD_LOCAL_ALLOC
    struct thread_local_freelists tlfs;
# endif
};

typedef struct GC_Thread_Rep * GC_thread;
typedef volatile struct GC_Thread_Rep * GC_vthread;

/*
 * We assumed that volatile ==> memory ordering, at least among
 * volatiles.  This code should consistently use atomic_ops.
 */

volatile GC_bool GC_please_stop = FALSE;

/*
 * We track thread attachments while the world is supposed to be stopped.
 * Unfortunately, we can't stop them from starting, since blocking in
 * DllMain seems to cause the world to deadlock.  Thus we have to recover
 * If we notice this in the middle of marking.
 */

AO_t GC_attached_thread = FALSE;
/* Return TRUE if an thread was attached since we last asked or	*/
/* since GC_attached_thread was explicitly reset.		*/
GC_bool GC_started_thread_while_stopped(void)
{
  AO_t result;

  if (GC_win32_dll_threads) {
    AO_nop_full();	/* Prior heap reads need to complete earlier. */
    result = AO_load(&GC_attached_thread);
    if (result) {
      AO_store(&GC_attached_thread, FALSE);
    }
    return ((GC_bool)result);
  } else {
    return FALSE;
  }
}

/* Thread table used if GC_win32_dll_threads is set.	*/
/* This is a fixed size array.				*/
/* Since we use runtime conditionals, both versions	*/
/* are always defined.					*/
# ifndef MAX_THREADS
#   define MAX_THREADS 512
#  endif
  /* Things may get quite slow for large numbers of threads,	*/
  /* since we look them up with sequential search.		*/

  volatile struct GC_Thread_Rep dll_thread_table[MAX_THREADS];

  volatile LONG GC_max_thread_index = 0;
  			/* Largest index in dll_thread_table	*/
		        /* that was ever used.			*/

/* And now the version used if GC_win32_dll_threads is not set.	*/
/* This is a chained hash table, with much of the code borrowed	*/
/* From the Posix implementation.				*/
#ifndef THREAD_TABLE_SZ
# define THREAD_TABLE_SZ 256	/* Must be power of 2	*/
#endif
  STATIC GC_thread GC_threads[THREAD_TABLE_SZ];
  
  /* It may not be safe to allocate when we register the first thread.	*/
  /* Thus we allocated one statically.					*/
  static struct GC_Thread_Rep first_thread;
  static GC_bool first_thread_used = FALSE;

/* Add a thread to GC_threads.  We assume it wasn't already there.	*/
/* Caller holds allocation lock.					*/
/* Unlike the pthreads version, the id field is set by the caller.	*/
STATIC GC_thread GC_new_thread(DWORD id)
{
    word hv = ((word)id) % THREAD_TABLE_SZ;
    GC_thread result;
    
    GC_ASSERT(I_HOLD_LOCK());
    if (!first_thread_used) {
    	result = &first_thread;
    	first_thread_used = TRUE;
    } else {
        GC_ASSERT(!GC_win32_dll_threads);
        result = (struct GC_Thread_Rep *)
        	 GC_INTERNAL_MALLOC(sizeof(struct GC_Thread_Rep), NORMAL);
	/* result can be NULL */
	if (result == 0) return(0);
    }
    /* result -> id = id; Done by caller.	*/
    result -> next = GC_threads[hv];
    GC_threads[hv] = result;
#   ifdef GC_PTHREADS
      GC_ASSERT(result -> flags == 0 /* && result -> thread_blocked == 0 */);
#   endif
    return(result);
}

extern LONG WINAPI GC_write_fault_handler(struct _EXCEPTION_POINTERS *exc_info);

#if defined(GWW_VDB) && defined(MPROTECT_VDB)
  extern GC_bool GC_gww_dirty_init(void);
  /* Defined in os_dep.c.  Returns TRUE if GetWriteWatch is available. 	*/
  /* may be called repeatedly.						*/
#endif

GC_bool GC_in_thread_creation = FALSE;  /* Protected by allocation lock. */

/*
 * This may be called from DllMain, and hence operates under unusual
 * constraints.  In particular, it must be lock-free if GC_win32_dll_threads
 * is set.  Always called from the thread being added.
 * If GC_win32_dll_threads is not set, we already hold the allocation lock,
 * except possibly during single-threaded start-up code.
 */
static GC_thread GC_register_my_thread_inner(struct GC_stack_base *sb,
					     DWORD thread_id)
{
  GC_vthread me;

  /* The following should be a no-op according to the win32	*/
  /* documentation.  There is empirical evidence that it	*/
  /* isn't.		- HB					*/
# if defined(MPROTECT_VDB)
#   if defined(GWW_VDB)
      if (GC_incremental && !GC_gww_dirty_init())
	SetUnhandledExceptionFilter(GC_write_fault_handler);
#   else
      if (GC_incremental) SetUnhandledExceptionFilter(GC_write_fault_handler);
#   endif
# endif

  if (GC_win32_dll_threads) {
    int i;
    /* It appears to be unsafe to acquire a lock here, since this	*/
    /* code is apparently not preemptible on some systems.		*/
    /* (This is based on complaints, not on Microsoft's official	*/
    /* documentation, which says this should perform "only simple	*/
    /* initialization tasks".)						*/
    /* Hence we make do with nonblocking synchronization.		*/
    /* It has been claimed that DllMain is really only executed with	*/
    /* a particular system lock held, and thus careful use of locking	*/
    /* around code that doesn't call back into the system libraries	*/
    /* might be OK.  But this hasn't been tested across all win32	*/
    /* variants.							*/
                /* cast away volatile qualifier */
    for (i = 0; InterlockedExchange((void*)&dll_thread_table[i].in_use,1) != 0;
	 i++) {
      /* Compare-and-swap would make this cleaner, but that's not 	*/
      /* supported before Windows 98 and NT 4.0.  In Windows 2000,	*/
      /* InterlockedExchange is supposed to be replaced by		*/
      /* InterlockedExchangePointer, but that's not really what I	*/
      /* want here.							*/
      /* FIXME: We should eventually declare Win95 dead and use AO_	*/
      /* primitives here.						*/
      if (i == MAX_THREADS - 1)
        ABORT("too many threads");
    }
    /* Update GC_max_thread_index if necessary.  The following is safe,	*/
    /* and unlike CompareExchange-based solutions seems to work on all	*/
    /* Windows95 and later platforms.					*/
    /* Unfortunately, GC_max_thread_index may be temporarily out of 	*/
    /* bounds, so readers have to compensate.				*/
    while (i > GC_max_thread_index) {
      InterlockedIncrement((IE_t)&GC_max_thread_index);
    }
    if (GC_max_thread_index >= MAX_THREADS) {
      /* We overshot due to simultaneous increments.	*/
      /* Setting it to MAX_THREADS-1 is always safe.	*/
      GC_max_thread_index = MAX_THREADS - 1;
    }
    me = dll_thread_table + i;
  } else /* Not using DllMain */ {
    GC_ASSERT(I_HOLD_LOCK());
    GC_in_thread_creation = TRUE; /* OK to collect from unknown thread. */
    me = GC_new_thread(thread_id);
    GC_in_thread_creation = FALSE;
    if (me == 0)
      ABORT("Failed to allocate memory for thread registering.");
  }
# ifdef GC_PTHREADS
    /* me can be NULL -> segfault */
    me -> pthread_id = pthread_self();
# endif

  if (!DuplicateHandle(GetCurrentProcess(),
                 	GetCurrentThread(),
		        GetCurrentProcess(),
		        (HANDLE*)&(me -> handle),
		        0,
		        0,
		        DUPLICATE_SAME_ACCESS)) {
	GC_err_printf("Last error code: %d\n", (int)GetLastError());
	ABORT("DuplicateHandle failed");
  }
  me -> last_stack_min = ADDR_LIMIT;
  me -> stack_base = sb -> mem_base;
# ifdef IA64
      me -> backing_store_end = sb -> reg_base;
# endif
  /* Up until this point, GC_push_all_stacks considers this thread	*/
  /* invalid.								*/
  /* Up until this point, this entry is viewed as reserved but invalid	*/
  /* by GC_delete_thread.						*/
  me -> id = thread_id;
# if defined(THREAD_LOCAL_ALLOC)
      GC_init_thread_local((GC_tlfs)(&(me->tlfs)));
# endif
  if (me -> stack_base == NULL) 
      ABORT("Bad stack base in GC_register_my_thread_inner");
  if (GC_win32_dll_threads) {
    if (GC_please_stop) {
      AO_store(&GC_attached_thread, TRUE);
      AO_nop_full();  /* Later updates must become visible after this.	*/
    }
    /* We'd like to wait here, but can't, since waiting in DllMain 	*/
    /* provokes deadlocks.						*/
    /* Thus we force marking to be restarted instead.			*/
  } else {
    GC_ASSERT(!GC_please_stop);
  	/* Otherwise both we and the thread stopping code would be	*/
  	/* holding the allocation lock.					*/
  }
  return (GC_thread)(me);
}

/*
 * GC_max_thread_index may temporarily be larger than MAX_THREADS.
 * To avoid subscript errors, we check on access.
 */
#ifdef __GNUC__
__inline__
#endif
STATIC LONG GC_get_max_thread_index(void)
{
  LONG my_max = GC_max_thread_index;

  if (my_max >= MAX_THREADS) return MAX_THREADS-1;
  return my_max;
}

/* Return the GC_thread corresponding to a thread id.  May be called 	*/
/* without a lock, but should be called in contexts in which the	*/
/* requested thread cannot be asynchronously deleted, e.g. from the	*/
/* thread itself.							*/
/* This version assumes that either GC_win32_dll_threads is set, or	*/
/* we hold the allocator lock.						*/
/* Also used (for assertion checking only) from thread_local_alloc.c.	*/
GC_thread GC_lookup_thread_inner(DWORD thread_id) {
  if (GC_win32_dll_threads) {
    int i;
    LONG my_max = GC_get_max_thread_index();
    for (i = 0;
       i <= my_max &&
       (!AO_load_acquire(&(dll_thread_table[i].in_use))
	|| dll_thread_table[i].id != thread_id);
       /* Must still be in_use, since nobody else can store our thread_id. */
       i++) {}
    if (i > my_max) {
      return 0;
    } else {
      return (GC_thread)(dll_thread_table + i);
    }
  } else {
    word hv = ((word)thread_id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    
    GC_ASSERT(I_HOLD_LOCK());
    while (p != 0 && p -> id != thread_id) p = p -> next;
    return(p);
  }
}

/* Make sure thread descriptor t is not protected by the VDB		*/
/* implementation.							*/
/* Used to prevent write faults when the world is (partially) stopped,	*/
/* since it may have been stopped with a system lock held, and that 	*/
/* lock may be required for fault handling.				*/
# if defined(MPROTECT_VDB) && !defined(MSWINCE)
#    define UNPROTECT(t) \
       if (GC_dirty_maintained && !GC_win32_dll_threads && \
           t != &first_thread) { \
         GC_ASSERT(SMALL_OBJ(GC_size(t))); \
         GC_remove_protection(HBLKPTR(t), 1, FALSE); \
       }
# else
#    define UNPROTECT(p)
# endif

/* If a thread has been joined, but we have not yet		*/
/* been notified, then there may be more than one thread 	*/
/* in the table with the same win32 id.				*/
/* This is OK, but we need a way to delete a specific one.	*/
/* Assumes we hold the allocation lock unless			*/
/* GC_win32_dll_threads is set.					*/
/* If GC_win32_dll_threads is set it should be called from the	*/
/* thread being deleted.					*/
STATIC void GC_delete_gc_thread(GC_vthread gc_id)
{
  CloseHandle(gc_id->handle);
  if (GC_win32_dll_threads) {
    /* This is intended to be lock-free.				*/
    /* It is either called synchronously from the thread being deleted,	*/
    /* or by the joining thread.					*/
    /* In this branch asynchronous changes to *gc_id are possible.	*/
    gc_id -> stack_base = 0;
    gc_id -> id = 0;
#   ifdef CYGWIN32
      gc_id -> pthread_id = 0;
#   endif /* CYGWIN32 */
#   ifdef GC_WIN32_PTHREADS
      gc_id -> pthread_id.p = NULL;
#   endif /* GC_WIN32_PTHREADS */
    AO_store_release(&(gc_id->in_use), FALSE);
  } else {
    /* Cast away volatile qualifier, since we have lock. */
    GC_thread gc_nvid = (GC_thread)gc_id;
    DWORD id = gc_nvid -> id;
    word hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    register GC_thread prev = 0;

    GC_ASSERT(I_HOLD_LOCK());
    while (p != gc_nvid) {
        prev = p;
        p = p -> next;
    }
    if (prev == 0) {
        GC_threads[hv] = p -> next;
    } else {
        prev -> next = p -> next;
    }
    GC_INTERNAL_FREE(p);
  }
}

/* Delete a thread from GC_threads.  We assume it is there.	*/
/* (The code intentionally traps if it wasn't.)			*/
/* Assumes we hold the allocation lock unless			*/
/* GC_win32_dll_threads is set.					*/
/* If GC_win32_dll_threads is set it should be called from the	*/
/* thread being deleted.					*/
STATIC void GC_delete_thread(DWORD id)
{
  if (GC_win32_dll_threads) {
    GC_thread t = GC_lookup_thread_inner(id);

    if (0 == t) {
      WARN("Removing nonexistent thread %ld\n", (long)id);
    } else {
      GC_delete_gc_thread(t);
    }
  } else {
    word hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    register GC_thread prev = 0;
    
    GC_ASSERT(I_HOLD_LOCK());
    while (p -> id != id) {
        prev = p;
        p = p -> next;
    }
    CloseHandle(p->handle);
    if (prev == 0) {
        GC_threads[hv] = p -> next;
    } else {
        prev -> next = p -> next;
    }
    GC_INTERNAL_FREE(p);
  }
}

GC_API int GC_CALL GC_register_my_thread(struct GC_stack_base *sb) {
  DWORD t = GetCurrentThreadId();

  /* We lock here, since we want to wait for an ongoing GC.	*/
  LOCK();
  if (0 == GC_lookup_thread_inner(t)) {
    GC_register_my_thread_inner(sb, t);
    UNLOCK();
    return GC_SUCCESS;
  } else {
    UNLOCK();
    return GC_DUPLICATE;
  }
}

GC_API int GC_CALL GC_unregister_my_thread(void)
{
    DWORD t = GetCurrentThreadId();

    if (GC_win32_dll_threads) {
#     if defined(THREAD_LOCAL_ALLOC)
	/* Can't happen: see GC_use_DllMain(). */
	GC_ASSERT(FALSE);
#     endif
      /* FIXME: Should we just ignore this? */
      GC_delete_thread(t);
    } else {
      LOCK();
#     if defined(THREAD_LOCAL_ALLOC)
	{
	  GC_thread me = GC_lookup_thread_inner(t);
	  GC_destroy_thread_local(&(me->tlfs));
	}
#     endif
      GC_delete_thread(t);
      UNLOCK();
    }
    return GC_SUCCESS;
}


#ifdef GC_PTHREADS

/* A quick-and-dirty cache of the mapping between pthread_t	*/
/* and win32 thread id.						*/
#define PTHREAD_MAP_SIZE 512
DWORD GC_pthread_map_cache[PTHREAD_MAP_SIZE];
#define HASH(pthread_id) ((NUMERIC_THREAD_ID(pthread_id) >> 5) % PTHREAD_MAP_SIZE)
	/* It appears pthread_t is really a pointer type ... */
#define SET_PTHREAD_MAP_CACHE(pthread_id, win32_id) \
	(GC_pthread_map_cache[HASH(pthread_id)] = (win32_id))
#define GET_PTHREAD_MAP_CACHE(pthread_id) \
	GC_pthread_map_cache[HASH(pthread_id)]

/* Return a GC_thread corresponding to a given pthread_t.	*/
/* Returns 0 if it's not there.					*/
/* We assume that this is only called for pthread ids that	*/
/* have not yet terminated or are still joinable, and		*/
/* cannot be concurrently terminated.				*/
/* Assumes we do NOT hold the allocation lock.			*/
static GC_thread GC_lookup_pthread(pthread_t id)
{
  if (GC_win32_dll_threads) {
    int i;
    LONG my_max = GC_get_max_thread_index();

    for (i = 0;
         i <= my_max &&
         (!AO_load_acquire(&(dll_thread_table[i].in_use))
	  || THREAD_EQUAL(dll_thread_table[i].pthread_id, id));
       /* Must still be in_use, since nobody else can store our thread_id. */
       i++);
    if (i > my_max) return 0;
    return (GC_thread)(dll_thread_table + i);
  } else {
    /* We first try the cache.  If that fails, we use a very slow	*/
    /* approach.							*/
    int hv_guess = GET_PTHREAD_MAP_CACHE(id) % THREAD_TABLE_SZ;
    int hv;
    GC_thread p;

    LOCK();
    for (p = GC_threads[hv_guess]; 0 != p; p = p -> next) {
      if (THREAD_EQUAL(p -> pthread_id, id))
	goto foundit; 
    }
    for (hv = 0; hv < THREAD_TABLE_SZ; ++hv) {
      for (p = GC_threads[hv]; 0 != p; p = p -> next) {
        if (THREAD_EQUAL(p -> pthread_id, id))
	  goto foundit; 
      }
    }
    p = 0;
   foundit:
    UNLOCK();
    return p;
  }
}

#endif /* GC_PTHREADS */

void GC_push_thread_structures(void)
{
  GC_ASSERT(I_HOLD_LOCK());
  if (GC_win32_dll_threads) {
    /* Unlike the other threads implementations, the thread table here	*/
    /* contains no pointers to the collectable heap.  Thus we have	*/
    /* no private structures we need to preserve.			*/
#   ifdef GC_PTHREADS 
    { int i; /* pthreads may keep a pointer in the thread exit value */
      LONG my_max = GC_get_max_thread_index();

      for (i = 0; i <= my_max; i++)
        if (dll_thread_table[i].in_use)
	  GC_push_all((ptr_t)&(dll_thread_table[i].status),
                      (ptr_t)(&(dll_thread_table[i].status)+1));
    }
#   endif
  } else {
    GC_push_all((ptr_t)(GC_threads), (ptr_t)(GC_threads)+sizeof(GC_threads));
  }
# if defined(THREAD_LOCAL_ALLOC)
    GC_push_all((ptr_t)(&GC_thread_key),
      (ptr_t)(&GC_thread_key)+sizeof(&GC_thread_key));
    /* Just in case we ever use our own TLS implementation.	*/
# endif
}

#if defined(MPROTECT_VDB) && !defined(MSWINCE)
  extern volatile AO_TS_t GC_fault_handler_lock;  /* from os_dep.c */
#endif

/* Suspend the given thread, if it's still active.	*/
STATIC void GC_suspend(GC_thread t)
{
# ifdef MSWINCE
    /* SuspendThread will fail if thread is running kernel code */
      while (SuspendThread(t -> handle) == (DWORD)-1)
	Sleep(10);
# else
    /* Apparently the Windows 95 GetOpenFileName call creates	*/
    /* a thread that does not properly get cleaned up, and		*/
    /* SuspendThread on its descriptor may provoke a crash.		*/
    /* This reduces the probability of that event, though it still	*/
    /* appears there's a race here.					*/
    DWORD exitCode; 

    UNPROTECT(t);
    if (GetExitCodeThread(t -> handle, &exitCode) &&
        exitCode != STILL_ACTIVE) {
#     ifdef GC_PTHREADS
	t -> stack_base = 0; /* prevent stack from being pushed */
#     else
        /* this breaks pthread_join on Cygwin, which is guaranteed to  */
        /* only see user pthreads 	 			       */
	GC_ASSERT(GC_win32_dll_threads);
	GC_delete_gc_thread(t);
#     endif
      return;
    }
#   if defined(MPROTECT_VDB) && !defined(MSWINCE)
      /* Acquire the spin lock we use to update dirty bits.	*/
      /* Threads shouldn't get stopped holding it.  But we may	*/
      /* acquire and release it in the UNPROTECT call.		*/
      while (AO_test_and_set_acquire(&GC_fault_handler_lock) == AO_TS_SET) {}
#   endif

    if (SuspendThread(t -> handle) == (DWORD)-1)
      ABORT("SuspendThread failed");
# endif
   t -> suspended = TRUE;
#  if defined(MPROTECT_VDB) && !defined(MSWINCE)
     AO_CLEAR(&GC_fault_handler_lock);
#  endif
}

/* Defined in misc.c */
#ifndef CYGWIN32
  extern CRITICAL_SECTION GC_write_cs;
#endif

void GC_stop_world(void)
{
  DWORD thread_id = GetCurrentThreadId();
  int i;
  int my_max;

  if (!GC_thr_initialized) ABORT("GC_stop_world() called before GC_thr_init()");
  GC_ASSERT(I_HOLD_LOCK());

  /* This code is the same as in pthread_stop_world.c */
# ifdef PARALLEL_MARK
    if (GC_parallel) {
      GC_acquire_mark_lock();
      GC_ASSERT(GC_fl_builder_count == 0);
      /* We should have previously waited for it to become zero. */
    }
# endif /* PARALLEL_MARK */

  GC_please_stop = TRUE;
# ifndef CYGWIN32
    EnterCriticalSection(&GC_write_cs);
# endif
  if (GC_win32_dll_threads) {
    /* Any threads being created during this loop will end up setting   */
    /* GC_attached_thread when they start.  This will force marking to  */
    /* restart.								*/
    /* This is not ideal, but hopefully correct.			*/
    GC_attached_thread = FALSE;
    my_max = (int)GC_get_max_thread_index();
    for (i = 0; i <= my_max; i++) {
      GC_vthread t = dll_thread_table + i;
      if (t -> stack_base != 0
	  && t -> id != thread_id) {
	  GC_suspend((GC_thread)t);
      }
    }
  } else {
      GC_thread t;
      int i;

      for (i = 0; i < THREAD_TABLE_SZ; i++) {
        for (t = GC_threads[i]; t != 0; t = t -> next) {
	  if (t -> stack_base != 0
	  && !KNOWN_FINISHED(t)
	  && t -> id != thread_id) {
	    GC_suspend(t);
	  }
	}
      }
  }
# ifndef CYGWIN32
    LeaveCriticalSection(&GC_write_cs);
# endif    
# ifdef PARALLEL_MARK
    if (GC_parallel)
      GC_release_mark_lock();
# endif
}

void GC_start_world(void)
{
  DWORD thread_id = GetCurrentThreadId();
  int i;

  GC_ASSERT(I_HOLD_LOCK());
  if (GC_win32_dll_threads) {
    LONG my_max = GC_get_max_thread_index();
    for (i = 0; i <= my_max; i++) {
      GC_thread t = (GC_thread)(dll_thread_table + i);
      if (t -> stack_base != 0 && t -> suspended
	  && t -> id != thread_id) {
        if (ResumeThread(t -> handle) == (DWORD)-1)
	  ABORT("ResumeThread failed");
        t -> suspended = FALSE;
      }
    }
  } else {
    GC_thread t;
    int i;

    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (t = GC_threads[i]; t != 0; t = t -> next) {
        if (t -> stack_base != 0 && t -> suspended
	    && t -> id != thread_id) {
          if (ResumeThread(t -> handle) == (DWORD)-1)
	    ABORT("ResumeThread failed");
	  UNPROTECT(t);
          t -> suspended = FALSE;
        }
      }
    }
  }
  GC_please_stop = FALSE;
}

# ifdef MSWINCE
    /* The VirtualQuery calls below won't work properly on WinCE, but	*/
    /* since each stack is restricted to an aligned 64K region of	*/
    /* virtual memory we can just take the next lowest multiple of 64K.	*/
#   define GC_get_stack_min(s) \
        ((ptr_t)(((DWORD)(s) - 1) & 0xFFFF0000))
# else

    /* A cache holding the results of the last VirtualQuery call. 	*/
    /* Protected by the allocation lock.				*/
    static ptr_t last_address = 0;
    static MEMORY_BASIC_INFORMATION last_info;

    /* Probe stack memory region (starting at "s") to find out its	*/
    /* lowest address (i.e. stack top).					*/
    /* S must be a mapped address inside the region, NOT the first	*/
    /* unmapped address.						*/
    static ptr_t GC_get_stack_min(ptr_t s)
    {
	ptr_t bottom;

	GC_ASSERT(I_HOLD_LOCK());
	if (s != last_address) {
	    VirtualQuery(s, &last_info, sizeof(last_info));
	    last_address = s;
	}
	do {
	    bottom = last_info.BaseAddress;
	    VirtualQuery(bottom - 1, &last_info, sizeof(last_info));
	    last_address = bottom - 1;
	} while ((last_info.Protect & PAGE_READWRITE)
		 && !(last_info.Protect & PAGE_GUARD));
	return(bottom);
    }

    /* Return true if the page at s has protections appropriate	*/
    /* for a stack page.					*/
    static GC_bool GC_may_be_in_stack(ptr_t s)
    {
	GC_ASSERT(I_HOLD_LOCK());
	if (s != last_address) {
	    VirtualQuery(s, &last_info, sizeof(last_info));
	    last_address = s;
	}
	return (last_info.Protect & PAGE_READWRITE)
	        && !(last_info.Protect & PAGE_GUARD);
    }
# endif

STATIC void GC_push_stack_for(GC_thread thread)
{
    int dummy;
    ptr_t sp, stack_min;
    DWORD me = GetCurrentThreadId();

    if (thread -> stack_base) {
      if (thread -> id == me) {
	sp = (ptr_t) &dummy;
      } else {
        CONTEXT context;
        context.ContextFlags = CONTEXT_INTEGER|CONTEXT_CONTROL;
        if (!GetThreadContext(thread -> handle, &context))
	  ABORT("GetThreadContext failed");

        /* Push all registers that might point into the heap.  Frame	*/
        /* pointer registers are included in case client code was	*/
        /* compiled with the 'omit frame pointer' optimisation.		*/
#       define PUSH1(reg) GC_push_one((word)context.reg)
#       define PUSH2(r1,r2) PUSH1(r1), PUSH1(r2)
#       define PUSH4(r1,r2,r3,r4) PUSH2(r1,r2), PUSH2(r3,r4)
#       if defined(I386)
          PUSH4(Edi,Esi,Ebx,Edx), PUSH2(Ecx,Eax), PUSH1(Ebp);
	  sp = (ptr_t)context.Esp;
#	elif defined(X86_64)
	  PUSH4(Rax,Rcx,Rdx,Rbx); PUSH2(Rbp, Rsi); PUSH1(Rdi);
	  PUSH4(R8, R9, R10, R11); PUSH4(R12, R13, R14, R15);
	  sp = (ptr_t)context.Rsp;
#       elif defined(ARM32)
	  PUSH4(R0,R1,R2,R3),PUSH4(R4,R5,R6,R7),PUSH4(R8,R9,R10,R11),PUSH1(R12);
	  sp = (ptr_t)context.Sp;
#       elif defined(SHx)
	  PUSH4(R0,R1,R2,R3), PUSH4(R4,R5,R6,R7), PUSH4(R8,R9,R10,R11);
	  PUSH2(R12,R13), PUSH1(R14);
	  sp = (ptr_t)context.R15;
#       elif defined(MIPS)
	  PUSH4(IntAt,IntV0,IntV1,IntA0), PUSH4(IntA1,IntA2,IntA3,IntT0);
	  PUSH4(IntT1,IntT2,IntT3,IntT4), PUSH4(IntT5,IntT6,IntT7,IntS0);
	  PUSH4(IntS1,IntS2,IntS3,IntS4), PUSH4(IntS5,IntS6,IntS7,IntT8);
	  PUSH4(IntT9,IntK0,IntK1,IntS8);
	  sp = (ptr_t)context.IntSp;
#       elif defined(PPC)
	  PUSH4(Gpr0, Gpr3, Gpr4, Gpr5),  PUSH4(Gpr6, Gpr7, Gpr8, Gpr9);
	  PUSH4(Gpr10,Gpr11,Gpr12,Gpr14), PUSH4(Gpr15,Gpr16,Gpr17,Gpr18);
	  PUSH4(Gpr19,Gpr20,Gpr21,Gpr22), PUSH4(Gpr23,Gpr24,Gpr25,Gpr26);
	  PUSH4(Gpr27,Gpr28,Gpr29,Gpr30), PUSH1(Gpr31);
	  sp = (ptr_t)context.Gpr1;
#       elif defined(ALPHA)
	  PUSH4(IntV0,IntT0,IntT1,IntT2), PUSH4(IntT3,IntT4,IntT5,IntT6);
	  PUSH4(IntT7,IntS0,IntS1,IntS2), PUSH4(IntS3,IntS4,IntS5,IntFp);
	  PUSH4(IntA0,IntA1,IntA2,IntA3), PUSH4(IntA4,IntA5,IntT8,IntT9);
	  PUSH4(IntT10,IntT11,IntT12,IntAt);
	  sp = (ptr_t)context.IntSp;
#       else
#         error "architecture is not supported"
#       endif
      } /* ! current thread */

      /* Set stack_min to the lowest address in the thread stack, 	*/
      /* or to an address in the thread stack no larger than sp,	*/
      /* taking advantage of the old value to avoid slow traversals	*/
      /* of large stacks.						*/
      if (thread -> last_stack_min == ADDR_LIMIT) {
      	stack_min = GC_get_stack_min(thread -> stack_base);
        UNPROTECT(thread);
        thread -> last_stack_min = stack_min;
      } else {
	if (sp < thread -> stack_base && sp >= thread -> last_stack_min) {
	    stack_min = sp;
	} else {
#         ifdef MSWINCE
	    stack_min = GC_get_stack_min(thread -> stack_base);
#         else
            if (GC_may_be_in_stack(thread -> last_stack_min)) {
              stack_min = GC_get_stack_min(thread -> last_stack_min);
	    } else {
	      /* Stack shrunk?  Is this possible? */
	      stack_min = GC_get_stack_min(thread -> stack_base);
	    }
#	  endif
          UNPROTECT(thread);
          thread -> last_stack_min = stack_min;
	}
      }
      GC_ASSERT(stack_min == GC_get_stack_min(thread -> stack_base)
      		|| (sp >= stack_min && stack_min < thread -> stack_base
		   && stack_min > GC_get_stack_min(thread -> stack_base)));

      if (sp >= stack_min && sp < thread->stack_base) {
#       ifdef DEBUG_THREADS
	  GC_printf("Pushing stack for 0x%x from sp %p to %p from 0x%x\n",
		    (int)thread -> id, sp, thread -> stack_base, (int)me);
#       endif
        GC_push_all_stack(sp, thread->stack_base);
      } else {
	/* If not current thread then it is possible for sp to point to	*/
	/* the guarded (untouched yet) page just below the current	*/
	/* stack_min of the thread.					*/
	if (thread -> id == me || sp >= thread->stack_base
		|| sp + GC_page_size < stack_min)
	  WARN("Thread stack pointer %p out of range, pushing everything\n",
		sp);
#       ifdef DEBUG_THREADS
	  GC_printf("Pushing stack for 0x%x from (min) %p to %p from 0x%x\n",
		    (int)thread -> id, stack_min,
		    thread -> stack_base, (int)me);
#       endif
        GC_push_all_stack(stack_min, thread->stack_base);
      }
    } /* thread looks live */
}

void GC_push_all_stacks(void)
{
  DWORD me = GetCurrentThreadId();
  GC_bool found_me = FALSE;
# ifndef SMALL_CONFIG
    unsigned nthreads = 0;
# endif
  
  if (GC_win32_dll_threads) {
    int i;
    LONG my_max = GC_get_max_thread_index();

    for (i = 0; i <= my_max; i++) {
      GC_thread t = (GC_thread)(dll_thread_table + i);
      if (t -> in_use) {
#	ifndef SMALL_CONFIG
	  ++nthreads;
#	endif
        GC_push_stack_for(t);
        if (t -> id == me) found_me = TRUE;
      }
    }
  } else {
    GC_thread t;
    int i;

    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (t = GC_threads[i]; t != 0; t = t -> next) {
#	ifndef SMALL_CONFIG
	  ++nthreads;
#	endif
        if (!KNOWN_FINISHED(t)) GC_push_stack_for(t);
        if (t -> id == me) found_me = TRUE;
      }
    }
  }
# ifndef SMALL_CONFIG
    if (GC_print_stats == VERBOSE) {
      GC_log_printf("Pushed %d thread stacks%s\n", nthreads,
	     GC_win32_dll_threads ? " based on DllMain thread tracking" : "");
    }
# endif
  if (!found_me && !GC_in_thread_creation)
    ABORT("Collecting from unknown thread.");
}

#ifdef PARALLEL_MARK

# ifndef MAX_MARKERS
#   define MAX_MARKERS 16
# endif

  extern long GC_markers;	/* Number of mark threads we would	*/
				/* like to have.  Includes the 		*/
				/* initiating thread.			*/

  STATIC ptr_t marker_sp[MAX_MARKERS - 1]; /* The cold end of the stack	*/
					   /* for markers.		*/
# ifdef IA64
    STATIC ptr_t marker_bsp[MAX_MARKERS - 1];
# endif

  STATIC ptr_t marker_last_stack_min[MAX_MARKERS - 1];
				/* Last known minimum (hottest) address */
    				/* in stack (or ADDR_LIMIT if unset)	*/
    				/* for markers.				*/

#endif

/* Find stack with the lowest address which overlaps the 	*/
/* interval [start, limit).					*/
/* Return stack bounds in *lo and *hi.  If no such stack	*/
/* is found, both *hi and *lo will be set to an address 	*/
/* higher than limit.						*/
void GC_get_next_stack(char *start, char *limit,
		       char **lo, char **hi)
{
    int i;
    char * current_min = ADDR_LIMIT;  /* Least in-range stack base 	*/
    ptr_t *plast_stack_min = NULL;    /* Address of last_stack_min 	*/
    				      /* field for thread corresponding	*/
				      /* to current_min.		*/
    GC_thread thread = NULL;	      /* Either NULL or points to the	*/
				      /* thread's hash table entry	*/
				      /* containing *plast_stack_min.	*/

    /* First set current_min, ignoring limit. */
      if (GC_win32_dll_threads) {
        LONG my_max = GC_get_max_thread_index();
  
        for (i = 0; i <= my_max; i++) {
     	  ptr_t s = (ptr_t)(dll_thread_table[i].stack_base);

	  if (s > start && s < current_min) {
	    /* Update address of last_stack_min. */
	    plast_stack_min = (ptr_t * /* no volatile */)
	    			&dll_thread_table[i].last_stack_min;
	    current_min = s;
	  }
        }
      } else {
        for (i = 0; i < THREAD_TABLE_SZ; i++) {
 	  GC_thread t;

          for (t = GC_threads[i]; t != 0; t = t -> next) {
	    ptr_t s = t -> stack_base;

	    if (s > start && s < current_min) {
	      /* Update address of last_stack_min. */
	      plast_stack_min = &t -> last_stack_min;
	      thread = t; /* Remember current thread to unprotect. */
	      current_min = s;
	    }
          }
        }
#	ifdef PARALLEL_MARK
	  for (i = 0; i < GC_markers - 1; ++i) {
	    ptr_t s = marker_sp[i];
#	    ifdef IA64
		/* FIXME: not implemented */
#	    endif
	    if (s > start && s < current_min) {
	      GC_ASSERT(marker_last_stack_min[i] != NULL);
	      plast_stack_min = &marker_last_stack_min[i];
	      current_min = s;
	      thread = NULL; /* Not a thread's hash table entry. */
	    }
	  }
#	endif
      }

    *hi = current_min;
    if (current_min == ADDR_LIMIT) {
    	*lo = ADDR_LIMIT;
	return;
    }

    GC_ASSERT(current_min > start);

#   ifndef MSWINCE
      if (current_min > limit && !GC_may_be_in_stack(limit)) {
        /* Skip the rest since the memory region at limit address is    */
	/* not a stack (so the lowest address of the found stack would  */
	/* be above the limit value anyway).				*/
        *lo = ADDR_LIMIT;
        return;
      }
#   endif
    
    /* Get the minimum address of the found stack by probing its memory	*/
    /* region starting from the last known minimum (if set).		*/
      if (*plast_stack_min == ADDR_LIMIT
#	 ifndef MSWINCE
	   || !GC_may_be_in_stack(*plast_stack_min)
#	 endif
         ) {
        /* Unsafe to start from last value.	*/
        *lo = GC_get_stack_min(current_min);
      } else {
        /* Use last value value to optimize search for min address */
    	*lo = GC_get_stack_min(*plast_stack_min);
      }

    /* Remember current stack_min value. */
      if (thread != NULL) {
	UNPROTECT(thread);
      }
      *plast_stack_min = *lo;
}

#ifdef PARALLEL_MARK

  /* GC_mark_thread() is the same as in pthread_support.c	*/
#ifdef GC_PTHREADS
  STATIC void * GC_mark_thread(void * id)
#else
  STATIC unsigned __stdcall GC_mark_thread(void * id)
#endif
{
  word my_mark_no = 0;

  marker_sp[(word)id] = GC_approx_sp();
# ifdef IA64
    marker_bsp[(word)id] = GC_save_regs_in_stack();
# endif

  if ((word)id == (word)-1) return 0; /* to make compiler happy */

  for (;; ++my_mark_no) {
    if (my_mark_no - GC_mark_no > (word)2) {
	/* resynchronize if we get far off, e.g. because GC_mark_no	*/
	/* wrapped.							*/
	my_mark_no = GC_mark_no;
    }
#   ifdef DEBUG_THREADS
	GC_printf("Starting mark helper for mark number %lu\n",
		(unsigned long)my_mark_no);
#   endif
    GC_help_marker(my_mark_no);
  }
}

#ifdef GC_ASSERTIONS
  unsigned long GC_mark_lock_holder = NO_THREAD;
#endif

/* GC_mark_threads[] is unused here unlike that in pthread_support.c */

#ifdef GC_PTHREADS

/* start_mark_threads() is the same as in pthread_support.c except for:	*/
/* - GC_markers value is adjusted already;				*/
/* - thread stack is assumed to be large enough; and			*/
/* - statistics about the number of marker threads is already printed.	*/

STATIC void start_mark_threads(void)
{
    unsigned i;
    pthread_attr_t attr;
    pthread_t new_thread;

    if (0 != pthread_attr_init(&attr)) ABORT("pthread_attr_init failed");
	
    if (0 != pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
	ABORT("pthread_attr_setdetachstate failed");

    for (i = 0; i < GC_markers - 1; ++i) {
      marker_last_stack_min[i] = ADDR_LIMIT;
      if (0 != pthread_create(&new_thread, &attr,
			      GC_mark_thread, (void *)(word)i)) {
	WARN("Marker thread creation failed, errno = %ld.\n",
		/* (word) */ errno);
      }
    }
    pthread_attr_destroy(&attr);
}

STATIC pthread_mutex_t mark_mutex = PTHREAD_MUTEX_INITIALIZER;

STATIC pthread_cond_t builder_cv = PTHREAD_COND_INITIALIZER;

/* GC_acquire/release_mark_lock(), GC_wait_builder/marker(),		*/
/* GC_wait_for_reclaim(), GC_notify_all_builder/marker() are the same	*/
/* as in pthread_support.c except that GC_generic_lock() is not used.	*/

#ifdef LOCK_STATS
  AO_t GC_block_count = 0;
#endif

void GC_acquire_mark_lock(void)
{
    if (pthread_mutex_lock(&mark_mutex) != 0) {
	ABORT("pthread_mutex_lock failed");
    }
#   ifdef LOCK_STATS
	(void)AO_fetch_and_add1(&GC_block_count);
#   endif
    /* GC_generic_lock(&mark_mutex); */
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self());
#   endif
}

void GC_release_mark_lock(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NO_THREAD;
#   endif
    if (pthread_mutex_unlock(&mark_mutex) != 0) {
	ABORT("pthread_mutex_unlock failed");
    }
}

/* Collector must wait for a freelist builders for 2 reasons:		*/
/* 1) Mark bits may still be getting examined without lock.		*/
/* 2) Partial free lists referenced only by locals may not be scanned 	*/
/*    correctly, e.g. if they contain "pointer-free" objects, since the	*/
/*    free-list link may be ignored.					*/
/* STATIC */ void GC_wait_builder(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NO_THREAD;
#   endif
    if (pthread_cond_wait(&builder_cv, &mark_mutex) != 0) {
	ABORT("pthread_cond_wait failed");
    }
    GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self());
#   endif
}

void GC_wait_for_reclaim(void)
{
    GC_acquire_mark_lock();
    while (GC_fl_builder_count > 0) {
	GC_wait_builder();
    }
    GC_release_mark_lock();
}

void GC_notify_all_builder(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
    if (pthread_cond_broadcast(&builder_cv) != 0) {
	ABORT("pthread_cond_broadcast failed");
    }
}

STATIC pthread_cond_t mark_cv = PTHREAD_COND_INITIALIZER;

void GC_wait_marker(void)
{
    GC_ASSERT(GC_mark_lock_holder == NUMERIC_THREAD_ID(pthread_self()));
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NO_THREAD;
#   endif
    if (pthread_cond_wait(&mark_cv, &mark_mutex) != 0) {
	ABORT("pthread_cond_wait failed");
    }
    GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NUMERIC_THREAD_ID(pthread_self());
#   endif
}

void GC_notify_all_marker(void)
{
    if (pthread_cond_broadcast(&mark_cv) != 0) {
	ABORT("pthread_cond_broadcast failed");
    }
}

#else /* ! GC_PTHREADS */

STATIC void start_mark_threads(void)
{
      int i;
      GC_uintptr_t handle;
      unsigned thread_id;

      for (i = 0; i < GC_markers - 1; ++i) {
	marker_last_stack_min[i] = ADDR_LIMIT;
	handle = _beginthreadex(NULL /* security_attr */, 0 /* stack_size */,
			GC_mark_thread, (void *)(word)i, 0 /* flags */,
			&thread_id);
	if (!handle || handle == (GC_uintptr_t)-1L)
	  WARN("Marker thread creation failed\n", 0);
	else { /* We may detach the thread (if handle is of HANDLE type) */
	  /* CloseHandle((HANDLE)handle); */
	}
      }
}

STATIC HANDLE mark_mutex_event = (HANDLE)0; /* Event with auto-reset. */
volatile AO_t GC_mark_mutex_waitcnt = 0;	/* Number of waiters + 1; */
					 	/* 0 - unlocked. */

STATIC HANDLE builder_cv = (HANDLE)0; /* Event with manual reset */

/* mark_mutex_event, builder_cv, mark_cv are initialized in GC_thr_init(). */

/* #define LOCK_STATS */
#ifdef LOCK_STATS
  AO_t GC_block_count = 0;
  AO_t GC_unlocked_count = 0;
#endif

void GC_acquire_mark_lock(void)
{
    if (AO_fetch_and_add1_acquire(&GC_mark_mutex_waitcnt) != 0) {
#	ifdef LOCK_STATS
          (void)AO_fetch_and_add1(&GC_block_count);
#	endif
        if (WaitForSingleObject(mark_mutex_event, INFINITE) == WAIT_FAILED)
          ABORT("WaitForSingleObject() failed");
    }
#   ifdef LOCK_STATS
        else {
	  (void)AO_fetch_and_add1(&GC_unlocked_count);
	}
#   endif
  
    GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = (unsigned long)GetCurrentThreadId();
#   endif
}

void GC_release_mark_lock(void)
{
    GC_ASSERT(GC_mark_lock_holder == (unsigned long)GetCurrentThreadId());
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NO_THREAD;
#   endif
    GC_ASSERT(AO_load(&GC_mark_mutex_waitcnt) != 0);
    if (AO_fetch_and_sub1_release(&GC_mark_mutex_waitcnt) > 1 &&
	 SetEvent(mark_mutex_event) == FALSE)
	ABORT("SetEvent() failed");
}

/* In GC_wait_for_reclaim/GC_notify_all_builder() we emulate POSIX	*/
/* cond_wait/cond_broadcast() primitives with WinAPI Event object	*/
/* (working in "manual reset" mode).  This works here because		*/
/* GC_notify_all_builder() is always called holding lock on		*/
/* mark_mutex and the checked condition (GC_fl_builder_count == 0)	*/
/* is the only one for which broadcasting on builder_cv is performed.	*/

void GC_wait_for_reclaim(void)
{
    GC_ASSERT(builder_cv != 0);
    for (;;) {
	GC_acquire_mark_lock();
	if (GC_fl_builder_count == 0)
	    break;
	if (ResetEvent(builder_cv) == FALSE)
	    ABORT("ResetEvent() failed");
	GC_release_mark_lock();
	if (WaitForSingleObject(builder_cv, INFINITE) == WAIT_FAILED)
	    ABORT("WaitForSingleObject() failed");
    }
    GC_release_mark_lock();
}

void GC_notify_all_builder(void)
{
    GC_ASSERT(GC_mark_lock_holder == (unsigned long)GetCurrentThreadId());
    GC_ASSERT(builder_cv != 0);
    GC_ASSERT(GC_fl_builder_count == 0);
    if (SetEvent(builder_cv) == FALSE)
	ABORT("SetEvent() failed");
}

/* For GC_wait_marker/GC_notify_all_marker() the above technique does	*/
/* not work because they are used with different checked conditions in	*/
/* different places (and, in addition, notifying is done after leaving	*/
/* critical section) and this could result in a signal loosing between	*/
/* checking for a particular condition and calling WaitForSingleObject.	*/
/* So, we use PulseEvent() and NT SignalObjectAndWait() (which		*/
/* atomically sets mutex event to signaled state and starts waiting on	*/
/* condvar). A special case here is GC_mark_mutex_waitcnt == 1 (i.e.	*/
/* nobody waits for mark lock at this moment) - we don't change it	*/
/* (otherwise we may loose a signal sent between decrementing		*/
/* GC_mark_mutex_waitcnt and calling WaitForSingleObject()).		*/

STATIC HANDLE mark_cv = (HANDLE)0; /* Event with manual reset */

typedef DWORD (WINAPI * SignalObjectAndWait_type)(
		HANDLE, HANDLE, DWORD, BOOL);
STATIC SignalObjectAndWait_type signalObjectAndWait_func = 0;

void GC_wait_marker(void)
{
    /* Here we assume that GC_wait_marker() is always called	*/
    /* from a while(check_cond) loop.				*/
    AO_t waitcnt;
    GC_ASSERT(mark_cv != 0);
    GC_ASSERT(signalObjectAndWait_func != 0);

    /* We inline GC_release_mark_lock() to have atomic		*/
    /* unlock-and-wait action here.				*/
    GC_ASSERT(GC_mark_lock_holder == (unsigned long)GetCurrentThreadId());
#   ifdef GC_ASSERTIONS
	GC_mark_lock_holder = NO_THREAD;
#   endif
    
    if ((waitcnt = AO_load(&GC_mark_mutex_waitcnt)) > 1) {
	(void)AO_fetch_and_sub1_release(&GC_mark_mutex_waitcnt);
    } else {
	GC_ASSERT(AO_load(&GC_mark_mutex_waitcnt) != 0);
    }

    /* The state of mark_cv is non-signaled here. */
    if ((*signalObjectAndWait_func)(mark_mutex_event /* hObjectToSignal */,
				mark_cv /* hObjectToWaitOn */,
				INFINITE /* timeout */,
				FALSE /* isAlertable */) == WAIT_FAILED)
	ABORT("SignalObjectAndWait() failed");
    /* The state of mark_cv is non-signaled here again. */

    if (waitcnt > 1) {
	GC_acquire_mark_lock();
    } else {
	GC_ASSERT(GC_mark_mutex_waitcnt != 0);
	/* Acquire mark lock */
	if (WaitForSingleObject(mark_mutex_event, INFINITE) == WAIT_FAILED)
	    ABORT("WaitForSingleObject() failed");
	GC_ASSERT(GC_mark_lock_holder == NO_THREAD);
#	ifdef GC_ASSERTIONS
	    GC_mark_lock_holder = (unsigned long)GetCurrentThreadId();
#	endif
    }
}

void GC_notify_all_marker(void)
{
    GC_ASSERT(mark_cv != 0);
    if (PulseEvent(mark_cv) == FALSE)
	ABORT("PulseEvent() failed");
}

/* Defined in os_dep.c */
extern GC_bool GC_wnt;

#endif /* ! GC_PTHREADS */

#endif /* PARALLEL_MARK */

#ifndef GC_PTHREADS

/* We have no DllMain to take care of new threads.  Thus we	*/
/* must properly intercept thread creation.			*/

typedef struct {
    LPTHREAD_START_ROUTINE start;
    LPVOID param;
} thread_args;

STATIC void * GC_CALLBACK GC_win32_start_inner(struct GC_stack_base *sb,
						void *arg)
{
    void * ret;
    thread_args *args = (thread_args *)arg;

    GC_register_my_thread(sb); /* This waits for an in-progress GC. */

#   if DEBUG_WIN32_THREADS
      GC_printf("thread 0x%x starting...\n", (unsigned)GetCurrentThreadId());
#   endif

    /* Clear the thread entry even if we exit with an exception.	*/
    /* This is probably pointless, since an uncaught exception is	*/
    /* supposed to result in the process being killed.			*/
#ifndef __GNUC__
    __try {
#endif /* __GNUC__ */
	ret = (void *)(word)args->start (args->param);
#ifndef __GNUC__
    } __finally {
#endif /* __GNUC__ */
	GC_unregister_my_thread();
	GC_free(args);
#ifndef __GNUC__
    }
#endif /* __GNUC__ */

#   if DEBUG_WIN32_THREADS
      GC_printf("thread 0x%x returned from start routine.\n",
		(unsigned)GetCurrentThreadId());
#   endif
    return ret;
}

DWORD WINAPI GC_win32_start(LPVOID arg)
{
    return (DWORD)(word)GC_call_with_stack_base(GC_win32_start_inner, arg);
}

GC_API HANDLE WINAPI GC_CreateThread(
    LPSECURITY_ATTRIBUTES lpThreadAttributes, 
    DWORD dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, 
    LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId )
{
    HANDLE thread_h;

    thread_args *args;

    if (!parallel_initialized) GC_init_parallel();
    		/* make sure GC is initialized (i.e. main thread is attached,
		   tls initialized) */

#   if DEBUG_WIN32_THREADS
      GC_printf("About to create a thread from 0x%x\n",
		(unsigned)GetCurrentThreadId());
#   endif
    if (GC_win32_dll_threads) {
      return CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress,
                        lpParameter, dwCreationFlags, lpThreadId);
    } else {
      args = GC_malloc_uncollectable(sizeof(thread_args)); 
	/* Handed off to and deallocated by child thread.	*/
      if (0 == args) {
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
      }

      /* set up thread arguments */
    	args -> start = lpStartAddress;
    	args -> param = lpParameter;

      GC_need_to_lock = TRUE;
      thread_h = CreateThread(lpThreadAttributes,
    			      dwStackSize, GC_win32_start,
    			      args, dwCreationFlags,
    			      lpThreadId);
      if( thread_h == 0 ) GC_free( args );
      return thread_h;
    }
}

GC_API void WINAPI GC_ExitThread(DWORD dwExitCode)
{
  GC_unregister_my_thread();
  ExitThread(dwExitCode);
}

#ifndef MSWINCE

GC_API GC_uintptr_t GC_CALL GC_beginthreadex(
    void *security, unsigned stack_size,
    unsigned ( __stdcall *start_address )( void * ),
    void *arglist, unsigned initflag, unsigned *thrdaddr)
{
    GC_uintptr_t thread_h;

    thread_args *args;

    if (!parallel_initialized) GC_init_parallel();
    		/* make sure GC is initialized (i.e. main thread is attached,
		   tls initialized) */
#   if DEBUG_WIN32_THREADS
      GC_printf("About to create a thread from 0x%x\n",
		(unsigned)GetCurrentThreadId());
#   endif

    if (GC_win32_dll_threads) {
      return _beginthreadex(security, stack_size, start_address,
                            arglist, initflag, thrdaddr);
    } else {
      args = GC_malloc_uncollectable(sizeof(thread_args)); 
	/* Handed off to and deallocated by child thread.	*/
      if (0 == args) {
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return (GC_uintptr_t)(-1L);
      }

      /* set up thread arguments */
    	args -> start = (LPTHREAD_START_ROUTINE)start_address;
    	args -> param = arglist;

      GC_need_to_lock = TRUE;
      thread_h = _beginthreadex(security, stack_size,
      		 (unsigned (__stdcall *) (void *))GC_win32_start,
                                args, initflag, thrdaddr);
      if( thread_h == 0 ) GC_free( args );
      return thread_h;
    }
}

GC_API void GC_CALL GC_endthreadex(unsigned retval)
{
  GC_unregister_my_thread();
  _endthreadex(retval);
}

#endif /* !MSWINCE */

#endif /* !GC_PTHREADS */

#ifdef MSWINCE

typedef struct {
    HINSTANCE hInstance;
    HINSTANCE hPrevInstance;
    LPWSTR lpCmdLine;
    int nShowCmd;
} main_thread_args;

DWORD WINAPI main_thread_start(LPVOID arg);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		   LPWSTR lpCmdLine, int nShowCmd)
{
    DWORD exit_code = 1;

    main_thread_args args = {
	hInstance, hPrevInstance, lpCmdLine, nShowCmd
    };
    HANDLE thread_h;
    DWORD thread_id;

    /* initialize everything */
    GC_init();

    /* start the main thread */
    thread_h = GC_CreateThread(
	NULL, 0, main_thread_start, &args, 0, &thread_id);

    if (thread_h != NULL)
    {
	WaitForSingleObject (thread_h, INFINITE);
	GetExitCodeThread (thread_h, &exit_code);
	CloseHandle (thread_h);
    }

    GC_deinit();
    DeleteCriticalSection(&GC_allocate_ml);

    return (int) exit_code;
}

DWORD WINAPI main_thread_start(LPVOID arg)
{
    main_thread_args * args = (main_thread_args *) arg;

    return (DWORD) GC_WinMain (args->hInstance, args->hPrevInstance,
			       args->lpCmdLine, args->nShowCmd);
}

# else /* !MSWINCE */

/* Called by GC_init() - we hold the allocation lock.	*/
void GC_thr_init(void) {
    struct GC_stack_base sb;
#   ifdef GC_ASSERTIONS
      int sb_result;
#   endif

    GC_ASSERT(I_HOLD_LOCK());
    if (GC_thr_initialized) return;
    GC_main_thread = GetCurrentThreadId();
    GC_thr_initialized = TRUE;

    /* Add the initial thread, so we can stop it.	*/
#   ifdef GC_ASSERTIONS
      sb_result =
#   endif
	GC_get_stack_base(&sb);
    GC_ASSERT(sb_result == GC_SUCCESS);
    
#   ifdef PARALLEL_MARK
      /* Set GC_markers. */
      {
	char * markers_string = GETENV("GC_MARKERS");
	if (markers_string != NULL) {
	  GC_markers = atoi(markers_string);
	  if (GC_markers > MAX_MARKERS) {
	    WARN("Limiting number of mark threads\n", 0);
	    GC_markers = MAX_MARKERS;
	  }
	} else {
#	  ifdef _WIN64
	    DWORD_PTR procMask = 0;
	    DWORD_PTR sysMask;
#	  else
	    DWORD procMask = 0;
	    DWORD sysMask;
#	  endif
	  int ncpu = 0;
	  if (GetProcessAffinityMask(GetCurrentProcess(),
	  			(void *)&procMask, (void *)&sysMask)
	      && procMask) {
	    do {
	      ncpu++;
	    } while ((procMask &= procMask - 1) != 0);
	  }
	  GC_markers = ncpu;
	  if (GC_markers >= MAX_MARKERS)
	    GC_markers = MAX_MARKERS; /* silently limit GC_markers value */
	}
      }	
      
      /* Set GC_parallel. */
      {
#	ifndef GC_PTHREADS
	  HMODULE hK32;
	  /* SignalObjectAndWait() API call works only under NT.	*/
#	endif
	if (GC_markers <= 1 || GC_win32_dll_threads
#	    ifndef GC_PTHREADS
	      || GC_wnt == FALSE
	      || (hK32 = GetModuleHandleA("kernel32.dll")) == (HMODULE)0
	      || (signalObjectAndWait_func = (SignalObjectAndWait_type)
			GetProcAddress(hK32, "SignalObjectAndWait")) == 0
#	    endif
	   ) {
	  /* Disable parallel marking. */
	  GC_parallel = FALSE;
	  GC_markers = 1;
	} else {
#	  ifndef GC_PTHREADS
	    /* Initialize Win32 event objects for parallel marking.	*/
	    mark_mutex_event = CreateEventA(NULL /* attrs */,
				FALSE /* isManualReset */,
				FALSE /* initialState */, NULL /* name */);
	    builder_cv = CreateEventA(NULL /* attrs */,
				TRUE /* isManualReset */,
				FALSE /* initialState */, NULL /* name */);
	    mark_cv = CreateEventA(NULL /* attrs */, TRUE /* isManualReset */,
				FALSE /* initialState */, NULL /* name */);
	    if (mark_mutex_event == (HANDLE)0 || builder_cv == (HANDLE)0
		|| mark_cv == (HANDLE)0)
	      ABORT("CreateEvent() failed");
#	  endif
	  GC_parallel = TRUE;
	  /* Disable true incremental collection, but generational is OK. */
	  GC_time_limit = GC_TIME_UNLIMITED;
	}
      }
      
      if (GC_print_stats) {
	GC_log_printf("Number of marker threads = %ld\n", GC_markers);
      }
#   endif /* PARALLEL_MARK */
    
    GC_ASSERT(0 == GC_lookup_thread_inner(GC_main_thread));
    GC_register_my_thread_inner(&sb, GC_main_thread);

#   ifdef PARALLEL_MARK
      /* If we are using a parallel marker, actually start helper threads.  */
      if (GC_parallel) start_mark_threads();
#   endif
}

#ifdef GC_PTHREADS

struct start_info {
    void *(*start_routine)(void *);
    void *arg;
    GC_bool detached;
};

int GC_pthread_join(pthread_t pthread_id, void **retval) {
    int result;
    GC_thread joinee;

#   if DEBUG_CYGWIN_THREADS
      GC_printf("thread 0x%x(0x%x) is joining thread 0x%x.\n",
		(int)pthread_self(), (int)GetCurrentThreadId(),
		(int)pthread_id);
#   endif
#   if DEBUG_WIN32_PTHREADS
      GC_printf("thread 0x%x(0x%x) is joining thread 0x%x.\n",
		(int)(pthread_self()).p, (int)GetCurrentThreadId(),
		pthread_id.p);
#   endif

    if (!parallel_initialized) GC_init_parallel();
    /* Thread being joined might not have registered itself yet. */
    /* After the join,thread id may have been recycled.		 */
    /* FIXME: It would be better if this worked more like	 */
    /* pthread_support.c.					 */

#   ifndef GC_WIN32_PTHREADS
      while ((joinee = GC_lookup_pthread(pthread_id)) == 0) Sleep(10);
#   endif

    result = pthread_join(pthread_id, retval);

#   ifdef GC_WIN32_PTHREADS
      /* win32_pthreads id are unique */
      joinee = GC_lookup_pthread(pthread_id);
#   endif

    if (!GC_win32_dll_threads) {
      LOCK();
      GC_delete_gc_thread(joinee);
      UNLOCK();
    } /* otherwise dllmain handles it.	*/

#   if DEBUG_CYGWIN_THREADS
      GC_printf("thread 0x%x(0x%x) completed join with thread 0x%x.\n",
		(int)pthread_self(), (int)GetCurrentThreadId(),
		(int)pthread_id);
#   endif
#   if DEBUG_WIN32_PTHREADS
      GC_printf("thread 0x%x(0x%x) completed join with thread 0x%x.\n",
		(int)(pthread_self()).p, (int)GetCurrentThreadId(),
		pthread_id.p);
#   endif

    return result;
}

/* Cygwin-pthreads calls CreateThread internally, but it's not
 * easily interceptible by us..
 *   so intercept pthread_create instead
 */
int
GC_pthread_create(pthread_t *new_thread,
		  const pthread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg) {
    int result;
    struct start_info * si;

    if (!parallel_initialized) GC_init_parallel();
    		/* make sure GC is initialized (i.e. main thread is attached) */
    if (GC_win32_dll_threads) {
      return pthread_create(new_thread, attr, start_routine, arg);
    }
    
    /* This is otherwise saved only in an area mmapped by the thread */
    /* library, which isn't visible to the collector.		 */
    si = GC_malloc_uncollectable(sizeof(struct start_info)); 
    if (0 == si) return(EAGAIN);

    si -> start_routine = start_routine;
    si -> arg = arg;
    if (attr != 0 &&
        pthread_attr_getdetachstate(attr, &si->detached)
	== PTHREAD_CREATE_DETACHED) {
      si->detached = TRUE;
    }

#   if DEBUG_CYGWIN_THREADS
      GC_printf("About to create a thread from 0x%x(0x%x)\n",
		(int)pthread_self(), (int)GetCurrentThreadId);
#   endif
#   if DEBUG_WIN32_PTHREADS
      GC_printf("About to create a thread from 0x%x(0x%x)\n",
		(int)(pthread_self()).p, (int)GetCurrentThreadId());
#   endif
    GC_need_to_lock = TRUE;
    result = pthread_create(new_thread, attr, GC_pthread_start, si); 

    if (result) { /* failure */
      	GC_free(si);
    } 

    return(result);
}

STATIC void * GC_CALLBACK GC_pthread_start_inner(struct GC_stack_base *sb,
						void * arg)
{
    struct start_info * si = arg;
    void * result;
    void *(*start)(void *);
    void *start_arg;
    DWORD thread_id = GetCurrentThreadId();
    pthread_t pthread_id = pthread_self();
    GC_thread me;

#   if DEBUG_CYGWIN_THREADS
      GC_printf("thread 0x%x(0x%x) starting...\n",(int)pthread_id,
		      				  (int)thread_id);
#   endif
#   if DEBUG_WIN32_PTHREADS
      GC_printf("thread 0x%x(0x%x) starting...\n",(int) pthread_id.p,
      						  (int)thread_id);
#   endif

    GC_ASSERT(!GC_win32_dll_threads);
    /* If a GC occurs before the thread is registered, that GC will	*/
    /* ignore this thread.  That's fine, since it will block trying to  */
    /* acquire the allocation lock, and won't yet hold interesting 	*/
    /* pointers.							*/
    LOCK();
    /* We register the thread here instead of in the parent, so that	*/
    /* we don't need to hold the allocation lock during pthread_create. */
    me = GC_register_my_thread_inner(sb, thread_id);
    SET_PTHREAD_MAP_CACHE(pthread_id, thread_id);
    UNLOCK();

    start = si -> start_routine;
    start_arg = si -> arg;
    if (si-> detached) me -> flags |= DETACHED;
    me -> pthread_id = pthread_id;

    GC_free(si); /* was allocated uncollectable */

    pthread_cleanup_push(GC_thread_exit_proc, (void *)me);
    result = (*start)(start_arg);
    me -> status = result;
    pthread_cleanup_pop(1);

#   if DEBUG_CYGWIN_THREADS
      GC_printf("thread 0x%x(0x%x) returned from start routine.\n",
		(int)pthread_self(),(int)GetCurrentThreadId());
#   endif
#   if DEBUG_WIN32_PTHREADS
      GC_printf("thread 0x%x(0x%x) returned from start routine.\n",
		(int)(pthread_self()).p, (int)GetCurrentThreadId());
#   endif

    return(result);
}

STATIC void * GC_pthread_start(void * arg)
{
    return GC_call_with_stack_base(GC_pthread_start_inner, arg);
}

STATIC void GC_thread_exit_proc(void *arg)
{
    GC_thread me = (GC_thread)arg;

    GC_ASSERT(!GC_win32_dll_threads);
#   if DEBUG_CYGWIN_THREADS
      GC_printf("thread 0x%x(0x%x) called pthread_exit().\n",
		(int)pthread_self(),(int)GetCurrentThreadId());
#   endif
#   if DEBUG_WIN32_PTHREADS
      GC_printf("thread 0x%x(0x%x) called pthread_exit().\n",
		(int)(pthread_self()).p,(int)GetCurrentThreadId());
#   endif

    LOCK();
#   if defined(THREAD_LOCAL_ALLOC)
      GC_destroy_thread_local(&(me->tlfs));
#   endif
    if (me -> flags & DETACHED) {
      GC_delete_thread(GetCurrentThreadId());
    } else {
      /* deallocate it as part of join */
      me -> flags |= FINISHED;
    }
    UNLOCK();
}

#ifndef GC_WIN32_PTHREADS
/* win32 pthread does not support sigmask */
/* nothing required here... */
int GC_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset) {
  if (!parallel_initialized) GC_init_parallel();
  return pthread_sigmask(how, set, oset);
}
#endif

int GC_pthread_detach(pthread_t thread)
{
    int result;
    GC_thread thread_gc_id;
    
    if (!parallel_initialized) GC_init_parallel();
    LOCK();
    thread_gc_id = GC_lookup_pthread(thread);
    UNLOCK();
    result = pthread_detach(thread);
    if (result == 0) {
      LOCK();
      thread_gc_id -> flags |= DETACHED;
      /* Here the pthread thread id may have been recycled. */
      if (thread_gc_id -> flags & FINISHED) {
        GC_delete_gc_thread(thread_gc_id);
      }
      UNLOCK();
    }
    return result;
}

#else /* !GC_PTHREADS */

/*
 * We avoid acquiring locks here, since this doesn't seem to be preemptible.
 * This may run with an uninitialized collector, in which case we don't do much.
 * This implies that no threads other than the main one should be created
 * with an uninitialized collector.  (The alternative of initializing
 * the collector here seems dangerous, since DllMain is limited in what it
 * can do.)
 */
#ifdef GC_DLL
/*ARGSUSED*/
BOOL WINAPI DllMain(HINSTANCE inst, ULONG reason, LPVOID reserved)
{
  struct GC_stack_base sb;
  DWORD thread_id;
# ifdef GC_ASSERTIONS
    int sb_result;
# endif
  static int entry_count = 0;

  if (parallel_initialized && !GC_win32_dll_threads) return TRUE;

  switch (reason) {
   case DLL_THREAD_ATTACH:
#   ifdef PARALLEL_MARK
      /* Don't register marker threads. */
      if (GC_parallel) {
	  /* We could reach here only if parallel_initialized == FALSE.	*/
	  break;
      }
#   endif
    GC_ASSERT(entry_count == 0 || parallel_initialized);
    ++entry_count; /* and fall through: */
   case DLL_PROCESS_ATTACH:
    /* This may run with the collector uninitialized. */
    thread_id = GetCurrentThreadId();
    if (parallel_initialized && GC_main_thread != thread_id) {
	/* Don't lock here.	*/
#	ifdef GC_ASSERTIONS
          sb_result =
#       endif
	    GC_get_stack_base(&sb);
        GC_ASSERT(sb_result == GC_SUCCESS);
#       if defined(THREAD_LOCAL_ALLOC) || defined(PARALLEL_MARK)
	  ABORT("Cannot initialize thread local cache from DllMain");
#       endif
	GC_register_my_thread_inner(&sb, thread_id);
    } /* o.w. we already did it during GC_thr_init(), called by GC_init() */
    break;

   case DLL_THREAD_DETACH:
    /* We are hopefully running in the context of the exiting thread.	*/
    GC_ASSERT(parallel_initialized);
    if (!GC_win32_dll_threads) return TRUE;
    GC_delete_thread(GetCurrentThreadId());
    break;

   case DLL_PROCESS_DETACH:
    {
      int i;
      int my_max;

      if (!GC_win32_dll_threads) return TRUE;
      my_max = (int)GC_get_max_thread_index();
      for (i = 0; i <= my_max; ++i)
      {
          if (AO_load(&(dll_thread_table[i].in_use)))
	    GC_delete_gc_thread(dll_thread_table + i);
      }

      GC_deinit();
      DeleteCriticalSection(&GC_allocate_ml);
    }
    break;

  }
  return TRUE;
}
#endif /* GC_DLL */
#endif /* !GC_PTHREADS */

# endif /* !MSWINCE */

/* Perform all initializations, including those that	*/
/* may require allocation.				*/
/* Called without allocation lock.			*/
/* Must be called before a second thread is created.	*/
void GC_init_parallel(void)
{
    if (parallel_initialized) return;
    parallel_initialized = TRUE;
    /* GC_init() calls us back, so set flag first.	*/
    
    if (!GC_is_initialized) GC_init();
    if (GC_win32_dll_threads) {
      GC_need_to_lock = TRUE;
	/* Cannot intercept thread creation.  Hence we don't know if	*/
	/* other threads exist.  However, client is not allowed to 	*/
	/* create other threads before collector initialization.	*/
	/* Thus it's OK not to lock before this.			*/
    }
    /* Initialize thread local free lists if used.	*/
#   if defined(THREAD_LOCAL_ALLOC)
      LOCK();
      GC_init_thread_local(&GC_lookup_thread_inner(GetCurrentThreadId())->tlfs);
      UNLOCK();
#   endif
}

#if defined(USE_PTHREAD_LOCKS)
  /* Support for pthread locking code.		*/
  /* Pthread_mutex_try_lock may not win here,	*/
  /* due to builtin support for spinning first?	*/

volatile GC_bool GC_collecting = 0;
			/* A hint that we're in the collector and       */
                        /* holding the allocation lock for an           */
                        /* extended period.                             */

void GC_lock(void)
{
    pthread_mutex_lock(&GC_allocate_ml);
}
#endif /* USE_PTHREAD ... */

# if defined(THREAD_LOCAL_ALLOC)

/* Add thread-local allocation support.  Microsoft uses __declspec(thread) */

/* We must explicitly mark ptrfree and gcj free lists, since the free 	*/
/* list links wouldn't otherwise be found.  We also set them in the 	*/
/* normal free lists, since that involves touching less memory than if	*/
/* we scanned them normally.						*/
void GC_mark_thread_local_free_lists(void)
{
    int i;
    GC_thread p;
    
    for (i = 0; i < THREAD_TABLE_SZ; ++i) {
      for (p = GC_threads[i]; 0 != p; p = p -> next) {
#       ifdef DEBUG_THREADS
	  GC_printf("Marking thread locals for 0x%x\n", p -> id);
#	endif
	GC_mark_thread_local_fls_for(&(p->tlfs));
      }
    }
}

#if defined(GC_ASSERTIONS)
    void GC_check_tls_for(GC_tlfs p);
#   if defined(USE_CUSTOM_SPECIFIC)
      void GC_check_tsd_marks(tsd *key);
#   endif 
    /* Check that all thread-local free-lists are completely marked.	*/
    /* also check that thread-specific-data structures are marked.	*/
    void GC_check_tls(void) {
	int i;
	GC_thread p;
	
	for (i = 0; i < THREAD_TABLE_SZ; ++i) {
	  for (p = GC_threads[i]; 0 != p; p = p -> next) {
	    GC_check_tls_for(&(p->tlfs));
	  }
	}
#       if defined(USE_CUSTOM_SPECIFIC)
	  if (GC_thread_key != 0)
	    GC_check_tsd_marks(GC_thread_key);
#	endif 
    }
#endif /* GC_ASSERTIONS */

#endif /* THREAD_LOCAL_ALLOC ... */

#endif /* GC_WIN32_THREADS */
