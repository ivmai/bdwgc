/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this compiler for any purpose,
 * provided the above notices are retained on all copies.
 *
 * This file contains the functions:
 *	void new_hblk(n)
 *	static void clear_marks()
 *      mark(alignment)
 *      mark_all(b,t,alignment)
 *	void gcollect()
 *	expand_hp: func[val Short] val Void
 *	struct obj * _allocobj(sz)
 *	struct obj * _allocaobj(sz)
 */


# include <stdio.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/times.h>
# include "gc.h"

/* Leaving these defined enables output to stderr.  In order of */
/* increasing verbosity:                                        */
#define REPORT_FAILURE   /* Print values that looked "almost" like pointers */
#undef REPORT_FAILURE
#define DEBUG            /* Verbose debugging output */
#undef DEBUG
#define DEBUG2           /* EXTREMELY verbose debugging output */
#undef DEBUG2
#define USE_STACK       /* Put mark stack onto process stack.  This assumes */
			/* that it's safe to put data below the stack ptr,  */
			/* and that the system will expand the stack as     */
			/* necessary.  This is known to be true under Sun   */
			/* UNIX (tm) and Vax Berkeley UNIX.  It is also     */
			/* known to be false under some other UNIX          */
			/* implementations.                                 */
#undef USE_HEAP
#ifdef RT
#   define USE_HEAP
#   undef USE_STACK
#endif
#ifdef MIPS
#   define USE_HEAP
#   undef USE_STACK
#endif

/*
 * This is an attempt at a garbage collecting storage allocator
 * that should run on most UNIX systems.  The garbage
 * collector is overly conservative in that it may fail to reclaim
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
 * up to MAXOBJSZ or MAXAOBJSZ.
 * The lists objfreelist[i] contain free objects of size i which may
 * contain nested pointers.  The lists aobjfreelist[i] contain free
 * atomic objects, which may not contain nested pointers.
 * The call allocobj(i) insures that objfreelist[i] points to a non-empty
 * free list it returns a pointer to the first entry on the free list.
 * Allocobj may be called to allocate an object of (small) size i
 * as follows:
 *
 *            opp = &(objfreelist[i]);
 *            if (*opp == (struct obj *)0) allocobj(i);
 *            ptr = *opp;
 *            *opp = ptr->next;
 *
 * The call to allocobj may be replaced by a call to _allocobj if it
 * is made from C, or if C register save conventions are sufficient.
 * Note that this is very fast if the free list is non-empty; it should
 * only involve the execution of 4 or 5 simple instructions.
 * All composite objects on freelists are cleared, except for
 * their first longword.
 */

/*
 *  The allocator uses allochblk to allocate large chunks of objects.
 * These chunks all start on addresses which are multiples of
 * HBLKSZ.  All starting addresses are maintained on a contiguous
 * list so that they can be traversed in the sweep phase of garbage collection.
 * This makes it possible to check quickly whether an
 * arbitrary address corresponds to an object administered by the
 * allocator.
 *  We make the (probably false) claim that this can be interrupted
 * by a signal with at most the loss of some chunk of memory.
 */

/* Declarations for fundamental data structures.  These are grouped */
/* together, so that the collector can skip over them.              */
/* This relies on some assumptions about the compiler that are not  */
/* guaranteed valid, but ...                                        */

long heapsize = 0;      /* Heap size in bytes */

long non_gc_bytes = 0;  /* Number of bytes not intended to be collected */

char copyright[] = "Copyright 1988,1989 Hans-J. Boehm and Alan J. Demers";
char copyright2[] =
         "Copyright (c) 1991 by Xerox Corporation.  All rights reserved.";
char copyright3[] =
	 "THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY";
char copyright4[] =
	 " EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.";

/* Return a rough approximation to the stack pointer.  A hack,  */
/* but it's semi-portable.                                      */
word * get_current_sp()
{
    word x;
    return(&x);
}

/*
 * Allocate a new heapblock for objects of size n.
 * Add all of the heapblock's objects to the free list for objects
 * of that size.  A negative n requests atomic objects.
 */
