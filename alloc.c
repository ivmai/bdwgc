/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 *
 * This file contains the functions:
 *	static void clear_marks()
 *	bool GC_gcollect_inner(force)
 *	void GC_gcollect()
 *	bool GC_expand_hp(n)
 *	ptr_t GC_allocobj(sz, kind)
 */


# include <stdio.h>
# include <signal.h>
# include <sys/types.h>
# include "gc_private.h"

/*
 * This is a garbage collecting storage allocator
 * that should run on most UNIX systems.  The garbage
 * collector is overly conservative in that it may fail to GC_reclaim
 * inaccessible storage.  On the other hand, it does not assume
 * any runtime tag information.
 * We make the following assumptions:
 *  1.  We are running under something that looks like Berkeley UNIX,
 *      on one of the supported architectures.
 *  2.  For every accessible object, a pointer to it is stored in
 *          a) the stack segment, or
 *          b) the data or bss segment, or
 *          c) the registers, or
 *          d) an accessible block.
 *
 */

/*
 * Separate free lists are maintained for different sized objects
 * up to MAXOBJSZ.
 * The call GC_allocobj(i,k) ensures that the freelist for
 * kind k objects of size i points to a non-empty
 * free list. It returns a pointer to the first entry on the free list.
 * In a single-threaded world, GC_allocobj may be called to allocate
 * an object of (small) size i as follows:
 *
 *            opp = &(GC_objfreelist[i]);
 *            if (*opp == 0) GC_allocobj(i, NORMAL);
 *            ptr = *opp;
 *            *opp = ptr->next;
 *
 * Note that this is very fast if the free list is non-empty; it should
 * only involve the execution of 4 or 5 simple instructions.
 * All composite objects on freelists are cleared, except for
 * their first word.
 */

/*
 *  The allocator uses GC_allochblk to allocate large chunks of objects.
 * These chunks all start on addresses which are multiples of
 * HBLKSZ.   Each allocated chunk has an associated header,
 * which can be located quickly based on the address of the chunk.
 * (See headers.c for details.) 
 * This makes it possible to check quickly whether an
 * arbitrary address corresponds to an object administered by the
 * allocator.
 */

word GC_non_gc_bytes = 0;  /* Number of bytes not intended to be collected */

word GC_gc_no = 0;

char * GC_copyright[] =
{"Copyright 1988,1989 Hans-J. Boehm and Alan J. Demers",
"Copyright (c) 1991,1992 by Xerox Corporation.  All rights reserved.",
"THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY",
" EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK."};


/* some more variables */

extern signed_word GC_mem_found;  /* Number of reclaimed longwords	*/
				  /* after garbage collection      	*/

/* clear all mark bits in the header */
void GC_clear_hdr_marks(hhdr)
register hdr * hhdr;
{
    bzero((char *)hhdr -> hb_marks, (int)(MARK_BITS_SZ*sizeof(word)));
}

/*
 * Clear all mark bits associated with block h.
 */
/*ARGSUSED*/
static void clear_marks_for_block(h, dummy)
struct hblk *h;
word dummy;
{
    register hdr * hhdr = HDR(h);
    
    GC_clear_hdr_marks(hhdr);
}

/*
 * Clear mark bits in all allocated heap blocks
 */
static void clear_marks()
{
    GC_apply_to_all_blocks(clear_marks_for_block, (word)0);
}

bool GC_dont_expand = 0;

word GC_free_space_divisor = 4;

/* Return the minimum number of words that must be allocated between	*/
/* collections to amortize the collection cost.				*/
static word min_words_allocd()
{
    int dummy;
#   ifdef THREADS
 	/* We punt, for now. */
 	register signed_word stack_size = 10000;
#   else
        register signed_word stack_size = (ptr_t)(&dummy) - GC_stackbottom;
#   endif
    register word total_root_size;  /* includes double stack size,	*/
    				    /* since the stack is expensive	*/
    				    /* to scan.				*/
    
    if (stack_size < 0) stack_size = -stack_size;
    total_root_size = 2 * stack_size + GC_root_size;
    return(BYTES_TO_WORDS(GC_heapsize + total_root_size)/GC_free_space_divisor);
}

