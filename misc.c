/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991,1992 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */

#define DEBUG       /* Some run-time consistency checks */
#undef DEBUG
#define VERBOSE
#undef VERBOSE

#include <stdio.h>
#include <signal.h>
#define I_HIDE_POINTERS	/* To make GC_call_with_alloc_lock visible */
#include "gc_private.h"

# ifdef THREADS
#   ifdef PCR
#     include "pcr/il/PCR_IL.h"
      struct PCR_Th_MLRep GC_allocate_ml;
#   else
	--> declare allocator lock here
#   endif
# endif

struct _GC_arrays GC_arrays = { 0 };

/* Initialize GC_obj_kinds properly and standard free lists properly.  	*/
/* This must be done statically since they may be accessed before 	*/
/* GC_init is called.							*/
struct obj_kind GC_obj_kinds[MAXOBJKINDS] = {
/* PTRFREE */ { GC_aobjfreelist, GC_areclaim_list, GC_no_mark_proc, FALSE },
/* NORMAL  */ { GC_objfreelist, GC_reclaim_list, GC_normal_mark_proc, TRUE },
};

ptr_t GC_stackbottom = 0;

word GC_hincr;

int GC_n_kinds = 2;

bool GC_dont_gc = 0;

extern signed_word GC_mem_found;

# ifdef ALL_INTERIOR_POINTERS
#   define ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1))
# else
#   define ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1) - 1)
# endif

# ifdef MERGE_SIZES
    /* Set things up so that GC_size_map[i] >= words(i),		*/
    /* but not too much bigger						*/
    /* and so that size_map contains relatively few distinct entries 	*/
    /* This is stolen from Russ Atkinson's Cedar quantization		*/
    /* alogrithm (but we precompute it).				*/
    
#   if (CPP_WORDSZ != 32) 
  	--> fix the following code
#   endif



    void GC_init_size_map()
    {
	register unsigned i;
	register unsigned sz_rounded_up = 0;

	/* Map size 0 to 1.  This avoids problems at lower levels. */
	  GC_size_map[0] = 1;
	/* One word objects don't have to be 2 word aligned.	   */
	  GC_size_map[1] = 1;
	  GC_size_map[2] = 1;
	  GC_size_map[3] = 1;
	  GC_size_map[4] = ROUNDED_UP_WORDS(4);
	for (i = 5; i <= 32; i++) {
#           ifdef ALIGN_DOUBLE
	      GC_size_map[i] = (ROUNDED_UP_WORDS(i) + 1) & (~1);
#           else
	      GC_size_map[i] = ROUNDED_UP_WORDS(i);
#           endif
	}
	
	for (i = 33; i <= WORDS_TO_BYTES(MAXOBJSZ); i++) {
	    if (sz_rounded_up < ROUNDED_UP_WORDS(i)) {
	        register int size = ROUNDED_UP_WORDS(i);
                register unsigned m = 0;
            
                while (size > 7) {
                  m += 1;
                  size += 1;
                  size >>= 1;
                }
	        sz_rounded_up = size << m;
		if (sz_rounded_up > MAXOBJSZ) {
		    sz_rounded_up = MAXOBJSZ;
		}
	    }
	    GC_size_map[i] = sz_rounded_up;
	}
    }
# endif

# ifdef ALL_INTERIOR_POINTERS
#   define SMALL_OBJ(bytes) ((bytes) < WORDS_TO_BYTES(MAXOBJSZ))
#   define ADD_SLOP(bytes) ((bytes)+1)
# else
#   define SMALL_OBJ(bytes) ((bytes) <= WORDS_TO_BYTES(MAXOBJSZ))
#   define ADD_SLOP(bytes) (bytes)
# endif

/*
 * The following is a gross hack to deal with a problem that can occur
 * on machines that are sloppy about stack frame sizes, notably SPARC.
 * Bogus pointers may be written to the stack and not cleared for
 * a LONG time, because they always fall into holes in stack frames
 * that are not written.  We partially address this by randomly clearing
 * sections of the stack whenever we get control.
 */
word GC_stack_last_cleared = 0;	/* GC_no when we last did this */
# define CLEAR_SIZE 213
# define CLEAR_THRESHOLD 10000
# define DEGRADE_RATE 50

ptr_t GC_min_sp;	/* Coolest stack pointer value from which we've */
			/* already cleared the stack.			*/
			
