/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
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
/* Boehm, July 4, 1994 10:46 am PDT */
# include <stdio.h>
# include "gc_priv.h"

# ifdef PCR
#   define MAX_ROOT_SETS 1024
# else
#   ifdef MSWIN32
#	define MAX_ROOT_SETS 512
	    /* Under NT, we add only written pages, which can result 	*/
	    /* in many small root sets.					*/
#   else
#       define MAX_ROOT_SETS 64
#   endif
# endif

/* Data structure for list of root sets.				*/
/* We keep a hash table, so that we can filter out duplicate additions.	*/
/* Under Win32, we need to do a better job of filtering overlaps, so	*/
/* we resort to sequential search, and pay the price.			*/
struct roots {
	ptr_t r_start;
	ptr_t r_end;
#	ifndef MSWIN32
	  struct roots * r_next;
#	endif
};

static struct roots static_roots[MAX_ROOT_SETS];

static int n_root_sets = 0;

	/* static_roots[0..n_root_sets) contains the valid root sets. */

#ifndef MSWIN32
#   define LOG_RT_SIZE 6
#   define RT_SIZE (1 << LOG_RT_SIZE)  /* Power of 2, may be != MAX_ROOT_SETS */

    static struct roots * root_index[RT_SIZE];
	/* Hash table header.  Used only to check whether a range is 	*/
	/* already present.						*/

static int rt_hash(addr)
char * addr;
{
    word result = (word) addr;
#   if CPP_WORDSZ > 8*LOG_RT_SIZE
	result ^= result >> 8*LOG_RT_SIZE;
#   endif
#   if CPP_WORDSZ > 4*LOG_RT_SIZE
	result ^= result >> 4*LOG_RT_SIZE;
#   endif
    result ^= result >> 2*LOG_RT_SIZE;
    result ^= result >> LOG_RT_SIZE;
    result &= (RT_SIZE-1);
    return(result);
}

/* Is a range starting at b already in the table? If so return a	*/
/* pointer to it, else NIL.						*/
struct roots * GC_roots_present(b)
char *b;
{
    register int h = rt_hash(b);
    register struct roots *p = root_index[h];
    
    while (p != 0) {
        if (p -> r_start == (ptr_t)b) return(p);
        p = p -> r_next;
    }
    return(FALSE);
}

/* Add the given root structure to the index. */
static void add_roots_to_index(p)
struct roots *p;
{
    register int h = rt_hash(p -> r_start);
    
    p -> r_next = root_index[h];
    root_index[h] = p;
}

# else /* MSWIN32 */

#   define add_roots_to_index(p)

# endif




word GC_root_size = 0;

void GC_add_roots(b, e)
char * b; char * e;
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    GC_add_roots_inner(b, e);
    UNLOCK();
    ENABLE_SIGNALS();
}


