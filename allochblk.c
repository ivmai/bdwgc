/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */

#define DEBUG
#undef DEBUG
#include <stdio.h>
#include "gc_private.h"


/**/
/* allocate/free routines for heap blocks
/* Note that everything called from outside the garbage collector
/* should be prepared to abort at any point as the result of a signal.
/**/

/*
 * Free heap blocks are kept on a list sorted by address.
 * The hb_hdr.hbh_sz field of a free heap block contains the length
 * (in bytes) of the entire block.
 * Neighbors are coalesced.
 */
 
# define MAX_BLACK_LIST_ALLOC (2*HBLKSIZE)
		/* largest block we will allocate starting on a black   */
		/* listed block.  Must be >= HBLKSIZE.			*/

struct hblk * GC_hblkfreelist = 0;

struct hblk *GC_savhbp = (struct hblk *)0;  /* heap block preceding next */
					 /* block to be examined by   */
					 /* GC_allochblk.                */

/*
 * Return TRUE if there is a heap block sufficient for object size sz,
 * FALSE otherwise.  Advance GC_savhbp to point to the block prior to the
 * first such block.
 */
bool GC_sufficient_hb(sz, kind)
word sz;
int kind;
{
register struct hblk *hbp;
register hdr * hhdr;
struct hblk *prevhbp;
signed_word size_needed;
signed_word size_avail;
bool first_time = TRUE;

    size_needed = WORDS_TO_BYTES(sz);
    size_needed = (size_needed+HDR_BYTES+HBLKSIZE-1) & ~HBLKMASK;
#   ifdef DEBUG
	GC_printf("GC_sufficient_hb: sz = %ld, size_needed = 0x%lx\n",
		  (long)sz, (unsigned long)size_needed);
#   endif
    /* search for a big enough block in free list */
	hbp = GC_savhbp;
	hhdr = HDR(hbp);
	for(;;) {
	    prevhbp = hbp;
	    hbp = ((prevhbp == (struct hblk *)0)
		    ? GC_hblkfreelist
		    : hhdr->hb_next);
	    hhdr = HDR(hbp);

	    if( prevhbp == GC_savhbp && !first_time) {
		/* no sufficiently big blocks on free list */
		return(FALSE);
	    }
	    first_time = 0;
	    if( hbp == (struct hblk *)0 ) continue;
	    size_avail = hhdr->hb_sz;
	    if ( kind != PTRFREE || size_needed > MAX_BLACK_LIST_ALLOC) {
	      struct hblk * thishbp;
	      struct hblk * lasthbp = hbp;
	      
	      while ((ptr_t)lasthbp - (ptr_t)hbp < size_avail
	             && (thishbp = GC_is_black_listed(lasthbp,
	             				      (word)size_needed))) {
	          lasthbp = thishbp;
	      }
	      size_avail -= (ptr_t)lasthbp - (ptr_t)hbp;
	    }
	    if( size_avail >= size_needed ) {
		GC_savhbp = prevhbp;
		return(TRUE);
	    }
	}
}

/*
 * Allocate (and return pointer to) a heap block
 *   for objects of size |sz| words.
 *
 * NOTE: We set obj_map field in header correctly.
 *       Caller is resposnsible for building an object freelist in block.
 *
 * We clear the block if it is destined for large objects, and if
 * kind requires that newly allocated objects be cleared.
 */
