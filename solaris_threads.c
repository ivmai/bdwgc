/* 
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
/*
 * Support code for Solaris threads.  Provides functionality we wish Sun
 * had provided.  Relies on some information we probably shouldn't rely on.
 */
/* Boehm, April 5, 1994 1:30 pm PDT */

# if defined(SOLARIS_THREADS)

# include "gc_priv.h"
# include <thread.h>
# include <synch.h>
# include <sys/types.h>
# include <sys/mman.h>
# include <sys/time.h>
# include <sys/resource.h>
# define _CLASSIC_XOPEN_TYPES
# include <unistd.h>

#undef thr_join
#undef thr_create
#undef thr_suspend
#undef thr_continue

mutex_t GC_thr_lock;		/* Acquired before allocation lock	*/
cond_t GC_prom_join_cv;		/* Broadcast whenany thread terminates	*/
cond_t GC_create_cv;		/* Signalled when a new undetached	*/
				/* thread starts.			*/

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
/* Caller holds GC_thr_lock.					*/
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
        mprotect(result, GC_page_sz, PROT_NONE);
        GC_is_fresh((struct hblk *)result, divHBLKSZ(search_sz));
        result += GC_page_sz;
    }
    *stack_size = search_sz;
    return(result);
}

/* Caller holds GC_thr_lock.					*/
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

void GC_my_stack_limits();

/* Notify virtual dirty bit implementation that known empty parts of	*/
/* stacks do not contain useful data.					*/ 
void GC_old_stacks_are_fresh()
{
    register int i;
    register ptr_t p;
    register size_t sz;
    register struct hblk * h;
    int dummy;
    
    if (!GC_thr_initialized) GC_thr_init();
    for (i = 0, sz= GC_min_stack_sz; i < N_FREE_LISTS;
         i++, sz *= 2) {
         for (p = GC_stack_free_lists[i]; p != 0; p = *(ptr_t *)p) {
             h = (struct hblk *)(((word)p + HBLKSIZE-1) & ~(HBLKSIZE-1));
             if ((ptr_t)h == p) {
                 GC_is_fresh((struct hblk *)p, divHBLKSZ(sz));
             } else {
                 GC_is_fresh((struct hblk *)p, divHBLKSZ(sz) - 1);
                 BZERO(p, (ptr_t)h - p);
             }
         }
    }
    GC_my_stack_limits();
}

/* The set of all known threads.  We intercept thread creation and 	*/
/* joins.  We never actually create detached threads.  We allocate all 	*/
/* new thread stacks ourselves.  These allow us to maintain this	*/
/* data structure.							*/
/* Protected by GC_thr_lock.						*/
/* Some of this should be declared vaolatile, but that's incosnsistent	*/
/* with some library routine declarations.  In particular, the 		*/
/* definition of cond_t doesn't mention volatile!			*/
typedef struct GC_Thread_Rep {
    struct GC_Thread_Rep * next;
    thread_t id;
    word flags;
#	define FINISHED 1   	/* Thread has exited.	*/
#	define DETACHED 2	/* Thread is intended to be detached.	*/
#	define CLIENT_OWNS_STACK	4
				/* Stack was supplied by client.	*/
#	define SUSPENDED 8	/* Currently suspended.	*/	
    ptr_t stack;
    size_t stack_size;
    cond_t join_cv;
    void * status;
} * GC_thread;

# define THREAD_TABLE_SZ 128	/* Must be power of 2	*/
volatile GC_thread GC_threads[THREAD_TABLE_SZ];

/* Add a thread to GC_threads.  We assume it wasn't already there.	*/
/* Caller holds GC_thr_lock if there is > 1 thread.			*/
/* Initial caller may hold allocation lock.				*/
GC_thread GC_new_thread(thread_t id)
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
        result = GC_NEW(struct GC_Thread_Rep);
    }
    if (result == 0) return(0);
    result -> id = id;
    result -> next = GC_threads[hv];
    GC_threads[hv] = result;
    /* result -> finished = 0; */
    (void) cond_init(&(result->join_cv), USYNC_THREAD, 0);
    return(result);
}

/* Delete a thread from GC_threads.  We assume it is there.	*/
/* (The code intentionally traps if it wasn't.)			*/
/* Caller holds GC_thr_lock.					*/
void GC_delete_thread(thread_t id)
{
    int hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    register GC_thread prev = 0;
    
    while (p -> id != id) {
        prev = p;
        p = p -> next;
    }
    if (prev == 0) {
        GC_threads[hv] = p -> next;
    } else {
        prev -> next = p -> next;
    }
}

/* Return the GC_thread correpsonding to a given thread_t.	*/
/* Returns 0 if it's not there.					*/
/* Caller holds GC_thr_lock.					*/
GC_thread GC_lookup_thread(thread_t id)
{
    int hv = ((word)id) % THREAD_TABLE_SZ;
    register GC_thread p = GC_threads[hv];
    
    while (p != 0 && p -> id != id) p = p -> next;
    return(p);
}

