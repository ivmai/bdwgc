/* mit_threads.c
/*
/* Support for MIT-pthreads in Boehm/Demer/Weiser garbage collector.
/* Depends on a gc.a compiled with -DMIT_PTHREADS, plus local modifications.
/* All files should be compiled with pgcc.
/*
/* GC_init() should be called before doing anything else (the collector tries
/* to do this early enough, but may not always succeed).
/*
/* Author: Ian.Piumarta@INRIA.fr
/* 
/* Copyright (C) 1996 by INRIA and Ian Piumarta
/*
/* last edited: Sun Nov 17 22:49:05 1996 by piumarta (Ian Piumarta) on corto
/*
/***************************************************************************/

#ifdef MIT_PTHREADS

#undef DEBUG

#include "config.h"

/* there seems to be no other (easy) way to get at these... */

#if defined(ALPHA)
#  define THREAD_SP(thr) (thr)->machdep_data.machdep_state[34]	 /* JB_SP */
#elif defined(SPARC)
#  define THREAD_SP(thr) (thr)->machdep_data.machdep_state[2]	 /* sc_sp */
#elif defined(LINUX)
#  define THREAD_SP(thr) (thr)->machdep_data.machdep_state->__sp /* __sp  */
#elif defined(HP_PA)
#  define THREAD_SP(thr) (((int*)((thr)->machdep_data.machdep_state))[1])
#elif defined(MIPS)
#  define THREAD_SP(thr) (thr)->machdep_data.machdep_state[JB_SP]
#else
/*  ...define THREAD_SP for your architecture here...
 */
--> where is your stack pointer?
#endif

#include <stdio.h>
#define PTHREAD_KERNEL
#include <pthread.h>
#undef RETURN

extern void *GC_get_stack_base();

/*  use malloc-safe printf from the GC, and GC-safe malloc from pthreads
 */
extern GC_err_printf();
extern void *malloc(int);

#ifdef DEBUG
#  define _CR             GC_err_printf("\n")
#  define ENTER(X)        static char*_me_=#X;GC_err_printf("ENTER: %s\n",_me_)
#  define RETURN          GC_err_printf("LEAVE: %s\n",_me_);return
#  define PRINT(X)        GC_err_printf("%s: %s\n",_me_,(X))
#  define PRINT1(X,P)     GC_err_printf("%s: ",_me_);GC_err_printf((X),(P));_CR
#  define PRINT2(X,P,Q)   GC_err_printf("%s: ",_me_);GC_err_printf((X),(P),(Q));_CR
#  define PRINT3(X,P,Q,R) GC_err_printf("%s: ",_me_);GC_err_printf((X),(P),(Q),(R));_CR
#  define ERR(X)          {GC_err_printf("%s: ",_me_);perror(X);abort();}
#else
#  define ENTER(X)
#  define RETURN          return
#  define PRINT(X)
#  define PRINT1(X,P)
#  define PRINT2(X,P,Q)
#  define PRINT3(X,P,Q,R)
#  define ERR(X)          {perror(X);abort();}
#endif

/* dequeues */

#define struct_base(PTR, FLD, TYP) ((TYP*)((char*)(PTR)-(char*)(&(((TYP*)0)->FLD))))

typedef struct QUEUE {
  struct QUEUE *flink, *blink;
} queue;

/* GC thread info structure */

typedef struct GC_PT_INFO {
  queue q;		     /* the dequeue on which this structure exists */
  pthread_t thread;	     /* the corresponding thread structure */
  void *stack_top;	     /* the highest address in this thread's stack */
  void *(*launch)(void *);   /* the thread's real start routine */
  void *arg;		     /* the thread's real start routine argument */
  int mark_state;	     /* this info structures current civil status */
} gc_pt_info;

#define GC_PT_UNMARKED	0
#define GC_PT_MARKED	1
#define GC_PT_NASCENT	2

static queue infos = { &infos, &infos };  /* the dequeue of info structures */
extern int GC_pt_init_ok;		  /* true when init is done */

static pthread_key_t info_key;		  /* identity mechanism */


/* stop the threads world, somewhat gracelessly
 */
void GC_pt_stop_world()
{
  pthread_kernel_lock++;
}

