/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this compiler for any purpose,
 * provided the above notices are retained on all copies.
 */

#define DEBUG
#undef DEBUG
#include <stdio.h>
#include "gc.h"


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

struct hblk *savhbp = (struct hblk *)0;  /* heap block preceding next */
					 /* block to be examined by   */
					 /* allochblk.                */

/*
 * Return 1 if there is a heap block sufficient for object size sz,
 * 0 otherwise.  Advance savhbp to point to the block prior to the
 * first such block.
 */
int sufficient_hb(sz)
int sz;
{
register struct hblk *hbp;
struct hblk *prevhbp;
int size_needed, size_avail;
int first_time = 1;

    size_needed = WORDS_TO_BYTES(sz>0? sz : -sz);
    size_needed = (size_needed+sizeof(struct hblkhdr)+HBLKSIZE-1) & ~HBLKMASK;
#   ifdef DEBUG
	gc_printf("sufficient_hb: sz = %d, size_needed = 0x%X\n",
		  sz, size_needed);
#   endif
    /* search for a big enough block in free list */
	hbp = savhbp;
	for(;;) {
	    prevhbp = hbp;
	    hbp = ((prevhbp == (struct hblk *)0)
		    ? hblkfreelist
		    : prevhbp->hb_next);

	    if( prevhbp == savhbp && !first_time) {
		/* no sufficiently big blocks on free list */
		return(0);
	    }
	    first_time = 0;
	    if( hbp == (struct hblk *)0 ) continue;
	    size_avail = hbp->hb_sz;
	    if( size_avail >= size_needed ) {
		savhbp = prevhbp;
		return(1);
	    }
	}
}

/*
 * Allocate (and return pointer to) a heap block
 *   for objects of size |sz|.
 *
 * NOTE: Caller is responsible for adding it to global hblklist
 *       and for building an object freelist in it.
 *
 * The new block is guaranteed to be cleared if sz > 0.
 */
struct hblk *
allochblk(sz)
long sz;
{
    register struct hblk *thishbp;
    register struct hblk *hbp;
    struct hblk *prevhbp;
    long size_needed,            /* number of bytes in requested objects */
         uninit,                 /* => Found uninitialized block         */
         size_avail;
    int first_time = 1;

    char *sbrk();			/* data segment size increasing	*/
    char *brk();			/* functions			*/

    size_needed = WORDS_TO_BYTES(sz>0? sz : -sz);
    size_needed = (size_needed+sizeof(struct hblkhdr)+HBLKSIZE-1) & ~HBLKMASK;
#   ifdef DEBUG
	gc_printf("(allochblk) sz = %x, size_needed = 0x%X\n", sz, size_needed);
#   endif

    /* search for a big enough block in free list */
	hbp = savhbp;
	for(;;) {

	    prevhbp = hbp;
	    hbp = ((prevhbp == (struct hblk *)0)
                    ? hblkfreelist
		    : prevhbp->hb_next);

	    if( prevhbp == savhbp && !first_time) {
		/* no sufficiently big blocks on free list, */
		/* let thishbp --> a newly-allocated block, */
		/* free it (to merge into existing block    */
		/* list) and start the search again, this   */
		/* time with guaranteed success.            */
                  int size_to_get = size_needed + hincr * HBLKSIZE;
		  extern int holdsigs();
		  int Omask;

		  /* Don't want to deal with signals in the middle of this */
		      Omask = holdsigs();

                    update_hincr;
		    thishbp = HBLKPTR(((unsigned)sbrk(0))+HBLKSIZE-1 );
		    heaplim = (char *) (((unsigned)thishbp) + size_to_get);

		    if( (brk(heaplim)) == ((char *)-1) ) {
                        write(2,"Out of Memory!  Giving up ...\n", 30);
			exit(-1);
		    }
#                   ifdef PRINTSTATS
			gc_printf("Need to increase heap size by %d\n",
			          size_to_get);
#                   endif
		    heapsize += size_to_get;
		    thishbp->hb_sz = 
			BYTES_TO_WORDS(size_to_get - sizeof(struct hblkhdr));
		    freehblk(thishbp);
		    /* Reenable signals */
		      sigsetmask(Omask);
		    hbp = savhbp;
		    first_time = 1;
		continue;
	    }

	    first_time = 0;

	    if( hbp == (struct hblk *)0 ) continue;

	    size_avail = hbp->hb_sz;
	    if( size_avail >= size_needed ) {
		/* found a big enough block       */
		/* let thishbp --> the block      */
		/* set prevhbp, hbp to bracket it */
		    thishbp = hbp;
		    if( size_avail == size_needed ) {
			hbp = hbp->hb_next;
			uninit = thishbp -> hb_uninit;
		    } else {
			uninit = thishbp -> hb_uninit;
			thishbp -> hb_uninit = 1; 
				/* Just in case we get interrupted by a */
				/* signal                               */
			hbp = (struct hblk *)
			    (((unsigned)thishbp) + size_needed);
			hbp->hb_uninit = uninit;
			hbp->hb_next = thishbp->hb_next;
			hbp->hb_sz = size_avail - size_needed;
		    }
		/* remove *thishbp from hblk freelist */
		    if( prevhbp == (struct hblk *)0 ) {
			hblkfreelist = hbp;
		    } else {
			prevhbp->hb_next = hbp;
		    }
		/* save current list search position */
		    savhbp = prevhbp;
		break;
	    }
	}

    /* set size and mask field of *thishbp correctly */
	thishbp->hb_sz = sz;
	thishbp->hb_mask = -1;  /* may be changed by new_hblk */

    /* Clear block if necessary */
	if (uninit && sz > 0) {
	    register word * p = &(thishbp -> hb_body[0]);
	    register word * plim;

	    plim = (word *)(((char *)thishbp) + size_needed);
	    while (p < plim) {
		*p++ = 0;
	    }
	}
    /* Clear mark bits */
	{
	    register word *p = (word *)(&(thishbp -> hb_marks[0]));
	    register word * plim = (word *)(&(thishbp -> hb_marks[MARK_BITS_SZ]));
	    while (p < plim) {
		*p++ = 0;
	    }
	}

#   ifdef DEBUG
	gc_printf("Returning 0x%X\n", thishbp);
#   endif
    return( thishbp );
}
 
