/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 2000 by Hewlett-Packard Company.  All rights reserved.
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

/*
 * These are extra allocation routines which are likely to be less
 * frequently used than those in malloc.c.  They are separate in the
 * hope that the .o file will be excluded from statically linked
 * executables.  We should probably break this up further.
 */

#include <stdio.h>
#include "private/gc_priv.h"

extern ptr_t GC_clear_stack();  /* in misc.c, behaves like identity */
void GC_extend_size_map();      /* in misc.c. */
GC_bool GC_alloc_reclaim_list();	/* in malloc.c */

/* Some externally visible but unadvertised variables to allow access to */
/* free lists from inlined allocators without including gc_priv.h	 */
/* or introducing dependencies on internal data structure layouts.	 */
ptr_t * GC_CONST GC_objfreelist_ptr = GC_objfreelist;
ptr_t * GC_CONST GC_aobjfreelist_ptr = GC_aobjfreelist;
ptr_t * GC_CONST GC_uobjfreelist_ptr = GC_uobjfreelist;
# ifdef ATOMIC_UNCOLLECTABLE
    ptr_t * GC_CONST GC_auobjfreelist_ptr = GC_auobjfreelist;
# endif



/* Allocate a composite object of size n bytes.  The caller guarantees  */
/* that pointers past the first page are not relevant.  Caller holds    */
/* allocation lock.                                                     */
ptr_t GC_generic_malloc_inner_ignore_off_page(lb, k)
register size_t lb;
register int k;
{
    register word lw;
    ptr_t op;

    if (lb <= HBLKSIZE)
        return(GC_generic_malloc_inner((word)lb, k));
    lw = ROUNDED_UP_WORDS(lb);
    op = (ptr_t)GC_alloc_large_and_clear(lw, k, IGNORE_OFF_PAGE);
    GC_words_allocd += lw;
    return op;
}

/* The same thing, except caller does not hold allocation lock.	*/
/* We avoid holding allocation lock while we clear memory.	*/
ptr_t GC_generic_malloc_ignore_off_page(lb, k)
register size_t lb;
register int k;
{
    register ptr_t result;
    word lw;
    word n_blocks;
    GC_bool init;
    DCL_LOCK_STATE;
    
    if (SMALL_OBJ(lb))
        return(GC_generic_malloc((word)lb, k));
    lw = ROUNDED_UP_WORDS(lb);
    n_blocks = OBJ_SZ_TO_BLOCKS(lw);
    init = GC_obj_kinds[k].ok_init;
    GC_INVOKE_FINALIZERS();
    DISABLE_SIGNALS();
    LOCK();
    result = (ptr_t)GC_alloc_large(lw, k, IGNORE_OFF_PAGE);
    if (GC_debugging_started) {
	BZERO(result, n_blocks * HBLKSIZE - HDR_BYTES);
    }
    GC_words_allocd += lw;
    UNLOCK();
    ENABLE_SIGNALS();
    if (0 == result) {
        return((*GC_oom_fn)(lb));
    } else {
    	if (init & !GC_debugging_started) {
	    BZERO(result, n_blocks * HBLKSIZE - HDR_BYTES);
        }
        return(result);
    }
}

# if defined(__STDC__) || defined(__cplusplus)
  void * GC_malloc_ignore_off_page(size_t lb)
# else
  char * GC_malloc_ignore_off_page(lb)
  register size_t lb;
# endif
{
    return((GC_PTR)GC_generic_malloc_ignore_off_page(lb, NORMAL));
}

# if defined(__STDC__) || defined(__cplusplus)
  void * GC_malloc_atomic_ignore_off_page(size_t lb)
# else
  char * GC_malloc_atomic_ignore_off_page(lb)
  register size_t lb;
# endif
{
    return((GC_PTR)GC_generic_malloc_ignore_off_page(lb, PTRFREE));
}

/* Increment GC_words_allocd from code that doesn't have direct access 	*/
/* to GC_arrays.							*/
# ifdef __STDC__
void GC_incr_words_allocd(size_t n)
{
    GC_words_allocd += n;
}

/* The same for GC_mem_freed.				*/
void GC_incr_mem_freed(size_t n)
{
    GC_mem_freed += n;
}
# endif /* __STDC__ */

