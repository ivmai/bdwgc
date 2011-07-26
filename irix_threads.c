/* 
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
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
 * Support code for Irix (>=6.2) Pthreads.  This relies on properties
 * not guaranteed by the Pthread standard.  It may or may not be portable
 * to other implementations.
 */

# if defined(IRIX_THREADS)

# include "gc_priv.h"
# include <pthread.h>
# include <time.h>
# include <errno.h>
# include <unistd.h>
# include <sys/mman.h>
# include <sys/time.h>

#undef pthread_create
#undef pthread_sigmask
#undef pthread_join

#ifdef UNDEFINED
void GC_print_sig_mask()
{
    sigset_t blocked;
    int i;

    if (pthread_sigmask(SIG_BLOCK, NULL, &blocked) != 0)
    	ABORT("pthread_sigmask");
    GC_printf0("Blocked: ");
    for (i = 1; i <= MAXSIG; i++) {
        if (sigismember(&blocked, i)) { GC_printf1("%ld ",(long) i); }
    }
    GC_printf0("\n");
}
#endif

/* We use the allocation lock to protect thread-related data structures. */

/* The set of all known threads.  We intercept thread creation and 	*/
/* joins.  We never actually create detached threads.  We allocate all 	*/
/* new thread stacks ourselves.  These allow us to maintain this	*/
/* data structure.							*/
/* Protected by GC_thr_lock.						*/
/* Some of this should be declared volatile, but that's incosnsistent	*/
/* with some library routine declarations.  		 		*/
typedef struct GC_Thread_Rep {
    struct GC_Thread_Rep * next;  /* More recently allocated threads	*/
				  /* with a given pthread id come 	*/
				  /* first.  (All but the first are	*/
				  /* guaranteed to be dead, but we may  */
				  /* not yet have registered the join.) */
    pthread_t id;
    word flags;
#	define FINISHED 1   	/* Thread has exited.	*/
#	define DETACHED 2	/* Thread is intended to be detached.	*/
#	define CLIENT_OWNS_STACK	4
				/* Stack was supplied by client.	*/
    ptr_t stack;
    ptr_t stack_ptr;  		/* Valid only when stopped. */
				/* But must be within stack region at	*/
				/* all times.				*/
    size_t stack_size;		/* 0 for original thread.	*/
    void * status;		/* Used only to avoid premature 	*/
				/* reclamation of any data it might 	*/
				/* reference.				*/
} * GC_thread;

GC_thread GC_lookup_thread(pthread_t id);

/*
 * The only way to suspend threads given the pthread interface is to send
 * signals.  Unfortunately, this means we have to reserve
 * SIGUSR2, and intercept client calls to change the signal mask.
 */
# define SIG_SUSPEND SIGUSR2

pthread_mutex_t GC_suspend_lock = PTHREAD_MUTEX_INITIALIZER;
volatile unsigned GC_n_stopped = 0;
				/* Number of threads stopped so far	*/
pthread_cond_t GC_suspend_ack_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t GC_continue_cv = PTHREAD_COND_INITIALIZER;

void GC_suspend_handler(int sig)
{
    int dummy;
    GC_thread me;
    sigset_t all_sigs;
    sigset_t old_sigs;
    int i;

    if (sig != SIG_SUSPEND) ABORT("Bad signal in suspend_handler");
    pthread_mutex_lock(&GC_suspend_lock);
    me = GC_lookup_thread(pthread_self());
    /* The lookup here is safe, since I'm doing this on behalf  */
    /* of a thread which holds the allocation lock in order	*/
    /* to stop the world.  Thus concurrent modification of the	*/
    /* data structure is impossible.				*/
    me -> stack_ptr = (ptr_t)(&dummy);
    GC_n_stopped++;
    pthread_cond_signal(&GC_suspend_ack_cv);
    pthread_cond_wait(&GC_continue_cv, &GC_suspend_lock);
    pthread_mutex_unlock(&GC_suspend_lock);
    /* GC_printf1("Continuing 0x%x\n", pthread_self()); */
}


bool GC_thr_initialized = FALSE;

size_t GC_min_stack_sz;

size_t GC_page_sz;

# define N_FREE_LISTS 25
ptr_t GC_stack_free_lists[N_FREE_LISTS] = { 0 };
		/* GC_stack_free_lists[i] is free list for stacks of 	*/
		/* size GC_min_stack_sz*2**i.				*/
		/* Free lists are linked through first word.		*/