/* Return the number of words allocated, adjusted for explicit storage	*/
/* management.  This number can be used in deciding when to trigger	*/
/* collections.								*/
word GC_adj_words_allocd()
{
    register signed_word result;
    register signed_word expl_managed =
    		BYTES_TO_WORDS((long)GC_non_gc_bytes
    				- (long)GC_non_gc_bytes_at_gc);
    
    /* Don't count what was explicitly freed, or newly allocated for	*/
    /* explicit management.  Note that deallocating an explicitly	*/
    /* managed object should not alter result, assuming the client	*/
    /* is playing by the rules.						*/
    result = (signed_word)GC_words_allocd
    	     - (signed_word)GC_mem_freed - expl_managed;
    if (result > (signed_word)GC_words_allocd) result = GC_words_allocd;
    	/* probably client bug or unfortunate scheduling */
    if (result < (signed_word)(GC_words_allocd >> 2)) {
    	/* Always count at least 1/8 of the allocations.  We don't want	*/
    	/* to collect too infrequently, since that would inhibit	*/
    	/* coalescing of free storage blocks.				*/
    	/* This also makes us partially robust against client bugs.	*/
        return(GC_words_allocd >> 3);
    } else {
        return(result);
    }
}


/* Clear up a few frames worth og garbage left at the top of the stack.	*/
/* This is used to prevent us from accidentally treating garbade left	*/
/* on the stack by other parts of the collector as roots.  This 	*/
/* differs from the code in misc.c, which actually tries to keep the	*/
/* stack clear of long-lived, client-generated garbage.			*/
void GC_clear_a_few_frames()
{
#   define NWORDS 64
    word frames[NWORDS];
    register int i;
    
    for (i = 0; i < NWORDS; i++) frames[i] = 0;
}

/*
 * Restore inaccessible objects to the free list 
 * update GC_mem_found (number of reclaimed longwords after
 * garbage collection)
 * We assume we hold the allocation lock, and are not interruptable by
 * signals, if that matters.
 * If force is FALSE and we didn't do anything, return FALSE.
 * Otherwise return TRUE
 */
