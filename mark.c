
/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991,1992 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 *
 */


# include <stdio.h>
# include "gc_private.h"

# define INITIAL_MARK_STACK_SIZE (1*HBLKSIZE)
		/* INITIAL_MARK_STACK_SIZE * sizeof(mse) should be a 	*/
		/* multiple of HBLKSIZE.				*/

/*
 * Limits of stack for GC_mark routine.
 * All ranges between GC_mark_stack(incl.) and GC_mark_stack_top(incl.) still
 * need to be marked from.
 */

word GC_n_rescuing_pages;	/* Number of dirty pages we marked from */
				/* excludes ptrfree pages, etc.		*/

mse * GC_mark_stack;

word GC_mark_stack_size = 0;
 
mse * GC_mark_stack_top;

static struct hblk * scan_ptr;


typedef int mark_state_t;	/* Current state of marking, as follows:*/
				/* Used to remember where we are during */
				/* concurrent marking.			*/

				/* We say something is dirty if it was	*/
				/* written since the last time we	*/
				/* retrieved dirty bits.  We say it's 	*/
				/* grungy if it was marked dirty in the	*/
				/* last set of bits we retrieved.	*/
				
				/* Invariant I: all roots and marked	*/
				/* objects p are either dirty, or point */
				/* objects q that are either marked or	*/
				/* a pointer to q appears in a range	*/
				/* on the mark stack.			*/

# define MS_NONE 0		/* No marking in progress. I holds.	*/
				/* Mark stack is empty.			*/

# define MS_PUSH_RESCUERS 1	/* Rescuing objects are currently 	*/
				/* being pushed.  I holds, except	*/
				/* that grungy roots may point to 	*/
				/* unmarked objects, as may marked	*/
				/* grungy objects above scan_ptr.	*/

# define MS_PUSH_UNCOLLECTABLE 2
				/* I holds, except that marked 		*/
				/* uncollectable objects above scan_ptr */
				/* may point to unmarked objects.	*/
				/* Roots may point to unmarked objects	*/

# define MS_ROOTS_PUSHED 3	/* I holds, mark stack may be nonempty  */

# define MS_PARTIALLY_INVALID 4	/* I may not hold, e.g. because of M.S. */
				/* overflow.  However marked heap	*/
				/* objects below scan_ptr point to	*/
				/* marked or stacked objects.		*/

# define MS_INVALID 5		/* I may not hold.			*/

mark_state_t GC_mark_state = MS_NONE;

bool GC_objects_are_marked = FALSE;	/* Are there collectable marked	*/
					/* objects in the heap?		*/

bool GC_collection_in_progress()
{
    return(GC_mark_state != MS_NONE);
}

/* clear all mark bits in the header */
void GC_clear_hdr_marks(hhdr)
register hdr * hhdr;
{
    bzero((char *)(hhdr -> hb_marks), (int)(MARK_BITS_SZ*sizeof(word)));
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
    
    if (hhdr -> hb_obj_kind == UNCOLLECTABLE) return;
        /* Mark bit for these is cleared only once the object is 	*/
        /* explicitly deallocated.  This either frees the block, or	*/
        /* the bit is cleared once the object is on the free list.	*/
    GC_clear_hdr_marks(hhdr);
}

/* Slow but general routines for setting/clearing/asking about mark bits */
void GC_set_mark_bit(p)
ptr_t p;
{
    register struct hblk *h = HBLKPTR(p);
    register hdr * hhdr = HDR(h);
    register int word_no = (word *)p - (word *)h;
    
    set_mark_bit_from_hdr(hhdr, word_no);
}

void GC_clear_mark_bit(p)
ptr_t p;
{
    register struct hblk *h = HBLKPTR(p);
    register hdr * hhdr = HDR(h);
    register int word_no = (word *)p - (word *)h;
    
    clear_mark_bit_from_hdr(hhdr, word_no);
}

bool GC_is_marked(p)
ptr_t p;
{
    register struct hblk *h = HBLKPTR(p);
    register hdr * hhdr = HDR(h);
    register int word_no = (word *)p - (word *)h;
    
    return(mark_bit_from_hdr(hhdr, word_no));
}


