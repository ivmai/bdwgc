/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1993 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
 
#include <stdio.h>
#include "gc_private.h"

extern void GC_clear_stack();	/* in misc.c */

# ifdef ALL_INTERIOR_POINTERS
#   define SMALL_OBJ(bytes) ((bytes) < WORDS_TO_BYTES(MAXOBJSZ))
#   define ADD_SLOP(bytes) ((bytes)+1)
# else
#   define SMALL_OBJ(bytes) ((bytes) <= WORDS_TO_BYTES(MAXOBJSZ))
#   define ADD_SLOP(bytes) (bytes)
# endif


/* allocate lb bytes for an object of kind.	*/
/* Should not be used to directly to allocate	*/
/* objects such as STUBBORN objects that	*/
/* require special handling on allocation.	*/
/* First a version that assumes we already	*/
/* hold lock:					*/
ptr_t GC_generic_malloc_inner(lb, k)
register word lb;
register int k;
{
register word lw;
register ptr_t op;
register ptr_t *opp;

    if( SMALL_OBJ(lb) ) {
#       ifdef MERGE_SIZES
	  lw = GC_size_map[lb];
#	else
	  lw = ROUNDED_UP_WORDS(lb);
	  if (lw == 0) lw = 1;
#       endif
	opp = &(GC_obj_kinds[k].ok_freelist[lw]);
        if( (op = *opp) == 0 ) {
            if (!GC_is_initialized) {
                GC_init_inner();
                return(GC_generic_malloc_inner(lb, k));
            }
            GC_clear_stack();
	    op = GC_allocobj(lw, k);
	    if (op == 0) goto out;
        }
        /* Here everything is in a consistent state.	*/
        /* We assume the following assignment is	*/
        /* atomic.  If we get aborted			*/
        /* after the assignment, we lose an object,	*/
        /* but that's benign.				*/
        /* Volatile declarations may need to be added	*/
        /* to prevent the compiler from breaking things.*/
        *opp = obj_link(op);
        obj_link(op) = 0;
    } else {
	register struct hblk * h;
	register word n_blocks = divHBLKSZ(lb + HDR_BYTES + HBLKSIZE-1);
	
	if (!GC_is_initialized) GC_init_inner();
	/* Do our share of marking work */
          if(GC_incremental && !GC_dont_gc) GC_collect_a_little((int)n_blocks);
	lw = ROUNDED_UP_WORDS(lb);
	while ((h = GC_allochblk(lw, k)) == 0
		&& GC_collect_or_expand(n_blocks));
	if (h == 0) {
	    op = 0;
	} else {
	    op = (ptr_t) (h -> hb_body);
	    GC_words_wasted += BYTES_TO_WORDS(n_blocks * HBLKSIZE) - lw;
	}
    }
    GC_words_allocd += lw;
    
out:
    return((ptr_t)op);
}

ptr_t GC_generic_malloc(lb, k)
register word lb;
register int k;
{
    ptr_t result;
    DCL_LOCK_STATE;

    DISABLE_SIGNALS();
    LOCK();
    result = GC_generic_malloc_inner(lb, k);
    UNLOCK();
    ENABLE_SIGNALS();
    return(result);
}   


/* Analogous to the above, but assumes a small object size, and 	*/
/* bypasses MERGE_SIZES mechanism.  Used by gc_inline.h.		*/
ptr_t GC_generic_malloc_words_small(lw, k)
register word lw;
register int k;
{
register ptr_t op;
register ptr_t *opp;
DCL_LOCK_STATE;

    DISABLE_SIGNALS();
    LOCK();
    opp = &(GC_obj_kinds[k].ok_freelist[lw]);
    if( (op = *opp) == 0 ) {
        if (!GC_is_initialized) {
            GC_init_inner();
        }
        GC_clear_stack();
	op = GC_allocobj(lw, k);
	if (op == 0) goto out;
    }
    *opp = obj_link(op);
    obj_link(op) = 0;
    GC_words_allocd += lw;
    
out:
    UNLOCK();
    ENABLE_SIGNALS();
    return((ptr_t)op);
}

/* Allocate lb bytes of atomic (pointerfree) data */
# ifdef __STDC__
    extern_ptr_t GC_malloc_atomic(size_t lb)