/* Analogous to the above, but assumes a small object size, and 	*/
/* bypasses MERGE_SIZES mechanism.  Used by gc_inline.h.		*/
ptr_t GC_generic_malloc_words_small_inner(lw, k)
register word lw;
register int k;
{
register ptr_t op;
register ptr_t *opp;
register struct obj_kind * kind = GC_obj_kinds + k;

    opp = &(kind -> ok_freelist[lw]);
    if( (op = *opp) == 0 ) {
        if (!GC_is_initialized) {
            GC_init_inner();
        }
	if (kind -> ok_reclaim_list != 0 || GC_alloc_reclaim_list(kind)) {
	    op = GC_clear_stack(GC_allocobj((word)lw, k));
	}
	if (op == 0) {
	    UNLOCK();
	    ENABLE_SIGNALS();
	    return ((*GC_oom_fn)(WORDS_TO_BYTES(lw)));
	}
    }
    *opp = obj_link(op);
    obj_link(op) = 0;
    GC_words_allocd += lw;
    return((ptr_t)op);
}

/* Analogous to the above, but assumes a small object size, and 	*/
/* bypasses MERGE_SIZES mechanism.  Used by gc_inline.h.		*/
#ifdef __STDC__
     ptr_t GC_generic_malloc_words_small(size_t lw, int k)
#else 
     ptr_t GC_generic_malloc_words_small(lw, k)
     register word lw;
     register int k;
#endif
{
register ptr_t op;
DCL_LOCK_STATE;

    GC_INVOKE_FINALIZERS();
    DISABLE_SIGNALS();
    LOCK();
    op = GC_generic_malloc_words_small_inner(lw, k);
    UNLOCK();
    ENABLE_SIGNALS();
    return((ptr_t)op);
}

#if defined(THREADS) && !defined(SRC_M3)

extern signed_word GC_mem_found;   /* Protected by GC lock.  */

#ifdef PARALLEL_MARK
volatile signed_word GC_words_allocd_tmp = 0;
                        /* Number of words of memory allocated since    */
                        /* we released the GC lock.  Instead of         */
                        /* reacquiring the GC lock just to add this in, */
                        /* we add it in the next time we reacquire      */
                        /* the lock.  (Atomically adding it doesn't     */
                        /* work, since we would have to atomically      */
                        /* update it in GC_malloc, which is too         */
                        /* expensive.                                   */
#endif /* PARALLEL_MARK */

/* See reclaim.c: */
extern ptr_t GC_reclaim_generic();