# ifdef STACK_GROWS_DOWN
#   define COOLER_THAN >
#   define HOTTER_THAN <
#   define MAKE_COOLER(x,y) if ((word)(x)+(y) > (word)(x)) {(x) += (y);} \
			    else {(x) = (ptr_t)ONES;}
#   define MAKE_HOTTER(x,y) (x) -= (y)
# else
#   define COOLER_THAN <
#   define HOTTER_THAN >
#   define MAKE_COOLER(x,y) if ((word)(x)-(y) < (word)(x)) {(x) -= (y);} else {(x) = 0;}
#   define MAKE_HOTTER(x,y) (x) += (y)
# endif

ptr_t GC_high_water;
			/* "hottest" stack pointer value we have seen	*/
			/* recently.  Degrades over time.		*/
/*ARGSUSED*/
void GC_clear_stack_inner(d)
word *d;
{
    word dummy[CLEAR_SIZE];
    
    bzero((char *)dummy, (int)(CLEAR_SIZE*sizeof(word)));
#   ifdef THREADS
  	GC_noop(dummy);
#   else
        if ((ptr_t)(dummy) COOLER_THAN GC_min_sp) {
            GC_clear_stack_inner(dummy);
        }
#   endif
}

void GC_clear_stack()
{
    word dummy;


# ifdef THREADS
    GC_clear_stack_inner(&dummy);
# else
    if (GC_gc_no > GC_stack_last_cleared) {
        /* Start things over, so we clear the entire stack again */
        if (GC_stack_last_cleared == 0) GC_high_water = GC_stackbottom;
        GC_min_sp = GC_high_water;
        GC_stack_last_cleared = GC_gc_no;
    }
    /* Adjust GC_high_water */
        MAKE_COOLER(GC_high_water, WORDS_TO_BYTES(DEGRADE_RATE));
        if ((word)(&dummy) HOTTER_THAN (word)GC_high_water) {
            GC_high_water = (ptr_t)(&dummy);
        }
    if ((word)(&dummy) COOLER_THAN (word)GC_min_sp) {
        GC_clear_stack_inner(&dummy);
        GC_min_sp = (ptr_t)(&dummy);
    }
# endif
}

/* allocate lb bytes for an object of kind k */
ptr_t GC_generic_malloc(lb, k)
register word lb;
register int k;
{
register word lw;
register ptr_t op;
register ptr_t *opp;
DCL_LOCK_STATE;

    DISABLE_SIGNALS();
    LOCK();
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
                ENABLE_SIGNALS();
                /* This may have fixed GC_size_map */
                UNLOCK();
                return(GC_generic_malloc(lb, k));
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
	
	if (!GC_is_initialized) GC_init_inner();
	lw = ROUNDED_UP_WORDS(lb);
	if (!GC_sufficient_hb(lw, k) && !GC_dont_gc) {
            GC_gcollect_inner(FALSE);
	}
	h = GC_allochblk(lw, k);
	if (h == 0) {
	    op = 0;
	} else {
	    op = (ptr_t) (h -> hb_body);
	}
    }
    GC_words_allocd += lw;
    
out:
    UNLOCK();
    ENABLE_SIGNALS();
    return((ptr_t)op);
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

    LOCK();
    DISABLE_SIGNALS();
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
	  /* Extra area is already cleared by allochblk. */
    }
    if (ADD_SLOP(lb) <= sz) {
	if (lb >= (sz >> 1)) {
	    if (orig_sz > lb) {
	      /* Clear unneeded part of object to avoid bogus pointer */
	      /* tracing.					      */
	        bzero(((char *)p) + lb, (int)(orig_sz - lb));
	    }
	    return(p);
	} else {
	    /* shrink */
	      extern_ptr_t result = GC_generic_malloc((word)lb, obj_kind);

	      if (result == 0) return(0);
	          /* Could also return original object.  But this 	*/
	          /* gives the client warning of imminent disaster.	*/
	      bcopy(p, result, (int)lb);
	      GC_free(p);
	      return(result);
	}
    } else {
	/* grow */
	  extern_ptr_t result = GC_generic_malloc((word)lb, obj_kind);

	  if (result == 0) return(0);
	  bcopy(p, result, (int)sz);
	  GC_free(p);
	  return(result);
    }
}

