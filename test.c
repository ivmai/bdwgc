/* An incomplete test for the garbage collector.  		*/
/* Some more obscure entry points are not tested at all.	*/
/* Boehm, November 24, 1993 5:14 pm PST */
# include <stdlib.h>
# include <stdio.h>
# include "gc.h"
# ifdef PCR
#   include "th/PCR_ThCrSec.h"
#   include "th/PCR_Th.h"
# endif

# ifdef AMIGA
   long __stack = 200000;
#  define FAR __far
# else
#  define FAR
# endif

# define FAIL (void)abort()

/* AT_END may be defined to excercise the interior pointer test	*/
/* if the collector is configured with ALL_INTERIOR_POINTERS.   */
/* As it stands, this test should succeed with either		*/
/* configuration.  In the FIND_LEAK configuration, it should	*/
/* find lots of leaks, since we free almost nothing.		*/

struct SEXPR {
    struct SEXPR * sexpr_car;
    struct SEXPR * sexpr_cdr;
};

# ifdef __STDC__
    typedef void * void_star;
# else
    typedef char * void_star;
# endif

typedef struct SEXPR * sexpr;

extern sexpr cons();

# define nil ((sexpr) 0)
# define car(x) ((x) -> sexpr_car)
# define cdr(x) ((x) -> sexpr_cdr)
# define is_nil(x) ((x) == nil)


int extra_count = 0;        /* Amount of space wasted in cons node */

/* Silly implementation of Lisp cons. Intentionally wastes lots of space */
/* to test collector.                                                    */
sexpr cons (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    register int *p;
    register my_extra = extra_count;
    
    r = (sexpr) GC_MALLOC_STUBBORN(sizeof(struct SEXPR) + my_extra);
    if (r == 0) {
        (void)printf("Out of memory\n");
        exit(1);
    }
    for (p = (int *)r;
         ((char *)p) < ((char *)r) + my_extra + sizeof(struct SEXPR); p++) {
	if (*p) {
	    (void)printf("Found nonzero at %X - allocator is broken\n", p);
	    FAIL;
        }
        *p = 13;
    }
#   ifdef AT_END
	r = (sexpr)((char *)r + (my_extra & ~7));
#   endif
    r -> sexpr_car = x;
    r -> sexpr_cdr = y;
    my_extra++;
    if ( my_extra >= 5000 ) {
        extra_count = 0;
    } else {
        extra_count = my_extra;
    }
    GC_END_STUBBORN_CHANGE((char *)r);
    return(r);
}

sexpr small_cons (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    
    r = (sexpr) GC_MALLOC(sizeof(struct SEXPR));
    if (r == 0) {
        (void)printf("Out of memory\n");
        exit(1);
    }
    r -> sexpr_car = x;
    r -> sexpr_cdr = y;
    return(r);
}

sexpr small_cons_uncollectable (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    
    r = (sexpr) GC_MALLOC_UNCOLLECTABLE(sizeof(struct SEXPR));
    if (r == 0) {
        (void)printf("Out of memory\n");
        exit(1);
    }
    r -> sexpr_car = x;
    r -> sexpr_cdr = (sexpr) (~(unsigned long)y);
    return(r);
}

/* Return reverse(x) concatenated with y */
sexpr reverse1(x, y)
sexpr x, y;
{
    if (is_nil(x)) {
        return(y);
    } else {
        return( reverse1(cdr(x), cons(car(x), y)) );
    }
}

sexpr reverse(x)
sexpr x;
{
    return( reverse1(x, nil) );
}

sexpr ints(low, up)
int low, up;
{
    if (low > up) {
	return(nil);
    } else {
        return(small_cons(small_cons((sexpr)low, (sexpr)0), ints(low+1, up)));
    }
}

/* Too check uncollectable allocation we build lists with disguised cdr	*/
/* pointers, and make sure they don't go away.				*/
sexpr uncollectable_ints(low, up)
int low, up;
{
    if (low > up) {
	return(nil);
    } else {
        return(small_cons_uncollectable(small_cons((sexpr)low, (sexpr)0),
               uncollectable_ints(low+1, up)));
    }
}

