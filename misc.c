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

#define DEBUG       /* Some run-time consistency checks */
#undef DEBUG
#define VERBOSE
#undef VERBOSE

#include <stdio.h>
#include <signal.h>
#include "gc.h"

int dont_gc = 0;
extern long mem_found;

# ifdef MERGE_SIZES
#   if MAXOBJSZ == MAXAOBJSZ
#       define MAXSZ MAXOBJSZ
#   else
	--> causes problems here, since we cant map any size to a
	    size that doesnt have a free list.  Either initialization
	    needs to be cleverer, or we need separate maps for atomic
	    and composite objects.
#   endif
    long size_map[MAXSZ+1];

    /* Set things up so that size_map[i] >= i, but not too much bigger */
    /* and so that size_map contains relatively few distinct entries   */
    void init_size_map()
    {
	register int i;
	register int i_rounded_up = 0;

	for (i = 1; i < 8; i++) {
#           ifdef ALIGN_DOUBLE
	      size_map[i] = (i + 1) & (~1);
#           else
	      size_map[i] = i;
#           endif
	}
	for (i = 8; i <= MAXSZ; i++) {
	    if (i_rounded_up < i) {
#               ifdef ALIGN_DOUBLE
		  i_rounded_up = (i + (i >> 1) + 1) & (~1);
#               else                                       
		  i_rounded_up = i + (i >> 1);             
#               endif
		if (i_rounded_up > MAXSZ) {
		    i_rounded_up = MAXSZ;
		}
	    }
	    size_map[i] = i_rounded_up;
	}
    }
# endif


/* allocate lb bytes of atomic data */
struct obj * gc_malloc_atomic(lb)
int lb;
{
register struct obj *op;
register struct obj **opp;
register int lw = BYTES_TO_WORDS(lb + (sizeof (word)) -1);

#   ifdef VERBOSE
	gc_printf("Here we are in gc_malloc_atomic(%d)\n",lw);
#   endif
    if( lw <= MAXAOBJSZ ) {
#       ifdef MERGE_SIZES
	  lw = size_map[lw];
#       endif
	opp = &(aobjfreelist[lw]);
        if( (op = *opp) == ((struct obj *)0) ) {
	    op = _allocaobj(lw);
        }
#       ifdef DEBUG
	    if ((op -> obj_link != ((struct obj *) 0)
		&& (((unsigned)(op -> obj_link)) > ((unsigned) HEAPLIM)
		   || ((unsigned)(op -> obj_link)) < ((unsigned) HEAPSTART)))) {
		fprintf(stderr, "Bad free list in gc_malloc_atomic\n");
		abort(op);
            }
#       endif
        *opp = op->obj_link;
        op->obj_link = (struct obj *)0;
    } else {
	register struct hblk * h;
	if (!sufficient_hb(-lw) && !dont_gc) {
            gcollect();
	}
#       ifdef VERBOSE
	    gc_printf("gc_malloc_atomic calling allochblk(%x)\n",lw);
#	endif
	h = allochblk(-lw);
	add_hblklist(h);
	op = (struct obj *) (h -> hb_body);
    }
    return(op);
}

/* allocate lb bytes of possibly composite data */
struct obj * gc_malloc(lb)
int lb;
{
register struct obj *op;
register struct obj **opp;
register int lw = BYTES_TO_WORDS(lb + (sizeof (word)) -1);

    if( lw <= MAXOBJSZ ) {
#       ifdef MERGE_SIZES
	  lw = size_map[lw];
#       endif
	opp = &(objfreelist[lw]);
        if( (op = *opp) == ((struct obj *)0) ) {
	    op = _allocobj(lw);
        }
#       ifdef DEBUG
	    if ((op -> obj_link != ((struct obj *) 0)
		&& (((unsigned)(op -> obj_link)) > ((unsigned) HEAPLIM)
		   || ((unsigned)(op -> obj_link)) < ((unsigned) HEAPSTART)))) {
		fprintf(stderr, "Bad free list in gc_malloc\n");
		abort(op);
            }
#       endif
        *opp = op->obj_link;
        op->obj_link = (struct obj *)0;
    } else {
	register struct hblk * h;

	if (!sufficient_hb(lw) && !dont_gc) {
            gcollect();
	}
#       ifdef VERBOSE
	    gc_printf("gc_malloc calling allochblk(%x)\n",lw);
#	endif
	h = allochblk(lw);
	add_hblklist(h);
	op = (struct obj *) (h -> hb_body);
    }
    return(op);
}

void gc_free();

