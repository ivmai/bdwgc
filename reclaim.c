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

#include <stdio.h>
#include "gc.h"
#define DEBUG
#undef DEBUG
#ifdef PRINTSTATS
#  define GATHERSTATS
#endif

long mem_found = 0;     /* Number of longwords of memory reclaimed     */

long composite_in_use;  /* Number of longwords in accessible composite */
			/* objects.                                    */

long atomic_in_use;     /* Number of longwords in accessible atomic */
			/* objects.                                 */

# ifdef FIND_LEAK
static report_leak(p, sz)
long p, sz;
{
    /* Negative size ==> pointer-free (atomic) object */
    /* sz is in words.				      */
    abort(p, sz);
}

#   define FOUND_FREE(hblk, word_no) \
      if (abort_if_found) { \
         report_leak((long)hblk + WORDS_TO_BYTES(word_no), hblk -> hb_sz); \
      }
# else
#   define FOUND_FREE(hblk, word_no)
# endif

/*
 * reclaim phase
 *
 */

reclaim(abort_if_found)
int abort_if_found;		/* Abort if a reclaimable object is found */
{
register struct hblk *hbp;	/* ptr to current heap block		*/
register int word_no;		/* Number of word in block		*/
register long i;
register word *p;		/* pointer to current word in block	*/
register int mb;		/* mark bit of current word		*/
int sz;				/* size of objects in current block	*/
word *plim;
struct hblk **nexthbp;		/* ptr to ptr to current heap block	*/
int nonempty;			/* nonempty ^ done with block => block empty*/
struct obj *list;		/* used to build list of free words in block*/
register int is_atomic;         /* => current block contains atomic objs */

#   ifdef DEBUG
        gc_printf("clearing all between %x and %x, %x and %x\n",
                  objfreelist, &objfreelist[MAXOBJSZ+1],
                  aobjfreelist,&aobjfreelist[MAXAOBJSZ+1]);
#   endif
    if (!abort_if_found) {
        register struct obj **fop;
        
	for( fop = objfreelist; fop < &objfreelist[MAXOBJSZ+1]; fop++ ) {
	    *fop = (struct obj *)0;
	}
	for( fop = aobjfreelist; fop < &aobjfreelist[MAXAOBJSZ+1]; fop++ ) {
	    *fop = (struct obj *)0;
	}
    } /* otherwise free list objects are marked, and its safe to leave them */
    
    atomic_in_use = 0;
    composite_in_use = 0;

#   ifdef PRINTBLOCKS
        gc_printf("reclaim: current block sizes:\n");
#   endif

  /* go through all heap blocks (in hblklist) and reclaim unmarked objects */
# ifdef HBLK_MAP
    hbp = (struct hblk *) heapstart;
    for (; ((char *)hbp) < heaplim; hbp++) if (is_hblk(hbp)) {
/* fprintf(stderr, "Reclaiming in 0x%X\n", hbp); */
# else
    nexthbp = hblklist;
    while( nexthbp < last_hblk ) {
	hbp = *nexthbp++;
# endif

	nonempty = FALSE;
	sz = hbp -> hb_sz;
	is_atomic = 0;
	if (sz < 0) {
	    sz = -sz;
	    is_atomic = 1;		/* this block contains atomic objs */
	}
#	ifdef PRINTBLOCKS
            gc_printf("%d(%c",sz, (is_atomic)? 'a' : 'c');
#	endif

	if( sz > (is_atomic? MAXAOBJSZ : MAXOBJSZ) ) {  /* 1 big object */
	    mb = mark_bit(hbp, (hbp -> hb_body) - ((word *)(hbp)));
	    if( mb ) {
#               ifdef GATHERSTATS
		    if (is_atomic) {
			atomic_in_use += sz;
		    } else {
			composite_in_use += sz;
		    }
#               endif
		nonempty = TRUE;
	    } else {
	        FOUND_FREE(hbp, (hbp -> hb_body) - ((word *)(hbp)));
		mem_found += sz;
	    }
	} else {				/* group of smaller objects */
	    p = (word *)(hbp->hb_body);
	    word_no = ((word *)p) - ((word *)hbp);
	    plim = (word *)((((unsigned)hbp) + HBLKSIZE)
		       - WORDS_TO_BYTES(sz));

	    list = (is_atomic) ? aobjfreelist[sz] : objfreelist[sz];

	  /* go through all words in block */
	    while( p <= plim )  {
		mb = mark_bit(hbp, word_no);

		if( mb ) {
#                   ifdef GATHERSTATS
			if (is_atomic) atomic_in_use += sz;
			else           composite_in_use += sz;
#                   endif
#                   ifdef DEBUG
                        gc_printf("found a reachable obj\n");
#		    endif
		    nonempty = TRUE;
		    p += sz;
		} else {
		  FOUND_FREE(hbp, word_no);
		  mem_found += sz;
		  /* word is available - put on list */
		    ((struct obj *)p)->obj_link = list;
		    list = ((struct obj *)p);
		  if (is_atomic) {
		    p += sz;
		  } else {
		    /* Clear object, advance p to next object in the process */
			i = (long)(p + sz);
                        p++; /* Skip link field */
                        while (p < (word *)i) {
			    *p++ = 0;
			}
		  }
		}
		word_no += sz;
	    }

	  /*
	   * if block has reachable words in it, we can't reclaim the
	   * whole thing so put list of free words in block back on
	   * free list for this size.
	   */
	    if( nonempty ) {
		if ( is_atomic )	aobjfreelist[sz] = list;
		else			objfreelist[sz] = list;
	    }
	} 

#	ifdef PRINTBLOCKS
            gc_printf("%c),", nonempty ? 'n' : 'e' );
#	endif
	if (!nonempty) {
            if (!is_atomic && sz <= MAXOBJSZ) {
                /* Clear words at beginning of objects */
                /* Since most of it is already cleared */
		  p = (word *)(hbp->hb_body);
		  plim = (word *)((((unsigned)hbp) + HBLKSIZE)
			 - WORDS_TO_BYTES(sz));
		  while (p <= plim) {
		    *p = 0;
		    p += sz;
		  }
		hbp -> hb_uninit = 0;
	    } else {
		/* Mark it as being uninitialized */
		hbp -> hb_uninit = 1;
	    }

	  /* remove this block from list of active blocks */
	    del_hblklist(hbp);	

#           ifndef HBLK_MAP
	      /* This entry in hblklist just got replaced; look at it again  */
	      /* This admittedly depends on the internals of del_hblklist... */
	      nexthbp--;
#           endif

	    freehblk(hbp);
	}  /* end if (one big object...) */
    } /* end while (nexthbp ...) */

#   ifdef PRINTBLOCKS
        gc_printf("\n");
#   endif
}