bool GC_gcollect_inner(force)
bool force;	/* Collect even if only a small amount of allocation	*/
		/* has taken place.  Otherwise we refuse, allowing the  */
		/* heap to grow.					*/
{
#   ifdef PRINTTIMES
	CLOCK_TYPE start_time;
	CLOCK_TYPE mark_time;
	CLOCK_TYPE done_time;
#   endif

    if (!force && !GC_dont_expand
        && GC_adj_words_allocd() < min_words_allocd()) return(FALSE);
#   ifdef PRINTTIMES
	GET_TIME(start_time);
#   endif
#   ifdef PRINTSTATS
	GC_printf2("Collection %lu reclaimed %ld bytes\n",
		  (unsigned long) GC_gc_no,
	   	  (long)WORDS_TO_BYTES(GC_mem_found));
#   endif
    GC_gc_no++;
#   ifdef GATHERSTATS
        GC_mem_found = 0;
        GC_composite_in_use = 0;
        GC_atomic_in_use = 0;
#   endif
#   ifdef PRINTSTATS
      GC_printf2("Collection number %lu after %lu allocated bytes ",
      	        (unsigned long) GC_gc_no,
      	        (unsigned long) WORDS_TO_BYTES(GC_words_allocd));
      GC_printf1("(heapsize = %lu bytes)\n",
      	        (unsigned long) GC_heapsize);
      /* Printf arguments may be pushed in funny places.  Clear the	*/
      /* space.								*/
      GC_printf0("");
#   endif      	        

    clear_marks();

    STOP_WORLD();

    /* Mark from all roots.  */
        /* Minimize junk left in my registers and on the stack */
            GC_clear_a_few_frames();
            GC_noop(0,0,0,0,0,0);
	GC_mark_roots();
	GC_promote_black_lists();

    /* Check all debugged objects for consistency */
        if (GC_debugging_started) {
            GC_check_heap();
        }
	
    START_WORLD();
    
#   ifdef PRINTTIMES
	GET_TIME(mark_time);
#   endif


#   ifdef FIND_LEAK
      /* Mark all objects on the free list.  All objects should be */
      /* marked when we're done.				   */
	{
	  register word size;		/* current object size		*/
	  register ptr_t p;	/* pointer to current object	*/
	  register struct hblk * h;	/* pointer to block containing *p */
	  register hdr * hhdr;
	  register int word_no;           /* "index" of *p in *q          */
	  int kind;

	  for (kind = 0; kind < GC_n_kinds; kind++) {
	    for (size = 1; size <= MAXOBJSZ; size++) {
	      for (p= GC_obj_kinds[kind].ok_freelist[size];
	           p != 0; p=obj_link(p)){
		h = HBLKPTR(p);
		hhdr = HDR(h);
		word_no = (((word *)p) - ((word *)h));
		set_mark_bit_from_hdr(hhdr, word_no);
	      }
	    }
	  }
	}
      /* Check that everything is marked */
	GC_start_reclaim(TRUE);
#   else

      GC_finalize();

      /* Clear free list mark bits, in case they got accidentally marked   */
      /* Note: HBLKPTR(p) == pointer to head of block containing *p        */
      /* Also subtract memory remaining from GC_mem_found count.           */
      /* Note that composite objects on free list are cleared.             */
      /* Thus accidentally marking a free list is not a problem;  only     */
      /* objects on the list itself will be marked, and that's fixed here. */
      {
	register word size;		/* current object size		*/
	register ptr_t p;	/* pointer to current object	*/
	register struct hblk * h;	/* pointer to block containing *p */
	register hdr * hhdr;
	register int word_no;           /* "index" of *p in *q          */
	int kind;

	for (kind = 0; kind < GC_n_kinds; kind++) {
	  for (size = 1; size <= MAXOBJSZ; size++) {
	    for (p= GC_obj_kinds[kind].ok_freelist[size];
	         p != 0; p=obj_link(p)){
		h = HBLKPTR(p);
		hhdr = HDR(h);
		word_no = (((word *)p) - ((word *)h));
		clear_mark_bit_from_hdr(hhdr, word_no);
		GC_mem_found -= size;
	    }
	  }
	}
      }


#     ifdef PRINTSTATS
	GC_printf1("Bytes recovered before GC_reclaim - f.l. count = %ld\n",
	          (long)WORDS_TO_BYTES(GC_mem_found));
#     endif

    /* Reconstruct free lists to contain everything not marked */
      GC_start_reclaim(FALSE);
    
#   endif /* FIND_LEAK */

#   ifdef PRINTSTATS
	GC_printf2(
		  "Immediately reclaimed %ld bytes in heap of size %lu bytes\n",
	          (long)WORDS_TO_BYTES(GC_mem_found),
	          (unsigned long)GC_heapsize);
	GC_printf2("%lu (atomic) + %lu (composite) bytes in use\n",
	           (unsigned long)WORDS_TO_BYTES(GC_atomic_in_use),
	           (unsigned long)WORDS_TO_BYTES(GC_composite_in_use));
#   endif

    /* Reset or increment counters for next cycle */
      GC_words_allocd_before_gc += GC_words_allocd;
      GC_non_gc_bytes_at_gc = GC_non_gc_bytes;
      GC_words_allocd = 0;
      GC_mem_freed = 0;

  /* Get final time */
#   ifdef PRINTTIMES
	GET_TIME(done_time);
	GC_printf2("Garbage collection took %lu + %lu msecs\n",
	           MS_TIME_DIFF(mark_time,start_time),
	           MS_TIME_DIFF(done_time,mark_time));
#   endif
    return(TRUE);
}

/* Externally callable version of above */
void GC_gcollect()
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    if (!GC_is_initialized) GC_init_inner();
    /* Minimize junk left in my registers */
      GC_noop(0,0,0,0,0,0);
    (void) GC_gcollect_inner(TRUE);
    UNLOCK();
    ENABLE_SIGNALS();
}

/*
 * Use the chunk of memory starting at p of syze bytes as part of the heap.
 * Assumes p is HBLKSIZE aligned, and bytes is a multiple of HBLKSIZE.
 */