/*
 * Clear mark bits in all allocated heap blocks.  This invalidates
 * the marker invariant, and sets GC_mark_state to reflect this.
 * (This implicitly starts marking to reestablish the
 */
void GC_clear_marks()
{
    GC_apply_to_all_blocks(clear_marks_for_block, (word)0);
    GC_objects_are_marked = FALSE;
    GC_mark_state = MS_INVALID;
    scan_ptr = 0;
#   ifdef GATHERSTATS
	/* Counters reflect currently marked objects: reset here */
        GC_composite_in_use = 0;
        GC_atomic_in_use = 0;
#   endif

}


/*
 * Push some dummy entries onto bottom of mark stack to allow
 * marker to operate in bigger chunks between bounds checks.
 * This is a pretty extreme performance hack ...
 */
void GC_prime_marker()
{
    register int i;
    static word dummy = 0;

    for (i = 0; i < INITIAL_MARK_STACK_SIZE/64; i++) {
        GC_push_all((ptr_t)(&dummy), (ptr_t)(&dummy + 1));
    }
}
			
/* Initiate full marking.	*/
void GC_initiate_full()
{
#   ifdef PRINTSTATS
	GC_printf2("Full mark for collection %lu after %ld allocd bytes\n",
		  (unsigned long) GC_gc_no+1,
	   	  (long)WORDS_TO_BYTES(GC_words_allocd));
#   endif
    GC_promote_black_lists();
    GC_reclaim_or_delete_all();
    GC_clear_marks();
    GC_read_dirty();
#   ifdef STUBBORN_ALLOC
    	GC_read_changed();
#   endif
#   ifdef CHECKSUMS
	{
	    extern void GC_check_dirty();
	    
	    GC_check_dirty();
	}
#   endif
#   ifdef GATHERSTATS
	GC_n_rescuing_pages = 0;
#   endif
    GC_prime_marker();
}

/* Initiate partial marking.	*/
/*ARGSUSED*/
void GC_initiate_partial(gc_no)
word gc_no;
{
#   ifdef PRINTSTATS
      if (gc_no > GC_gc_no) {
	GC_printf2("Partial mark for collection %lu after %ld allocd bytes\n",
		  (unsigned long) gc_no,
	   	  (long)WORDS_TO_BYTES(GC_words_allocd));
       }  /* else the world is stopped, and we just printed this */
#   endif
    if (GC_incremental) GC_read_dirty();
#   ifdef STUBBORN_ALLOC
    	GC_read_changed();
#   endif
#   ifdef CHECKSUMS
	{
	    extern void GC_check_dirty();
	    
	    if (GC_incremental) GC_check_dirty();
	}
#   endif
#   ifdef GATHERSTATS
	GC_n_rescuing_pages = 0;
#   endif
    if (GC_mark_state == MS_NONE) {
        GC_mark_state = MS_PUSH_RESCUERS;
    } else if (GC_mark_state != MS_INVALID) {
    	ABORT("unexpected state");
    } /* else this is really a full collection, and mark	*/
      /* bits are invalid.					*/
    scan_ptr = 0;
    GC_prime_marker();
}


static void alloc_mark_stack();