/* Return a list of 1 or more objects of the indicated size, linked	*/
/* through the first word in the object.  This has the advantage that	*/
/* it acquires the allocation lock only once, and may greatly reduce	*/
/* time wasted contending for the allocation lock.  Typical usage would */
/* be in a thread that requires many items of the same size.  It would	*/
/* keep its own free list in thread-local storage, and call		*/
/* GC_malloc_many or friends to replenish it.  (We do not round up	*/
/* object sizes, since a call indicates the intention to consume many	*/
/* objects of exactly this size.)					*/
/* Note that the client should usually clear the link field.		*/
ptr_t GC_generic_malloc_many(lb, k)
register word lb;
register int k;
{
ptr_t op;
ptr_t p;
ptr_t *opp;
word lw;
word my_words_allocd = 0;
struct obj_kind * ok = &(GC_obj_kinds[k]);
DCL_LOCK_STATE;

#   if defined(GATHERSTATS) || defined(PARALLEL_MARK)
#     define COUNT_ARG , &my_words_allocd
#   else
#     define COUNT_ARG
#     define NEED_TO_COUNT
#   endif
    if (!SMALL_OBJ(lb)) {
        op = GC_generic_malloc(lb, k);
        if(0 != op) obj_link(op) = 0;
        return(op);
    }
    lw = ALIGNED_WORDS(lb);
    GC_INVOKE_FINALIZERS();
    DISABLE_SIGNALS();
    LOCK();
    /* First see if we can reclaim a page of objects waiting to be */
    /* reclaimed.						   */
    {
	struct hblk ** rlh = ok -> ok_reclaim_list;
	struct hblk * hbp;
	hdr * hhdr;

  	if (rlh == 0) return;	/* No blocks of this kind.	*/
	rlh += lw;
    	while ((hbp = *rlh) != 0) {
            hhdr = HDR(hbp);
            *rlh = hhdr -> hb_next;
#	    ifdef PARALLEL_MARK
		{
		  signed_word my_words_allocd_tmp = GC_words_allocd_tmp;

		  GC_ASSERT(my_words_allocd_tmp >= 0);
		  /* We only decrement it while holding the GC lock.	*/
		  /* Thus we can't accidentally adjust it down in more	*/
		  /* than one thread simultaneously.			*/
		  if (my_words_allocd_tmp != 0) {
		    (void)GC_atomic_add(&GC_words_allocd_tmp,
					-my_words_allocd_tmp);
		    GC_words_allocd += my_words_allocd_tmp;
		  }
		}
		GC_acquire_mark_lock();
		++ GC_fl_builder_count;
		UNLOCK();
		ENABLE_SIGNALS();
		GC_release_mark_lock();
#	    endif
	    op = GC_reclaim_generic(hbp, hhdr, lw,
				    ok -> ok_init, 0 COUNT_ARG);
            if (op != 0) {
#	      ifdef NEED_TO_COUNT
		/* We are neither gathering statistics, nor marking in	*/
		/* parallel.  Thus GC_reclaim_generic doesn't count	*/
		/* for us.						*/
    		for (p = op; p != 0; p = obj_link(p)) {
        	  my_words_allocd += lw;
		}
#	      endif
#	      if defined(GATHERSTATS)
	        /* We also reclaimed memory, so we need to adjust 	*/
	        /* that count.						*/
		/* This should be atomic, so the results may be		*/
		/* inaccurate.						*/
		GC_mem_found += my_words_allocd;
#	      endif
#	      ifdef PARALLEL_MARK
	        (void)GC_atomic_add(&GC_words_allocd_tmp, my_words_allocd);
		GC_acquire_mark_lock();
		-- GC_fl_builder_count;
		if (GC_fl_builder_count == 0) GC_notify_all_builder();
		GC_release_mark_lock();
		return op;
#	      else
	        GC_words_allocd += my_words_allocd;
	        goto out;
#	      endif
	    }
#	    ifdef PARALLEL_MARK
	      GC_acquire_mark_lock();
	      -- GC_fl_builder_count;
	      if (GC_fl_builder_count == 0) GC_notify_all_builder();
	      GC_release_mark_lock();
	      DISABLE_SIGNALS();
	      LOCK();
	      /* GC lock is needed for reclaim list access.	We	*/
	      /* must decrement fl_builder_count before reaquiring GC	*/
	      /* lock.  Hopefully this path is rare.			*/
#	    endif
    	}
    }
    /* Next try to allocate a new block worth of objects of this size.	*/
    {
	struct hblk *h = GC_allochblk(lw, k, 0);
	if (h != 0) {
	  if (IS_UNCOLLECTABLE(k)) GC_set_hdr_marks(HDR(h));
	  GC_words_allocd += BYTES_TO_WORDS(HBLKSIZE)
			       - BYTES_TO_WORDS(HBLKSIZE) % lw;
#	  ifdef PARALLEL_MARK
	    GC_acquire_mark_lock();
	    ++ GC_fl_builder_count;
	    UNLOCK();
	    ENABLE_SIGNALS();
	    GC_release_mark_lock();
#	  endif

	  op = GC_build_fl(h, lw, ok -> ok_init, 0);
#	  ifdef PARALLEL_MARK
	    GC_acquire_mark_lock();
	    -- GC_fl_builder_count;
	    if (GC_fl_builder_count == 0) GC_notify_all_builder();
	    GC_release_mark_lock();
	    return op;
#	  else
	    goto out;
#	  endif
	}
    }
    
    op = GC_generic_malloc_inner(lb, k);
    obj_link(op) = 0;
    
#   ifndef PARALLEL_MARK
      out:
#   endif
    UNLOCK();
    ENABLE_SIGNALS();
    return(op);
}

void * GC_malloc_many(size_t lb)
{
    return(GC_generic_malloc_many(lb, NORMAL));
}

/* Note that the "atomic" version of this would be unsafe, since the	*/
/* links would not be seen by the collector.				*/
# endif

/* Allocate lb bytes of pointerful, traced, but not collectable data */
# ifdef __STDC__
    GC_PTR GC_malloc_uncollectable(size_t lb)
# else
    GC_PTR GC_malloc_uncollectable(lb)
    size_t lb;