/* Notify dirty bit implementation of unused parts of my stack. */
void GC_my_stack_limits()
{
    int dummy;
    register ptr_t hottest = (ptr_t)((word)(&dummy) & ~(HBLKSIZE-1));
    register GC_thread me = GC_lookup_thread(thr_self());
    register size_t stack_size = me -> stack_size;
    register ptr_t stack;
    
    if (stack_size == 0) {
      /* original thread */
        struct rlimit rl;
         
        if (getrlimit(RLIMIT_STACK, &rl) != 0) ABORT("getrlimit failed");
        /* Empirically, what should be the stack page with lowest	*/
        /* address is actually inaccessible.				*/
        stack_size = ((word)rl.rlim_cur & ~(HBLKSIZE-1)) - GC_page_sz;
        stack = GC_stackbottom - stack_size + GC_page_sz;
    } else {
        stack = me -> stack;
    }
    if (stack > hottest || stack + stack_size < hottest) {
    	ABORT("sp out of bounds");
    }
    GC_is_fresh((struct hblk *)stack, divHBLKSZ(hottest - stack));
}


/* Caller holds allocation lock.	*/
void GC_stop_world()
{
    thread_t my_thread = thr_self();
    register int i;
    register GC_thread p;
    
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = GC_threads[i]; p != 0; p = p -> next) {
        if (p -> id != my_thread && !(p -> flags & SUSPENDED)) {
            if (thr_suspend(p -> id) < 0) ABORT("thr_suspend failed");
        }
      }
    }
}

/* Caller holds allocation lock.	*/
void GC_start_world()
{
    thread_t my_thread = thr_self();
    register int i;
    register GC_thread p;
    
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = GC_threads[i]; p != 0; p = p -> next) {
        if (p -> id != my_thread && !(p -> flags & SUSPENDED)) {
            if (thr_continue(p -> id) < 0) ABORT("thr_continue failed");
        }
      }
    }
}


void GC_push_all_stacks()
{
    /* We assume the world is stopped. */
    register int i;
    register GC_thread p;
    word dummy;
    register ptr_t sp = (ptr_t) (&dummy);
    register ptr_t bottom, top;
    struct rlimit rl;
    
#   define PUSH(bottom,top) \
      if (GC_dirty_maintained) { \
	GC_push_dirty((bottom), (top), GC_page_was_ever_dirty, \
		      GC_push_all_stack); \
      } else { \
        GC_push_all((bottom), (top)); \
      }
    if (!GC_thr_initialized) GC_thr_init();
    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      for (p = GC_threads[i]; p != 0; p = p -> next) {
        if (p -> stack_size != 0) {
            bottom = p -> stack;
            top = p -> stack + p -> stack_size;
        } else {
            /* The original stack. */
            if (getrlimit(RLIMIT_STACK, &rl) != 0) ABORT("getrlimit failed");
            bottom = GC_stackbottom - rl.rlim_cur + GC_page_sz;
            top = GC_stackbottom;
        }
        if ((word)sp > (word)bottom && (word)sp < (word)top) bottom = sp;
        PUSH(bottom, top);
      }
    }
}

/* The only thread that ever really performs a thr_join.	*/
void * GC_thr_daemon(void * dummy)
{
    void *status;
    thread_t departed;
    register GC_thread t;
    register int i;
    register int result;
    
    for(;;) {
      start:
        result = thr_join((thread_t)0, &departed, &status);
    	mutex_lock(&GC_thr_lock);
    	if (result != 0) {
    	    /* No more threads; wait for create. */
    	    for (i = 0; i < THREAD_TABLE_SZ; i++) {
    	        for (t = GC_threads[i]; t != 0; t = t -> next) {
                    if (!(t -> flags & (DETACHED | FINISHED))) {
                      mutex_unlock(&GC_thr_lock);
                      goto start; /* Thread started just before we */
                      		  /* acquired the lock.		   */
                    }
                }
            }
            cond_wait(&GC_create_cv, &GC_thr_lock);
            mutex_unlock(&GC_thr_lock);
            goto start;
    	}
    	t = GC_lookup_thread(departed);
    	if (!(t -> flags & CLIENT_OWNS_STACK)) {
    	    GC_stack_free(t -> stack, t -> stack_size);
    	}
    	if (t -> flags & DETACHED) {
    	    GC_delete_thread(departed);
    	} else {
    	    t -> status = status;
    	    t -> flags |= FINISHED;
    	    cond_signal(&(t -> join_cv));
    	    cond_broadcast(&GC_prom_join_cv);
    	}
    	mutex_unlock(&GC_thr_lock);
    }
}