/* Perform a small amount of marking.			*/
/* We try to touch roughly a page of memory.		*/
/* Return TRUE if we just finished a mark phase.	*/
bool GC_mark_some()
{
    switch(GC_mark_state) {
    	case MS_NONE:
    	    return(FALSE);
    	    
    	case MS_PUSH_RESCUERS:
    	    if (GC_mark_stack_top
    	        >= GC_mark_stack + INITIAL_MARK_STACK_SIZE/4) {
    	        GC_mark_from_mark_stack();
    	        return(FALSE);
    	    } else {
    	        scan_ptr = GC_push_next_marked_dirty(scan_ptr);
    	        if (scan_ptr == 0) {
#		    ifdef PRINTSTATS
			GC_printf1("Marked from %lu dirty pages\n",
				   (unsigned long)GC_n_rescuing_pages);
#		    endif
    	    	    GC_push_roots(FALSE);
    	    	    if (GC_mark_state != MS_INVALID) {
    	    	        GC_mark_state = MS_ROOTS_PUSHED;
    	    	    }
    	    	}
    	    }
    	    return(FALSE);
    	
    	case MS_PUSH_UNCOLLECTABLE:
    	    if (GC_mark_stack_top
    	        >= GC_mark_stack + INITIAL_MARK_STACK_SIZE/4) {
    	        GC_mark_from_mark_stack();
    	        return(FALSE);
    	    } else {
    	        scan_ptr = GC_push_next_marked_uncollectable(scan_ptr);
    	        if (scan_ptr == 0) {
    	    	    GC_push_roots(TRUE);
    	    	    if (GC_mark_state != MS_INVALID) {
    	    	        GC_mark_state = MS_ROOTS_PUSHED;
    	    	    }
    	    	}
    	    }
    	    return(FALSE);
    	
    	case MS_ROOTS_PUSHED:
    	    if (GC_mark_stack_top >= GC_mark_stack) {
    	        GC_mark_from_mark_stack();
    	        return(FALSE);
    	    } else {
    	        GC_mark_state = MS_NONE;
    	        return(TRUE);
    	    }
    	    
    	case MS_INVALID:
    	case MS_PARTIALLY_INVALID:
	    if (!GC_objects_are_marked) {
		GC_mark_state = MS_PUSH_UNCOLLECTABLE;
		return(FALSE);
	    }
    	    if (GC_mark_stack_top >= GC_mark_stack) {
    	        GC_mark_from_mark_stack();
    	        return(FALSE);
    	    }
    	    if (scan_ptr == 0 && GC_mark_state == MS_INVALID) {
    	        alloc_mark_stack(2*GC_mark_stack_size);
#		ifdef PRINTSTATS
		    GC_printf1("Grew mark stack to %lu frames\n",
		    	       (unsigned long) GC_mark_stack_size);
#		endif
		GC_mark_state = MS_PARTIALLY_INVALID;
    	    }
    	    scan_ptr = GC_push_next_marked(scan_ptr);
    	    if (scan_ptr == 0 && GC_mark_state == MS_PARTIALLY_INVALID) {
    	    	GC_push_roots(TRUE);
    	    	if (GC_mark_state != MS_INVALID) {
    	    	    GC_mark_state = MS_ROOTS_PUSHED;
    	    	}
    	    }
    	    return(FALSE);
    	default:
    	    ABORT("GC_mark_some: bad state");
    	    return(FALSE);
    }
}

/* Mark procedure for objects that may contain arbitrary pointers.	*/
/* Msp is the current mark stack pointer. Msl limits the stack.		*/
/* We return the new stack pointer value.				*/
/* The object at addr has already been marked.  Our job is to make	*/
/* sure that its descendents are marked.				*/
mse * GC_normal_mark_proc(addr, hhdr, msp, msl)
register word * addr;
register hdr * hhdr;
register mse * msp, * msl;
{
    register word sz = hhdr -> hb_sz;
    
    msp++;
    /* Push the contents of the object on the mark stack. */
        if (msp >= msl) {
            GC_mark_state = MS_INVALID;
#	    ifdef PRINTSTATS
              GC_printf1("Mark stack overflow; current size = %lu entries\n",
	    	         GC_mark_stack_size);
#	    endif
	    return(msp-INITIAL_MARK_STACK_SIZE/8);
	}
        msp -> mse_start = addr;
        msp -> mse_end = addr + sz;
#   ifdef GATHERSTATS
	GC_composite_in_use += sz;
#   endif
    return(msp);
}

/* Mark procedure for objects that are known to contain no pointers.	*/
/*ARGSUSED*/
mse * GC_no_mark_proc(addr, hhdr, msp, msl)
register word * addr;
register hdr * hhdr;
register mse * msp, * msl;
{
#   ifdef GATHERSTATS
	GC_atomic_in_use += hhdr -> hb_sz;
#   endif
    return(msp);
}


bool GC_mark_stack_empty()
{
    return(GC_mark_stack_top < GC_mark_stack);
}	

