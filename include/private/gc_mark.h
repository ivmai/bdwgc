/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */
/* Boehm, November 7, 1994 4:56 pm PST */

/*
 * Declarations of mark stack.  Needed by marker and client supplied mark
 * routines.  To be included after gc_priv.h.
 */
#ifndef GC_MARK_H
# define GC_MARK_H

# ifdef KEEP_BACK_PTRS
#   include "dbg_mlc.h"
# endif

/* A client supplied mark procedure.  Returns new mark stack pointer.	*/
/* Primary effect should be to push new entries on the mark stack.	*/
/* Mark stack pointer values are passed and returned explicitly.	*/
/* Global variables decribing mark stack are not necessarily valid.	*/
/* (This usually saves a few cycles by keeping things in registers.)	*/
/* Assumed to scan about PROC_BYTES on average.  If it needs to do	*/
/* much more work than that, it should do it in smaller pieces by	*/
/* pushing itself back on the mark stack.				*/
/* Note that it should always do some work (defined as marking some	*/
/* objects) before pushing more than one entry on the mark stack.	*/
/* This is required to ensure termination in the event of mark stack	*/
/* overflows.								*/
/* This procedure is always called with at least one empty entry on the */
/* mark stack.								*/
/* Currently we require that mark procedures look for pointers in a	*/
/* subset of the places the conservative marker would.  It must be safe	*/
/* to invoke the normal mark procedure instead.				*/
/* WARNING: Such a mark procedure may be invoked on an unused object    */
/* residing on a free list.  Such objects are cleared, except for a	*/
/* free list link field in the first word.  Thus mark procedures may	*/
/* not count on the presence of a type descriptor, and must handle this	*/
/* case correctly somehow.						*/
# define PROC_BYTES 100
/* The real declarations of the following are in gc_priv.h, so that	*/
/* we can avoid scanning the following table.				*/
/*
typedef struct ms_entry * (*mark_proc)(   word * addr,
					  struct ms_entry *mark_stack_ptr,
					  struct ms_entry *mark_stack_limit,
					  word env   );
					  
# define LOG_MAX_MARK_PROCS 6
# define MAX_MARK_PROCS (1 << LOG_MAX_MARK_PROCS)
extern mark_proc GC_mark_procs[MAX_MARK_PROCS];
*/

extern word GC_n_mark_procs;

/* Number of mark stack entries to discard on overflow.	*/
#define GC_MARK_STACK_DISCARDS (INITIAL_MARK_STACK_SIZE/8)

/* In a few cases it's necessary to assign statically known indices to	*/
/* certain mark procs.  Thus we reserve a few for well known clients.	*/
/* (This is necessary if mark descriptors are compiler generated.)	*/
#define GC_RESERVED_MARK_PROCS 8
#   define GCJ_RESERVED_MARK_PROC_INDEX 0

/* Object descriptors on mark stack or in objects.  Low order two	*/
/* bits are tags distinguishing among the following 4 possibilities	*/
/* for the high order 30 bits.						*/
#define DS_TAG_BITS 2
#define DS_TAGS   ((1 << DS_TAG_BITS) - 1)
#define DS_LENGTH 0	/* The entire word is a length in bytes that	*/
			/* must be a multiple of 4.			*/
#define DS_BITMAP 1	/* 30 bits are a bitmap describing pointer	*/
			/* fields.  The msb is 1 iff the first word	*/
			/* is a pointer.				*/
			/* (This unconventional ordering sometimes	*/
			/* makes the marker slightly faster.)		*/
			/* Zeroes indicate definite nonpointers.  Ones	*/
			/* indicate possible pointers.			*/
			/* Only usable if pointers are word aligned.	*/
#   define BITMAP_BITS (WORDSZ - DS_TAG_BITS)
#define DS_PROC   2
			/* The objects referenced by this object can be */
			/* pushed on the mark stack by invoking		*/
			/* PROC(descr).  ENV(descr) is passed as the	*/
			/* last argument.				*/
#   define PROC(descr) \
		(GC_mark_procs[((descr) >> DS_TAG_BITS) & (MAX_MARK_PROCS-1)])
#   define ENV(descr) \
		((descr) >> (DS_TAG_BITS + LOG_MAX_MARK_PROCS))
#   define MAX_ENV \
  	      (((word)1 << (WORDSZ - DS_TAG_BITS - LOG_MAX_MARK_PROCS)) - 1)
#   define MAKE_PROC(proc_index, env) \
	    (((((env) << LOG_MAX_MARK_PROCS) | (proc_index)) << DS_TAG_BITS) \
	    | DS_PROC)