void GC_add_to_heap(p, bytes)
struct hblk *p;
word bytes;
{
    word words;
    
    GC_install_header(p);
    words = BYTES_TO_WORDS(bytes - HDR_BYTES);
    HDR(p) -> hb_sz = words;
    GC_freehblk(p);
    GC_heapsize += bytes;
    if ((ptr_t)p <= GC_least_plausible_heap_addr
        || GC_least_plausible_heap_addr == 0) {
        GC_least_plausible_heap_addr = (ptr_t)p - sizeof(word);
        	/* Making it a little smaller than necessary prevents	*/
        	/* us from getting a false hit from the variable	*/
        	/* itself.  There's some unintentional reflection	*/
        	/* here.						*/
    }
    if ((ptr_t)p + bytes >= GC_greatest_plausible_heap_addr) {
        GC_greatest_plausible_heap_addr = (ptr_t)p + bytes;
    }
}

ptr_t GC_least_plausible_heap_addr = (ptr_t)ONES;
ptr_t GC_greatest_plausible_heap_addr = 0;

ptr_t GC_max(x,y)
ptr_t x, y;
{
    return(x > y? x : y);
}

ptr_t GC_min(x,y)
ptr_t x, y;
{
    return(x < y? x : y);
}

/*
 * this explicitly increases the size of the heap.  It is used
 * internally, but my also be invoked from GC_expand_hp by the user.
 * The argument is in units of HBLKSIZE.
 * Returns FALSE on failure.
 */
bool GC_expand_hp_inner(n)
word n;
{
    word bytes = n * HBLKSIZE;
    struct hblk * space = GET_MEM(bytes);
    word expansion_slop;	/* Number of bytes by which we expect the */
    				/* heap to expand soon.			  */

    if (n > 2*GC_hincr) {
	GC_hincr = n/2;
    }
    if( space == 0 ) {
	return(FALSE);
    }
#   ifdef PRINTSTATS
	GC_printf1("Increasing heap size by %lu\n",
	           (unsigned long)bytes);
#   endif
    expansion_slop = 8 * WORDS_TO_BYTES(min_words_allocd());
    if (5 * HBLKSIZE * MAXHINCR > expansion_slop) {
        expansion_slop = 5 * HBLKSIZE * MAXHINCR;
    }
    if (GC_last_heap_addr == 0 && !((word)space & SIGNB)
        || GC_last_heap_addr != 0 && GC_last_heap_addr < (ptr_t)space) {
        /* Assume the heap is growing up */
        GC_greatest_plausible_heap_addr =
            GC_max(GC_greatest_plausible_heap_addr,
                   (ptr_t)space + bytes + expansion_slop);
    } else {
        /* Heap is growing down */
        GC_least_plausible_heap_addr =
            GC_min(GC_least_plausible_heap_addr,
                   (ptr_t)space - expansion_slop);
    }
    GC_prev_heap_addr = GC_last_heap_addr;
    GC_last_heap_addr = (ptr_t)space;
    GC_add_to_heap(space, bytes);
    return(TRUE);
}

/* Really returns a bool, but it's externally visible, so that's clumsy. */
int GC_expand_hp(n)
int n;
{
    int result;
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    if (!GC_is_initialized) GC_init_inner();
    result = (int)GC_expand_hp_inner((word)n);
    UNLOCK();
    ENABLE_SIGNALS();
    return(result);
}

void GC_collect_or_expand(needed_blocks)
word needed_blocks;
{
    static int count = 0;  /* How many failures? */
    
    if (GC_dont_gc || !GC_gcollect_inner(FALSE)) {
      if (!GC_expand_hp_inner(GC_hincr + needed_blocks)
        && !GC_expand_hp_inner(needed_blocks)) {
      	if (count++ < 20) {
      	    WARN("Out of Memory!  Trying to continue ...\n");
	    (void) GC_gcollect_inner(TRUE);
	} else {
	    GC_err_printf0("Out of Memory!  Giving up!\n");
	    EXIT();
	}
      }
      update_GC_hincr;
    }
}

/*
 * Make sure the object free list for sz is not empty.
 * Return a pointer to the first object on the free list.
 * The object MUST BE REMOVED FROM THE FREE LIST BY THE CALLER.
 *
 */
ptr_t GC_allocobj(sz, kind)
word sz;
int kind;
{
    register ptr_t * flh = &(GC_obj_kinds[kind].ok_freelist[sz]);
    
    if (sz == 0) return(0);

    while (*flh == 0) {
      GC_continue_reclaim(sz, kind);
      if (*flh == 0) {
        GC_new_hblk(sz, kind);
      }
      if (*flh == 0) {
        GC_collect_or_expand((word)1);
      }
    }
    return(*flh);
}
