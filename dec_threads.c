/* dec_threads.c
/* 
/* Support for DECthreads in Boehm/Demer/Weiser garbage collector.
/* Depends on a gc.a compiled with -DDEC_PTHREADS, plus local modifications.
/* Final executable must be linked with "-lpthreads -lmach -lc_r".
/* 
/* Author: Ian.Piumarta@INRIA.fr
/* 
/* Copyright (C) 1996 by INRIA and Ian Piumarta
/*
/* Known problems:
/*
/*      - debugging needs to be tidied up
/*
/*      - some redundant code should be removed
/*
/* last edited: Wed Nov 13 15:42:21 1996 by piumarta (Ian Piumarta) on xombul
/*
/***************************************************************************/

#ifdef DEC_PTHREADS

/* Turn on debugging output */
#undef DEBUG

/* Turn on trace messages for thread creation/deletion */
#undef TRACE

/* visual indication of GC:			*/
/*	* = push stack for GC thread,		*/
/*	+ = push stack for other thread		*/
#undef VISUAL_CLUES

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/user.h>
#include <sys/table.h>
#include <mach/mach_interface.h>
#include <mach/thread_status.h>

extern char *GC_malloc_uncollectable();
extern pthread_mutex_t GC_allocate_ml;
extern int GC_pt_init_ok;

#define ERR(X) { perror(X); abort(); }

extern GC_err_printf();

#ifdef DEBUG
#  define FPRINTF(X) GC_err_printf X
#else
#  define FPRINTF(X)
#endif

#define PRINT_INFO(X,Y,Z)	/* or #define PRINT_INFO print_info */

#define CHECK_DUPLICATE	(2<<0)
#define CHECK_OVERLAP	(2<<1)

#define struct_base(PTR, FLD, TYP) \
	((TYP*)((char*)(PTR)-(char*)(&(((TYP*)0)->FLD))))

typedef struct QUEUE {
  struct QUEUE *flink, *blink;
} queue;

typedef struct GC_PT_THREAD_INFO {
  queue q;
  /* user thread structure */
  pthread_t thread;
  /* start routine and argument */
  pthread_startroutine_t starter;
  pthread_addr_t arg;
  /* mach port */
  port_t port;
  /* stack top */
  void *stack_top;
  /* approximate thread status */
  short active;
  short exited;
  short detached;
  /* saved register state */
  struct alpha_thread_state reg_state;
} gc_pt_thread_info;

#define TID(info) (info->port)

static queue infos = { &infos, &infos };

#define DISABLE_SIGNALS() GC_disable_signals()
#define ENABLE_SIGNALS() GC_enable_signals()