# endif
{
register ptr_t op;
register ptr_t *opp;
register word lw;
DCL_LOCK_STATE;

    if( SMALL_OBJ(lb) ) {
#       ifdef MERGE_SIZES
#	  ifdef ADD_BYTE_AT_END
	    if (lb != 0) lb--;
	    	  /* We don't need the extra byte, since this won't be	*/
	    	  /* collected anyway.					*/
#	  endif
	  lw = GC_size_map[lb];
#	else
	  lw = ALIGNED_WORDS(lb);
#       endif
	opp = &(GC_uobjfreelist[lw]);
	FASTLOCK();
        if( FASTLOCK_SUCCEEDED() && (op = *opp) != 0 ) {
            /* See above comment on signals.	*/
            *opp = obj_link(op);
            obj_link(op) = 0;
            GC_words_allocd += lw;
            /* Mark bit ws already set on free list.  It will be	*/
	    /* cleared only temporarily during a collection, as a 	*/
	    /* result of the normal free list mark bit clearing.	*/
            GC_non_gc_bytes += WORDS_TO_BYTES(lw);
            FASTUNLOCK();
            return((GC_PTR) op);
        }
        FASTUNLOCK();
        op = (ptr_t)GC_generic_malloc((word)lb, UNCOLLECTABLE);
    } else {
	op = (ptr_t)GC_generic_malloc((word)lb, UNCOLLECTABLE);
    }
    if (0 == op) return(0);
    /* We don't need the lock here, since we have an undisguised 	*/
    /* pointer.  We do need to hold the lock while we adjust		*/
    /* mark bits.							*/
    {
	register struct hblk * h;
	
	h = HBLKPTR(op);
	lw = HDR(h) -> hb_sz;
	
	DISABLE_SIGNALS();
	LOCK();
	GC_set_mark_bit(op);
	GC_non_gc_bytes += WORDS_TO_BYTES(lw);
	UNLOCK();
	ENABLE_SIGNALS();
	return((GC_PTR) op);
    }
}

# ifdef ATOMIC_UNCOLLECTABLE
/* Allocate lb bytes of pointerfree, untraced, uncollectable data 	*/
/* This is normally roughly equivalent to the system malloc.		*/
/* But it may be useful if malloc is redefined.				*/
# ifdef __STDC__
    GC_PTR GC_malloc_atomic_uncollectable(size_t lb)
# else
    GC_PTR GC_malloc_atomic_uncollectable(lb)
    size_t lb;
# endif
{
register ptr_t op;
register ptr_t *opp;
register word lw;
DCL_LOCK_STATE;

    if( SMALL_OBJ(lb) ) {
#       ifdef MERGE_SIZES
#	  ifdef ADD_BYTE_AT_END
	    if (lb != 0) lb--;
	    	  /* We don't need the extra byte, since this won't be	*/
	    	  /* collected anyway.					*/
#	  endif
	  lw = GC_size_map[lb];
#	else
	  lw = ALIGNED_WORDS(lb);
#       endif
	opp = &(GC_auobjfreelist[lw]);
	FASTLOCK();
        if( FASTLOCK_SUCCEEDED() && (op = *opp) != 0 ) {
            /* See above comment on signals.	*/
            *opp = obj_link(op);
            obj_link(op) = 0;
            GC_words_allocd += lw;
	    /* Mark bit was already set while object was on free list. */
            GC_non_gc_bytes += WORDS_TO_BYTES(lw);
            FASTUNLOCK();
            return((GC_PTR) op);
        }
        FASTUNLOCK();
        op = (ptr_t)GC_generic_malloc((word)lb, AUNCOLLECTABLE);
    } else {
	op = (ptr_t)GC_generic_malloc((word)lb, AUNCOLLECTABLE);
    }
    if (0 == op) return(0);
    /* We don't need the lock here, since we have an undisguised 	*/
    /* pointer.  We do need to hold the lock while we adjust		*/
    /* mark bits.							*/
    {
	register struct hblk * h;
	
	h = HBLKPTR(op);
	lw = HDR(h) -> hb_sz;
	
	DISABLE_SIGNALS();
	LOCK();
	GC_set_mark_bit(op);
	GC_non_gc_bytes += WORDS_TO_BYTES(lw);
	UNLOCK();
	ENABLE_SIGNALS();
	return((GC_PTR) op);
    }
}

#endif /* ATOMIC_UNCOLLECTABLE */