void new_hblk(n)
long n;
{
    register word *p,
		  *r;
    word *last_object;		/* points to last object in new hblk	*/
    register struct hblk *h;	/* the new heap block			*/
    register long abs_sz;	/* |n|	*/
    register int i;

#   ifdef PRINTSTATS
	if ((sizeof (struct hblk)) > HBLKSIZE) {
	    abort("HBLK SZ inconsistency");
        }
#   endif

  /* Allocate a new heap block */
    h = allochblk(n);

  /* Add it to hblklist */
    add_hblklist(h);

  /* Add objects to free list */
    abs_sz = abs(n);
    p = &(h -> hb_body[abs_sz]);	/* second object in *h	*/
    r = &(h -> hb_body[0]);       	/* One object behind p	*/
    last_object = ((word *)((char *)h + HBLKSIZE)) - abs_sz;
			    /* Last place for last object to start */

  /* make a list of all objects in *h with head as last object */
    while (p <= last_object) {
      /* current object's link points to last object */
	((struct obj *)p) -> obj_link = (struct obj *)r;
	r = p;
	p += abs_sz;
    }
    p -= abs_sz;			/* p now points to last object */

  /*
   * put p (which is now head of list of objects in *h) as first
   * pointer in the appropriate free list for this size.
   */
    if (n < 0) {
	((struct obj *)(h -> hb_body)) -> obj_link = aobjfreelist[abs_sz];
	aobjfreelist[abs_sz] = ((struct obj *)p);
    } else {
	((struct obj *)(h -> hb_body)) -> obj_link = objfreelist[abs_sz];
	objfreelist[abs_sz] = ((struct obj *)p);
    }

  /*
   * Set up mask in header to facilitate alignment checks
   * See "gc.h" for a description of how this works.
   */
#   ifndef RT
	switch (abs_sz) {
	    case 1:
		h -> hb_mask = 0x3;
		break;
	    case 2:
		h -> hb_mask = 0x7;
		break;
	    case 4:
		h -> hb_mask = 0xf;
		break;
	    case 8:
		h -> hb_mask = 0x1f;
		break;
	    case 16:
		h -> hb_mask = 0x3f;
		break;
	    /* By default it remains set to a negative value */
	}
#   else
      /* the 4.2 pcc C compiler did not produce correct code for the switch */
	if (abs_sz == 1)	{ h -> hb_mask = 0x3; }
	else if (abs_sz == 2)	{ h -> hb_mask = 0x7; }
	else if (abs_sz == 4)	{ h -> hb_mask = 0xf; }
	else if (abs_sz == 8)	{ h -> hb_mask = 0x1f; }
	else if (abs_sz == 16)	{ h -> hb_mask = 0x3f; }
	/* else skip; */
#   endif

#   ifdef DEBUG
	gc_printf("Allocated new heap block at address 0x%X\n",
		h);
#   endif
}


/* some more variables */

extern long mem_found;  /* Number of reclaimed longwords */
			/* after garbage collection      */

extern long atomic_in_use, composite_in_use;
extern errno;

/*
 * Clear mark bits in all allocated heap blocks
 */