/*
 * Mark objects pointed to by the regions described by
 * mark stack entries between GC_mark_stack and GC_mark_stack_top,
 * inclusive.  Assumes the upper limit of a mark stack entry
 * is never 0.  A mark stack entry never has size 0.
 * We try to traverse on the order of a hblk of memory before we return.
 * Caller is responsible for calling this until the mark stack is empty.
 */
void GC_mark_from_mark_stack()
{
  mse * GC_mark_stack_reg = GC_mark_stack;
  mse * GC_mark_stack_top_reg = GC_mark_stack_top;
  mse * mark_stack_limit = &(GC_mark_stack[GC_mark_stack_size]);
  int credit = HBLKSIZE;	/* Remaining credit for marking work	*/
  register int safe_credit;     /* Amount of credit we can safely use	*/
  				/* before checking stack bounds.	*/
  				/* Gross hack to safe a couple of	*/
  				/* instructions in the loop.		*/
  register word * current_p;	/* Pointer to current candidate ptr.	*/
  register word current;	/* Candidate pointer.			*/
  register word * limit;	/* (Incl) limit of current candidate 	*/
  				/* range				*/
  register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
  register ptr_t least_ha = GC_least_plausible_heap_addr;
# define SPLIT_RANGE_WORDS 128

  GC_objects_are_marked = TRUE;
  while (GC_mark_stack_top_reg >= GC_mark_stack_reg && credit > 0) {
   safe_credit = WORDS_TO_BYTES(GC_mark_stack_top_reg - GC_mark_stack_reg + 1);
   if (safe_credit > credit) {
       safe_credit = credit;
       credit = 0;
   } else {
       credit -= safe_credit;
   }
   while (safe_credit > 0) {
    /* Stack must be nonempty */
    register int displ;  /* Displacement in block; first bytes, then words */
    register hdr * hhdr;
    register map_entry_type map_entry;

    current_p = GC_mark_stack_top_reg -> mse_start;
    limit = GC_mark_stack_top_reg -> mse_end;
    safe_credit -= (ptr_t)limit - (ptr_t)current_p;
    if (limit - current_p > SPLIT_RANGE_WORDS) {
      /* Process part of the range to avoid pushing too much on the	*/
      /* stack.								*/
         GC_mark_stack_top_reg -> mse_start =
         	limit = current_p + SPLIT_RANGE_WORDS;
      /* Make sure that pointers overlapping the two ranges are		*/
      /* considered. 							*/
         limit += sizeof(word) - ALIGNMENT;
    } else {
      GC_mark_stack_top_reg--;
    }
    limit -= 1;
    
    while (current_p <= limit) {
      current = *current_p;
      current_p = (word *)((char *)current_p + ALIGNMENT);
      
      if ((ptr_t)current < least_ha) continue;
      if ((ptr_t)current >= greatest_ha) continue;
      GET_HDR(current,hhdr);
      if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
#	ifdef ALL_INTERIOR_POINTERS
	  if (hhdr != 0) {
	    register word orig = current;
	    
	    current = (word)HBLKPTR(current) + HDR_BYTES;
	    do {
	      current = current - HBLKSIZE*(int)hhdr;
	      hhdr = HDR(current);
	    } while(IS_FORWARDING_ADDR_OR_NIL(hhdr));
	    /* current points to the start of the large object */
	    if ((word *)orig - (word *)current
	         >= hhdr->hb_sz) {
	        /* Pointer past the end of the block */
	        GC_ADD_TO_BLACK_LIST_NORMAL(current);
            	continue;
	    }
	  } else {
	    GC_ADD_TO_BLACK_LIST_NORMAL(current);
            continue;
          }
#	else
          GC_ADD_TO_BLACK_LIST_NORMAL(current);
          continue;
#	endif         
      }
      displ = HBLKDISPL(current);
      map_entry = MAP_ENTRY((hhdr -> hb_map), displ);
      if (map_entry == OBJ_INVALID) {
          GC_ADD_TO_BLACK_LIST_NORMAL(current);
          continue;
      }
      displ = BYTES_TO_WORDS(displ);
      displ -= map_entry;

      {
          register word * mark_word_addr = hhdr -> hb_marks + divWORDSZ(displ);
          register word mark_word = *mark_word_addr;
          register word mark_bit = (word)1 << modWORDSZ(displ);
          
          if (mark_word & mark_bit) {
	      /* Mark bit is already set */
	      continue;
          }
          *mark_word_addr = mark_word | mark_bit;
      }
    
      GC_mark_stack_top_reg =
          (* (hhdr -> hb_mark_proc))((word *)(HBLKPTR(current)) + displ, hhdr,
          			     GC_mark_stack_top_reg, mark_stack_limit);
    }
   }
  }
  GC_mark_stack_top = GC_mark_stack_top_reg;
}