void check_ints(list, low, up)
sexpr list;
int low, up;
{
    if ((int)(car(car(list))) != low) {
        (void)printf(
           "List reversal produced incorrect list - collector is broken\n");
        exit(1);
    }
    if (low == up) {
        if (cdr(list) != nil) {
           (void)printf("List too long - collector is broken\n");
           exit(1);
        }
    } else {
        check_ints(cdr(list), low+1, up);
    }
}

# define UNCOLLECTABLE_CDR(x) (sexpr)(~(unsigned long)(cdr(x)))

void check_uncollectable_ints(list, low, up)
sexpr list;
int low, up;
{
    if ((int)(car(car(list))) != low) {
        (void)printf(
           "Uncollectable list corrupted - collector is broken\n");
        exit(1);
    }
    if (low == up) {
        if (UNCOLLECTABLE_CDR(list) != nil) {
           (void)printf("Uncollectable ist too long - collector is broken\n");
           exit(1);
        }
    } else {
        check_uncollectable_ints(UNCOLLECTABLE_CDR(list), low+1, up);
    }
}

/* Not used, but useful for debugging: */
void print_int_list(x)
sexpr x;
{
    if (is_nil(x)) {
        (void)printf("NIL\n");
    } else {
        (void)printf("(%d)", car(car(x)));
        if (!is_nil(cdr(x))) {
            (void)printf(", ");
            (void)print_int_list(cdr(x));
        } else {
            (void)printf("\n");
        }
    }
}

/* Try to force a to be strangely aligned */
struct {
  char dummy;
  sexpr aa;
} A;
#define a A.aa

/*
 * Repeatedly reverse lists built out of very different sized cons cells.
 * Check that we didn't lose anything.
 */
reverse_test()
{
    int i;
    sexpr b;
    sexpr c;
    sexpr d;
    sexpr e;
#   define BIG 4500

    a = ints(1, 49);
    b = ints(1, 50);
    c = ints(1, BIG);
    d = uncollectable_ints(1, 100);
    e = uncollectable_ints(1, 1);
    /* Superficially test interior pointer recognition on stack */
    c = (sexpr)((char *)c + sizeof(char *));
    d = (sexpr)((char *)d + sizeof(char *));
#   ifdef __STDC__
        GC_FREE((void *)e);
#   else
        GC_FREE((char *)e);
#   endif
    for (i = 0; i < 50; i++) {
        b = reverse(reverse(b));
    }
    for (i = 0; i < 60; i++) {
    	/* This maintains the invariant that a always points to a list of */
    	/* 49 integers.  Thus this is thread safe without locks.	  */
        a = reverse(reverse(a));
#	if !defined(AT_END) && !defined(PCR)
	  /* This is not thread safe, since realloc explicitly deallocates */
          if (i & 1) {
            a = (sexpr)GC_REALLOC((void_star)a, 500);
          } else {
            a = (sexpr)GC_REALLOC((void_star)a, 8200);
          }
#	endif
    }
    check_ints(a,1,49);
    check_ints(b,1,50);
    c = (sexpr)((char *)c - sizeof(char *));
    d = (sexpr)((char *)d - sizeof(char *));
    check_ints(c,1,BIG);
    check_uncollectable_ints(d, 1, 100);
    a = b = c = 0;
}

/*
 * The rest of this builds balanced binary trees, checks that they don't
 * disappear, and tests finalization.
 */
typedef struct treenode {
    int level;
    struct treenode * lchild;
    struct treenode * rchild;
} tn;

int finalizable_count = 0;
int finalized_count = 0;
int dropped_something = 0;

# ifdef __STDC__
  void finalizer(void * obj, void * client_data)
# else
  void finalizer(obj, client_data)
  char * obj;
  char * client_data;