/* restart the threads world; also lacks charm
 */
void GC_pt_start_world()
{
  pthread_kernel_lock--;
}


extern pthread_mutex_t GC_allocate_ml;

/* initialise the threads-related GC stuff
 */
void GC_pt_init()
{
  ENTER(GC_pt_init);
  if (!GC_pt_init_ok)
    {
      GC_pt_init_ok= 1;
      if (pthread_key_create(&info_key, 0)) ERR("pthread_key_create");
      /*
       * create an info structure for the initial thread and push it onto
       * the info dequeue
       */
      {
	gc_pt_info *info= (gc_pt_info *)malloc(sizeof(gc_pt_info));
	if (!info) ERR("malloc");
	infos.flink= infos.blink= &info->q;
	info->q.flink= info->q.blink= &infos;
	info->thread= pthread_initial;
	info->stack_top= GC_get_stack_base();
	PRINT3("initial thread %lx: info %lx, stack %lx",
	       pthread_initial, info, info->stack_top);
	if (pthread_setspecific(info_key, info)) ERR("pthread_setspecific");
	/*	GC_err_printf("THREAD INITIAL %lx: info set to %lx\n", pthread_run, info);*/
      }
    }
  RETURN;
}


static void GC_pt_print_threads()
{
  pthread_t thread;

  GC_err_printf("PLL:");

  for(thread= pthread_link_list; thread; thread= thread->pll)
      GC_err_printf(" %lx->%lx", thread, thread->pll);
  GC_err_printf("\n");
}


/* print the info list
 */
static void gc_pt_print_info()
{
  queue *ptr= infos.flink;
  ENTER(GC_pt_print_info);
  while (ptr != &infos) 
    {
      gc_pt_info *info= struct_base(ptr, q, gc_pt_info);
      GC_err_printf("INFO %lx:\n", info);
      GC_err_printf("  queue:      %lx\n", info->q);
      GC_err_printf("  thread:     %lx\n", info->thread);
      GC_err_printf("  stack_top:  %lx\n", info->stack_top);
      GC_err_printf("  launch:     %lx\n", info->launch);
      GC_err_printf("  arg:        %lx\n", info->arg);
      GC_err_printf("  mark_state: %lx\n", info->mark_state);
      ptr= ptr->flink;
    }
  RETURN;
}


#define FIND_INFO_BY_LOOKUP

/* given some thread, find the corresponding info
 */
static gc_pt_info *GC_pt_find_info(pthread_t target)
{
#ifdef FIND_INFO_BY_LOOKUP
  queue *ptr= infos.flink;
  ENTER(GC_pt_find_info);
  PRINT1("looking for thread %lx", target);
  while (ptr != &infos) 
    {
      gc_pt_info *info= struct_base(ptr, q, gc_pt_info);
      PRINT1("looking at %lx", info->thread);
      if (info->thread == target)
	{
	  RETURN(info);
	}
      ptr= ptr->flink;
    }
  GC_err_printf("I can't find the info -- giving up.\n");
  abort();
#else 

#  ifdef FIND_INFO_BY_GETSPECIFIC	/* THIS APPEARS TO BE BROKEN */
  gc_pt_info *info= (gc_pt_info *)pthread_getspecific(info_key);
  ENTER(GC_pt_find_info);
  PRINT1("looking for thread %lx", target);
  /*  GC_err_printf("THREAD %lx: found info %lx\n", target, info);*/
  return info;
#  else

#    ifdef FIND_INFO_BY_INTERNALS
  ENTER(GC_pt_find_info);
  PRINT1("looking for thread %lx", target);
  if (!target->specific_data)
    {
      PRINT("GC_pt_find_info: no specific data\n");
      return 0;
    }
  if (!target->specific_data[info_key])
    {
      PRINT("GC_pt_find_info: info is null\n");
      return 0;
    }
  return (gc_pt_info *)target->specific_data[info_key];
#    else

-->    so how do you do want me to find the info?

#    endif
#  endif
#endif
}


/* unmark all info structures
 */
