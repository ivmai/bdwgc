
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
 * This file contains the functions:
 *      GC_mark()  -  Mark from the mark stack
 *	GC_mark_reliable()  -  as above, but fix things up after
 *					a mark stack overflow.
 *      GC_mark_all(b,t)  - Mark from everything in a range
 *	GC_mark_all_stack(b,t)  - Mark from everything in a range,
 *					    consider interior pointers as valid
 * 	GC_remark()  -  Mark from all marked objects.  Used
 *				 only if we had to drop something.
 */


# include <stdio.h>
# include "gc_private.h"

# define INITIAL_MARK_STACK_SIZE (1*HBLKSIZE)
		/* INITIAL_MARK_STACK_SIZE * sizeof(mse) should be a 	*/
		/* multiple of HBLKSIZE.				*/

/*
 * Limits of stack for GC_mark routine.  Set by caller to GC_mark.
 * All items between GC_mark_stack_top and GC_mark_stack_bottom-1 still need
 * to be marked.
 */
 
mse * GC_mark_stack;

word GC_mark_stack_size = 0;
 
mse * GC_mark_stack_top;

static bool dropped_some = FALSE;
				/* We ran out of space and were forced  */
  				/* to drop some pointers during marking	*/

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
	    dropped_some = TRUE;
	    return(msp-1);
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
	

/*
 * Mark all objects pointed to by the regions described by
 * mark stack entries between GC_mark_stack and GC_mark_stack_top,
 * inclusive.  Assumes the upper limit of a mark stack entry
 * is never 0.
 */
void GC_mark()
{
  mse * GC_mark_stack_reg = GC_mark_stack;
  mse * GC_mark_stack_top_reg = GC_mark_stack_top;
  mse * mark_stack_limit = &(GC_mark_stack[GC_mark_stack_size]);
  register word * current_p;	/* Pointer to current candidate ptr.	*/
  register word current;	/* Candidate pointer.			*/
  register word * limit;	/* (Incl) limit of current candidate 	*/
  				/* range				*/
  register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
  register ptr_t least_ha = GC_least_plausible_heap_addr;
# define SPLIT_RANGE_WORDS 128

  while (GC_mark_stack_top_reg >= GC_mark_stack_reg) {
    register int displ;  /* Displacement in block; first bytes, then words */
    register hdr * hhdr;
    register map_entry_type map_entry;

    current_p = GC_mark_stack_top_reg -> mse_start;
    limit = GC_mark_stack_top_reg -> mse_end;
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
      hhdr = HDR(current);
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
          register word mark_bit = 1 << modWORDSZ(displ);
          
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
  GC_mark_stack_top = GC_mark_stack_top_reg;
}

/* Allocate or reallocate space for mark stack of size s words  */
/* May silently fail.						*/
static void alloc_mark_stack(n)
word n;
{
    mse * new_stack = (mse *)GET_MEM(n * sizeof(struct ms_entry));
    
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

/* Identical to GC_mark, but guarantee that dropped_some is false */
/* when we finish.						  */
void GC_mark_reliable()
{
    dropped_some = FALSE;
    GC_mark();
    while (dropped_some) {
        dropped_some = FALSE;
#	ifdef PRINTSTATS
	    GC_printf1("Mark stack overflow; current size = %lu entries\n",
	    	       GC_mark_stack_size);
#	endif      
        alloc_mark_stack(2*GC_mark_stack_size);
        GC_remark();
    }
}

/*********************************************************************/
/* Mark all locations reachable via pointers located between b and t */
/* b is the first location to be checked. t is one past the last     */
/* location to be checked.                                           */
/*********************************************************************/
void GC_mark_all(bottom, top)
ptr_t bottom;
ptr_t top;
{
    word * b = (word *)(((long) bottom + ALIGNMENT-1) & ~(ALIGNMENT-1));
    word * t = (word *)(((long) top) & ~(ALIGNMENT-1));
    
    if (GC_mark_stack_top != GC_mark_stack-1) {
        ABORT("GC_mark_all: bad mark stack\n");
    }
    if (top == 0) return;
    GC_mark_stack_top++;
    GC_mark_stack_top -> mse_start = b;
    GC_mark_stack_top -> mse_end = t;
    GC_mark_reliable();
}

word * GC_buffer;	/* Buffer for stack marking */
# define BUFSIZE 1024

/*
 * A version of GC_mark_all that treats all interior pointers as valid
 */
void GC_mark_all_stack(bottom, top)
ptr_t bottom;
ptr_t top;
{
# ifdef ALL_INTERIOR_POINTERS
    GC_mark_all(bottom, top);
# else
    word * b = (word *)(((long) bottom + ALIGNMENT-1) & ~(ALIGNMENT-1));
    word * t = (word *)(((long) top) & ~(ALIGNMENT-1));
    register word *p;
    register word q;
    register word r;
    register word *lim;
    word * bufptr;
    word * limit;
    register ptr_t greatest_ha = GC_greatest_plausible_heap_addr;
    register ptr_t least_ha = GC_least_plausible_heap_addr;

    if (top == 0) return;
  /* Allocate GC_buffer someplace where collector won't accidentally	*/
  /* see old sections.							*/
    if (GC_buffer == 0) {
        GC_buffer = (word *)GC_scratch_alloc((word)(BUFSIZE * sizeof(word)));
    }
    bufptr = GC_buffer;
    limit = GC_buffer+BUFSIZE;
  /* check all pointers in range and put in buffer if they appear */
  /* to be valid.						  */
    lim = t - 1 /* longword */;
    for (p = b; p <= lim; p = (word *)(((char *)p) + ALIGNMENT)) {
	q = *p;
	if ((ptr_t)q < least_ha 
	    || (ptr_t)q >= greatest_ha) {
	    continue;
	}
#	ifdef __STDC__
	    r = (word)GC_base((void *)q);
#	else
	    r = (word)GC_base((char *)q);
#	endif
	if (r == 0) {
	    GC_add_to_black_list_stack(*p);
	} else {
	    *(bufptr++) = r;
	    if (bufptr == limit) {
	        GC_mark_all((ptr_t)GC_buffer, (ptr_t)limit);
	        bufptr = GC_buffer;
	    }
	}
    }
    if (bufptr != GC_buffer) GC_mark_all((ptr_t)GC_buffer, (ptr_t)bufptr);
# endif
}

/* Mark all objects reachable from marked objects in the given block */
/*ARGSUSED*/
static void remark_block(h, dummy)
struct hblk *h;
word dummy;
{
    register hdr * hhdr = HDR(h);
    register int sz = hhdr -> hb_sz;
    register word * p;
    register int word_no;
    register word * lim;
    register mse * GC_mark_stack_top_reg = GC_mark_stack_top;
    
    if (hhdr -> hb_obj_kind == PTRFREE) return;
    if (sz > MAXOBJSZ) {
        lim = (word *)(h + 1);
    } else {
        lim = (word *)(h + 1) - sz;
    }
    
    for (p = (word *)h + HDR_WORDS, word_no = HDR_WORDS; p <= lim;
         p += sz, word_no += sz) {
         if (mark_bit_from_hdr(hhdr, word_no)) {
           /* Mark from fields inside the object */
             GC_mark_stack_top_reg++;
             GC_mark_stack_top_reg -> mse_start = p;
             GC_mark_stack_top_reg -> mse_end = p + sz;
         }
    }
    GC_mark_stack_top = GC_mark_stack_top_reg;
    GC_mark();   
}

/*
 * Traverse the heap.  Mark all objects reachable from marked objects.
 */
void GC_remark()
{
    GC_apply_to_all_blocks(remark_block, 0);
}