/* Return a stack of size at least *stack_size.  *stack_size is	*/
/* replaced by the actual stack size.				*/
/* Caller holds allocation lock.				*/
ptr_t GC_stack_alloc(size_t * stack_size)
{
    register size_t requested_sz = *stack_size;
    register size_t search_sz = GC_min_stack_sz;
    register int index = 0;	/* = log2(search_sz/GC_min_stack_sz) */
    register ptr_t result;
    
    while (search_sz < requested_sz) {
        search_sz *= 2;
        index++;
    }
    if ((result = GC_stack_free_lists[index]) == 0
        && (result = GC_stack_free_lists[index+1]) != 0) {
        /* Try next size up. */
        search_sz *= 2; index++;
    }
    if (result != 0) {
        GC_stack_free_lists[index] = *(ptr_t *)result;
    } else {
        result = (ptr_t) GC_scratch_alloc(search_sz + 2*GC_page_sz);
        result = (ptr_t)(((word)result + GC_page_sz) & ~(GC_page_sz - 1));
        /* Protect hottest page to detect overflow. */
        /* mprotect(result, GC_page_sz, PROT_NONE); */
        result += GC_page_sz;
    }
    *stack_size = search_sz;
    return(result);
}

/* Caller holds allocation lock.					*/
void GC_stack_free(ptr_t stack, size_t size)
{
    register int index = 0;
    register size_t search_sz = GC_min_stack_sz;
    
    while (search_sz < size) {
        search_sz *= 2;
        index++;
    }
    if (search_sz != size) ABORT("Bad stack size");
    *(ptr_t *)stack = GC_stack_free_lists[index];
    GC_stack_free_lists[index] = stack;
}



# define THREAD_TABLE_SZ 128	/* Must be power of 2	*/
volatile GC_thread GC_threads[THREAD_TABLE_SZ];

/* Add a thread to GC_threads.  We assume it wasn't already there.	*/
/* Caller holds allocation lock.					*/
GC_thread GC_new_thread(pthread_t id)
{
    int hv = ((word)id) % THREAD_TABLE_SZ;
    GC_thread result;
    static struct GC_Thread_Rep first_thread;
    static bool first_thread_used = FALSE;
    
    if (!first_thread_used) {
    	result = &first_thread;
    	first_thread_used = TRUE;
    	/* Dont acquire allocation lock, since we may already hold it. */
    } else {
        result = (struct GC_Thread_Rep *)
        	 GC_generic_malloc_inner(sizeof(struct GC_Thread_Rep), NORMAL);
    }
    if (result == 0) return(0);
    result -> id = id;
    result -> next = GC_threads[hv];
    GC_threads[hv] = result;
    /* result -> flags = 0; */
    return(result);
}

/* Delete a thread from GC_threads.  We assume it is there.	*/
/* (The code intentionally traps if it wasn't.)			*/
/* Caller holds allocation lock.				*/
void GC_delete_thread(pthread_t id)
{
    int hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    register GC_thread prev = 0;
    
    while (!pthread_equal(p -> id, id)) {
        prev = p;
        p = p -> next;
    }
    if (prev == 0) {
        GC_threads[hv] = p -> next;
    } else {
        prev -> next = p -> next;
    }
}

/* If a thread has been joined, but we have not yet		*/
/* been notified, then there may be more than one thread 	*/
/* in the table with the same pthread id.			*/
/* This is OK, but we need a way to delete a specific one.	*/
void GC_delete_gc_thread(pthread_t id, GC_thread gc_id)
{
    int hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    register GC_thread prev = 0;

    while (p != gc_id) {
        prev = p;
        p = p -> next;
    }
    if (prev == 0) {
        GC_threads[hv] = p -> next;
    } else {
        prev -> next = p -> next;
    }
}

/* Return a GC_thread corresponding to a given thread_t.	*/
/* Returns 0 if it's not there.					*/
/* Caller holds  allocation lock or otherwise inhibits 		*/
/* updates.							*/
/* If there is more than one thread with the given id we 	*/
/* return the most recent one.					*/
GC_thread GC_lookup_thread(pthread_t id)
{
    int hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    
    while (p != 0 && !pthread_equal(p -> id, id)) p = p -> next;
    return(p);
}