/* Return a pointer to the base address of p, given a pointer to a	*/
/* an address within an object.  Return 0 o.w.				*/
# ifdef __STDC__
    extern_ptr_t GC_base(extern_ptr_t p)
# else
    extern_ptr_t GC_base(p)
    extern_ptr_t p;
# endif
{
    register word r;
    register struct hblk *h;
    register hdr *candidate_hdr;
    
    r = (word)p;
    h = HBLKPTR(r);
    candidate_hdr = HDR(r);
    if (candidate_hdr == 0) return(0);
    /* If it's a pointer to the middle of a large object, move it	*/
    /* to the beginning.						*/
	while (IS_FORWARDING_ADDR_OR_NIL(candidate_hdr)) {
	   h = h - (int)candidate_hdr;
	   r = (word)h + HDR_BYTES;
	   candidate_hdr = HDR(h);
	}
    if (candidate_hdr -> hb_map == GC_invalid_map) return(0);
    /* Make sure r points to the beginning of the object */
	r &= ~(WORDS_TO_BYTES(1) - 1);
        {
	    register int offset =
	        	(word *)r - (word *)(HBLKPTR(r)) - HDR_WORDS;
	    register signed_word sz = candidate_hdr -> hb_sz;
	    register int correction;
	        
	    correction = offset % sz;
	    r -= (WORDS_TO_BYTES(correction));
	    if (((word *)r + sz) > (word *)(h + 1)
	        && sz <= MAXOBJSZ) {
	        return(0);
	    }
	}
    return((extern_ptr_t)r);
}

/* Return the size of an object, given a pointer to its base.		*/
/* (For small obects this also happens to work from interior pointers,	*/
/* but that shouldn't be relied upon.)					*/
# ifdef __STDC__
    size_t GC_size(extern_ptr_t p)
# else
    size_t GC_size(p)
    extern_ptr_t p;
# endif
{
    register int sz;
    register hdr * hhdr = HDR(p);
    
    sz = WORDS_TO_BYTES(hhdr -> hb_sz);
    if (sz < 0) {
        return(-sz);
    } else {
        return(sz);
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
    register struct obj_kind * ok;

    if (p == 0) return;
    	/* Required by ANSI.  It's not my fault ...	*/
    h = HBLKPTR(p);
    hhdr = HDR(h);
    sz = hhdr -> hb_sz;
    ok = &GC_obj_kinds[hhdr -> hb_obj_kind];
  
    if (sz > MAXOBJSZ) {
	GC_freehblk(h);
    } else {
        ok = &GC_obj_kinds[hhdr -> hb_obj_kind];
	if (ok -> ok_init) {
	    bzero((char *)((word *)p + 1), (int)(WORDS_TO_BYTES(sz-1)));
	}
	flh = &(ok -> ok_freelist[sz]);
	obj_link(p) = *flh;
	*flh = (ptr_t)p;
    }
}

bool GC_is_initialized = FALSE;

void GC_init()
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    GC_init_inner();
    UNLOCK();
    ENABLE_SIGNALS();

}