#define DS_PER_OBJECT 3	/* The real descriptor is at the		*/
			/* byte displacement from the beginning of the	*/
			/* object given by descr & ~DS_TAGS		*/
			/* If the descriptor is negative, the real	*/
			/* descriptor is at (*<object_start>) -		*/
			/* (descr & ~DS_TAGS) - INDIR_PER_OBJ_BIAS	*/
			/* The latter alternative can be used if each	*/
			/* object contains a type descriptor in the	*/
			/* first word.					*/
#define INDIR_PER_OBJ_BIAS 0x10
			
typedef struct ms_entry {
    word * mse_start;   /* First word of object */
    word mse_descr;	/* Descriptor; low order two bits are tags,	*/
    			/* identifying the upper 30 bits as one of the	*/
    			/* following:					*/
} mse;

extern word GC_mark_stack_size;

#ifdef PARALLEL_MARK
  extern mse * VOLATILE GC_mark_stack_top;
#else
  extern mse * GC_mark_stack_top;
#endif

extern mse * GC_mark_stack;

#ifdef PARALLEL_MARK
    /*
     * Allow multiple threads to participate in the marking process.
     * This works roughly as follows:
     *  The main mark stack never shrinks, but it can grow.
     *
     *	The initiating threads holds the GC lock, and sets GC_help_wanted.
     *  
     *  Other threads:
     *     1) update helper_count (while holding mark_lock.)
     *	   2) allocate a local mark stack
     *     repeatedly:
     *		3) Steal a global mark stack entry by atomically replacing
     *		   its descriptor with 0.
     *		4) Copy it to the local stack.
     *	        5) Mark on the local stack until it is empty, or
     *		   it may be profitable to copy it back.
     *	        6) If necessary, copy local stack to global one,
     *		   holding mark lock.
     *    7) Stop when the global mark stack is empty.
     *    8) decrement helper_count (holding mark_lock).
     *
     * This is an experiment to see if we can do something along the lines
     * of the University of Tokyo SGC in a less intrusive, though probably
     * also less performant, way.
     */
    void GC_do_parallel_mark();
		/* inititate parallel marking.	*/

    extern GC_bool GC_help_wanted;	/* Protected by mark lock	*/
    extern unsigned GC_helper_count;	/* Number of running helpers.	*/
					/* Protected by mark lock	*/
    extern unsigned GC_active_count;	/* Number of active helpers.	*/
					/* Protected by mark lock	*/
					/* May increase and decrease	*/
					/* within each mark cycle.  But	*/
					/* once it returns to 0, it	*/
					/* stays zero for the cycle.	*/
    /* GC_mark_stack_top is also protected by mark lock.	*/
    extern mse * VOLATILE GC_first_nonempty;
					/* Lowest entry on mark stack	*/
					/* that may be nonempty.	*/
					/* Updated only by initiating 	*/
					/* thread.			*/
    /*
     * GC_notify_all_marker() is used when GC_help_wanted is first set,
     * when the last helper becomes inactive,
     * when something is added to the global mark stack, and just after
     * GC_mark_no is incremented.
     * This could be split into multiple CVs (and probably should be to
     * scale to really large numbers of processors.)
     */
#endif /* PARALLEL_MARK */

ptr_t GC_find_start();

mse * GC_signal_mark_stack_overflow();

# ifdef GATHERSTATS
#   define ADD_TO_ATOMIC(sz) GC_atomic_in_use += (sz)
#   define ADD_TO_COMPOSITE(sz) GC_composite_in_use += (sz)
# else
#   define ADD_TO_ATOMIC(sz)
#   define ADD_TO_COMPOSITE(sz)
# endif

/* Push the object obj with corresponding heap block header hhdr onto 	*/
/* the mark stack.							*/
# define PUSH_OBJ(obj, hhdr, mark_stack_top, mark_stack_limit) \
{ \
    register word _descr = (hhdr) -> hb_descr; \
        \
    if (_descr == 0) { \
    	ADD_TO_ATOMIC((hhdr) -> hb_sz); \
    } else { \
        ADD_TO_COMPOSITE((hhdr) -> hb_sz); \
        mark_stack_top++; \
        if (mark_stack_top >= mark_stack_limit) { \
          mark_stack_top = GC_signal_mark_stack_overflow(mark_stack_top); \
        } \
        mark_stack_top -> mse_start = (obj); \
        mark_stack_top -> mse_descr = _descr; \
    } \
}

#ifdef PRINT_BLACK_LIST
#   define GC_FIND_START(current, hhdr, source) \
	GC_find_start(current, hhdr, source)