static void GC_pt_unmark_info()
{
  int size =0;
  queue *ptr= infos.flink;
  ENTER(GC_pt_unmark_info);
  while (ptr != &infos) 
    {
      gc_pt_info *info= struct_base(ptr, q, gc_pt_info);
      if (info->mark_state != GC_PT_NASCENT)
	info->mark_state= GC_PT_UNMARKED;
      ptr= ptr->flink;
      size++;
    }
  PRINT1("INFO LIST SIZE IS %d", size);
}


/* cleanup for info structure -- this used to be called as a destructor
 * for the thread-specific data, but now it's only ever called explicitly
 * from within this file
 */
static void GC_pt_delete_info(gc_pt_info *info)
{
  ENTER(GC_pt_delete_info);
  PRINT1("pthread_run is %lx", pthread_run);
  PRINT1("info is %lx", info);
  /*  GC_err_printf("THREAD %lx: DELETE INFO %lx\n", info->thread, info);*/
  pthread_mutex_lock(&GC_allocate_ml);
  {
    info->q.blink->flink= info->q.flink;
    info->q.flink->blink= info->q.blink;
  }
  pthread_mutex_unlock(&GC_allocate_ml);
  PRINT1("info is %lx", info);
  free(info);
  RETURN;
}


/* sweep up unused info structures
 */
static void GC_pt_sweep_info()
{
  queue *ptr= infos.flink;
  ENTER(GC_pt_sweep_info);
  while (ptr != &infos) 
    {
      gc_pt_info *info= struct_base(ptr, q, gc_pt_info);
      ptr= ptr->flink;
      if (info->mark_state == GC_PT_UNMARKED)
	GC_pt_delete_info(info);
    }
}


/* traverse the pthread linked list (containing all threads) pushing
 * the pthread structures and stack (if appropriate) for each thread.
 */
void GC_pt_push_all_stacks()
{
  pthread_t thread;
  ENTER(GC_pt_push_all_stacks);

  /*  GC_err_printf("MARKING STACKS...\n");*/

  if ((infos.flink == &infos) && (infos.blink == &infos)) { RETURN; }

  /* unmark all info structures */
  GC_pt_unmark_info();

  /* flush (register windows and other) state for running thread */
  machdep_save_state();

  /*  GC_err_printf("THREAD LOOP: START\n");*/

  for(thread= pthread_link_list; thread; thread= thread->pll)
    {
      /*      GC_err_printf("THREAD LOOP: THREAD %lx, PLL %lx\n", thread, thread->pll);*/

      PRINT("thread structures");
      /* mark from the pthread internal structures */
      PRINT2("GC_push_all(%lx, %lx)", thread, (char *)thread + sizeof(*thread));
      GC_push_all(thread, (char *)thread + sizeof(*thread));
      PRINT2("GC_push_all(%lx, %lx)", thread->machdep_data.machdep_state,
	     (char *)thread->machdep_data.machdep_state
	     + sizeof(*thread->machdep_data.machdep_state));
      GC_push_all(thread->machdep_data.machdep_state,
		  (char *)thread->machdep_data.machdep_state
		  + sizeof(*thread->machdep_data.machdep_state));
      PRINT("specific data");
      /* mark the thread-specific data area */
      if (thread->specific_data_count)
	{
	  PRINT2("GC_push_all(%lx, %lx)\n", (void *)thread->specific_data,
		 ((void **)thread->specific_data) + PTHREAD_DATAKEYS_MAX - 1);
	  GC_push_all((void *)thread->specific_data,
		      ((void **)thread->specific_data) + PTHREAD_DATAKEYS_MAX - 1);
	}
      PRINT("thread stacks");
      /* mark from the thread's stack */
/*      if (thread->state != PS_DEAD)
 */
	{
	  gc_pt_info *info= GC_pt_find_info(thread);
	  /*	  GC_err_printf("LOOKING UP INFO FOR %lx\n", thread);*/
	  if (info)
	    {
	      /*	      GC_err_printf("INFO FOR %lx AT %lx\n", thread, info);*/
	      GC_push_all(info, (char *)info+sizeof(*info));
	      /*	      GC_err_printf("THREAD %lx: MARK INFO %lx\n", info->thread, info);*/
	      info->mark_state= GC_PT_MARKED;
	      if (thread == pthread_run)
		{
		  PRINT2("(RUN) GC_push_all_stack(%lx, %lx)", &thread, info->stack_top);
                  if ((void*)&thread < info->stack_top)
		    GC_push_all_stack(&thread, info->stack_top);
                  else
		    GC_push_all_stack(info->stack_top, &thread);
		}
	      else /* suspended */
		{
		  if ((void*)THREAD_SP(thread) < info->stack_top)
		    {
		      PRINT2("(SUS) GC_push_all_stack(%lx, %lx)",
			     THREAD_SP(thread), info->stack_top);
		      GC_push_all_stack(THREAD_SP(thread), info->stack_top);
		    }
		  else
		    {
		      /*PRINT2("(SUS) GC_push_all_stack(%lx, %lx) --- IGNORING BAD SP!!!",
			     THREAD_SP(thread), info->stack_top);*/
                      GC_push_all_stack(info->stack_top, THREAD_SP(thread));
		    }
		    
		}
	    }
	  else
	    GC_err_printf("THREAD %lx: NO INFO!!!!!!!!!\n", thread);
	  /*      else /* we're a dead thread */
	  /*	{
	  /*	  PRINT1("NO STACK (thread %lx is dead)", thread);
	  /*	}
	   */
	}
    }

  /*  GC_err_printf("THREAD LOOP: END\n");*/

  /* sweep up dead info */
  GC_pt_sweep_info();

  RETURN;
}