# endif
{
  tn * t = (tn *)obj;
  if ((int)client_data != t -> level) {
     (void)printf("Wrong finalization data - collector is broken\n");
     FAIL;
  }
  finalized_count++;
}

size_t counter = 0;

# define MAX_FINALIZED 8000
FAR GC_word live_indicators[MAX_FINALIZED] = {0};
int live_indicators_count = 0;

tn * mktree(n)
int n;
{
    tn * result = (tn *)GC_MALLOC(sizeof(tn));
    
    if (n == 0) return(0);
    if (result == 0) {
        (void)printf("Out of memory\n");
        exit(1);
    }
    result -> level = n;
    result -> lchild = mktree(n-1);
    result -> rchild = mktree(n-1);
    if (counter++ % 17 == 0 && n >= 2) {
        tn * tmp = result -> lchild -> rchild;
        
        result -> lchild -> rchild = result -> rchild -> lchild;
        result -> rchild -> lchild = tmp;
    }
    if (counter++ % 119 == 0) {
        GC_REGISTER_FINALIZER((void_star)result, finalizer, (void_star)n,
        		      (GC_finalization_proc *)0, (void_star *)0);
        live_indicators[live_indicators_count] = 13;
        if (GC_general_register_disappearing_link(
         	(void_star *)(&(live_indicators[live_indicators_count])),
         	(void_star)result) != 0) {
         	printf("GC_general_register_disappearing_link failed\n");
         	FAIL;
        }
        if (GC_unregister_disappearing_link(
         	(void_star *)
         	   (&(live_indicators[live_indicators_count]))) == 0) {
         	printf("GC_unregister_disappearing_link failed\n");
         	FAIL;
        }
        if (GC_general_register_disappearing_link(
         	(void_star *)(&(live_indicators[live_indicators_count])),
         	(void_star)result) != 0) {
         	printf("GC_general_register_disappearing_link failed 2\n");
         	FAIL;
        }
        live_indicators_count++;
#	ifdef PCR
 	    PCR_ThCrSec_EnterSys();
 	    /* Losing a count here causes erroneous report of failure. */
#	endif
        finalizable_count++;
#	ifdef PCR
 	    PCR_ThCrSec_ExitSys();
#	endif
    }
    return(result);
}

void chktree(t,n)
tn *t;
int n;
{
    if (n == 0 && t != 0) {
        (void)printf("Clobbered a leaf - collector is broken\n");
        FAIL;
    }
    if (n == 0) return;
    if (t -> level != n) {
        (void)printf("Lost a node at level %d - collector is broken\n", n);
        FAIL;
    }
    if (counter++ % 373 == 0) (void) GC_MALLOC(counter%5001);
    chktree(t -> lchild, n-1);
    if (counter++ % 73 == 0) (void) GC_MALLOC(counter%373);
    chktree(t -> rchild, n-1);
}

void alloc_small(n)
int n;
{
    register int i;
    
    for (i = 0; i < n; i += 8) {
        if (GC_MALLOC_ATOMIC(8) == 0) {
            (void)printf("Out of memory\n");
            FAIL;
        }
    }
}

tree_test()
{
    tn * root = mktree(16);
    register int i;
    
    alloc_small(5000000);
    chktree(root, 16);
    if (finalized_count && ! dropped_something) {
        (void)printf("Premature finalization - collector is broken\n");
        FAIL;
    }
    dropped_something = 1;
    root = mktree(16);
    chktree(root, 16);
    for (i = 16; i >= 0; i--) {
        root = mktree(i);
        chktree(root, i);
    }
    alloc_small(5000000);
}

# include "gc_private.h"

int n_tests = 0;

void run_one_test()
{
    DCL_LOCK_STATE;
    
#   ifndef GC_DEBUG
	if (GC_size(GC_MALLOC(7)) != 8
	    || GC_size(GC_MALLOC(15)) != 16) {
	    (void)printf ("GC_size produced unexpected results\n");
	    FAIL;
	}
#   endif
    reverse_test();
    tree_test();
    LOCK();
    n_tests++;
    UNLOCK();
    
}