/* Allocate or reallocate space for mark stack of size s words  */
/* May silently fail.						*/
static void alloc_mark_stack(n)
word n;
{
    mse * new_stack = (mse *)GC_scratch_alloc(n * sizeof(struct ms_entry));
    
    if (GC_mark_stack_size != 0) {
        if (new_stack != 0) {
          /* Recycle old space */
            GC_add_to_heap((struct hblk *)GC_mark_stack,
            		   GC_mark_stack_size * sizeof(struct ms_entry));
          GC_mark_stack = new_stack;
          GC_mark_stack_size = n;
        }
    } else {
        if (new_stack == 0) {
            GC_err_printf0("No space for mark stack\n");
            EXIT();
        }
        GC_mark_stack = new_stack;
        GC_mark_stack_size = n;
    }
    GC_mark_stack_top = GC_mark_stack-1;
}

void GC_mark_init()
{
    alloc_mark_stack(INITIAL_MARK_STACK_SIZE);
}

/*
 * Push all locations between b and t onto the mark stack.
 * b is the first location to be checked. t is one past the last
 * location to be checked.
 * Should only be used if there is no possibility of mark stack
 * overflow.
 */
void GC_push_all(bottom, top)
ptr_t bottom;
ptr_t top;
{
    bottom = (ptr_t)(((word) bottom + ALIGNMENT-1) & ~(ALIGNMENT-1));
    top = (ptr_t)(((word) top) & ~(ALIGNMENT-1));
    
    if (top == 0 || bottom == top) return;
    GC_mark_stack_top++;
    if (GC_mark_stack_top >= GC_mark_stack + GC_mark_stack_size) {
	ABORT("unexpected mark stack overflow");
    }
    GC_mark_stack_top -> mse_start = (word *)bottom;
    GC_mark_stack_top -> mse_end = (word *)top;
}

/*
 * Analogous to the above, but push only those pages that may have been
 * dirtied.
 * Should not overflow mark stack.
 */
void GC_push_dirty(bottom, top)
ptr_t bottom;
ptr_t top;
{
    register struct hblk * h;

    bottom = (ptr_t)(((long) bottom + ALIGNMENT-1) & ~(ALIGNMENT-1));
    top = (ptr_t)(((long) top) & ~(ALIGNMENT-1));

    if (top == 0 || bottom == top) return;
    h = HBLKPTR(bottom + HBLKSIZE);
    if (top <= (ptr_t) h) {
  	if (GC_page_was_dirty(h-1)) {
	    GC_push_all(bottom, top);
	}
	return;
    }
    if (GC_page_was_dirty(h-1)) {
        GC_push_all(bottom, (ptr_t)h);
    }
    while ((ptr_t)(h+1) <= top) {
	if (GC_page_was_dirty(h)) {
	    GC_mark_stack_top++;
    	    GC_mark_stack_top -> mse_start = (word *)h;
	    h++;
	    if (GC_mark_stack_top - GC_mark_stack
		> 3 * GC_mark_stack_size / 4) {
	 	/* Danger of mark stack overflow */
		GC_mark_stack_top -> mse_end = (word *)top;
		return;
	    } else {
		GC_mark_stack_top -> mse_end = (word *)h;
	    }
	} else {
	    h++;
	}
    }
    if ((ptr_t)h != top) {
	if (GC_page_was_dirty(h)) {
            GC_push_all((ptr_t)h, top);
        }
    }
    if (GC_mark_stack_top >= GC_mark_stack + GC_mark_stack_size) {
        ABORT("unexpected mark stack overflow");
    }
}