# else
    extern_ptr_t GC_malloc_atomic(lb)
    size_t lb;
# endif
{
register ptr_t op;
register ptr_t * opp;
register word lw;
DCL_LOCK_STATE;

    if( SMALL_OBJ(lb) ) {
#       ifdef MERGE_SIZES
	  lw = GC_size_map[lb];
#	else
	  lw = ROUNDED_UP_WORDS(lb);
#       endif
	opp = &(GC_aobjfreelist[lw]);
	FASTLOCK();
        if( !FASTLOCK_SUCCEEDED() || (op = *opp) == 0 ) {
            FASTUNLOCK();
            return(GC_generic_malloc((word)lb, PTRFREE));
        }
        /* See above comment on signals.	*/
        *opp = obj_link(op);
        GC_words_allocd += lw;
        FASTUNLOCK();
        return((extern_ptr_t) op);
   } else {
       return((extern_ptr_t)
       		GC_generic_malloc((word)lb, PTRFREE));
   }
}

/* Allocate lb bytes of composite (pointerful) data */
# ifdef __STDC__
    extern_ptr_t GC_malloc(size_t lb)
# else
    extern_ptr_t GC_malloc(lb)
    size_t lb;
# endif
{
register ptr_t op;
register ptr_t *opp;
register word lw;
DCL_LOCK_STATE;

    if( SMALL_OBJ(lb) ) {
#       ifdef MERGE_SIZES
	  lw = GC_size_map[lb];
#	else
	  lw = ROUNDED_UP_WORDS(lb);
#       endif
	opp = &(GC_objfreelist[lw]);
	FASTLOCK();
        if( !FASTLOCK_SUCCEEDED() || (op = *opp) == 0 ) {
            FASTUNLOCK();
            return(GC_generic_malloc((word)lb, NORMAL));
        }
        /* See above comment on signals.	*/
        *opp = obj_link(op);
        obj_link(op) = 0;
        GC_words_allocd += lw;
        FASTUNLOCK();
        return((extern_ptr_t) op);
   } else {
       return((extern_ptr_t)
          	GC_generic_malloc((word)lb, NORMAL));
   }
}

/* Allocate lb bytes of pointerful, traced, but not collectable data */
# ifdef __STDC__
    extern_ptr_t GC_malloc_uncollectable(size_t lb)
# else
    extern_ptr_t GC_malloc_uncollectable(lb)
    size_t lb;
# endif
{
register ptr_t op;
register ptr_t *opp;
register word lw;
DCL_LOCK_STATE;

    if( SMALL_OBJ(lb) ) {
#       ifdef MERGE_SIZES
	  lw = GC_size_map[lb];
#	else
	  lw = ROUNDED_UP_WORDS(lb);
#       endif
	opp = &(GC_uobjfreelist[lw]);
	FASTLOCK();
        if( FASTLOCK_SUCCEEDED() && (op = *opp) != 0 ) {
            /* See above comment on signals.	*/
            *opp = obj_link(op);
            obj_link(op) = 0;
            GC_words_allocd += lw;
            GC_set_mark_bit(op);
            GC_non_gc_bytes += WORDS_TO_BYTES(lw);
            FASTUNLOCK();
            return((extern_ptr_t) op);
        }
        FASTUNLOCK();
        op = (ptr_t)GC_generic_malloc((word)lb, UNCOLLECTABLE);
    } else {
	op = (ptr_t)GC_generic_malloc((word)lb, UNCOLLECTABLE);
    }
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
	return((extern_ptr_t) op);
    }
}

extern_ptr_t GC_generic_or_special_malloc(lb,knd)
word lb;
int knd;
{
    switch(knd) {
#     ifdef STUBBORN_ALLOC
	case STUBBORN:
	    return(GC_malloc_stubborn((size_t)lb));
#     endif
	case UNCOLLECTABLE:
	    return(GC_malloc_uncollectable((size_t)lb));
	default:
	    return(GC_generic_malloc(lb,knd));
    }
}