#define LOCK(ML) \
  if(pthread_mutex_lock(&GC_pt_##ML##_ml)<0) ERR("LOCK(GC_pt_"#ML"_ml)");

#define UNLOCK(ML) \
  if(pthread_mutex_unlock(&GC_pt_##ML##_ml)<0) ERR("UNLOCK(GC_pt_"#ML"_ml)");

static pthread_mutex_t GC_pt_info_ml;
static pthread_mutex_t GC_pt_init_ml;

static pthread_key_t info_key;


static print_info(gc_pt_thread_info *info, char *msg, long arg)
{
  GC_err_printf("********** INFO structure at %lx size %d for ",
	  info, sizeof(*info));
  GC_err_printf(msg, arg);
  GC_err_printf(":\n");
  /* this is VERY naughty: this structure is supposed to be opaque. */
  GC_err_printf("  thread at %lx = (%lx, %lx)\n",
	  info->thread, info->thread.field1, info->thread.field2);
  GC_err_printf("  startroutine at %lx, arg %lx\n", info->starter, info->arg);
  GC_err_printf("  mach port number is %d\n", info->port);
  GC_err_printf("  stack_top is %lx\n", info->stack_top);
  GC_err_printf("  stack pointer is %lx\n", info->reg_state.r30);
  GC_err_printf("  thread is %s", info->active ? "active" : "inactive");
  GC_err_printf(", %s", info->exited ? "exited" : "unexited");
  GC_err_printf(", %s", info->detached ? "detached" : "attached");
  GC_err_printf("\n  flink %lx, blink %lx\n", info->q.flink, info->q.blink);
}


static print_infos()
{
  queue *ptr= infos.flink;

  GC_err_printf("infos.flink = %lx\n", infos.flink);
  print_info(struct_base(ptr, q, gc_pt_thread_info), "flink", 0);
  GC_err_printf("infos.blink = %lx\n", infos.flink);
  print_info(struct_base(ptr, q, gc_pt_thread_info), "blink", 0);

  while (ptr != &infos) {
    GC_err_printf("q ===> %lx:", ptr);
    print_info(struct_base(ptr, q, gc_pt_thread_info), "print_infos", 0);
    ptr= ptr->flink;
  }
}


/* GC_pt_check
 *
 * Check the given gc_pt_thread_info structure is consistent with the others.
 *
 * Caller must hold the info lock.
 */
static int GC_pt_check(gc_pt_thread_info *info)
{
  queue *ptr= infos.flink;
  int fail= 0;
  while (ptr != &infos) {
    gc_pt_thread_info *old= struct_base(ptr, q, gc_pt_thread_info);
/*
    if (info != old && info->port == old->port) {
      fail|= CHECK_DUPLICATE;
      FPRINTF((stderr, "duplicate thread: %d\n", info->port));
    }
*/
    if (info != old && pthread_equal(info->thread, old->thread)) {
      fail|= CHECK_DUPLICATE;
      FPRINTF((stderr, "duplicate thread: %d\n", info->port));
    }
/*
    if (info != old &&
	(((char *)info->stack_top > (char *)old->reg_state.r30 &&
	  (char *)info->stack_top < (char *)old->stack_top) ||
	 ((char *)info->reg_state.r30 > (char *)old->reg_state.r30 &&
	  (char *)info->reg_state.r30 < (char *)old->stack_top))) {
      fail|= CHECK_OVERLAP;
      FPRINTF((stderr, "stack overlaps for threads: %lx and %lx\n",
	       TID(old), TID(info)));
    }
*/
    ptr= ptr->flink;
  }
  return fail;
}


/* This must be called with the info lock set.
 */
static gc_pt_thread_info *GC_pt_find_pthread_info(pthread_t *thread)
{
  queue *ptr= infos.flink;
  while (ptr != &infos) {
    gc_pt_thread_info *info= struct_base(ptr, q, gc_pt_thread_info);
    if (pthread_equal(*thread, info->thread))
      return info;
    ptr= ptr->flink;
  }
  return 0;
}


static void GC_pt_info_destroy(pthread_addr_t infop);


/* GC_pt_init
 *
 * Initialises DECthreads-specific data structures.  Should be called
 * before any threads are created, and before any posibility of GC
 * activity.
 */
void GC_pt_init()
{
  gc_pt_thread_info *info;
  struct user u;
  pthread_mutexattr_t attr;

  if(GC_pt_init_ok) return;
  GC_pt_init_ok= 1;
  FPRINTF((stderr, "initialising GC_dt...\n"));
  FPRINTF((stderr, "ENTER GC_pt_init()\n"));
  if (pthread_mutexattr_create(&attr)
      || pthread_mutexattr_setkind_np(&attr, MUTEX_RECURSIVE_NP)
      || pthread_mutex_init(&GC_pt_info_ml, attr)
      || pthread_mutex_init(&GC_pt_init_ml, attr)
      || pthread_keycreate(&info_key, GC_pt_info_destroy))
    ERR("GC_pt_init");

  if (table(TBL_UAREA, getpid(), &u, 1, sizeof(struct user)) < 1) {
    GC_err_printf("problem getting u area\n");
  }

  info= (gc_pt_thread_info *)GC_malloc_uncollectable(sizeof(gc_pt_thread_info));
  if (!info) {
    GC_err_printf("GC_dt: could not allocate thread_info\n");
    exit(1);
  }

  info->port= thread_self();
  info->stack_top= u.u_stack_end;
  info->active= 1;
  info->exited= 0;
  info->detached= 0;
  info->q.flink= &infos;
  info->q.blink= &infos;
  bzero((char *)&info->reg_state, sizeof(info->reg_state));

  LOCK(info);
  infos.flink= infos.blink= &info->q;
  UNLOCK(info);

  PRINT_INFO(info, "default thread", 0);

  FPRINTF((stderr, "LEAVE GC_pt_init()\n"));
}


static void GC_pt_apply_other_threads(kern_return_t (*func)(port_t))
{
  port_t me= thread_self();

  FPRINTF((stderr, "APPLYING for thread %d\n", me));

  LOCK(info);
  {
    queue *ptr= infos.flink;
    while (ptr != &infos)
      {
	gc_pt_thread_info *info= struct_base(ptr, q, gc_pt_thread_info);
	if (info->port != me && info->active && !info->exited) 
	  {
	    FPRINTF((stderr, "applying to thread %d:", info->port));
	    FPRINTF((stderr, " %s %s %s\n",
		     info->active ? "active" : "inactive",
		     info->exited ? "exited" : "unexited",
		     info->detached ? "detached" : "attached"));
	    if (func(info->port) != KERN_SUCCESS)
	      {
		UNLOCK(info);
		GC_err_printf("GC_pt_apply_other_threads failed:\n");
		print_info(info, "apply failed from thread %d", me);
		abort();
	      }
	  }
	else
	  {
	    FPRINTF((stderr, "NOT applying to thread %d:", info->port));
	    FPRINTF((stderr, " %s %s %s\n",
		     info->active ? "active" : "inactive",
		     info->exited ? "exited" : "unexited",
		     info->detached ? "detached" : "attached"));
	  }
	ptr= ptr->flink;
      }
  }
  UNLOCK(info);
}


void GC_pt_stop_world()
{
  if (!GC_pt_init_ok) GC_init();
  FPRINTF((stderr, "stopping the world...\n"));
  GC_pt_apply_other_threads(thread_suspend);
  FPRINTF((stderr, "...stopped!\n"));
}

void GC_pt_start_world()
{
  FPRINTF((stderr, "starting the world...\n"));
  GC_pt_apply_other_threads(thread_resume);
  FPRINTF((stderr, "...started!\n"));
}


/* GC_pt_push_all_stacks
 *
 * Pushes all thread stacks, including the default stack.  Default stack
 * pointer read from /proc, other thread stacks taken from stacks queue.
 * Locks the stacks to prevent thread creation/deletion during pushing.
 */
void GC_pt_push_all_stacks()
{
  queue *ptr= infos.flink;
  FPRINTF((stderr, "PUSHING THREAD STACKS:\n"));
  while (ptr != &infos)
    {
      gc_pt_thread_info *info= struct_base(ptr, q, gc_pt_thread_info);
      if (info->active && !info->exited)
	if (info->port == thread_self())
	  {
#ifdef VISUAL_CLUES
	    fputc('*', stderr);
#endif VISUAL_CLUES
	    FPRINTF((stderr, "[%lx,%lx]\n", &ptr, info->stack_top));
	    GC_push_all_stack(&ptr, info->stack_top);
	  }
	else
	  {
	    int count= ALPHA_THREAD_STATE_COUNT;
	    FPRINTF((stderr, "+"));
	    /* get register state to (a) know sp, and (b) mark registers */
	    PRINT_INFO(info, "thread %d PRE-GETSTATE", info->port);
	    FPRINTF((stderr, "  reg_state dumped to %lx, size %d\n",
		     &(info->reg_state), sizeof(info->reg_state)));
	    if (thread_get_state(info->port,
				 ALPHA_THREAD_STATE,
				 (thread_state_t)&(info->reg_state),
				 &count) != KERN_SUCCESS)
	      {
		GC_err_printf("problem in thread_get_state");
		exit(1);
	      }
	    PRINT_INFO(info, "thread %d POST-GETSTATE", info->port);
#ifdef VISUAL_CLUES
	    fputc('+', stderr);
#endif VISUAL_CLUES
	    FPRINTF((stderr, "[%lx,%lx]\n",
		     info->reg_state.r30, info->stack_top));
	    GC_push_all_stack(info->reg_state.r30, info->stack_top);
	  }
      ptr= ptr->flink;
    }
}


/* GC_pt_thread_init
 *
 * Creates a gc_pt_thread_info structure describing the calling thread in the
 * stacks queue.
 */
static void GC_pt_thread_init(gc_pt_thread_info *info, void *stacktop)
{
  FPRINTF((stderr, "ENTER GC_pt_thread_init(%lx)\n", TID(info)));
  {
    int status;
    info->port= thread_self();
    info->stack_top= stacktop;
    PRINT_INFO(info, "thread %d", info->port);
    if (status= GC_pt_check(info)) {
      if (status & CHECK_DUPLICATE)
	GC_err_printf("GC_pt_thread_init: duplicated thread %lx\n",
		TID(info));
      if (status & CHECK_OVERLAP)
	GC_err_printf("GC_pt_thread_init: stack overlap %lx\n",
		TID(info));
      if (status) {
	GC_err_printf("GC_pt_thread_init: aborting, sorry...\n");
	abort();
      }
    }
  }
  FPRINTF((stderr, "LEAVE GC_pt_thread_init(%lx)\n", TID(info)));
}


/* GC_pt_info_destroy
 *
 * The thread is finished, and about to exit -- either due to a normal
 * return or a cancel request.  If it is already detached, remove its
 * stack.  If not, mark the thread as exited so that its stack will be
 * removed when it is eventually detached.
 */
static void GC_pt_info_destroy(pthread_addr_t infop)
{
  gc_pt_thread_info *info= (gc_pt_thread_info *)infop;

  FPRINTF((stderr, "ENTER GC_pt_info_destroy(%lx)\n", infop));
  if (pthread_mutex_lock(&GC_allocate_ml) < 0)
    ERR("GC_pt_info_destroy: lock(&GC_allocate_ml)");
  {
    /* safety first: check that the info structure really exists */
    queue *target= &info->q;
    queue *ptr= infos.flink;
    while (ptr != &infos) 
      {
	if (ptr == target)
	  break;
	else
	  ptr= ptr->flink;
      }
    if (ptr != target)
      {
	if (pthread_mutex_unlock(&GC_allocate_ml) < 0)
	  ERR("GC_pt_info_destroy: unlock(&GC_allocate_ml)");
	GC_err_printf("GC_pt_info_destroy: unknown info -- aborting\n");
	abort();
      }
    if (info->detached) 
      {
	/* thread detached => delete stack on exit */
	queue *ptr= &info->q;
	ptr->flink->blink= ptr->blink;
	ptr->blink->flink= ptr->flink;
#ifdef TRACE
	GC_err_printf("DELETE (exit) %lx\n", TID(info));
#endif TRACE
	GC_free(info);
      }
    else
      {
	info->exited= 1;  /* cancelled thread won't do this otherwise */
	info->active= 0;
      }
  }
  if (pthread_mutex_unlock(&GC_allocate_ml) < 0)
    ERR("GC_pt_info_destroy: unlock(&GC_allocate_ml)");
  FPRINTF((stderr, "LEAVE GC_pt_info_destroy(%lx)\n", infop));
}


static pthread_addr_t GC_pt_start_routine(gc_pt_thread_info *info)
{
  pthread_addr_t status;
  LOCK(info);
  /* wait for GC_pt_pthread_create to finish building info */
  FPRINTF((stderr, "ENTER GC_pt_start_routine(%lx)\n", TID(info)));
  GC_pt_thread_init(info, &info);
  UNLOCK(info); /* it is now safe to start the next thread */
  pthread_setspecific(info_key, info);  /* the reaper for the thread */
  /* this thread might be starting up during a GC!  Do not proceed beyond
     this point until the allocation lock becomes available. */
  pthread_mutex_lock(&GC_allocate_ml);
  info->active= 1;
  pthread_mutex_unlock(&GC_allocate_ml);
#ifdef TRACE
  GC_err_printf("LAUNCH (start) %lx\n", TID(info));
#endif TRACE
  status= info->starter(info->arg);
  info->active= 0;
  info->exited= 1;
  FPRINTF((stderr, "LEAVE GC_pt_start_routine(%lx)\n", TID(info)));
  return status;
}

/* GC_pt_pthread_create
 *
 * Creates a new thread, allocates a gc_pt_thread_info object describing the
 * new thread,
 *
 * Get stack information by intercepting thread creation.
 */
int GC_pt_pthread_create(pthread_t *thread, pthread_attr_t attr,
			 pthread_startroutine_t start_routine,
			 pthread_addr_t arg)
{
  int result;
  gc_pt_thread_info *info;

  FPRINTF((stderr, "ENTER GC_pt_pthread_create(%lx)\n", thread));
  info= (void *)GC_malloc_uncollectable(sizeof(gc_pt_thread_info));
  info->starter= start_routine;
  info->arg= arg;
  info->active= 0;
  info->detached= 0;
  info->exited= 0;
  LOCK(info);
  if (result= pthread_create(thread, attr,
			     (pthread_startroutine_t)GC_pt_start_routine,
			     (pthread_addr_t)info)) {
    GC_free(info);
    FPRINTF((stderr, "LEAVE GC_pt_pthread_create(%lx) ===> ERROR\n", thread));
    goto end; /* something is amiss */
  }
  info->thread= *thread;
  PRINT_INFO(info, "uninitialized new thread", 0);
  /* add to end of info queue */
  {
    queue *ptr= &info->q;
    ptr->flink= &infos;
    ptr->blink= infos.blink;
    infos.blink->flink= ptr;
    infos.blink= ptr;
  }
  FPRINTF((stderr, "LEAVE GC_pt_pthread_create(%lx) ===> %lx\n",
	   thread, TID(info)));
 end:
  UNLOCK(info);
  return result;
}


/* GC_pt_pthread_detach
 *
 * If thread already exited, remove its stack.  Otherwise mark the thread
 * as detached, so that its stack will be removed when it eventually exits.
 */
int GC_pt_pthread_detach(pthread_t *thread)
{
  int result;
  FPRINTF((stderr, "ENTER GC_pt_pthread_detach(%lx)\n", thread));
  LOCK(info);
  {
    gc_pt_thread_info *info= GC_pt_find_pthread_info(thread);
    if (!info)
      {
	UNLOCK(info);
	errno= ESRCH;
	return -1;
      }
    if (info->exited)
      {
	/* thread exited => delete stack on detach */
	queue *ptr= &info->q;
	ptr->flink->blink= ptr->blink;
	ptr->blink->flink= ptr->flink;
	FPRINTF((stderr, "DELETE (detach) %lx\n", TID(info)));
	GC_free(info);
      }
    else
      {
	info->detached= 1;
      }
  }
  UNLOCK(info);
  result= pthread_detach(thread);
  FPRINTF((stderr, "LEAVE GC_pt_pthread_detach(%lx)\n", thread));
  return result;
}


#endif DEC_PTHREADS
