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
#     include "il/PCR_IL.h"
      struct PCR_Th_MLRep GC_allocate_ml;
#   else
#     ifdef SRC_M3
	/* Critical section counter is defined in the M3 runtime 	*/
	/* That's all we use.						*/
#     else
	--> declare allocator lock here
#     endif
#   endif
# endif

FAR struct _GC_arrays GC_arrays = { 0 };

/* Initialize GC_obj_kinds properly and standard free lists properly.  	*/
/* This must be done statically since they may be accessed before 	*/
/* GC_init is called.							*/
struct obj_kind GC_obj_kinds[MAXOBJKINDS] = {
/* PTRFREE */ { &GC_aobjfreelist[0], &GC_areclaim_list[0],
		GC_no_mark_proc, FALSE },
/* NORMAL  */ { &GC_objfreelist[0], &GC_reclaim_list[0],
		GC_normal_mark_proc, TRUE },
/* UNCOLLECTABLE */
	      { &GC_uobjfreelist[0], &GC_ureclaim_list[0],
		GC_normal_mark_proc, TRUE },
# ifdef STUBBORN_ALLOC
/*STUBBORN*/ { &GC_sobjfreelist[0], &GC_sreclaim_list[0],
		GC_normal_mark_proc, TRUE },
# endif
};

# ifdef STUBBORN_ALLOC
  int GC_n_kinds = 4;
# else
  int GC_n_kinds = 3;
# endif

bool GC_debugging_started = FALSE;
	/* defined here so we don't have to load debug_malloc.o */

void (*GC_check_heap)() = (void (*)())0;

ptr_t GC_stackbottom = 0;

bool GC_dont_gc = 0;

bool GC_quiet = 0;

extern signed_word GC_mem_found;