#else
#   define GC_FIND_START(current, hhdr, source) \
	GC_find_start(current, hhdr)
#endif

/* Push the contents of current onto the mark stack if it is a valid	*/
/* ptr to a currently unmarked object.  Mark it.			*/
/* If we assumed a standard-conforming compiler, we could probably	*/
/* generate the exit_label transparently.				*/
# define PUSH_CONTENTS(current, mark_stack_top, mark_stack_limit, \
		       source, exit_label) \
{ \
    hdr * my_hhdr; \
    ptr_t my_current = current; \
 \
    GET_HDR(my_current, my_hhdr); \
    if (IS_FORWARDING_ADDR_OR_NIL(my_hhdr)) { \
         my_current = GC_FIND_START(my_current, my_hhdr, (word)source); \
         if (my_current == 0) goto exit_label; \
         my_hhdr = GC_find_header(my_current); \
    } \
    PUSH_CONTENTS_HDR(my_current, mark_stack_top, mark_stack_limit, \
		  source, exit_label, my_hhdr);	\
exit_label: ; \
}

/* As above, but use header cache for header lookup.	*/
# define HC_PUSH_CONTENTS(current, mark_stack_top, mark_stack_limit, \
		       source, exit_label) \
{ \
    hdr * my_hhdr; \
    ptr_t my_current = current; \
 \
    HC_GET_HDR(my_current, my_hhdr, source); \
    PUSH_CONTENTS_HDR(my_current, mark_stack_top, mark_stack_limit, \
		  source, exit_label, my_hhdr);	\
exit_label: ; \
}

/* As above, but deal with two pointers in interleaved fashion.	*/
# define HC_PUSH_CONTENTS2(current1, current2, mark_stack_top, \
			   mark_stack_limit, \
		           source1, source2, exit_label1, exit_label2) \
{ \
    hdr * hhdr1; \
    ptr_t my_current1 = current1; \
    hdr * hhdr2; \
    ptr_t my_current2 = current2; \
 \
    HC_GET_HDR2(my_current1, hhdr1, source1, my_current2, hhdr2, source2); \
    PUSH_CONTENTS_HDR(my_current1, mark_stack_top, mark_stack_limit, \
		  source1, exit_label1, hhdr1);	\
exit_label1: ; \
    if (0 != hhdr2) { \
      PUSH_CONTENTS_HDR(my_current2, mark_stack_top, mark_stack_limit, \
		  source2, exit_label2, hhdr2);	\
    } \
exit_label2: ; \
}

/* Set mark bit, exit if it was already set.	*/

# ifdef USE_MARK_BYTES
    /* Unlike the mark bit case, there is a race here, and we may set	*/
    /* the bit twice in the concurrent case.  This can result in the	*/
    /* object being pushed twice.  But that's only a performance issue.	*/
#   define SET_MARK_BIT_EXIT_IF_SET(hhdr,displ,exit_label) \
    { \
        register VOLATILE char * mark_byte_addr = \
				hhdr -> hb_marks + ((displ) >> 1); \
        register char mark_byte = *mark_byte_addr; \
          \
	if (mark_byte) goto exit_label; \
	*mark_byte_addr = 1;  \
    } 
# else
#   define SET_MARK_BIT_EXIT_IF_SET(hhdr,displ,exit_label) \
    { \
        register word * mark_word_addr = hhdr -> hb_marks + divWORDSZ(displ); \
        register word mark_word = *mark_word_addr; \
          \
        OR_WORD_EXIT_IF_SET(mark_word_addr, (word)1 << modWORDSZ(displ), \
			    exit_label); \
    } 
# endif /* USE_MARK_BYTES */

# define PUSH_CONTENTS_HDR(current, mark_stack_top, mark_stack_limit, \
		           source, exit_label, hhdr) \
{ \
    int displ;  /* Displacement in block; first bytes, then words */ \
    map_entry_type map_entry; \
    \
    displ = HBLKDISPL(current); \
    map_entry = MAP_ENTRY((hhdr -> hb_map), displ); \
    if (map_entry == OBJ_INVALID) { \
        GC_ADD_TO_BLACK_LIST_NORMAL((word)current, source); goto exit_label; \
    } \
    displ = BYTES_TO_WORDS(displ); \
    displ -= map_entry; \
    SET_MARK_BIT_EXIT_IF_SET(hhdr, displ, exit_label); \
    GC_STORE_BACK_PTR((ptr_t)source, (ptr_t)HBLKPTR(current) \
				      + WORDS_TO_BYTES(displ)); \
    PUSH_OBJ(((word *)(HBLKPTR(current)) + displ), hhdr, \
    	     mark_stack_top, mark_stack_limit) \
}