/* Change the size of the block pointed to by p to contain at least   */
/* lb bytes.  The object may be (and quite likely will be) moved.     */
/* The new object is assumed to be atomic if the original object was. */
/* Shrinking of large blocks is not implemented well.                 */
struct obj * gc_realloc(p,lb)
struct obj * p;
int lb;
{
register struct obj *op;
register struct obj **opp;
register struct hblk * h;
register int sz;	 /* Current size in bytes	*/
register int orig_sz;	 /* Original sz in bytes	*/
int is_atomic;

    h = HBLKPTR(p);
    sz = h -> hb_sz;
    if (sz < 0) {
	sz = -sz;
	is_atomic = TRUE;
    } else {
	is_atomic = FALSE;
    }
    sz = WORDS_TO_BYTES(sz);
    orig_sz = sz;

    if (is_atomic) {
      if (sz > WORDS_TO_BYTES(MAXAOBJSZ)) {
	/* Round it up to the next whole heap block */
	  sz = (sz+sizeof(struct hblkhdr)+HBLKSIZE-1)
		& (~HBLKMASK);
	  sz -= sizeof(struct hblkhdr);
	  h -> hb_sz = BYTES_TO_WORDS(sz);
      }
      if (lb <= sz) {
	if (lb >= (sz >> 1)) {
	    /* Already big enough, but not too much bigger than object. */
	    /* Ignore the request.                                      */
	    /* If sz is big enough, we should probably deallocate       */
	    /* part of the heap block here, but ...                     */
	    return(p);
	} else {
	    /* shrink */
	      struct obj * result = gc_malloc_atomic(lb);

	      bcopy(p, result, lb);
	      gc_free(p);
	      return(result);
	}
      } else {
	/* grow */
	  struct obj * result = gc_malloc_atomic(lb);

	  bcopy(p, result, sz);
	  gc_free(p);
	  return(result);
      }
    } else /* composite */ {
      if (sz > WORDS_TO_BYTES(MAXOBJSZ)) {
	/* Round it up to the next whole heap block */
	  sz = (sz+sizeof(struct hblkhdr)+HBLKSIZE-1)
		& (~HBLKMASK);
	  sz -= sizeof(struct hblkhdr);
	  h -> hb_sz = BYTES_TO_WORDS(sz);
	  /* Extra area is already cleared by allochblk. */
      }
      if (lb <= sz) {
	if (lb >= (sz >> 1)) {
	    if (orig_sz > lb) {
	      /* Clear unneeded part of object to avoid bogus pointer */
	      /* tracing.					      */
	        bzero(((char *)p) + lb, orig_sz - lb);
	    }
	    return(p);
	} else {
	    /* shrink */
	      struct obj * result = gc_malloc(lb);

	      bcopy(p, result, lb);
	      gc_free(p);
	      return(result);
	}
      } else {
	/* grow */
	  struct obj * result = gc_malloc(lb);

	  bcopy(p, result, sz);
	  gc_free(p);
	  return(result);
      }
    }
}

/* Explicitly deallocate an object p */
void gc_free(p)
struct obj *p;
{
    register struct hblk *h;
    register int sz;
    register word * i;
    register word * limit;

    h = HBLKPTR(p);
    sz = h -> hb_sz;
    if (sz < 0) {
        sz = -sz;
        if (sz > MAXAOBJSZ) {
	    h -> hb_uninit = 1;
	    del_hblklist(h);
	    freehblk(h);
	} else {
	    p -> obj_link = aobjfreelist[sz];
	    aobjfreelist[sz] = p;
	}
    } else {
	/* Clear the object, other than link field */
	    limit = &(p -> obj_component[sz]);
	    for (i = &(p -> obj_component[1]); i < limit; i++) {
		*i = 0;
	    }
	if (sz > MAXOBJSZ) {
	    p -> obj_link = 0;
	    h -> hb_uninit = 0;
	    del_hblklist(h);
	    freehblk(h);
	} else {
	    p -> obj_link = objfreelist[sz];
	    objfreelist[sz] = p;
	}
    }
    /* Add it to mem_found to prevent anomalous heap expansion */
    /* in the event of repeated explicit frees of objects of   */
    /* varying sizes.                                          */
        mem_found += sz;
}


/*
 * Disable non-urgent signals
 */
int holdsigs()
{
    unsigned mask = 0xffffffff;

    mask &= ~(1<<(SIGSEGV-1));
    mask &= ~(1<<(SIGILL-1));
    mask &= ~(1<<(SIGBUS-1));
    mask &= ~(1<<(SIGIOT-1));
    mask &= ~(1<<(SIGEMT-1));
    mask &= ~(1<<(SIGTRAP-1));
    mask &= ~(1<<(SIGQUIT-1));
    return(sigsetmask(mask));
}

void gc_init()
{
    word dummy;
#   define STACKTOP_ALIGNMENT_M1 0xffffff

    heaplim = (char *) (sbrk(0));
#   ifdef HBLK_MAP
	heapstart = (char *) (HBLKPTR(((unsigned)sbrk(0))+HBLKSIZE-1 ));
#   endif
#   ifdef STACKTOP
	stacktop = STACKTOP;
#   else
	stacktop = (word *)((((long)(&dummy)) + STACKTOP_ALIGNMENT_M1)
			    & ~STACKTOP_ALIGNMENT_M1);
#   endif
    hincr = HINCR;
    expand_hp(hincr);
    init_hblklist();
#   ifdef MERGE_SIZES
      init_size_map();
#   endif
}

/* A version of printf that is unlikely to call malloc, and is thus safer */
/* to call from the collector in case malloc has been bound to gc_malloc. */
/* Assumes that no more than 1023 characters are written at once.	  */
gc_printf(format, a, b, c, d, e, f)
char * format;
int a, b, c, d, e, f;
{
    char buf[1025];
    
    buf[1025] = 0x15;
    sprintf(buf, format, a, b, c, d, e, f);
    if (buf[1025] != 0x15) abort("gc_printf clobbered stack");
    if (write(1, buf, strlen(buf)) < 0) abort("write to stdout failed");
}