# ifdef MERGE_SIZES
    /* Set things up so that GC_size_map[i] >= words(i),		*/
    /* but not too much bigger						*/
    /* and so that size_map contains relatively few distinct entries 	*/
    /* This is stolen from Russ Atkinson's Cedar quantization		*/
    /* alogrithm (but we precompute it).				*/


    void GC_init_size_map()
    {
	register unsigned i;
	register unsigned sz_rounded_up = 0;

	/* Map size 0 to 1.  This avoids problems at lower levels. */
	  GC_size_map[0] = 1;
	/* One word objects don't have to be 2 word aligned.	   */
	  for (i = 1; i < sizeof(word); i++) {
	      GC_size_map[i] = 1;
	  }
	  GC_size_map[sizeof(word)] = ROUNDED_UP_WORDS(sizeof(word));
	for (i = sizeof(word) + 1; i <= 8 * sizeof(word); i++) {
#           ifdef ALIGN_DOUBLE
	      GC_size_map[i] = (ROUNDED_UP_WORDS(i) + 1) & (~1);
#           else
	      GC_size_map[i] = ROUNDED_UP_WORDS(i);
#           endif
	}
	
	for (i = 8*sizeof(word)+1; i <= WORDS_TO_BYTES(MAXOBJSZ); i++) {
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
	        && sz <= BYTES_TO_WORDS(HBLKSIZE) - HDR_WORDS) {
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
        GC_err_printf0("sizeof (ptr_t) != sizeof(word)\n");
        ABORT("sizeof (ptr_t) != sizeof(word)\n");
    }
    if  (sizeof (signed_word) != sizeof(word)) {
        GC_err_printf0("sizeof (signed_word) != sizeof(word)\n");
        ABORT("sizeof (signed_word) != sizeof(word)\n");
    }
    if  (sizeof (struct hblk) != HBLKSIZE) {
        GC_err_printf0("sizeof (struct hblk) != HBLKSIZE\n");
        ABORT("sizeof (struct hblk) != HBLKSIZE\n");
    }
#   ifndef THREADS
#     if defined(STACK_GROWS_UP) && defined(STACK_GROWS_DOWN)
  	GC_err_printf0(
  	  "Only one of STACK_GROWS_UP and STACK_GROWS_DOWN should be defd\n");
  	ABORT("stack direction 1\n");
#     endif
#     if !defined(STACK_GROWS_UP) && !defined(STACK_GROWS_DOWN)
  	GC_err_printf0(
  	  "One of STACK_GROWS_UP and STACK_GROWS_DOWN should be defd\n");
  	ABORT("stack direction 2\n");
#     endif
#     ifdef STACK_GROWS_DOWN
        if ((word)(&dummy) > (word)GC_stackbottom) {
          GC_err_printf0(
          	"STACK_GROWS_DOWN is defd, but stack appears to grow up\n");
          GC_err_printf2("sp = 0x%lx, GC_stackbottom = 0x%lx\n",
          	   	 (unsigned long) (&dummy),
          	   	 (unsigned long) GC_stackbottom);
          ABORT("stack direction 3\n");
        }
#     else
        if ((word)(&dummy) < (word)GC_stackbottom) {
          GC_err_printf0(
          	"STACK_GROWS_UP is defd, but stack appears to grow down\n");
          GC_err_printf2("sp = 0x%lx, GC_stackbottom = 0x%lx\n",
          	       	 (unsigned long) (&dummy),
          	     	 (unsigned long) GC_stackbottom);
          ABORT("stack direction 4");
        }
#     endif
#   endif
#   if !defined(_AUX_SOURCE) || defined(__GNUC__)
      if ((word)(-1) < (word)0) {
    	GC_err_printf0("The type word should be an unsigned integer type\n");
    	GC_err_printf0("It appears to be signed\n");
    	ABORT("word");
      }
#   endif
    if ((signed_word)(-1) >= (signed_word)0) {
    	GC_err_printf0(
    		"The type signed_word should be a signed integer type\n");
    	GC_err_printf0("It appears to be unsigned\n");
    	ABORT("signed_word");
    }
    
    GC_init_headers();
    GC_bl_init();
    GC_mark_init();
    if (!GC_expand_hp_inner((word)MINHINCR)) {
        GC_err_printf0("Can't start up: not enough memory\n");
        EXIT();
    }
    /* Preallocate large object map.  It's otherwise inconvenient to 	*/
    /* deal with failure.						*/
      if (!GC_add_map_entry((word)0)) {
        GC_err_printf0("Can't start up: not enough memory\n");
        EXIT();
      }
    GC_register_displacement_inner(0L);
#   ifdef MERGE_SIZES
      GC_init_size_map();
#   endif
    /* Add initial guess of root sets */
      GC_register_data_segments();
#   ifdef PCR
      PCR_IL_Lock(PCR_Bool_false, PCR_allSigsBlocked, PCR_waitForever);
      PCR_IL_Unlock();
      GC_pcr_install();
#   endif
    /* Get black list set up */
      GC_gcollect_inner();
#   ifdef STUBBORN_ALLOC
    	GC_stubborn_init();
#   endif
    /* Convince lint that some things are used */
#   ifdef LINT
      {
          extern char * GC_copyright[];
          extern GC_read();
          
          GC_noop(GC_copyright, GC_find_header, GC_print_block_list,
                  GC_push_one, GC_call_with_alloc_lock, GC_read,
                  GC_print_hblkfreelist, GC_dont_expand);
      }
#   endif
}

void GC_enable_incremental()
{
    DCL_LOCK_STATE;
    
# ifndef FIND_LEAK
    DISABLE_SIGNALS();
    LOCK();
    if (!GC_is_initialized) {
        GC_init_inner();
    }
    if (GC_words_allocd > 0) {
    	/* There may be unmarked reachable objects */
    	GC_gcollect_inner();
    }   /* else we're OK in assumeing everything's */
    	/* clean since nothing can point to an	   */
    	/* unmarked object.			   */
    GC_dirty_init();
    GC_read_dirty();
    GC_incremental = TRUE;
    UNLOCK();
    ENABLE_SIGNALS();
# endif
}

/* A version of printf that is unlikely to call malloc, and is thus safer */
/* to call from the collector in case malloc has been bound to GC_malloc. */
/* Assumes that no more than 1023 characters are written at once.	  */
/* Assumes that all arguments have been converted to something of the	  */
/* same size as long, and that the format conversions expect something	  */
/* of that size.							  */
void GC_printf(format, a, b, c, d, e, f)
char * format;
long a, b, c, d, e, f;
{
    char buf[1025];
    
    if (GC_quiet) return;
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

# ifdef SRC_M3
void GC_enable()
{
    GC_dont_gc--;
}

void GC_disable()
{
    GC_dont_gc++;
}
# endif