GC_thr_init()
{
    GC_thread t;
    /* This gets called from the first thread creation, so	*/
    /* mutual exclusion is not an issue.			*/
    GC_thr_initialized = TRUE;
    GC_min_stack_sz = ((thr_min_stack() + HBLKSIZE-1) & ~(HBLKSIZE - 1));
    GC_page_sz = sysconf(_SC_PAGESIZE);
    mutex_init(&GC_thr_lock, USYNC_THREAD, 0);
    cond_init(&GC_prom_join_cv, USYNC_THREAD, 0);
    cond_init(&GC_create_cv, USYNC_THREAD, 0);
    /* Add the initial thread, so we can stop it.	*/
      t = GC_new_thread(thr_self());
      t -> stack_size = 0;
      t -> flags = DETACHED;
    if (thr_create(0 /* stack */, 0 /* stack_size */, GC_thr_daemon,
    		   0 /* arg */, THR_DETACHED | THR_DAEMON,
    		   0 /* thread_id */) != 0) {
    	ABORT("Cant fork daemon");
    }
    
}

/* We acquire the allocation lock to prevent races with 	*/
/* stopping/starting world.					*/
int GC_thr_suspend(thread_t target_thread)
{
    GC_thread t;
    int result;
    
    mutex_lock(&GC_thr_lock);
    LOCK();
    result = thr_suspend(target_thread);
    if (result == 0) {
    	t = GC_lookup_thread(target_thread);
    	if (t == 0) ABORT("thread unknown to GC");
        t -> flags |= SUSPENDED;
    }
    UNLOCK();
    mutex_unlock(&GC_thr_lock);
    return(result);
}

int GC_thr_continue(thread_t target_thread)
{
    GC_thread t;
    int result;
    
    mutex_lock(&GC_thr_lock);
    LOCK();
    result = thr_continue(target_thread);
    if (result == 0) {
    	t = GC_lookup_thread(target_thread);
    	if (t == 0) ABORT("thread unknown to GC");
        t -> flags &= ~SUSPENDED;
    }
    UNLOCK();
    mutex_unlock(&GC_thr_lock);
    return(result);
}

int GC_thr_join(thread_t wait_for, thread_t *departed, void **status)
{
    register GC_thread t;
    int result = 0;
    
    mutex_lock(&GC_thr_lock);
    if (wait_for == 0) {
        register int i;
        register bool thread_exists;
    
    	for (;;) {
    	  thread_exists = FALSE;
    	  for (i = 0; i < THREAD_TABLE_SZ; i++) {
    	    for (t = GC_threads[i]; t != 0; t = t -> next) {
              if (!(t -> flags & DETACHED)) {
                if (t -> flags & FINISHED) {
                  goto found;
                }
                thread_exists = TRUE;
              }
            }
          }
          if (!thread_exists) {
              result = ESRCH;
    	      goto out;
          }
          cond_wait(&GC_prom_join_cv, &GC_thr_lock);
        }
    } else {
        t = GC_lookup_thread(wait_for);
    	if (t == 0 || t -> flags & DETACHED) {
    	    result = ESRCH;
    	    goto out;
    	}
    	if (wait_for == thr_self()) {
    	    result = EDEADLK;
    	    goto out;
    	}
    	while (!(t -> flags & FINISHED)) {
            cond_wait(&(t -> join_cv), &GC_thr_lock);
    	}
    	
    }
  found:
    if (status) *status = t -> status;
    if (departed) *departed = t -> id;
    cond_destroy(&(t -> join_cv));
    GC_delete_thread(t -> id);
  out:
    mutex_unlock(&GC_thr_lock);
    return(result);
}


int
GC_thr_create(void *stack_base, size_t stack_size,
              void *(*start_routine)(void *), void *arg, long flags,
              thread_t *new_thread)
{
    int result;
    GC_thread t;
    thread_t my_new_thread;
    word my_flags = 0;
    void * stack = stack_base;
   
    if (!GC_thr_initialized) GC_thr_init();
    mutex_lock(&GC_thr_lock);
    if (stack == 0) {
     	if (stack_size == 0) stack_size = GC_min_stack_sz;
     	stack = (void *)GC_stack_alloc(&stack_size);
     	if (stack == 0) {
     	    mutex_unlock(&GC_thr_lock);
     	    return(ENOMEM);
     	}
    } else {
    	my_flags |= CLIENT_OWNS_STACK;
    }
    if (flags & THR_DETACHED) my_flags |= DETACHED;
    if (flags & THR_SUSPENDED) my_flags |= SUSPENDED;
    result = thr_create(stack, stack_size, start_routine,
   		        arg, flags & ~THR_DETACHED, &my_new_thread);
    if (result == 0) {
        t = GC_new_thread(my_new_thread);
        t -> flags = my_flags;
        if (!(my_flags & DETACHED)) cond_init(&(t -> join_cv), USYNC_THREAD, 0);
        t -> stack = stack;
        t -> stack_size = stack_size;
        if (new_thread != 0) *new_thread = my_new_thread;
        cond_signal(&GC_create_cv);
    } else if (!(my_flags & CLIENT_OWNS_STACK)) {
      	GC_stack_free(stack, stack_size);
    }        
    mutex_unlock(&GC_thr_lock);  
    return(result);
}

# else

#ifndef LINT
  int GC_no_sunOS_threads;
#endif

# endif /* SOLARIS_THREADS */