struct hblk *
GC_allochblk(sz, kind)
word sz;
int kind;
{
    register struct hblk *thishbp;
    register hdr * thishdr;		/* Header corr. to thishbp */
    register struct hblk *hbp;
    register hdr * hhdr;		/* Header corr. to hbp */
    struct hblk *prevhbp;
    register hdr * phdr;		/* Header corr. to prevhbp */
    signed_word size_needed;    /* number of bytes in requested objects */
    signed_word size_avail;	/* bytes available in this block	*/
    bool first_time = TRUE;

    size_needed = WORDS_TO_BYTES(sz);
    size_needed = (size_needed+HDR_BYTES+HBLKSIZE-1) & ~HBLKMASK;

    /* search for a big enough block in free list */
	hbp = GC_savhbp;
	hhdr = HDR(hbp);
	for(;;) {

	    prevhbp = hbp;
	    phdr = hhdr;
	    hbp = (prevhbp == 0? GC_hblkfreelist : phdr->hb_next);
	    hhdr = HDR(hbp);

	    if( prevhbp == GC_savhbp && !first_time) {
		/* no sufficiently big blocks on free list, */
		/* let thishbp --> a newly-allocated block, */
		/* free it (to merge into existing block    */
		/* list) and start the search again, this   */
		/* time with guaranteed success.            */
                  word size_to_get = size_needed + GC_hincr * HBLKSIZE;

		  if (! GC_expand_hp_inner(divHBLKSZ(size_to_get))) {
		      if (! GC_expand_hp_inner(divHBLKSZ((word)size_needed)))
		      {
		          /* GC_printf("Out of Memory!  Giving up ...\n"); */
		          /* There are other things we could try.  It 	*/
		          /* would probably be reasonable to clear 	*/
		          /* black lists at tthis point.		*/
		          return(0);
		      } else {
		          WARN("Out of Memory!  Trying to continue ...\n");
		          GC_gcollect_inner(TRUE);
		      }
		  }
                  update_GC_hincr;
                  return (GC_allochblk(sz, kind));
	    }

	    first_time = 0;

	    if( hbp == 0 ) continue;

	    size_avail = hhdr->hb_sz;
	    if (size_avail < size_needed) continue;
	    /* If the next heap block is obviously better, go on.	*/
	    /* This prevents us from disassembling a single large block */
	    /* to get tiny blocks.					*/
	    {
	      word next_size;
	      
	      thishbp = hhdr -> hb_next;
	      if (thishbp == 0) thishbp = GC_hblkfreelist; 
	      thishdr = HDR(thishbp);
	      next_size = thishdr -> hb_sz;
	      if (next_size < size_avail
	          && next_size >= size_needed
	          && !GC_is_black_listed(thishbp, (word)size_needed)) {
	          continue;
	      }
	    }
	    if ( kind != PTRFREE || size_needed > MAX_BLACK_LIST_ALLOC) {
	      struct hblk * lasthbp = hbp;
	      
	      while (size_avail >= size_needed
	             && (thishbp = GC_is_black_listed(lasthbp,
	             				      (word)size_needed))) {
	        lasthbp = thishbp;
	      }
	      size_avail -= (ptr_t)lasthbp - (ptr_t)hbp;
	      thishbp = lasthbp;
	      if (size_avail >= size_needed && thishbp != hbp) {
	          /* Split the block at thishbp */
	              GC_install_header(thishbp);
	              thishdr = HDR(thishbp);
	              /* GC_invalidate_map not needed, since we will	*/
	              /* allocate this block.				*/
		      thishdr -> hb_next = hhdr -> hb_next;
		      thishdr -> hb_sz = size_avail;
		      hhdr -> hb_sz = (ptr_t)thishbp - (ptr_t)hbp;
		      hhdr -> hb_next = thishbp;
		  /* Advance to thishbp */
		      prevhbp = hbp;
		      phdr = hhdr;
		      hbp = thishbp;
		      hhdr = thishdr;
	      } 
	    }
	    if( size_avail >= size_needed ) {
		/* found a big enough block       */
		/* let thishbp --> the block      */
		/* set prevhbp, hbp to bracket it */
		    thishbp = hbp;
		    thishdr = hhdr;
		    if( size_avail == size_needed ) {
			hbp = hhdr->hb_next;
			hhdr = HDR(hbp);
		    } else {
			hbp = (struct hblk *)
			    (((unsigned)thishbp) + size_needed);
			GC_install_header(hbp);
			hhdr = HDR(hbp);
			GC_invalidate_map(hhdr);
			hhdr->hb_next = thishdr->hb_next;
			hhdr->hb_sz = size_avail - size_needed;
		    }
		/* remove *thishbp from hblk freelist */
		    if( prevhbp == 0 ) {
			GC_hblkfreelist = hbp;
		    } else {
			phdr->hb_next = hbp;
		    }
		/* save current list search position */
		    GC_savhbp = hbp;
		break;
	    }
	}

    /* set size and mask field of *thishbp correctly */
	thishdr->hb_sz = sz;

    /* Clear block if necessary */
	if (sz > MAXOBJSZ && GC_obj_kinds[kind].ok_init) {
	    bzero((char *)thishbp + HDR_BYTES,  (int)(size_needed - HDR_BYTES));
	}
	
    /* Clear mark bits */
	{
	    register word *p = (word *)(&(thishdr -> hb_marks[0]));
	    register word * plim =
	    		(word *)(&(thishdr -> hb_marks[MARK_BITS_SZ]));
	    while (p < plim) {
		*p++ = 0;
	    }
	}
	
    /* Add it to data structure describing hblks in use */
        GC_install_counts(thishbp, (word)size_needed);
        
    /* Add description of valid object pointers. */
        GC_add_map_entry(sz);
        thishdr -> hb_map = GC_obj_map[sz > MAXOBJSZ? 0 : sz];
        
    /* Set kind related fields */
        thishdr -> hb_obj_kind = kind;
        thishdr -> hb_mark_proc = GC_obj_kinds[kind].ok_mark_proc;

    return( thishbp );
}
 