static void clear_marks()
{
    register int j;
    register struct hblk **p;
    register struct hblk *q;

# ifdef HBLK_MAP
    for (q = (struct hblk *) heapstart; ((char*)q) < heaplim; q++)
      if (is_hblk(q)) {
# else
    for (p = hblklist; p < last_hblk; p++) {
	q = *p;
# endif
        for (j = 0; j < MARK_BITS_SZ; j++) {
	    q -> hb_marks[j] = 0;
        }
    }
}

/* Limits of stack for mark routine.  Set by caller to mark.           */
/* All items between mark_stack_top and mark_stack_bottom-1 still need */
/* to be marked.  All items on the stack satisfy quicktest.  They do   */
/* not necessarily reference real objects.                             */
word * mark_stack_bottom;
word * mark_stack_top;

#ifdef USE_STACK
# define STACKGAP 1024 /* Gap in longwords between hardware stack and	*/
		       /* the mark stack.				*/
		       /* Must suffice for printf calls and signal      */
		       /* handling.					*/
#endif


#ifdef USE_STACK
#   define PUSH_MS(ptr) *(--mark_stack_top) = (word) ptr
#   define NOT_DONE(a,b) (a < b)
#else
# ifdef USE_HEAP
    char *cur_break = 0;

#   define STACKINCR 0x4000
#   define PUSH_MS(ptr) 						\
	mark_stack_top++;                                               \
	if ((char*)mark_stack_top >= cur_break) { 			\
	    if (sbrk(STACKINCR) == -1) {				\
		fprintf(stderr, "sbrk failed, code = %d\n",errno);      \
		exit(1);						\
	    } else {							\
		cur_break += STACKINCR;                                \
	    }								\
	}								\
	*mark_stack_top = (word) ptr
#   define NOT_DONE(a,b) (a > b)
# else
	--> where does the mark stack go? <--
# endif
#endif


/* Mark all objects corresponding to pointers between mark_stack_bottom */
/* and mark_stack_top.  Assume that nested pointers are aligned         */
/* on alignment-byte boundaries.					*/
mark(alignment)
int alignment;
{
  register long sz;
  extern char end, etext;
  register struct obj *p; /* pointer to current object to be marked */

  while (NOT_DONE(mark_stack_top,mark_stack_bottom)) {
      register long word_no;
      register long mask;
      register struct hblk * h;

#    ifdef USE_STACK
	  p = (struct obj *)(*mark_stack_top++);
#    else
#     ifdef USE_HEAP
	p = (struct obj *)(*mark_stack_top--);
#     else
	--> fixit <--
#     endif
#    endif

  /* if not a pointer to obj on heap, skip it */
    if (((char *) p) >= heaplim) {
	continue;
    }

    h = HBLKPTR(p);

# ifndef INTERIOR_POINTERS
    /* Check mark bit first, since this test is much more likely to */
    /* fail than later ones.                                        */
      word_no = ((word *)p) - ((word *)h);
      if (mark_bit(h, word_no)) {
	continue;
      }
# endif

# ifdef INTERIOR_POINTERS
    if (!is_hblk(h)) {
	char m = get_map(h);
	while (m > 0 && m < 0x7f) {
	    h -= m;
	    m = get_map(h);
	}
	if (m == HBLK_INVALID) {
#         ifdef REPORT_FAILURE
	    gc_printf("-> Pointer to non-heap loc: %X\n", p);
#         endif
	  continue;
	}
    }
    if (((long)p) - ((long)h) < sizeof (struct hblkhdr)) {
	continue;
    }
# else
    if (!is_hblk(h)) {
#	ifdef REPORT_FAILURE
	  gc_printf("-> Pointer to non-heap loc: %X\n", p);
#       endif
	continue;
    }
# endif
    sz = HB_SIZE(h);
    mask = h -> hb_mask;

# ifdef INTERIOR_POINTERS
    word_no = get_word_no(p,h,sz,mask);
# else
    if (!is_proper_obj(p,h,sz,mask)) {
#       ifdef REPORT_FAILURE
	    gc_printf("-> Bad pointer to heap block: %X,sz = %d\n",p,sz);
#	endif
	continue;
    }
# endif

    if (word_no + sz > BYTES_TO_WORDS(HBLKSIZE)
	&& word_no != BYTES_TO_WORDS(sizeof(struct hblkhdr))
	   /* Not first object */) {
      /* 
       * Note that we dont necessarily check for pointers to the block header.
       * This doesn't cause any problems, since we have mark
       * bits allocated for such bogus objects.
       * We have to check for references past the last object, since
       * marking from uch an "object" could cause an exception.
       */
#       ifdef REPORT_FAILURE
	    gc_printf("-> Bad pointer to heap block: %X,sz = %d\n",p,sz);
#	endif
	continue;
    }

#   ifdef INTERIOR_POINTERS
      if (mark_bit(h, word_no)) {
	continue;
      }
#   endif

#   ifdef DEBUG2
	gc_printf("*** set bit for heap %x, word %x\n",h,word_no);
#   endif
    set_mark_bit(h, word_no);
    if (h -> hb_sz < 0) {
	/* Atomic object */
	  continue;
    }
    {
      /* Mark from fields inside the object */
	register struct obj ** q;
	register struct obj * r;
	register long lim;   /* Should be struct obj **, but we're out of */
			     /* A registers on a 68000.                   */

#       ifdef INTERIOR_POINTERS
	  /* Adjust p, so that it's properly aligned */
#           ifdef DEBUG
	      if (p != ((struct obj *)(((word *)h) + word_no))) {
		gc_printf("Adjusting from %X to ", p);
		p = ((struct obj *)(((word *)h) + word_no));
		gc_printf("%X\n", p);
	      } else {
		p = ((struct obj *)(((word *)h) + word_no));
	      }
#           else
	      p = ((struct obj *)(((word *)h) + word_no));
#           endif
#       endif
#       ifdef UNALIGNED
	  lim = ((long)(&(p -> obj_component[sz]))) - 3;
#       else
	  lim = (long)(&(p -> obj_component[sz]));
#       endif
	for (q = (struct obj **)(&(p -> obj_component[0]));
					q < (struct obj **)lim;) {
	    r = *q;
	    if (quicktest(r)) {
#               ifdef DEBUG2
		    gc_printf("Found plausible nested pointer");
		    gc_printf(": 0x%X inside 0x%X at 0x%X\n", r, p, q);
#               endif
		PUSH_MS(((word)r));
	    }
#           ifdef UNALIGNED
		q = ((struct obj **)(((long)q)+alignment));
#           else
		q++;
#           endif 
	}
    }
  }
}


/*********************************************************************/
/* Mark all locations reachable via pointers located between b and t */
/* b is the first location to be checked. t is one past the last     */
/* location to be checked.                                           */
/* Assume that pointers are aligned on alignment-byte                */
/* boundaries.							     */
/*********************************************************************/
void mark_all(b, t, alignment)
word * b;
word * t;
int alignment;
{
    register word *p;
    register word r;
    register word *lim;

#   ifdef DEBUG
	gc_printf("Checking for pointers between 0x%X and 0x%X\n",
		  b, t);
#   endif

    /* Allocate mark stack, leaving a hole below the real stack. */
#     ifdef USE_STACK
	mark_stack_bottom = get_current_sp() - STACKGAP;
	mark_stack_top = mark_stack_bottom;
#     else
#       ifdef USE_HEAP
	  mark_stack_bottom = (word *) sbrk(0); /* current break */
	  cur_break = (char *) mark_stack_bottom;
	  mark_stack_top = mark_stack_bottom;
#       else
	  -> then where should the mark stack go ? <-
#       endif
#     endif

  /* Round b down so it is properly aligned */
#   ifdef UNALIGNED
      if (alignment == 2) {
        b = (word *)(((long) b) & ~1);
      } else if (alignment == 4) {
	b = (word *)(((long) b) & ~3);
      } else if (alignment != 1) {
	fprintf(stderr, "Bad alignment parameter to mark_all\n");
	abort(alignment);
      }
#   else
      b = (word *)(((long) b) & ~3);
#   endif

  /* check all pointers in range and put on mark_stack if quicktest true */
    lim = t - 1 /* longword */;
    for (p = b; ((unsigned) p) <= ((unsigned) lim);) {
	    /* Coercion to unsigned in the preceding appears to be necessary */
	    /* due to a bug in the 4.2BSD C compiler.                        */
	r = *p;
	if (quicktest(r)) {
#           ifdef DEBUG2
		gc_printf("Found plausible pointer: %X\n", r);
#           endif
	    PUSH_MS(r);         /* push r onto the mark stack */
	}
#       ifdef UNALIGNED
	  p = (word *)(((char *)p) + alignment);
#       else
	  p++;
#       endif
    }
    if (mark_stack_top != mark_stack_bottom) mark(alignment);

#   ifdef USE_HEAP
      brk(mark_stack_bottom);     /* reset break to where it was before */
      cur_break = (char *) mark_stack_bottom;
#   endif
}

/*
 * Restore inaccessible objects to the free list 
 * update mem_found (number of reclaimed longwords after garbage collection)
 */
void gcollect()
{
    extern void mark_regs();

    extern int holdsigs();  /* disables non-urgent signals - see the	*/
			    /* file "callcc.c"				*/

    long Omask = 0;     /* mask to restore signal mask to after
			 * critical section.
			 */

#   ifdef PRINTTIMES
      /* some debugging values */
	double start_time = 0;
	double mark_time = 0;
	double done_time = 0;
	static struct tms time_buf;
#       define FTIME \
		 (((double)(time_buf.tms_utime + time_buf.tms_stime))/FLOAT_HZ)

      /* Get starting time */
	    times(&time_buf);
	    start_time = FTIME;
#   endif

#   ifdef DEBUG2
	gc_printf("Here we are in gcollect\n"); 
#   endif

    /* Don't want to deal with signals in the middle so mask 'em out */
	Omask = holdsigs();

    /* Mark from all roots.  */
	mark_roots();

#   ifdef FIND_LEAK
      /* Mark all objects on the free list.  All objects should be */
      /* marked when we're done.				   */
	{
	  register int size;		/* current object size		*/
	  register struct obj * p;	/* pointer to current object	*/
	  register struct hblk * q;	/* pointer to block containing *p */
	  register int word_no;           /* "index" of *p in *q          */

	  for (size = 1; size < MAXOBJSZ; size++) {
	    for (p= objfreelist[size]; p != ((struct obj *)0); p=p->obj_link){
		q = HBLKPTR(p);
		word_no = (((word *)p) - ((word *)q));
		set_mark_bit(q, word_no);
	    }
	  }
	  for (size = 1; size < MAXAOBJSZ; size++) {
	    for(p= aobjfreelist[size]; p != ((struct obj *)0); p=p->obj_link){
		q = HBLKPTR(p);
		word_no = (((long *)p) - ((long *)q));
		set_mark_bit(q, word_no);
	    }
	  }
	}
	/* Check that everything is marked */
	  reclaim(TRUE);
#   endif

    /* Clear free list mark bits, in case they got accidentally marked   */
    /* Note: HBLKPTR(p) == pointer to head of block containing *p        */
    /* Also subtract memory remaining from mem_found count.              */
    /* Note that composite objects on free list are cleared.             */
    /* Thus accidentally marking a free list is not a problem;  only     */
    /* objects on the list itself will be marked, and that's fixed here. */
      {
	register int size;		/* current object size		*/
	register struct obj * p;	/* pointer to current object	*/
	register struct hblk * q;	/* pointer to block containing *p */
	register int word_no;           /* "index" of *p in *q          */
#       ifdef REPORT_FAILURE
	    int prev_failure = 0;
#       endif

	for (size = 1; size < MAXOBJSZ; size++) {
	    for (p= objfreelist[size]; p != ((struct obj *)0); p=p->obj_link){
		q = HBLKPTR(p);
		word_no = (((word *)p) - ((word *)q));
#               ifdef REPORT_FAILURE
		  if (!prev_failure && mark_bit(q, word_no)) {
		    gc_printf("-> Pointer to composite free list: %X,sz = %d\n",
			      p, size);
		    prev_failure = 1;
		  }
#               endif
		clear_mark_bit(q, word_no);
		mem_found -= size;
	    }
#           ifdef REPORT_FAILURE
		prev_failure = 0;
#           endif
	}
	for (size = 1; size < MAXAOBJSZ; size++) {
	    for(p= aobjfreelist[size]; p != ((struct obj *)0); p=p->obj_link){
		q = HBLKPTR(p);
		word_no = (((long *)p) - ((long *)q));
#               ifdef REPORT_FAILURE
		  if (!prev_failure && mark_bit(q, word_no)) {
		    gc_printf("-> Pointer to atomic free list: %X,sz = %d\n",
			      p, size);
		    prev_failure = 1;
		  }
#               endif
		clear_mark_bit(q, word_no);
		mem_found -= size;
	    }
#           ifdef REPORT_FAILURE
		prev_failure = 0;
#           endif
	}
      }

#   ifdef PRINTTIMES
      /* Get intermediate time */
	times(&time_buf);
	mark_time = FTIME;
#   endif

#   ifdef PRINTSTATS
	gc_printf("Bytes recovered before reclaim - f.l. count = %d\n",
	          WORDS_TO_BYTES(mem_found));
#   endif

  /* Reconstruct free lists to contain everything not marked */
    reclaim(FALSE);

  /* clear mark bits in all allocated heap blocks */
    clear_marks();

#   ifdef PRINTSTATS
	gc_printf("Reclaimed %d bytes in heap of size %d bytes\n",
	          WORDS_TO_BYTES(mem_found), heapsize);
	gc_printf("%d (atomic) + %d (composite) bytes in use\n",
	          WORDS_TO_BYTES(atomic_in_use),
	          WORDS_TO_BYTES(composite_in_use));
#   endif

  /*
   * What follows is somewhat heuristic.  Constant may benefit
   * from tuning ...
   */
#   ifndef FIND_LEAK
    /* In the leak finding case, we expect gcollect to be called manually */
    /* before we're out of heap space.					  */
      if (WORDS_TO_BYTES(mem_found) * 4 < heapsize) {
        /* Less than about 1/4 of available memory was reclaimed - get more */
	  {
	    long size_to_get = HBLKSIZE + hincr * HBLKSIZE;
	    struct hblk * thishbp;
	    char * nheaplim;

	    thishbp = HBLKPTR(((unsigned)sbrk(0))+HBLKSIZE-1 );
	    nheaplim = (char *) (((unsigned)thishbp) + size_to_get);
	    if( ((char *) brk(nheaplim)) == ((char *)-1) ) {
		write(2,"Out of memory, trying to continue ...\n",38);
	    } else {
		heaplim = nheaplim;
		thishbp->hb_sz = 
		    BYTES_TO_WORDS(size_to_get - sizeof(struct hblkhdr));
		freehblk(thishbp);
		heapsize += size_to_get;
		update_hincr;
	    }
#           ifdef PRINTSTATS
		gc_printf("Gcollect: needed to increase heap size by %d\n",
		          size_to_get);
#           endif
	  }
      }
#  endif

   /* Reset mem_found for next collection */
     mem_found = 0;

  /* Reenable signals */
    sigsetmask(Omask);

  /* Get final time */
#   ifdef PRINTTIMES
	times(&time_buf);
	done_time = FTIME;
	gc_printf("Garbage collection took %d + %d msecs\n",
	          (int)(1000.0 * (mark_time - start_time)),
	          (int)(1000.0 * (done_time - mark_time)));
#   endif
}

/*
 * this explicitly increases the size of the heap.  It is used
 * internally, but my also be invoked directly by the user.
 * The argument is in units of HBLKSIZE.
 */
void expand_hp(n)
int n;
{
    struct hblk * thishbp = HBLKPTR(((unsigned)sbrk(0))+HBLKSIZE-1 );
    extern int holdsigs();
    int Omask;

    /* Don't want to deal with signals in the middle of this */
	Omask = holdsigs();

    heaplim = (char *) (((unsigned)thishbp) + n * HBLKSIZE);
    if (n > 2*hincr) {
	hincr = n/2;
    }
    if( ((char *) brk(heaplim)) == ((char *)-1) ) {
	write(2,"Out of Memory!\n",15);
	exit(-1);
    }
#   ifdef PRINTSTATS
	gc_printf("Voluntarily increasing heap size by %d\n",
	          n*HBLKSIZE);
#   endif
    thishbp->hb_sz = BYTES_TO_WORDS(n * HBLKSIZE - sizeof(struct hblkhdr));
    freehblk(thishbp);
    heapsize += ((char *)heaplim) - ((char *)thishbp);
    /* Reenable signals */
	sigsetmask(Omask);
}


extern int dont_gc;  /* Unsafe to start garbage collection */

/*
 * Make sure the composite object free list for sz is not empty.
 * Return a pointer to the first object on the free list.
 * The object MUST BE REMOVED FROM THE FREE LIST BY THE CALLER.
 *
 * note: _allocobj
 */
struct obj * _allocobj(sz)
long sz;
{
    if (sz == 0) return((struct obj *)0);

#   ifdef DEBUG2
	gc_printf("here we are in _allocobj\n");
#   endif

    if (objfreelist[sz] == ((struct obj *)0)) {
      if (hblkfreelist == ((struct hblk *)0) && !dont_gc) {
	if (GC_DIV * non_gc_bytes < GC_MULT * heapsize) {
#         ifdef DEBUG
	    gc_printf("Calling gcollect\n");
#         endif
	  gcollect();
	} else {
	  expand_hp(NON_GC_HINCR);
	}
      }
      if (objfreelist[sz] == ((struct obj *)0)) {
#       ifdef DEBUG
	    gc_printf("Calling new_hblk\n");
#	endif
	  new_hblk(sz);
      }
    }
#   ifdef DEBUG2
	gc_printf("Returning %x from _allocobj\n",objfreelist[sz]);
	gc_printf("Objfreelist[%d] = %x\n",sz,objfreelist[sz]);
#   endif
    return(objfreelist[sz]);
}

/*
 * Make sure the atomic object free list for sz is not empty.
 * Return a pointer to the first object on the free list.
 * The object MUST BE REMOVED FROM THE FREE LIST BY THE CALLER.
 *
 * note: this is called by allocaobj (see the file mach_dep.c)
 */
struct obj * _allocaobj(sz)
long sz;
{
    if (sz == 0) return((struct obj *)0);

    if (aobjfreelist[sz] == ((struct obj *) 0)) {
      if (hblkfreelist == ((struct hblk *)0) && !dont_gc) {
	if (GC_DIV * non_gc_bytes < GC_MULT * heapsize) {
#         ifdef DEBUG
	    gc_printf("Calling gcollect\n");
#         endif
	  gcollect();
	} else {
	  expand_hp(NON_GC_HINCR);
	}
      }
      if (aobjfreelist[sz] == ((struct obj *) 0)) {
	  new_hblk(-sz);
      }
    }
    return(aobjfreelist[sz]);
}

# ifdef SPARC
  put_mark_stack_bottom(val)
  long val;
  {
    mark_stack_bottom = (word *)val;
  }
# endif