void GC_push_conditional(bottom, top, all)
ptr_t bottom;
ptr_t top;
{
    if (all) {
	GC_push_all(bottom, top);
    } else {
	GC_push_dirty(bottom, top);
    }
}

/*
 * Push a single value onto mark stack. Mark from the object pointed to by p.
 * GC_push_one is normally called by GC_push_regs, and thus must be defined.
 * P is considered valid even if it is an interior pointer.
 * Previously marked objects are not pushed.  Hence we make progress even
 * if the mark stack overflows.
 */
# define GC_PUSH_ONE_STACK(p) \
    if ((ptr_t)(p) >= GC_least_plausible_heap_addr 	\
	 && (ptr_t)(p) < GC_greatest_plausible_heap_addr) {	\
	 GC_push_one_checked(p,TRUE);	\
    }

/*
 * As above, but interior pointer recognition as for
 * normal for heap pointers.
 */
# ifdef ALL_INTERIOR_POINTERS
#   define AIP TRUE
# else
#   define AIP FALSE
# endif
# define GC_PUSH_ONE_HEAP(p) \
    if ((ptr_t)(p) >= GC_least_plausible_heap_addr 	\
	 && (ptr_t)(p) < GC_greatest_plausible_heap_addr) {	\
	 GC_push_one_checked(p,AIP);	\
    }

void GC_push_one(p)
word p;
{
    GC_PUSH_ONE_STACK(p);
}

# ifdef __STDC__
#   define BASE(p) (word)GC_base((void *)(p))
# else
#   define BASE(p) (word)GC_base((char *)(p))
# endif

/* As above, but argument passed preliminary test. */
void GC_push_one_checked(p, interior_ptrs)
register word p;
register bool interior_ptrs;
{
    register word r;
    register hdr * hhdr; 
    register int displ;
  
    GET_HDR(p, hhdr);
    if (IS_FORWARDING_ADDR_OR_NIL(hhdr)) {
        if (hhdr != 0 && interior_ptrs) {
          r = BASE(p);
	  hhdr = HDR(r);
	  displ = BYTES_TO_WORDS(HBLKDISPL(r));
	} else {
	  hhdr = 0;
	}
    } else {
        register map_entry_type map_entry;
        
        displ = HBLKDISPL(p);
        map_entry = MAP_ENTRY((hhdr -> hb_map), displ);
        if (map_entry == OBJ_INVALID) {
          if (interior_ptrs) {
            r = BASE(p);
	    displ = BYTES_TO_WORDS(HBLKDISPL(r));
	    if (r == 0) hhdr = 0;
          } else {
            hhdr = 0;
          }
        } else {
          displ = BYTES_TO_WORDS(displ);
          displ -= map_entry;
          r = (word)((word *)(HBLKPTR(p)) + displ);
        }
    }
    /* If hhdr != 0 then r == GC_base(p), only we did it faster. */
    /* displ is the word index within the block.		 */
    if (hhdr == 0) {
    	if (interior_ptrs) {
	    GC_add_to_black_list_stack(p);
	} else {
	    GC_ADD_TO_BLACK_LIST_NORMAL(p);
	}
    } else {
	if (!mark_bit_from_hdr(hhdr, displ)) {
	    set_mark_bit_from_hdr(hhdr, displ);
	    GC_mark_stack_top =
	        (* (hhdr -> hb_mark_proc))((word *)r,
	        			  hhdr,
          			          GC_mark_stack_top,
          			          &(GC_mark_stack[GC_mark_stack_size]));
	}
    }
}

/*
 * A version of GC_push_all that treats all interior pointers as valid
 */