/* Caller holds allocation lock.	*/
void GC_stop_world()
{
    pthread_t my_thread = pthread_self();
    register int i;
    register GC_thread p;
    register int n_live_threads = 0;
    register int result;
    
    GC_n_stopped = 0;
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = GC_threads[i]; p != 0; p = p -> next) {
        if (p -> id != my_thread) {
            if (p -> flags & FINISHED) continue;
            n_live_threads++;
            result = pthread_kill(p -> id, SIG_SUSPEND);
	    /* GC_printf1("Sent signal to 0x%x\n", p -> id); */
	    switch(result) {
                case ESRCH:
                    /* Not really there anymore.  Possible? */
                    n_live_threads--;
                    break;
                case 0:
                    break;
                default:
                    ABORT("thr_kill failed");
            }
        }
      }
    }
    pthread_mutex_lock(&GC_suspend_lock);
    while(GC_n_stopped < n_live_threads) {
    	pthread_cond_wait(&GC_suspend_ack_cv, &GC_suspend_lock);
    }
    pthread_mutex_unlock(&GC_suspend_lock);
    /* GC_printf1("World stopped 0x%x\n", pthread_self()); */
}

/* Caller holds allocation lock.	*/
void GC_start_world()
{
    /* GC_printf0("World starting\n"); */
    pthread_mutex_lock(&GC_suspend_lock);
    /* All other threads are at pthread_cond_wait in signal handler.	*/
    /* Otherwise we couldn't have acquired the lock.			*/
    pthread_mutex_unlock(&GC_suspend_lock);
    pthread_cond_broadcast(&GC_continue_cv);
}


extern ptr_t GC_approx_sp();

/* We hold allocation lock.  We assume the world is stopped.	*/
void GC_push_all_stacks()
{
    register int i;
    register GC_thread p;
    register ptr_t sp = GC_approx_sp();
    register ptr_t lo, hi;
    pthread_t me = pthread_self();
    
    if (!GC_thr_initialized) GC_thr_init();
    /* GC_printf1("Pushing stacks from thread 0x%x\n", me); */
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = GC_threads[i]; p != 0; p = p -> next) {
        if (p -> flags & FINISHED) continue;
        if (pthread_equal(p -> id, me)) {
	    lo = GC_approx_sp();
	} else {
	    lo = p -> stack_ptr;
	}
        if (p -> stack_size != 0) {
            hi = p -> stack + p -> stack_size;
        } else {
            /* The original stack. */
            hi = GC_stackbottom;
        }
        GC_push_all_stack(lo, hi);
      }
    }
}


/* We hold the allocation lock.	*/
GC_thr_init()
{
    GC_thread t;

    GC_thr_initialized = TRUE;
    GC_min_stack_sz = HBLKSIZE;
    GC_page_sz = sysconf(_SC_PAGESIZE);
    if (sigset(SIG_SUSPEND, GC_suspend_handler) != SIG_DFL)
    	ABORT("Previously installed SIG_SUSPEND handler");
    /* Add the initial thread, so we can stop it.	*/
      t = GC_new_thread(pthread_self());
      t -> stack_size = 0;
      t -> stack_ptr = (ptr_t)(&t);
      t -> flags = DETACHED;
}

int GC_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
    sigset_t fudged_set;
    
    if (set != NULL && (how == SIG_BLOCK || how == SIG_SETMASK)) {
        fudged_set = *set;
        sigdelset(&fudged_set, SIG_SUSPEND);
        set = &fudged_set;
    }
    return(pthread_sigmask(how, set, oset));
}

struct start_info {
    void *(*start_routine)(void *);
    void *arg;
};

void GC_thread_exit_proc(void *dummy)
{
    GC_thread me;

    LOCK();
    me = GC_lookup_thread(pthread_self());
    if (me -> flags & DETACHED) {
    	GC_delete_thread(pthread_self());
    } else {
	me -> flags != FINISHED;
    }
    UNLOCK();
}

int GC_pthread_join(pthread_t thread, void **retval)
{
    int result;
    GC_thread thread_gc_id;
    
    LOCK();
    thread_gc_id = GC_lookup_thread(thread);
    /* This is guaranteed to be the intended one, since the thread id	*/
    /* cant have been recycled by pthreads.				*/
    UNLOCK();
    result = pthread_join(thread, retval);
    LOCK();
    /* Here the pthread thread id may have been recycled. */
    GC_delete_gc_thread(thread, thread_gc_id);
    UNLOCK();
    return result;
}