#if defined(PRINT_BLACK_LIST) || defined(KEEP_BACK_PTRS)
#   define PUSH_ONE_CHECKED(p, ip, source) \
	GC_push_one_checked(p, ip, (ptr_t)(source))
#else
#   define PUSH_ONE_CHECKED(p, ip, source) \
	GC_push_one_checked(p, ip)
#endif

/*
 * Push a single value onto mark stack. Mark from the object pointed to by p.
 * P is considered valid even if it is an interior pointer.
 * Previously marked objects are not pushed.  Hence we make progress even
 * if the mark stack overflows.
 */
# define GC_PUSH_ONE_STACK(p, source) \
    if ((ptr_t)(p) >= GC_least_plausible_heap_addr 	\
	 && (ptr_t)(p) < GC_greatest_plausible_heap_addr) {	\
	 PUSH_ONE_CHECKED(p, TRUE, source);	\
    }

/*
 * As above, but interior pointer recognition as for
 * normal for heap pointers.
 */
# ifdef ALL_INTERIOR_POINTERS
#   define AIP TRUE
# else
#   define AIP FALSE
# endif
# define GC_PUSH_ONE_HEAP(p,source) \
    if ((ptr_t)(p) >= GC_least_plausible_heap_addr 	\
	 && (ptr_t)(p) < GC_greatest_plausible_heap_addr) {	\
	 PUSH_ONE_CHECKED(p,AIP,source);	\
    }

/* Mark starting at mark stack entry top (incl.) down to	*/
/* mark stack entry bottom (incl.).  Stop after performing	*/
/* about one page worth of work.  Return the new mark stack	*/
/* top entry.							*/
mse * GC_mark_from GC_PROTO((mse * top, mse * bottom, mse *limit));

#define MARK_FROM_MARK_STACK() \
	GC_mark_stack_top = GC_mark_from(GC_mark_stack_top, \
					 GC_mark_stack, \
					 GC_mark_stack + GC_mark_stack_size);

/*
 * Mark from one finalizable object using the specified
 * mark proc. May not mark the object pointed to by 
 * real_ptr. That is the job of the caller, if appropriate
 */
# define GC_MARK_FO(real_ptr, mark_proc) \
{ \
    (*(mark_proc))(real_ptr); \
    while (!GC_mark_stack_empty()) MARK_FROM_MARK_STACK(); \
    if (GC_mark_state != MS_NONE) { \
        GC_set_mark_bit(real_ptr); \
        while (!GC_mark_some((ptr_t)0)); \
    } \
}

extern GC_bool GC_mark_stack_too_small;
				/* We need a larger mark stack.  May be	*/
				/* set by client supplied mark routines.*/

typedef int mark_state_t;	/* Current state of marking, as follows:*/
				/* Used to remember where we are during */
				/* concurrent marking.			*/

				/* We say something is dirty if it was	*/
				/* written since the last time we	*/
				/* retrieved dirty bits.  We say it's 	*/
				/* grungy if it was marked dirty in the	*/
				/* last set of bits we retrieved.	*/
				
				/* Invariant I: all roots and marked	*/
				/* objects p are either dirty, or point */
				/* to objects q that are either marked 	*/
				/* or a pointer to q appears in a range	*/
				/* on the mark stack.			*/

# define MS_NONE 0		/* No marking in progress. I holds.	*/
				/* Mark stack is empty.			*/

# define MS_PUSH_RESCUERS 1	/* Rescuing objects are currently 	*/
				/* being pushed.  I holds, except	*/
				/* that grungy roots may point to 	*/
				/* unmarked objects, as may marked	*/
				/* grungy objects above scan_ptr.	*/

# define MS_PUSH_UNCOLLECTABLE 2
				/* I holds, except that marked 		*/
				/* uncollectable objects above scan_ptr */
				/* may point to unmarked objects.	*/
				/* Roots may point to unmarked objects	*/

# define MS_ROOTS_PUSHED 3	/* I holds, mark stack may be nonempty  */

# define MS_PARTIALLY_INVALID 4	/* I may not hold, e.g. because of M.S. */
				/* overflow.  However marked heap	*/
				/* objects below scan_ptr point to	*/
				/* marked or stacked objects.		*/

# define MS_INVALID 5		/* I may not hold.			*/

extern mark_state_t GC_mark_state;

#endif  /* GC_MARK_H */