/* Clear the header information in a previously allocated heap block p */
/* so that it can be coalesced with an initialized heap block.         */
static clear_header(p)
register struct hblk *p;
{
    p -> hb_sz = 0;
#   ifndef HBLK_MAP
      p -> hb_index = (struct hblk **)0;
#   endif
    p -> hb_next = 0;
    p -> hb_mask = 0;
#   if MARK_BITS_SZ <= 60
	/* Since this block was deallocated, only spurious mark      */
	/* bits corresponding to the header could conceivably be set */
	p -> hb_marks[0] = 0;
	p -> hb_marks[1] = 0;
#   else
	--> fix it
#   endif
}

/*
 * Free a heap block.
 *
 * Assume the block is not currently on hblklist.
 *
 * Coalesce the block with its neighbors if possible.

 * All mark words (except possibly the first) are assumed to be cleared.
 * The body is assumed to be cleared unless hb_uninit is nonzero.
 */
void
freehblk(p)
register struct hblk *p;
{
register struct hblk *hbp, *prevhbp;
register int size;

    /* savhbp may become invalid due to coalescing.  Clear it. */
	savhbp = (struct hblk *)0;

    size = p->hb_sz;
    if( size < 0 ) size = -size;
    size = 
	((WORDS_TO_BYTES(size)+sizeof(struct hblkhdr)+HBLKSIZE-1)
		 & (~HBLKMASK));
    p->hb_sz = size;

    prevhbp = (struct hblk *) 0;
    hbp = hblkfreelist;

    while( (hbp != (struct hblk *)0) && (hbp < p) ) {
	prevhbp = hbp;
	hbp = hbp->hb_next;
    }

    /* Coalesce with successor, if possible */
      if( (((unsigned)p)+size) == ((unsigned)hbp) ) {
	(p -> hb_uninit) |= (hbp -> hb_uninit);
	p->hb_next = hbp->hb_next;
	p->hb_sz += hbp->hb_sz;
	if (!p -> hb_uninit) clear_header(hbp);
      } else {
	p->hb_next = hbp;
      }

    if( prevhbp == (struct hblk *)0 ) {
	hblkfreelist = p;
    } else if( (((unsigned)prevhbp) + prevhbp->hb_hdr.hbh_sz) ==
	    ((unsigned)p) ) {
      /* Coalesce with predecessor */
	(prevhbp->hb_uninit) |= (p -> hb_uninit);
	prevhbp->hb_next = p->hb_next;
	prevhbp->hb_sz += p->hb_sz;
	if (!prevhbp -> hb_uninit) clear_header(p);
    } else {
	prevhbp->hb_next = p;
    }
}

/* Add a heap block to hblklist or hblkmap.  */
void add_hblklist(hbp)
struct hblk * hbp;
{
# ifdef HBLK_MAP
    long size = hbp->hb_sz;
    long index = divHBLKSZ(((long)hbp) - ((long)heapstart));
    long i;

    if( size < 0 ) size = -size;
    size = (divHBLKSZ(WORDS_TO_BYTES(size)+sizeof(struct hblkhdr)+HBLKSIZE-1));
	   /* in units of HBLKSIZE */
    hblkmap[index] = HBLK_VALID;
    for (i = 1; i < size; i++) {
	if (i < 0x7f) {
	    hblkmap[index+i] = i;
	} else {
	    /* May overflow a char.  Store largest possible value */
	    hblkmap[index+i] = 0x7e;
	}
    }
# else
    if (last_hblk >= &hblklist[MAXHBLKS]) {
	fprintf(stderr, "Not configured for enough memory\n");
	exit(1);
    }
    *last_hblk = hbp;
    hbp -> hb_index = last_hblk;
    last_hblk++;
# endif
}

/* Delete a heap block from hblklist or hblkmap.  */
void del_hblklist(hbp)
struct hblk * hbp;
{
# ifdef HBLK_MAP
    long size = hbp->hb_sz;
    long index = divHBLKSZ(((long)hbp) - ((long)heapstart));
    long i;

    if( size < 0 ) size = -size;
    size = (divHBLKSZ(WORDS_TO_BYTES(size)+sizeof(struct hblkhdr)+HBLKSIZE-1));
	   /* in units of HBLKSIZE */
    for (i = 0; i < size; i++) {
	hblkmap[index+i] = HBLK_INVALID;
    }
# else
    register struct hblk ** list_entry;
    last_hblk--;
    /* Let **last_hblk use the slot previously occupied by *hbp */
	list_entry = hbp -> hb_index;
	(*last_hblk) -> hb_index = list_entry;
	*list_entry = *last_hblk;
# endif
}

/* Initialize hblklist */
void init_hblklist()
{
#   ifdef DEBUG
	gc_printf("Here we are in init_hblklist - ");
	gc_printf("last_hblk = %x\n",&(hblklist[0]));
#   endif
#   ifndef HBLK_MAP
      last_hblk = &(hblklist[0]);
#   endif
}