void * GC_start_routine(void * arg)
{
    struct start_info * si = arg;
    void * result;
    GC_thread me;

    LOCK();
    me = GC_lookup_thread(pthread_self());
    UNLOCK();
    pthread_cleanup_push(GC_thread_exit_proc, 0);
    result = (*(si -> start_routine))(si -> arg);
    me -> status = result;
    me -> flags |= FINISHED;
    pthread_cleanup_pop(1);
	/* This involves acquiring the lock, ensuring that we can't exit */
	/* while a collection that thinks we're alive is trying to stop  */
	/* us.								 */
    return(result);
}

int
GC_pthread_create(pthread_t *new_thread,
		  const pthread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg)
{
    int result;
    GC_thread t;
    pthread_t my_new_thread;
    void * stack;
    size_t stacksize;
    pthread_attr_t new_attr;
    int detachstate;
    word my_flags = 0;
    struct start_info * si = GC_malloc(sizeof(struct start_info)); 

    if (0 == si) return(ENOMEM);
    si -> start_routine = start_routine;
    si -> arg = arg;
    LOCK();
    if (!GC_thr_initialized) GC_thr_init();
    if (NULL == attr) {
        stack = 0;
	(void) pthread_attr_init(&new_attr);
    } else {
        new_attr = *attr;
	pthread_attr_getstackaddr(&new_attr, &stack);
    }
    pthread_attr_getstacksize(&new_attr, &stacksize);
    pthread_attr_getdetachstate(&new_attr, &detachstate);
    if (stacksize < GC_min_stack_sz) ABORT("Stack too small");
    if (0 == stack) {
     	stack = (void *)GC_stack_alloc(&stacksize);
     	if (0 == stack) {
     	    UNLOCK();
     	    return(ENOMEM);
     	}
	pthread_attr_setstackaddr(&new_attr, stack);
    } else {
    	my_flags |= CLIENT_OWNS_STACK;
    }
    if (PTHREAD_CREATE_DETACHED == detachstate) my_flags |= DETACHED;
    result = pthread_create(&my_new_thread, &new_attr, GC_start_routine, si);
    /* No GC can start until the thread is registered, since we hold	*/
    /* the allocation lock.						*/
    if (0 == result) {
        t = GC_new_thread(my_new_thread);
        t -> flags = my_flags;
        t -> stack = stack;
        t -> stack_size = stacksize;
	t -> stack_ptr = (ptr_t)stack + stacksize - sizeof(word);
        if (0 != new_thread) *new_thread = my_new_thread;
    } else if (!(my_flags & CLIENT_OWNS_STACK)) {
      	GC_stack_free(stack, stacksize);
    }        
    UNLOCK();  
    /* pthread_attr_destroy(&new_attr); */
    return(result);
}

bool GC_collecting = 0; /* A hint that we're in the collector and       */
                        /* holding the allocation lock for an           */
                        /* extended period.                             */

/* Reasonably fast spin locks.  Basically the same implementation */
/* as STL alloc.h.  This isn't really the right way to do this.   */
/* but until the POSIX scheduling mess gets straightened out ...  */

unsigned long GC_allocate_lock = 0;

void GC_lock()
{
#   define low_spin_max 30  /* spin cycles if we suspect uniprocessor */
#   define high_spin_max 1000 /* spin cycles for multiprocessor */
    static unsigned spin_max = low_spin_max;
    unsigned my_spin_max;
    static unsigned last_spins = 0;
    unsigned my_last_spins;
    unsigned junk;
#   define PAUSE junk *= junk; junk *= junk; junk *= junk; junk *= junk
    int i;

    if (!__test_and_set(&GC_allocate_lock, 1)) {
        return;
    }
    my_spin_max = spin_max;
    my_last_spins = last_spins;
    for (i = 0; i < my_spin_max; i++) {
        if (GC_collecting) goto yield;
        if (i < my_last_spins/2 || GC_allocate_lock) {
            PAUSE; 
            continue;
        }
        if (!__test_and_set(&GC_allocate_lock, 1)) {
	    /*
             * got it!
             * Spinning worked.  Thus we're probably not being scheduled
             * against the other process with which we were contending.
             * Thus it makes sense to spin longer the next time.
	     */
            last_spins = i;
            spin_max = high_spin_max;
            return;
        }
    }
    /* We are probably being scheduled against the other process.  Sleep. */
    spin_max = low_spin_max;
yield:
    for (;;) {
        if (!__test_and_set(&GC_allocate_lock, 1)) {
            return;
        }
        sched_yield();
    }
}



# else

#ifndef LINT
  int GC_no_Irix_threads;
#endif

# endif /* IRIX_THREADS */