void check_heap_stats()
{
    unsigned long max_heap_sz;
    register int i;
    int still_live;
    
    if (sizeof(char *) > 4) {
        max_heap_sz = 13000000;
    } else {
    	max_heap_sz = 10000000;
    }
#   ifdef GC_DEBUG
	max_heap_sz *= 2;
#   endif
    /* Garbage collect repeatedly so that all inaccessible objects	*/
    /* can be finalized.						*/
      for (i = 0; i < 16; i++) {
        GC_gcollect();
      }
    (void)printf("Completed %d tests\n", n_tests);
    (void)printf("Finalized %d/%d objects - ",
    		 finalized_count, finalizable_count);
    if (finalized_count > finalizable_count
        || finalized_count < finalizable_count/2) {
        (void)printf ("finalization is probably broken\n");
        FAIL;
    } else {
        (void)printf ("finalization is probably ok\n");
    }
    still_live = 0;
    for (i = 0; i < MAX_FINALIZED; i++) {
    	if (live_indicators[i] != 0) {
    	    still_live++;
    	}
    }
    if (still_live != finalizable_count - finalized_count) {
        (void)printf
            ("%d disappearing links remain - disappearing links are broken\n");
        FAIL;
    }
    (void)printf("Total number of bytes allocated is %d\n",
    	         WORDS_TO_BYTES(GC_words_allocd + GC_words_allocd_before_gc));
    (void)printf("Final heap size is %d bytes\n", GC_heapsize);
    if (WORDS_TO_BYTES(GC_words_allocd + GC_words_allocd_before_gc)
        < 33500000*n_tests) {
        (void)printf("Incorrect execution - missed some allocations\n");
        FAIL;
    }
    if (GC_heapsize > max_heap_sz*n_tests) {
        (void)printf("Unexpected heap growth - collector may be broken\n");
        FAIL;
    }
    (void)printf("Collector appears to work\n");
}

#ifndef PCR
main()
{
    n_tests = 0;
#   if defined(MPROTECT_VDB) || defined(PROC_VDB)
      GC_enable_incremental();
      (void) printf("Switched to incremental mode\n");
#     if defined(MPROTECT_VDB)
	(void)printf("Emulating dirty bits with mprotect/signals\n");
#     else
	(void)printf("Reading dirty bits from /proc\n");
#      endif
#   endif
    run_one_test();
    check_heap_stats();
    (void)fflush(stdout);
#   ifdef LINT
	/* Entry points we should be testing, but aren't */
	/* Some can be tested by defining GC_DEBUG at the top of this file */
	GC_noop(GC_expand_hp, GC_add_roots, GC_clear_roots,
	        GC_register_disappearing_link,
	        GC_print_obj, GC_debug_change_stubborn,
	        GC_debug_end_stubborn_change, GC_debug_malloc_uncollectable,
	        GC_debug_free, GC_debug_realloc, GC_generic_malloc_words_small,
	        GC_init, GC_make_closure, GC_debug_invoke_finalizer);
#   endif
    return(0);
}
# else
test()
{
    PCR_Th_T * th1;
    PCR_Th_T * th2;
    int code;

    n_tests = 0;
    GC_enable_incremental();
    th1 = PCR_Th_Fork(run_one_test, 0);
    th2 = PCR_Th_Fork(run_one_test, 0);
    run_one_test();
    if (PCR_Th_T_Join(th1, &code, NIL, PCR_allSigsBlocked, PCR_waitForever)
        != PCR_ERes_okay || code != 0) {
        (void)printf("Thread 1 failed\n");
    }
    if (PCR_Th_T_Join(th2, &code, NIL, PCR_allSigsBlocked, PCR_waitForever)
        != PCR_ERes_okay || code != 0) {
        (void)printf("Thread 2 failed\n");
    }
    check_heap_stats();
    (void)fflush(stdout);
    return(0);
}
#endif