void GC_push_all_stack(bottom, top)
ptr_t bottom;
ptr_t top;
{
# ifdef ALL_INTERIOR_POINTERS
    GC_push_all(bottom, top);
# else
    word * b = (word *)(((long) bottom + ALIGNMENT-1) & ~(ALIGNMENT-1));
    word * t = (word *)(((long) top) & ~(ALIGNMENT-1));
    register word *p;
    register word q;
    register word *lim;
    register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
    register ptr_t least_ha = GC_least_plausible_heap_addr;
#   define GC_greatest_plausible_heap_addr greatest_ha
#   define GC_least_plausible_heap_addr least_ha

    if (top == 0) return;
    /* check all pointers in range and put in push if they appear */
    /* to be valid.						  */
      lim = t - 1 /* longword */;
      for (p = b; p <= lim; p = (word *)(((char *)p) + ALIGNMENT)) {
	q = *p;
	GC_PUSH_ONE_STACK(q);
      }
#   undef GC_greatest_plausible_heap_addr
#   undef GC_least_plausible_heap_addr
# endif
}

/* Push all objects reachable from marked objects in the given block */
/* of size 1 objects.						     */
void GC_push_marked1(h, hhdr)
struct hblk *h;
register hdr * hhdr;
{
    word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p;
    word *plim;
    register int i;
    register word q;
    register word mark_word;
    register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
    register ptr_t least_ha = GC_least_plausible_heap_addr;
#   define GC_greatest_plausible_heap_addr greatest_ha
#   define GC_least_plausible_heap_addr least_ha
    
    p = (word *)(h->hb_body);
    plim = (word *)(((word)h) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    i = 0;
	    while(mark_word != 0) {
	      if (mark_word & 1) {
	          q = p[i];
	          GC_PUSH_ONE_HEAP(q);
	      }
	      i++;
	      mark_word >>= 1;
	    }
	    p += WORDSZ;
	}
#   undef GC_greatest_plausible_heap_addr
#   undef GC_least_plausible_heap_addr        
}


/* Push all objects reachable from marked objects in the given block */
/* of size 2 objects.						     */
void GC_push_marked2(h, hhdr)
struct hblk *h;
register hdr * hhdr;
{
    word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p;
    word *plim;
    register int i;
    register word q;
    register word mark_word;
    register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
    register ptr_t least_ha = GC_least_plausible_heap_addr;
#   define GC_greatest_plausible_heap_addr greatest_ha
#   define GC_least_plausible_heap_addr least_ha
    
    p = (word *)(h->hb_body);
    plim = (word *)(((word)h) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    i = 0;
	    while(mark_word != 0) {
	      if (mark_word & 1) {
	          q = p[i];
	          GC_PUSH_ONE_HEAP(q);
	          q = p[i+1];
	          GC_PUSH_ONE_HEAP(q);
	      }
	      i += 2;
	      mark_word >>= 2;
	    }
	    p += WORDSZ;
	}
#   undef GC_greatest_plausible_heap_addr
#   undef GC_least_plausible_heap_addr        
}

/* Push all objects reachable from marked objects in the given block */
/* of size 4 objects.						     */
/* There is a risk of mark stack overflow here.  But we handle that. */
/* And only unmarked objects get pushed, so it's not very likely.    */
void GC_push_marked4(h, hhdr)
struct hblk *h;
register hdr * hhdr;
{
    word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p;
    word *plim;
    register int i;
    register word q;
    register word mark_word;
    register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
    register ptr_t least_ha = GC_least_plausible_heap_addr;
#   define GC_greatest_plausible_heap_addr greatest_ha
#   define GC_least_plausible_heap_addr least_ha
    
    p = (word *)(h->hb_body);
    plim = (word *)(((word)h) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    i = 0;
	    while(mark_word != 0) {
	      if (mark_word & 1) {
	          q = p[i];
	          GC_PUSH_ONE_HEAP(q);
	          q = p[i+1];
	          GC_PUSH_ONE_HEAP(q);
	          q = p[i+2];
	          GC_PUSH_ONE_HEAP(q);
	          q = p[i+3];
	          GC_PUSH_ONE_HEAP(q);
	      }
	      i += 4;
	      mark_word >>= 4;
	    }
	    p += WORDSZ;
	}
#   undef GC_greatest_plausible_heap_addr
#   undef GC_least_plausible_heap_addr        
}