/* start routine wrapper, which places the threads id into the info
 * structure and registers it as thread-specific data; called with the
 * info structure as argument.
 */
static void *GC_pt_starter(void *arg)
{
  gc_pt_info *info= (gc_pt_info *)arg;
  void *result;
  ENTER(GC_pt_starter);
  if (pthread_setspecific(info_key, info)) ERR("pthread_setspecific");
  /*  GC_err_printf("THREAD %lx: info set to %lx\n", pthread_run, info);*/
  if (!GC_pt_find_info(pthread_run))
    {
      GC_err_printf("GC_pt_starter: the active thread does not exist!\n");
      abort();
    }
  PRINT3("*** STARTING %lx: pthread_run is %lx, info %lx",
	 info->thread, pthread_run, info);
  /*  GC_err_printf("THREAD RUN: %lx\n", info->thread);*/
  info->mark_state= GC_PT_MARKED;
  result= info->launch(info->arg);
  /*  GC_err_printf("THREAD EXIT: %lx\n", info->thread);*/
  PRINT3("*** EXITING %lx: pthread_run is %lx, info %lx",
	 info->thread, pthread_run, info);
  RETURN(result);
}


/* pthread_create wrapper, which creates an info structure on the info
 * dequeue and then has the thread start up in GC_pt_starter.
 */
int GC_pt_pthread_create(pthread_t *thread,
		   const pthread_attr_t *attr,
		   void *(*start_routine)(void *),
		   void *arg)
{
  int status;
  gc_pt_info *info= (gc_pt_info *)malloc(sizeof(gc_pt_info));
  ENTER(GC_pt_pthread_create);
  if (!info) ERR("malloc");

  /* thread mustn't start until we've built the info struct */
  GC_pt_stop_world();

  status= pthread_create(thread, attr, GC_pt_starter, (void *)info);
  if (!attr) attr= &pthread_attr_default;  /* no stack size? -- use the default */
  /* push the info onto the dequeue */
  info->q.flink= infos.flink;
  info->q.blink= &infos;
  infos.flink->blink= &info->q;
  infos.flink= &info->q;
  /* fill in the blanks */
  info->thread= *thread;
  info->launch= start_routine;
  info->arg= arg;
  info->mark_state= GC_PT_NASCENT;  /* thread not yet born */
  /* pthread_create filled in the initial SP -- profitons-en ! */
  info->stack_top= (void *)THREAD_SP(*thread);

  /*  GC_err_printf("THREAD CREATE: %lx\n", *thread);*/

  /*  GC_pt_print_threads();*/

  PRINT1("*** CREATED THREAD %lx", *thread);

  /* we're now ready for the thread to begin */
  GC_pt_start_world();
  RETURN(status);
}


#endif /* MIT_PTHREADS */