/* Change the size of the block pointed to by p to contain at least   */
/* lb bytes.  The object may be (and quite likely will be) moved.     */
/* The kind (e.g. atomic) is the same as that of the old.	      */
/* Shrinking of large blocks is not implemented well.                 */
# ifdef __STDC__
    extern_ptr_t GC_realloc(extern_ptr_t p, size_t lb)
# else
    extern_ptr_t GC_realloc(p,lb)
    extern_ptr_t p;
    size_t lb;
# endif
{
register struct hblk * h;
register hdr * hhdr;
register signed_word sz;	 /* Current size in bytes	*/
register word orig_sz;	 /* Original sz in bytes	*/
int obj_kind;

    if (p == 0) return(GC_malloc(lb));	/* Required by ANSI */
    h = HBLKPTR(p);
    hhdr = HDR(h);
    sz = hhdr -> hb_sz;
    obj_kind = hhdr -> hb_obj_kind;
    sz = WORDS_TO_BYTES(sz);
    orig_sz = sz;

    if (sz > WORDS_TO_BYTES(MAXOBJSZ)) {
	/* Round it up to the next whole heap block */
	  
	  sz = (sz+HDR_BYTES+HBLKSIZE-1)
		& (~HBLKMASK);
	  sz -= HDR_BYTES;
	  hhdr -> hb_sz = BYTES_TO_WORDS(sz);
	  if (obj_kind == UNCOLLECTABLE) GC_non_gc_bytes += (sz - orig_sz);
	  /* Extra area is already cleared by allochblk. */
    }
    if (ADD_SLOP(lb) <= sz) {
	if (lb >= (sz >> 1)) {
#	    ifdef STUBBORN_ALLOC
	        if (obj_kind == STUBBORN) GC_change_stubborn(p);
#	    endif
	    if (orig_sz > lb) {
	      /* Clear unneeded part of object to avoid bogus pointer */
	      /* tracing.					      */
	      /* Safe for stubborn objects.			      */
	        bzero(((char *)p) + lb, (int)(orig_sz - lb));
	    }
	    return(p);
	} else {
	    /* shrink */
	      extern_ptr_t result =
	      		GC_generic_or_special_malloc((word)lb, obj_kind);

	      if (result == 0) return(0);
	          /* Could also return original object.  But this 	*/
	          /* gives the client warning of imminent disaster.	*/
	      bcopy(p, result, (int)lb);
	      GC_free(p);
	      return(result);
	}
    } else {
	/* grow */
	  extern_ptr_t result =
	  	GC_generic_or_special_malloc((word)lb, obj_kind);

	  if (result == 0) return(0);
	  bcopy(p, result, (int)sz);
	  GC_free(p);
	  return(result);
    }
}

/* Explicitly deallocate an object p.				*/
# ifdef __STDC__
    void GC_free(extern_ptr_t p)
# else
    void GC_free(p)
    extern_ptr_t p;
# endif
{
    register struct hblk *h;
    register hdr *hhdr;
    register signed_word sz;
    register ptr_t * flh;
    register int knd;
    register struct obj_kind * ok;
    DCL_LOCK_STATE;

    if (p == 0) return;
    	/* Required by ANSI.  It's not my fault ...	*/
    h = HBLKPTR(p);
    hhdr = HDR(h);
    knd = hhdr -> hb_obj_kind;
    sz = hhdr -> hb_sz;
    ok = &GC_obj_kinds[knd];
    if (sz <= MAXOBJSZ) {
	LOCK();
	GC_mem_freed += sz;
	/* A signal here can make GC_mem_freed and GC_non_gc_bytes	*/
	/* inconsistent.  We claim this is benign.			*/
	if (knd == UNCOLLECTABLE) GC_non_gc_bytes -= sz;
	if (ok -> ok_init) {
	    bzero((char *)((word *)p + 1), (int)(WORDS_TO_BYTES(sz-1)));
	}
	flh = &(ok -> ok_freelist[sz]);
	obj_link(p) = *flh;
	*flh = (ptr_t)p;
	UNLOCK();
    } else {
    	DISABLE_SIGNALS();
        LOCK();
        GC_mem_freed += sz;
	if (knd == UNCOLLECTABLE) GC_non_gc_bytes -= sz;
        GC_freehblk(h);
        UNLOCK();
        ENABLE_SIGNALS();
    }
}