/* Push all objects reachable from marked objects in the given block */
void GC_push_marked(h, hhdr)
struct hblk *h;
register hdr * hhdr;
{
    register int sz = hhdr -> hb_sz;
    register word * p;
    register int word_no;
    register word * lim;
    register mse * GC_mark_stack_top_reg;
    register mse * mark_stack_limit = &(GC_mark_stack[GC_mark_stack_size]);
    
    /* Some quick shortcuts: */
        if (hhdr -> hb_obj_kind == PTRFREE) return;
        if (GC_block_empty(hhdr)/* nothing marked */) return;
#   ifdef GATHERSTATS
        GC_n_rescuing_pages++;
#   endif
    if (sz > MAXOBJSZ) {
        lim = (word *)(h + 1);
    } else {
        lim = (word *)(h + 1) - sz;
    }
    
    switch(sz) {
     case 1:
       GC_push_marked1(h, hhdr);
       break;
     case 2:
       GC_push_marked2(h, hhdr);
       break;
     case 4:
       GC_push_marked4(h, hhdr);
       break;
     default:
      GC_mark_stack_top_reg = GC_mark_stack_top;
      for (p = (word *)h + HDR_WORDS, word_no = HDR_WORDS; p <= lim;
         p += sz, word_no += sz) {
         /* This needs manual optimization: */
         if (mark_bit_from_hdr(hhdr, word_no)) {
           /* Mark from fields inside the object */
             GC_mark_stack_top_reg =
	        (* (hhdr -> hb_mark_proc))((word *)p,
	        			  hhdr,
          			          GC_mark_stack_top_reg,
          			          mark_stack_limit);
#	     ifdef GATHERSTATS
		/* Subtract this object from total, since it was	*/
		/* added in twice.					*/
		GC_composite_in_use -= sz;
#	     endif
         }
      }
      GC_mark_stack_top = GC_mark_stack_top_reg;
    }
}

/* Test whether any page in the given block is dirty	*/
bool GC_block_was_dirty(h, hhdr)
struct hblk *h;
register hdr * hhdr;
{
    register int sz = hhdr -> hb_sz;
    
    if (sz < MAXOBJSZ) {
         return(GC_page_was_dirty(h));
    } else {
    	 register ptr_t p = (ptr_t)h;
         sz += HDR_WORDS;
         sz = WORDS_TO_BYTES(sz);
         while (p < (ptr_t)h + sz) {
             if (GC_page_was_dirty((struct hblk *)p)) return(TRUE);
             p += HBLKSIZE;
         }
         return(FALSE);
    }
}

/* Identical to above, but return address of next block	*/
struct hblk * GC_push_next_marked(h)
struct hblk *h;
{
    register hdr * hhdr;
    
    h = GC_next_block(h);
    if (h == 0) return(0);
    hhdr = HDR(h);
    GC_push_marked(h, hhdr);
    return(h + OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz));
}

/* Identical to above, but mark only from dirty pages	*/
struct hblk * GC_push_next_marked_dirty(h)
struct hblk *h;
{
    register hdr * hhdr = HDR(h);
    
    if (!GC_incremental) { ABORT("dirty bits not set up"); }
    for (;;) {
        h = GC_next_block(h);
        if (h == 0) return(0);
        hhdr = HDR(h);
#	ifdef STUBBORN_ALLOC
          if (hhdr -> hb_obj_kind == STUBBORN) {
            if (GC_page_was_changed(h) && GC_block_was_dirty(h, hhdr)) {
                break;
            }
          } else {
            if (GC_block_was_dirty(h, hhdr)) break;
          }
#	else
	  if (GC_block_was_dirty(h, hhdr)) break;
#	endif
        h += OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
    }
    GC_push_marked(h, hhdr);
    return(h + OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz));
}

/* Similar to above, but for uncollectable pages.  Needed since we	*/
/* do not clear marks for such pages, even for full collections.	*/
struct hblk * GC_push_next_marked_uncollectable(h)
struct hblk *h;
{
    register hdr * hhdr = HDR(h);
    
    for (;;) {
        h = GC_next_block(h);
        if (h == 0) return(0);
        hhdr = HDR(h);
	if (hhdr -> hb_obj_kind == UNCOLLECTABLE) break;
        h += OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz);
    }
    GC_push_marked(h, hhdr);
    return(h + OBJ_SZ_TO_BLOCKS(hhdr -> hb_sz));
}