/*
 * Free a heap block.
 *
 * Coalesce the block with its neighbors if possible.
 *
 * All mark words are assumed to be cleared.
 */
void
GC_freehblk(p)
register struct hblk *p;
{
register hdr *phdr;	/* Header corresponding to p */
register struct hblk *hbp, *prevhbp;
register hdr *hhdr, *prevhdr;
register signed_word size;

    /* GC_savhbp may become invalid due to coalescing.  Clear it. */
	GC_savhbp = (struct hblk *)0;

    phdr = HDR(p);
    size = phdr->hb_sz;
    size = 
	((WORDS_TO_BYTES(size)+HDR_BYTES+HBLKSIZE-1)
		 & (~HBLKMASK));
    GC_remove_counts(p, (word)size);
    phdr->hb_sz = size;
    GC_invalidate_map(phdr);

    prevhbp = 0;
    hbp = GC_hblkfreelist;
    hhdr = HDR(hbp);

    while( (hbp != 0) && (hbp < p) ) {
	prevhbp = hbp;
	prevhdr = hhdr;
	hbp = hhdr->hb_next;
	hhdr = HDR(hbp);
    }
    
    /* Check for duplicate deallocation in the easy case */
      if (hbp != 0 && (ptr_t)p + size > (ptr_t)hbp
        || prevhbp != 0 && (ptr_t)prevhbp + prevhdr->hb_sz > (ptr_t)p) {
        GC_printf("Duplicate large block deallocation of 0x%lx\n",
        	  (unsigned long) p);
        GC_printf("Surrounding free blocks are 0x%lx and 0x%lx\n",
           	  (unsigned long) prevhbp, (unsigned long) hbp);
      }

    /* Coalesce with successor, if possible */
      if( (((word)p)+size) == ((word)hbp) ) {
	phdr->hb_next = hhdr->hb_next;
	phdr->hb_sz += hhdr->hb_sz;
	GC_remove_header(hbp);
      } else {
	phdr->hb_next = hbp;
      }

    
    if( prevhbp == 0 ) {
	GC_hblkfreelist = p;
    } else if( (((word)prevhbp) + prevhdr->hb_sz)
      	       == ((word)p) ) {
      /* Coalesce with predecessor */
	prevhdr->hb_next = phdr->hb_next;
	prevhdr->hb_sz += phdr->hb_sz;
	GC_remove_header(p);
    } else {
	prevhdr->hb_next = p;
    }
}