void GC_init_inner()
{
    word dummy;
    
    if (GC_is_initialized) return;
    GC_is_initialized = TRUE;
#   ifndef THREADS
      if (GC_stackbottom == 0) {
	GC_stackbottom = GC_get_stack_base();
      }
#   endif
    if  (sizeof (ptr_t) != sizeof(word)) {
        GC_printf("sizeof (ptr_t) != sizeof(word)\n");
        ABORT("sizeof (ptr_t) != sizeof(word)\n");
    }
    if  (sizeof (signed_word) != sizeof(word)) {
        GC_printf("sizeof (signed_word) != sizeof(word)\n");
        ABORT("sizeof (signed_word) != sizeof(word)\n");
    }
    if  (sizeof (struct hblk) != HBLKSIZE) {
        GC_printf("sizeof (struct hblk) != HBLKSIZE\n");
        ABORT("sizeof (struct hblk) != HBLKSIZE\n");
    }
#   ifndef THREADS
#     if defined(STACK_GROWS_UP) && defined(STACK_GROWS_DOWN)
  	GC_printf(
  	  "Only one of STACK_GROWS_UP and STACK_GROWS_DOWN should be defd\n");
  	ABORT("stack direction 1\n");
#     endif
#     if !defined(STACK_GROWS_UP) && !defined(STACK_GROWS_DOWN)
  	GC_printf(
  	  "One of STACK_GROWS_UP and STACK_GROWS_DOWN should be defd\n");
  	ABORT("stack direction 2\n");
#     endif
#     ifdef STACK_GROWS_DOWN
        if ((word)(&dummy) > (word)GC_stackbottom) {
          GC_printf("STACK_GROWS_DOWN is defd, but stack appears to grow up\n");
          GC_printf("sp = 0x%lx, GC_stackbottom = 0x%lx\n",
          	    (unsigned long) (&dummy),
          	    (unsigned long) GC_stackbottom);
          ABORT("stack direction 3\n");
        }
#     else
        if ((word)(&dummy) < (word)GC_stackbottom) {
          GC_printf("STACK_GROWS_UP is defd, but stack appears to grow down\n");
          GC_printf("sp = 0x%lx, GC_stackbottom = 0x%lx\n",
          	    (unsigned long) (&dummy),
          	    (unsigned long) GC_stackbottom);
          ABORT("stack direction 4");
        }
#     endif
#   endif
#   if !defined(_AUX_SOURCE) || defined(__GNUC__)
      if ((word)(-1) < (word)0) {
    	GC_printf("The type word should be an unsigned integer type\n");
    	GC_printf("It appears to be signed\n");
    	ABORT("word");
      }
#   endif
    if ((signed_word)(-1) >= (signed_word)0) {
    	GC_printf("The type signed_word should be a signed integer type\n");
    	GC_printf("It appears to be unsigned\n");
    	ABORT("signed_word");
    }
    
    GC_hincr = HINCR;
    GC_init_headers();
    GC_bl_init();
    GC_mark_init();
    if (!GC_expand_hp_inner(GC_hincr)) {
        GC_printf("Can't start up: no memory\n");
        EXIT();
    }
    GC_register_displacement_inner(0L);
#   ifdef MERGE_SIZES
      GC_init_size_map();
#   endif
    /* Add initial guess of root sets */
      GC_register_data_segments();
#   ifdef PCR
      GC_pcr_install();
#   endif
    /* Get black list set up */
      GC_gcollect_inner(TRUE);
      GC_gcollect_inner(TRUE);
    /* Convince lint that some things are used */
      {
          extern char * GC_copyright[];
          GC_noop(GC_copyright, GC_find_header,
                  GC_tl_mark, GC_call_with_alloc_lock);
      }
}

/* A version of printf that is unlikely to call malloc, and is thus safer */
/* to call from the collector in case malloc has been bound to GC_malloc. */
/* Assumes that no more than 1023 characters are written at once.	  */
/* Assumes that all arguments have been converted to something of the	  */
/* same size as long, and that the format conversions expect something	  */
/* of that size.							  */
/*VARARGS1*/
void GC_printf(format, a, b, c, d, e, f)
char * format;
long a, b, c, d, e, f;
{
    char buf[1025];
    
    buf[1024] = 0x15;
    (void) sprintf(buf, format, a, b, c, d, e, f);
    if (buf[1024] != 0x15) ABORT("GC_printf clobbered stack");
#   ifdef OS2
      /* We hope this doesn't allocate */
      if (fwrite(buf, 1, strlen(buf), stdout) != strlen(buf))
          ABORT("write to stdout failed");
#   else
      if (write(1, buf, strlen(buf)) < 0) ABORT("write to stdout failed");
#   endif
}

/*VARARGS1*/
void GC_err_printf(format, a, b, c, d, e, f)
char * format;
long a, b, c, d, e, f;
{
    char buf[1025];
    
    buf[1024] = 0x15;
    (void) sprintf(buf, format, a, b, c, d, e, f);
    if (buf[1024] != 0x15) ABORT("GC_err_printf clobbered stack");
#   ifdef OS2
      /* We hope this doesn't allocate */
      if (fwrite(buf, 1, strlen(buf), stderr) != strlen(buf))
          ABORT("write to stderr failed");
#   else
      if (write(2, buf, strlen(buf)) < 0) ABORT("write to stderr failed");
#   endif
}

void GC_err_puts(s)
char *s;
{
#   ifdef OS2
      /* We hope this doesn't allocate */
      if (fwrite(s, 1, strlen(s), stderr) != strlen(s))
          ABORT("write to stderr failed");
#   else
      if (write(2, s, strlen(s)) < 0) ABORT("write to stderr failed");
#   endif
}