/* Add [b,e) to the root set.  Adding the same interval a second time	*/
/* is a moderately fast noop, and hence benign.  We do not handle	*/
/* different but overlapping intervals efficiently.  (We do handle	*/
/* them correctly.)							*/
void GC_add_roots_inner(b, e)
char * b; char * e;
{
    struct roots * old;
    
    /* We exclude GC data structures from root sets.  It's usually safe	*/
    /* to mark from those, but it is a waste of time.			*/
    if ( (ptr_t)b < endGC_arrays && (ptr_t)e > beginGC_arrays) {
        if ((ptr_t)e <= endGC_arrays) {
            if ((ptr_t)b >= beginGC_arrays) return;
            e = (char *)beginGC_arrays;
        } else if ((ptr_t)b >= beginGC_arrays) {
            b = (char *)endGC_arrays;
        } else {
            GC_add_roots_inner(b, (char *)beginGC_arrays);
            GC_add_roots_inner((char *)endGC_arrays, e);
            return;
        }
    }
#   ifdef MSWIN32
      /* Spend the time to ensure that there are no overlapping	*/
      /* or adjacent intervals.					*/
      /* This could be done faster with e.g. a			*/
      /* balanced tree.  But the execution time here is		*/
      /* virtually guaranteed to be dominated by the time it	*/
      /* takes to scan the roots.				*/
      {
        register int i;
        
        for (i = 0; i < n_root_sets; i++) {
            old = static_roots + i;
            if ((ptr_t)b <= old -> r_end && (ptr_t)e >= old -> r_start) {
                if ((ptr_t)b < old -> r_start) {
                    old -> r_start = (ptr_t)b;
                }
                if ((ptr_t)e > old -> r_end) {
                    old -> r_end = (ptr_t)e;
                }
                break;
            }
        }
        if (i < n_root_sets) {
          /* merge other overlapping intervals */
            struct roots *other;
            
            for (i++; i < n_root_sets; i++) {
              other = static_roots + i;
              b = (char *)(other -> r_start);
              e = (char *)(other -> r_end);
              if ((ptr_t)b <= old -> r_end && (ptr_t)e >= old -> r_start) {
                if ((ptr_t)b < old -> r_start) {
                    old -> r_start = (ptr_t)b;
                }
                if ((ptr_t)e > old -> r_end) {
                    old -> r_end = (ptr_t)e;
                }
                /* Delete this entry. */
                  other -> r_start = static_roots[n_root_sets-1].r_start;
                  other -> r_end = static_roots[n_root_sets-1].r_end;
                  n_root_sets--;
              }
            }
          return;
        }
      }
#   else
      old = GC_roots_present(b);
      if (old != 0) {
        if ((ptr_t)e <= old -> r_end) /* already there */ return;
        /* else extend */
        GC_root_size += (ptr_t)e - old -> r_end;
        old -> r_end = (ptr_t)e;
        return;
      }
#   endif
    if (n_root_sets == MAX_ROOT_SETS) {
        ABORT("Too many root sets\n");
    }
    static_roots[n_root_sets].r_start = (ptr_t)b;
    static_roots[n_root_sets].r_end = (ptr_t)e;
#   ifndef MSWIN32
      static_roots[n_root_sets].r_next = 0;
#   endif
    add_roots_to_index(static_roots + n_root_sets);
    GC_root_size += (ptr_t)e - (ptr_t)b;
    n_root_sets++;
}

void GC_clear_roots(NO_PARAMS)
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    n_root_sets = 0;
    GC_root_size = 0;
    UNLOCK();
    ENABLE_SIGNALS();
}

ptr_t GC_approx_sp()
{
    word dummy;
    
    return((ptr_t)(&dummy));
}

/*
 * Call the mark routines (GC_tl_push for a single pointer, GC_push_conditional
 * on groups of pointers) on every top level accessible pointer.
 * If all is FALSE, arrange to push only possibly altered values.
 */

void GC_push_roots(all)
bool all;
{
    register int i;

    /*
     * push registers - i.e., call GC_push_one(r) for each
     * register contents r.
     */
        GC_push_regs(); /* usually defined in machine_dep.c */
        
    /*
     * Next push static data.  This must happen early on, since it's
     * not robust against mark stack overflow.
     */
     /* Reregister dynamic libraries, in case one got added.	*/
#      if (defined(DYNAMIC_LOADING) || defined(MSWIN32) || defined(PCR)) \
           && !defined(SRC_M3)
         GC_register_dynamic_libraries();
#      endif
     /* Mark everything in static data areas                             */
       for (i = 0; i < n_root_sets; i++) {
         GC_push_conditional(static_roots[i].r_start,
			     static_roots[i].r_end, all);
       }

    /*
     * Now traverse stacks.
     */
#   ifndef THREADS
        /* Mark everything on the stack.           */
#   	  ifdef STACK_GROWS_DOWN
	    GC_push_all_stack( GC_approx_sp(), GC_stackbottom );
#	  else
	    GC_push_all_stack( GC_stackbottom, GC_approx_sp() );
#	  endif
#   endif
    if (GC_push_other_roots != 0) (*GC_push_other_roots)();
    	/* In the threads case, this also pushes thread stacks.	*/
}

