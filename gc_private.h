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
/* Boehm, December 16, 1993 4:52 pm PST */
 

# ifndef GC_PRIVATE_H
# define GC_PRIVATE_H

# ifndef GC_H
#   include "gc.h"
# endif

typedef GC_word word;
typedef GC_signed_word signed_word;

# ifndef CONFIG_H
#   include "config.h"
# endif

# ifndef HEADERS_H
#   include "gc_headers.h"
# endif

# ifndef bool
    typedef int bool;
# endif
# define TRUE 1
# define FALSE 0

typedef char * ptr_t;	/* A generic pointer to which we can add	*/
			/* byte displacments.				*/
			/* Prefereably identical to caddr_t, if it 	*/
			/* exists.					*/
			
#ifdef __STDC__
#   include <stdlib.h>
#   if !(defined( sony_news ) )
#       include <stddef.h>
#   endif
    typedef void * extern_ptr_t;
#   define VOLATILE volatile
#else
    typedef char * extern_ptr_t;
#   define VOLATILE
#endif

#ifdef AMIGA
#   define FAR __far
#else
#   define FAR
#endif

/*********************************/
/*                               */
/* Definitions for conservative  */
/* collector                     */
/*                               */
/*********************************/

/*********************************/
/*                               */
/* Easily changeable parameters  */
/*                               */
/*********************************/

#define STUBBORN_ALLOC	/* Define stubborn allocation primitives	*/
#ifdef SRC_M3
# undef STUBBORN_ALLOC
#endif


#define ALL_INTERIOR_POINTERS
		    /* Forces all pointers into the interior of an 	*/
		    /* object to be considered valid.  Also causes the	*/
		    /* sizes of all objects to be inflated by at least 	*/
		    /* one byte.  This should suffice to guarantee	*/
		    /* that in the presence of a compiler that does	*/
		    /* not perform garbage-collector-unsafe		*/
		    /* optimizations, all portable, strictly ANSI	*/
		    /* conforming C programs should be safely usable	*/
		    /* with malloc replaced by GC_malloc and free	*/
		    /* calls removed.  There are several disadvantages: */
		    /* 1. There are probably no interesting, portable,	*/
		    /*    strictly ANSI	conforming C programs.		*/
		    /* 2. This option makes it hard for the collector	*/
		    /*    to allocate space that is not ``pointed to''  */
		    /*    by integers, etc.  Under SunOS 4.X with a 	*/
		    /*    statically linked libc, we empiricaly		*/
		    /*    observed that it would be difficult to 	*/
		    /*	  allocate individual objects larger than 100K.	*/
		    /* 	  Even if only smaller objects are allocated,	*/
		    /*    more swap space is likely to be needed.       */
		    /*    Fortunately, much of this will never be	*/
		    /*    touched.					*/
		    /* If you can easily avoid using this option, do.	*/
		    /* If not, try to keep individual objects small.	*/
#undef ALL_INTERIOR_POINTERS
		    
#define PRINTSTATS  /* Print garbage collection statistics          	*/
		    /* For less verbose output, undefine in reclaim.c 	*/

#define PRINTTIMES  /* Print the amount of time consumed by each garbage   */
		    /* collection.                                         */

#define PRINTBLOCKS /* Print object sizes associated with heap blocks,     */
		    /* whether the objects are atomic or composite, and    */
		    /* whether or not the block was found to be empty      */
		    /* duing the reclaim phase.  Typically generates       */
		    /* about one screenful per garbage collection.         */
#undef PRINTBLOCKS

#define PRINTBLACKLIST 	/* Print black listed blocks, i.e. values that     */
			/* cause the allocator to avoid allocating certain */
			/* blocks in order to avoid introducing "false	   */
			/* hits".					   */
#undef PRINTBLACKLIST

#ifdef SILENT
#  ifdef PRINTSTATS
#    undef PRINTSTATS
#  endif
#  ifdef PRINTTIMES
#    undef PRINTTIMES
#  endif
#  ifdef PRINTNBLOCKS
#    undef PRINTNBLOCKS
#  endif
#endif

#if defined(PRINTSTATS) && !defined(GATHERSTATS)
#   define GATHERSTATS
#endif

# if defined(PCR) || defined(SRC_M3)
#   define THREADS
# endif

#ifdef SPARC
#   define ALIGN_DOUBLE  /* Align objects of size > 1 word on 2 word   */
			 /* boundaries.  Wasteful of memory, but       */
			 /* apparently required by SPARC architecture. */
#   define ASM_CLEAR_CODE	/* Stack clearing is crucial, and we 	*/
				/* include assembly code to do it well.	*/
#endif

#if defined(SPARC) || defined(M68K) && defined(SUNOS4) || defined(IRIX5)
# if !defined(PCR)
#   define DYNAMIC_LOADING /* Search dynamic libraries for roots.	*/
# else
    /* PCR handles any dynamic loading whether with dlopen or otherwise */
# endif
#endif

#define MERGE_SIZES /* Round up some object sizes, so that fewer distinct */
		    /* free lists are actually maintained.  This applies  */
		    /* only to the top level routines in misc.c, not to   */
		    /* user generated code that calls GC_allocobj and     */
		    /* GC_allocaobj directly.                             */
		    /* Slows down average programs slightly.  May however */
		    /* substantially reduce fragmentation if allocation   */
		    /* request sizes are widely scattered.                */
		    /* May save significant amounts of space for obj_map  */
		    /* entries.						  */

/* ALIGN_DOUBLE requires MERGE_SIZES at present. */
# if defined(ALIGN_DOUBLE) && !defined(MERGE_SIZES)
#   define MERGE_SIZES
# endif


# define MINHINCR 16       /* Minimum heap increment, in blocks of HBLKSIZE  */
# define MAXHINCR 512      /* Maximum heap increment, in blocks              */

# define TIME_LIMIT 50	   /* We try to keep pause times from exceeding	 */
			   /* this by much. In milliseconds.		 */

/*********************************/
/*                               */
/* OS interface routines	 */
/*                               */
/*********************************/

#include <time.h>
#if !defined(__STDC__) && defined(SPARC) && defined(SUNOS4)
   clock_t clock();	/* Not in time.h, where it belongs	*/
#endif
#if !defined(CLOCKS_PER_SEC)
#   define CLOCKS_PER_SEC 1000000
/*
 * This is technically a bug in the implementation.  ANSI requires that
 * CLOCKS_PER_SEC be defined.  But at least under SunOS4.1.1, it isn't.
 * Also note that the combination of ANSI C and POSIX is incredibly gross
 * here. The type clock_t is used by both clock() and times().  But on
 * some machines thes use different notions of a clock tick,  CLOCKS_PER_SEC
 * seems to apply only to clock.  Hence we use it here.  On many machines,
 * including SunOS, clock actually uses units of microseconds (which are
 * not really clock ticks).
 */
#endif
#define CLOCK_TYPE clock_t
#define GET_TIME(x) x = clock()
#define MS_TIME_DIFF(a,b) ((unsigned long) \
		(1000.0*(double)((a)-(b))/(double)CLOCKS_PER_SEC))

/* We use bzero and bcopy internally.  They may not be available.	*/
# if defined(SPARC) && defined(SUNOS4)
#   define BCOPY_EXISTS
# endif
# if defined(M68K) && defined(AMIGA)
#   define BCOPY_EXISTS
# endif
# if defined(M68K) && defined(NEXT)
#   define BCOPY_EXISTS
# endif
# if defined(VAX)
#   define BCOPY_EXISTS
# endif
# if defined(AMIGA)
#   include <string.h>
#   define BCOPY_EXISTS
# endif

# ifndef BCOPY_EXISTS
#   include <string.h>
#   define bcopy(x,y,n) memcpy(y,x,n)
#   define bzero(x,n)  memset(x, 0, n)
# endif

/* HBLKSIZE aligned allocation.  0 is taken to mean failure 	*/
/* space is assumed to be cleared.				*/
# ifdef PCR
    char * real_malloc();
#   define GET_MEM(bytes) HBLKPTR(real_malloc((size_t)bytes + HBLKSIZE) \
				  + HBLKSIZE-1)
# else
#   ifdef OS2
      void * os2_alloc(size_t bytes);
#     define GET_MEM(bytes) HBLKPTR((ptr_t)os2_alloc((size_t)bytes + HBLKSIZE) \
                                    + HBLKSIZE-1)
#   else
#     if defined(AMIGA) || defined(NEXT)
#       define GET_MEM(bytes) HBLKPTR(calloc(1, (size_t)bytes + HBLKSIZE) \
				+ HBLKSIZE-1)
#     else
        extern ptr_t GC_unix_get_mem();
#       define GET_MEM(bytes) (struct hblk *)GC_unix_get_mem(bytes)
#     endif
#   endif
# endif

/*
 * Mutual exclusion between allocator/collector routines.
 * Needed if there is more than one allocator thread.
 * FASTLOCK() is assumed to try to acquire the lock in a cheap and
 * dirty way that is acceptable for a few instructions, e.g. by
 * inhibiting preemption.  This is assumed to have succeeded only
 * if a subsequent call to FASTLOCK_SUCCEEDED() returns TRUE.
 * FASTUNLOCK() is called whether or not FASTLOCK_SUCCEEDED().
 * If signals cannot be tolerated with the FASTLOCK held, then
 * FASTLOCK should disable signals.  The code executed under
 * FASTLOCK is otherwise immune to interruption, provided it is
 * not restarted.
 * DCL_LOCK_STATE declares any local variables needed by LOCK and UNLOCK
 * and/or DISABLE_SIGNALS and ENABLE_SIGNALS and/or FASTLOCK.
 * (There is currently no equivalent for FASTLOCK.)
 */  
# ifdef THREADS
#  ifdef PCR
#    include  "th/PCR_Th.h"
#    include  "th/PCR_ThCrSec.h"
     extern struct PCR_Th_MLRep GC_allocate_ml;
#    define DCL_LOCK_STATE  PCR_sigset_t GC_old_sig_mask
#    define LOCK() PCR_Th_ML_Acquire(&GC_allocate_ml) 
#    define UNLOCK() PCR_Th_ML_Release(&GC_allocate_ml)
#    define FASTLOCK() PCR_ThCrSec_EnterSys()
     /* Here we cheat (a lot): */
#        define FASTLOCK_SUCCEEDED() (*(int *)(&GC_allocate_ml) == 0)
		/* TRUE if nobody currently holds the lock */
#    define FASTUNLOCK() PCR_ThCrSec_ExitSys()
#  endif
#  ifdef SRC_M3
     extern word RT0u__inCritical;
#    define LOCK() RT0u__inCritical++
#    define UNLOCK() RT0u__inCritical--
#  endif
# else
#    define LOCK()
#    define UNLOCK()

# endif

# ifndef DCL_LOCK_STATE
#   define DCL_LOCK_STATE
# endif
# ifndef FASTLOCK
#   define FASTLOCK() LOCK()
#   define FASTLOCK_SUCCEEDED() TRUE
#   define FASTUNLOCK() UNLOCK()
# endif

/* Delay any interrupts or signals that may abort this thread.  Data	*/
/* structures are in a consistent state outside this pair of calls.	*/
/* ANSI C allows both to be empty (though the standard isn't very	*/
/* clear on that point).  Standard malloc implementations are usually	*/
/* neither interruptable nor thread-safe, and thus correspond to	*/
/* empty definitions.							*/
# ifdef PCR
#   define DISABLE_SIGNALS() \
		 PCR_Th_SetSigMask(PCR_allSigsBlocked,&GC_old_sig_mask)
#   define ENABLE_SIGNALS() \
		PCR_Th_SetSigMask(&GC_old_sig_mask, NIL)
# else
#   if defined(SRC_M3) || defined(AMIGA)
			/* Also useful for debugging, and unusually	*/
	 		/* correct client code.				*/
#     define DISABLE_SIGNALS()
#     define ENABLE_SIGNALS()
#   else
#     define DISABLE_SIGNALS() GC_disable_signals()
	void GC_disable_signals();
#     define ENABLE_SIGNALS() GC_enable_signals()
	void GC_enable_signals();
#   endif
# endif

/*
 * Stop and restart mutator threads.
 */
# ifdef PCR
#     include "th/PCR_ThCtl.h"
#     define STOP_WORLD() \
 	PCR_ThCtl_SetExclusiveMode(PCR_ThCtl_ExclusiveMode_stopNormal, \
 				   PCR_allSigsBlocked, \
 				   PCR_waitForever)
#     define START_WORLD() \
	PCR_ThCtl_SetExclusiveMode(PCR_ThCtl_ExclusiveMode_null, \
 				   PCR_allSigsBlocked, \
 				   PCR_waitForever);
# else
#     define STOP_WORLD()
#     define START_WORLD()
# endif

/* Abandon ship */
# ifdef PCR
    void PCR_Base_Panic(const char *fmt, ...);
#   define ABORT(s) PCR_Base_Panic(s)
# else
#   define ABORT(msg) { GC_err_printf1("%s\n", msg); (void) abort(); }
# endif

/* Exit abnormally, but without making a mess (e.g. out of memory) */
# ifdef PCR
    void PCR_Base_Exit(int status);
#   define EXIT() PCR_Base_Exit(1)
# else
#   define EXIT() (void)exit(1)
# endif

/* Print warning message, e.g. almost out of memory.	*/
# define WARN(s) GC_printf0(s)

/*********************************/
/*                               */
/* Word-size-dependent defines   */
/*                               */
/*********************************/

#if CPP_WORDSZ == 32
#  define WORDS_TO_BYTES(x)   ((x)<<2)
#  define BYTES_TO_WORDS(x)   ((x)>>2)
#  define LOGWL               ((word)5)    /* log[2] of CPP_WORDSZ */
#  define modWORDSZ(n) ((n) & 0x1f)          /* n mod size of word	    */
#endif

#if CPP_WORDSZ == 64
#  define WORDS_TO_BYTES(x)   ((x)<<3)
#  define BYTES_TO_WORDS(x)   ((x)>>3)
#  define LOGWL               ((word)6)    /* log[2] of CPP_WORDSZ */
#  define modWORDSZ(n) ((n) & 0x3f)          /* n mod size of word	    */
#endif

#define WORDSZ ((word)CPP_WORDSZ)
#define SIGNB  ((word)1 << (WORDSZ-1))
#define BYTES_PER_WORD      ((word)(sizeof (word)))
#define ONES                ((word)(-1))
#define divWORDSZ(n) ((n) >> LOGWL)	   /* divide n by size of word      */

/*********************/
/*                   */
/*  Size Parameters  */
/*                   */
/*********************/

/*  heap block size, bytes. Should be power of 2 */

#if CPP_WORDSZ == 32
#   define CPP_LOG_HBLKSIZE 12
#else
#   define CPP_LOG_HBLKSIZE 13
#endif
#define LOG_HBLKSIZE   ((word)CPP_LOG_HBLKSIZE)
#define CPP_HBLKSIZE (1 << CPP_LOG_HBLKSIZE)
#define HBLKSIZE ((word)CPP_HBLKSIZE)


/*  max size objects supported by freelist (larger objects may be   */
/*  allocated, but less efficiently)                                */

#define CPP_MAXOBJSZ    BYTES_TO_WORDS(CPP_HBLKSIZE/2)
#define MAXOBJSZ ((word)CPP_MAXOBJSZ)
		
# define divHBLKSZ(n) ((n) >> LOG_HBLKSIZE)

# define HBLK_PTR_DIFF(p,q) divHBLKSZ((ptr_t)p - (ptr_t)q)
	/* Equivalent to subtracting 2 hblk pointers.	*/
	/* We do it this way because a compiler should	*/
	/* find it hard to use an integer division	*/
	/* instead of a shift.  The bundled SunOS 4.1	*/
	/* o.w. sometimes pessimizes the subtraction to	*/
	/* involve a call to .div.			*/
 
# define modHBLKSZ(n) ((n) & (HBLKSIZE-1))
 
# define HBLKPTR(objptr) ((struct hblk *)(((word) (objptr)) & ~(HBLKSIZE-1)))

# define HBLKDISPL(objptr) (((word) (objptr)) & (HBLKSIZE-1))

/* Round up byte allocation requests to integral number of words: */
# ifdef ALL_INTERIOR_POINTERS
#   define ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1))
# else
#   define ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1) - 1)
# endif


/*
 * Hash table representation of sets of pages.  This assumes it is
 * OK to add spurious entries to sets.
 * Used by black-listing code, and perhaps by dirty bit maintenance code.
 */
 
# define LOG_PHT_ENTRIES  14	/* Collisions are likely if heap grows	*/
				/* to more than 16K hblks = 64MB.	*/
				/* Each hash table occupies 2K bytes.   */
# define PHT_ENTRIES ((word)1 << LOG_PHT_ENTRIES)
# define PHT_SIZE (PHT_ENTRIES >> LOGWL)
typedef word page_hash_table[PHT_SIZE];

# define PHT_HASH(addr) ((((word)(addr)) >> LOG_HBLKSIZE) & (PHT_ENTRIES - 1))

# define get_pht_entry_from_index(bl, index) \
		(((bl)[divWORDSZ(index)] >> modWORDSZ(index)) & 1)
# define set_pht_entry_from_index(bl, index) \
		(bl)[divWORDSZ(index)] |= (word)1 << modWORDSZ(index)
# define clear_pht_entry_from_index(bl, index) \
		(bl)[divWORDSZ(index)] &= ~((word)1 << modWORDSZ(index))
	


/********************************************/
/*                                          */
/*    H e a p   B l o c k s                 */
/*                                          */
/********************************************/

/*  heap block header */
#define HBLKMASK   (HBLKSIZE-1)

#define BITS_PER_HBLK (HBLKSIZE * 8)

#define MARK_BITS_PER_HBLK (BITS_PER_HBLK/CPP_WORDSZ)
	   /* upper bound                                    */
	   /* We allocate 1 bit/word.  Only the first word   */
	   /* in each object is actually marked.             */

# ifdef ALIGN_DOUBLE
#   define MARK_BITS_SZ (((MARK_BITS_PER_HBLK + 2*CPP_WORDSZ - 1) \
			  / (2*CPP_WORDSZ))*2)
# else
#   define MARK_BITS_SZ ((MARK_BITS_PER_HBLK + CPP_WORDSZ - 1)/CPP_WORDSZ)
# endif
	   /* Upper bound on number of mark words per heap block  */
	   
/* Mark stack entries. */
typedef struct ms_entry {
    word * mse_start;  /* inclusive */
    word * mse_end;	/* exclusive */
} mse;

typedef mse * (*mark_proc)(/* word * addr, hdr * hhdr, mse * msp, mse * msl */);
	  /* Procedure to arrange for the descendents of the object at	*/
	  /* addr to be marked.  Msp points at the top entry on the	*/
	  /* mark stack.  Msl delimits the hot end of the mark stack.	*/
	  /* hhdr is the hdr structure corresponding to addr.		*/
	  /* Returns the new mark stack pointer.			*/

struct hblkhdr {
    word hb_sz;  /* If in use, size in words, of objects in the block. */
		 /* if free, the size in bytes of the whole block      */
    struct hblk * hb_next; 	/* Link field for hblk free list	 */
    				/* and for lists of chunks waiting to be */
    				/* reclaimed.				 */
    mark_proc hb_mark_proc;   /* Procedure to mark objects.  Can	 */
    				/* also be retrived through obj_kind.	 */
    				/* But one level of indirection matters  */
    				/* here.				 */
    char* hb_map;	/* A pointer to a pointer validity map of the block. */
    		      	/* See GC_obj_map.				     */
    		     	/* Valid for all blocks with headers.		     */
    		     	/* Free blocks point to GC_invalid_map.		     */
    unsigned short hb_obj_kind;
    			 /* Kind of objects in the block.  Each kind 	*/
    			 /* identifies a mark procedure and a set of 	*/
    			 /* list headers.  sometimes called regions.	*/
    unsigned short hb_last_reclaimed;
    				/* Value of GC_gc_no when block was	*/
    				/* last allocated or swept. May wrap.   */
    word hb_marks[MARK_BITS_SZ];
			    /* Bit i in the array refers to the             */
			    /* object starting at the ith word (header      */
			    /* INCLUDED) in the heap block.                 */
			    /* The lsb of word 0 is numbered 0.		    */
};

/*  heap block body */

# define DISCARD_WORDS 0
	/* Number of words to be dropped at the beginning of each block	*/
	/* Must be a multiple of WORDSZ.  May reasonably be nonzero	*/
	/* on machines that don't guarantee longword alignment of	*/
	/* pointers, so that the number of false hits is minimized.	*/
	/* 0 and WORDSZ are probably the only reasonable values.	*/

# define BODY_SZ ((HBLKSIZE-WORDS_TO_BYTES(DISCARD_WORDS))/sizeof(word))

struct hblk {
#   if (DISCARD_WORDS != 0)
        word garbage[DISCARD_WORDS];
#   endif
    word hb_body[BODY_SZ];
};

# define HDR_WORDS ((word)DISCARD_WORDS)
# define HDR_BYTES ((word)WORDS_TO_BYTES(DISCARD_WORDS))

# define OBJ_SZ_TO_BLOCKS(sz) \
    divHBLKSZ(HDR_BYTES + WORDS_TO_BYTES(sz) + HBLKSIZE-1)
    /* Size of block (in units of HBLKSIZE) needed to hold objects of	*/
    /* given sz (in words).						*/

/* Object free list link */
# define obj_link(p) (*(ptr_t *)(p))

/*  lists of all heap blocks and free lists	*/
/* These are grouped together in a struct	*/
/* so that they can be easily skipped by the	*/
/* GC_mark routine.				*/
/* The ordering is weird to make GC_malloc	*/
/* faster by keeping the important fields	*/
/* sufficiently close together that a		*/
/* single load of a base register will do.	*/
/* Scalars that could easily appear to		*/
/* be pointers are also put here.		*/

struct _GC_arrays {
  word _heapsize;
  ptr_t _last_heap_addr;
  ptr_t _prev_heap_addr;
  word _words_allocd_before_gc;
		/* Number of words allocated before this	*/
		/* collection cycle.				*/
# ifdef GATHERSTATS
    word _composite_in_use;
   		/* Number of words in accessible composite	*/
		/* objects.					*/
    word _atomic_in_use;
   		/* Number of words in accessible atomic		*/
		/* objects.					*/
# endif
  word _words_allocd;
  	/* Number of words allocated during this collection cycle */
  word _words_wasted;
  	/* Number of words wasted due to internal fragmentation	 */
  	/* in large objects allocated since last gc. Approximate.*/
  word _non_gc_bytes_at_gc;
  	/* Number of explicitly managed bytes of storage 	*/
  	/* at last collection.					*/
  word _mem_freed;
  	/* Number of explicitly deallocated words of memory	*/
  	/* since last collection.				*/
  	
  ptr_t _objfreelist[MAXOBJSZ+1];
			  /* free list for objects */
# ifdef MERGE_SIZES
    unsigned _size_map[WORDS_TO_BYTES(MAXOBJSZ+1)];
    	/* Number of words to allocate for a given allocation request in */
    	/* bytes.							 */
# endif 
  ptr_t _aobjfreelist[MAXOBJSZ+1];
			  /* free list for atomic objs 	*/

  ptr_t _uobjfreelist[MAXOBJSZ+1];
			  /* uncollectable but traced objs 	*/

# ifdef STUBBORN_ALLOC
    ptr_t _sobjfreelist[MAXOBJSZ+1];
# endif
  			  /* free list for immutable objects	*/
  ptr_t _obj_map[MAXOBJSZ+1];
                       /* If not NIL, then a pointer to a map of valid  */
    		       /* object addresses. hbh_map[sz][i] is j if the  */
    		       /* address block_start+i is a valid pointer      */
    		       /* to an object at				*/
    		       /* block_start+i&~3 - WORDS_TO_BYTES(j).		*/
    		       /* (If ALL_INTERIOR_POINTERS is defined, then	*/
    		       /* instead ((short *)(hbh_map[sz])[i] is j if	*/
    		       /* block_start+WORDS_TO_BYTES(i) is in the	*/
    		       /* interior of an object starting at		*/
    		       /* block_start+WORDS_TO_BYTES(i-j)).		*/
    		       /* It is OBJ_INVALID if				*/
    		       /* block_start+WORDS_TO_BYTES(i) is not		*/
    		       /* valid as a pointer to an object.              */
    		       /* We assume that all values of j <= OBJ_INVALID */
    		       /* The zeroth entry corresponds to large objects.*/
#   ifdef ALL_INTERIOR_POINTERS
#	define map_entry_type short
#       define OBJ_INVALID 0x7fff
#	define MAP_ENTRY(map, bytes) \
		(((map_entry_type *)(map))[BYTES_TO_WORDS(bytes)])
#	define MAP_ENTRIES BYTES_TO_WORDS(HBLKSIZE)
#	define MAP_SIZE (MAP_ENTRIES * sizeof(map_entry_type))
#	define OFFSET_VALID(displ) TRUE
#	define CPP_MAX_OFFSET (HBLKSIZE - HDR_BYTES - 1)
#	define MAX_OFFSET ((word)CPP_MAX_OFFSET)
#   else
#	define map_entry_type char
#       define OBJ_INVALID 0x7f
#	define MAP_ENTRY(map, bytes) \
		(map)[bytes]
#	define MAP_ENTRIES HBLKSIZE
#	define MAP_SIZE MAP_ENTRIES
#	define CPP_MAX_OFFSET (WORDS_TO_BYTES(OBJ_INVALID) - 1)	
#	define MAX_OFFSET ((word)CPP_MAX_OFFSET)
# 	define VALID_OFFSET_SZ \
	  (CPP_MAX_OFFSET > WORDS_TO_BYTES(CPP_MAXOBJSZ)? \
	   CPP_MAX_OFFSET+1 \
	   : WORDS_TO_BYTES(CPP_MAXOBJSZ)+1)
  	char _valid_offsets[VALID_OFFSET_SZ];
				/* GC_valid_offsets[i] == TRUE ==> i 	*/
				/* is registered as a displacement.	*/
#	define OFFSET_VALID(displ) GC_valid_offsets[displ]
  	char _modws_valid_offsets[sizeof(word)];
				/* GC_valid_offsets[i] ==>		  */
				/* GC_modws_valid_offsets[i%sizeof(word)] */
#   endif
  struct hblk * _reclaim_list[MAXOBJSZ+1];
  struct hblk * _areclaim_list[MAXOBJSZ+1];
  struct hblk * _ureclaim_list[MAXOBJSZ+1];
# ifdef STUBBORN_ALLOC
      struct hblk * _sreclaim_list[MAXOBJSZ+1];
      page_hash_table _changed_pages;
        /* Stubborn object pages that were changes since last call to	*/
	/* GC_read_changed.						*/
      page_hash_table _prev_changed_pages;
        /* Stubborn object pages that were changes before last call to	*/
	/* GC_read_changed.						*/
# endif
# if defined(PROC_VDB) || defined(MPROTECT_VDB)
      page_hash_table _grungy_pages; /* Pages that were dirty at last 	   */
				     /* GC_read_dirty.			   */
# endif
# define MAX_HEAP_SECTS 256	/* Separately added heap sections. */
  struct HeapSect {
      ptr_t hs_start; word hs_bytes;
  } _heap_sects[MAX_HEAP_SECTS];
  /* Block header index; see gc_headers.h */
  bottom_index _all_nils;
  bottom_index * _top_index [TOP_SZ];
};

extern FAR struct _GC_arrays GC_arrays; 

# define GC_objfreelist GC_arrays._objfreelist
# define GC_aobjfreelist GC_arrays._aobjfreelist
# define GC_uobjfreelist GC_arrays._uobjfreelist
# define GC_sobjfreelist GC_arrays._sobjfreelist
# define GC_valid_offsets GC_arrays._valid_offsets
# define GC_modws_valid_offsets GC_arrays._modws_valid_offsets
# define GC_reclaim_list GC_arrays._reclaim_list
# define GC_areclaim_list GC_arrays._areclaim_list
# define GC_ureclaim_list GC_arrays._ureclaim_list
# ifdef STUBBORN_ALLOC
#    define GC_sreclaim_list GC_arrays._sreclaim_list
#    define GC_changed_pages GC_arrays._changed_pages
#    define GC_prev_changed_pages GC_arrays._prev_changed_pages
# endif
# define GC_obj_map GC_arrays._obj_map
# define GC_last_heap_addr GC_arrays._last_heap_addr
# define GC_prev_heap_addr GC_arrays._prev_heap_addr
# define GC_words_allocd GC_arrays._words_allocd
# define GC_words_wasted GC_arrays._words_wasted
# define GC_non_gc_bytes_at_gc GC_arrays._non_gc_bytes_at_gc
# define GC_mem_freed GC_arrays._mem_freed
# define GC_heapsize GC_arrays._heapsize
# define GC_words_allocd_before_gc GC_arrays._words_allocd_before_gc
# define GC_heap_sects GC_arrays._heap_sects
# define GC_all_nils GC_arrays._all_nils
# define GC_top_index GC_arrays._top_index
# if defined(PROC_VDB) || defined(MPROTECT_VDB)
#   define GC_grungy_pages GC_arrays._grungy_pages
# endif
# ifdef GATHERSTATS
#   define GC_composite_in_use GC_arrays._composite_in_use
#   define GC_atomic_in_use GC_arrays._atomic_in_use
# endif
# ifdef MERGE_SIZES
#   define GC_size_map GC_arrays._size_map
# endif

# define beginGC_arrays ((ptr_t)(&GC_arrays))
# define endGC_arrays (((ptr_t)(&GC_arrays)) + (sizeof GC_arrays))


# define MAXOBJKINDS 16

/* Object kinds: */
extern struct obj_kind {
   ptr_t *ok_freelist;	/* Array of free listheaders for this kind of object */
   			/* Point either to GC_arrays or to storage allocated */
   			/* with GC_scratch_alloc.			     */
   struct hblk **ok_reclaim_list;
   			/* List headers for lists of blocks waiting to be */
   			/* swept.					  */
   mark_proc ok_mark_proc; /* Procedure to either mark referenced objects,  */
   			   /* or push them on the mark stack.		    */
   bool ok_init;     /* Clear objects before putting them on the free list. */
} GC_obj_kinds[MAXOBJKINDS];
/* Predefined kinds: */
# define PTRFREE 0
# define NORMAL  1
# define UNCOLLECTABLE 2
# define STUBBORN 3

extern word GC_n_heap_sects;	/* Number of separately added heap	*/
				/* sections.				*/

extern int GC_n_kinds;

extern char * GC_invalid_map;
			/* Pointer to the nowhere valid hblk map */
			/* Blocks pointing to this map are free. */

extern struct hblk * GC_hblkfreelist;
				/* List of completely empty heap blocks	*/
				/* Linked through hb_next field of 	*/
				/* header structure associated with	*/
				/* block.				*/

extern bool GC_is_initialized;		/* GC_init() has been run.	*/

extern bool GC_objects_are_marked;	/* There are marked objects in  */
					/* the heap.			*/

# ifndef PCR
    extern ptr_t GC_stackbottom;	/* Cool end of user stack	*/
# endif

extern word GC_root_size;	/* Total size of registered root sections */

extern bool GC_debugging_started;	/* GC_debug_malloc has been called. */ 

extern ptr_t GC_least_plausible_heap_addr;
extern ptr_t GC_greatest_plausible_heap_addr;
			/* Bounds on the heap.  Guaranteed valid	*/
			/* Likely to include future heap expansion.	*/
			
/* Operations */
# ifndef abs
#   define abs(x)  ((x) < 0? (-(x)) : (x))
# endif


/*  Marks are in a reserved area in                          */
/*  each heap block.  Each word has one mark bit associated  */
/*  with it. Only those corresponding to the beginning of an */
/*  object are used.                                         */


/* Mark bit perations */

/*
 * Retrieve, set, clear the mark bit corresponding
 * to the nth word in a given heap block.
 *
 * (Recall that bit n corresponds to object beginning at word n
 * relative to the beginning of the block, including unused words)
 */

# define mark_bit_from_hdr(hhdr,n) (((hhdr)->hb_marks[divWORDSZ(n)] \
			    >> (modWORDSZ(n))) & (word)1)
# define set_mark_bit_from_hdr(hhdr,n) (hhdr)->hb_marks[divWORDSZ(n)] \
				|= (word)1 << modWORDSZ(n)

# define clear_mark_bit_from_hdr(hhdr,n) (hhdr)->hb_marks[divWORDSZ(n)] \
				&= ~((word)1 << modWORDSZ(n))

/* Important internal collector routines */

void GC_apply_to_all_blocks(/*fn, client_data*/);
			/* Invoke fn(hbp, client_data) for each 	*/
			/* allocated heap block.			*/
struct hblk * GC_next_block(/* struct hblk * h */);
mse * GC_no_mark_proc(/*addr,hhdr,msp,msl*/);
			/* Mark procedure for PTRFREE objects.	*/
mse * GC_normal_mark_proc(/*addr,hhdr,msp,msl*/);
			/* Mark procedure for NORMAL objects.	*/
void GC_mark_init();
void GC_clear_marks();	/* Clear mark bits for all heap objects. */
void GC_mark_from_mark_stack(); /* Mark from everything on the mark stack. */
				/* Return after about one pages worth of   */
				/* work.				   */
bool GC_mark_stack_empty();
bool GC_mark_some();	/* Perform about one pages worth of marking	*/
			/* work of whatever kind is needed.  Returns	*/
			/* quickly if no collection is in progress.	*/
			/* Return TRUE if mark phase finished.		*/
void GC_initiate_full();	/* initiate full collection.		*/
void GC_initiate_partial();	/* initiate partial collection.		*/			
void GC_push_all(/*b,t*/);	/* Push everything in a range 		*/
				/* onto mark stack.			*/
void GC_push_dirty(/*b,t*/);      /* Push all possibly changed	 	*/
				  /* subintervals of [b,t) onto		*/
				  /* mark stack.			*/
void GC_push_conditional(/* ptr_t b, ptr_t t, bool all*/);
                                /* Do either of the above, depending	*/
				/* on the third arg.			*/
void GC_push_all_stack(/*b,t*/);    /* As above, but consider		*/
				    /*  interior pointers as valid  	*/
void GC_push_roots(/* bool all */); /* Push all or dirty roots.	*/
void GC_push_regs();	/* Push register contents onto mark stack.	*/
void GC_remark();	/* Mark from all marked objects.  Used	*/
		 	/* only if we had to drop something.	*/
void GC_push_one(/*p*/);	/* If p points to an object, mark it	*/
				/* and push contents on the mark stack	*/
void GC_push_one_checked(/*p*/); /* Ditto, omits plausibility test	*/
void GC_push_marked(/* struct hblk h, hdr * hhdr */);
		/* Push contents of all marked objects in h onto	*/
		/* mark stack.						*/
struct hblk * GC_push_next_marked_dirty(/* h */);
		/* Invoke GC_push_marked on next dirty block above h.	*/
		/* Return a pointer just past the end of this block.	*/
struct hblk * GC_push_next_marked(/* h */);
		/* Ditto, but also mark from clean pages.	*/
struct hblk * GC_push_next_marked_uncollectable(/* h */);
		/* Ditto, but mark only from uncollectable pages.	*/
bool GC_stopped_mark(); /* Stop world and mark from all roots	*/
			/* and rescuers.			*/
void GC_clear_hdr_marks(/* hhdr */);  /* Clear the mark bits in a header */
void GC_add_roots_inner();
void GC_register_dynamic_libraries();
		/* Add dynamic library data sections to the root set. */

/* Machine dependent startup routines */
ptr_t GC_get_stack_base();
void GC_register_data_segments();
		 	
# ifndef ALL_INTERIOR_POINTERS
    void GC_add_to_black_list_normal(/* bits */);
			/* Register bits as a possible future false	*/
			/* reference from the heap or static data	*/
#   define GC_ADD_TO_BLACK_LIST_NORMAL(bits) GC_add_to_black_list_normal(bits)
# else
#   define GC_ADD_TO_BLACK_LIST_NORMAL(bits) GC_add_to_black_list_stack(bits)
# endif

void GC_add_to_black_list_stack(/* bits */);
struct hblk * GC_is_black_listed(/* h, len */);
			/* If there are likely to be false references	*/
			/* to a block starting at h of the indicated    */
			/* length, then return the next plausible	*/
			/* starting location for h that might avoid	*/
			/* these false references.			*/
void GC_promote_black_lists();
			/* Declare an end to a black listing phase.	*/
		 	
ptr_t GC_scratch_alloc(/*bytes*/);
				/* GC internal memory allocation for	*/
				/* small objects.  Deallocation is not  */
				/* possible.				*/
				
void GC_invalidate_map(/* hdr */);
				/* Remove the object map associated	*/
				/* with the block.  This identifies	*/
				/* the block as invalid to the mark	*/
				/* routines.				*/
bool GC_add_map_entry(/*sz*/);
				/* Add a heap block map for objects of	*/
				/* size sz to obj_map.			*/
				/* Return FALSE on failure.		*/
void GC_register_displacement_inner(/*offset*/);
				/* Version of GC_register_displacement	*/
				/* that assumes lock is already held	*/
				/* and signals are already disabled.	*/
				
void GC_init_inner();
				
void GC_new_hblk(/*size_in_words, kind*/);
				/* Allocate a new heap block, and build */
				/* a free list in it.			*/				
struct hblk * GC_allochblk(/*size_in_words, kind*/);
				/* Allocate a heap block, clear it if	*/
				/* for composite objects, inform	*/
				/* the marker that block is valid	*/
				/* for objects of indicated size.	*/
				/* sz < 0 ==> atomic.			*/ 
void GC_freehblk();		/* Deallocate a heap block and mark it  */
				/* as invalid.				*/
				
void GC_start_reclaim(/*abort_if_found*/);
				/* Restore unmarked objects to free	*/
				/* lists, or (if abort_if_found is	*/
				/* TRUE) report them.			*/
				/* Sweeping of small object pages is	*/
				/* largely deferred.			*/
void GC_continue_reclaim(/*size, kind*/);
				/* Sweep pages of the given size and	*/
				/* kind, as long as possible, and	*/
				/* as long as the corr. free list is    */
				/* empty.				*/
void GC_reclaim_or_delete_all();
				/* Arrange for all reclaim lists to be	*/
				/* empty.  Judiciously choose between	*/
				/* sweeping and discarding each page.	*/
bool GC_block_empty(/* hhdr */); /* Block completely unmarked? 	*/
void GC_gcollect_inner();
				/* Collect; caller must have acquired	*/
				/* lock and disabled signals.		*/
				/* FALSE return indicates nothing was	*/
				/* done due to insufficient allocation. */
void GC_finish_collection();	/* Finish collection.  Mark bits are	*/
				/* consistent and lock is still held.	*/
bool GC_collect_or_expand(/* needed_blocks */);
				/* Collect or expand heap in an attempt */
				/* make the indicated number of free	*/
				/* blocks available.  Should be called	*/
				/* until it fails by returning FALSE.	*/
void GC_init();			/* Initialize collector.		*/
void GC_collect_a_little(/* n */);
				/* Do n units worth of garbage 		*/
				/* collection work, if appropriate.	*/
				/* A unit is an amount appropriate for  */
				/* HBLKSIZE bytes of allocation.	*/
ptr_t GC_generic_malloc(/* bytes, kind */);
				/* Allocate an object of the given	*/
				/* kind.  By default, there are only	*/
				/* two kinds: composite, and atomic.	*/
				/* We claim it's possible for clever	*/
				/* client code that understands GC	*/
				/* internals to add more, e.g. to	*/
				/* communicate object layout info	*/
				/* to the collector.			*/
ptr_t GC_generic_malloc_inner(/* bytes, kind */);
				/* Ditto, but I already hold lock, etc.	*/
ptr_t GC_generic_malloc_words_small(/*words, kind*/);
				/* As above, but size in units of words */
				/* Bypasses MERGE_SIZES.  Assumes	*/
				/* words <= MAXOBJSZ.			*/
ptr_t GC_allocobj(/* sz_inn_words, kind */);
				/* Make the indicated 			  */
				/* free list nonempty, and return its	*/
				/* head.				*/
				
bool GC_install_header(/*h*/);
				/* Install a header for block h.	*/
				/* Return FALSE on failure.		*/
bool GC_install_counts(/*h, sz*/);
				/* Set up forwarding counts for block	*/
				/* h of size sz.			*/
				/* Return FALSE on failure.		*/
void GC_remove_header(/*h*/);
				/* Remove the header for block h.	*/
void GC_remove_counts(/*h, sz*/);
				/* Remove forwarding counts for h.	*/
hdr * GC_find_header(/*p*/);	/* Debugging only.			*/

void GC_finalize();	/* Perform all indicated finalization actions	*/
			/* on unmarked objects.				*/
			
void GC_add_to_heap(/*p, bytes*/);
			/* Add a HBLKSIZE aligned chunk to the heap.	*/

void GC_print_obj(/* ptr_t p */);
			/* P points to somewhere inside an object with	*/
			/* debugging info.  Print a human readable	*/
			/* description of the object to stderr.		*/
extern void (*GC_check_heap)();
			/* Check that all objects in the heap with 	*/
			/* debugging info are intact.  Print 		*/
			/* descriptions of any that are not.		*/
			
/* Virtual dirty bit implementation:		*/
/* Each implementation exports the following:	*/
void GC_read_dirty();	/* Retrueve dirty bits.	*/
bool GC_page_was_dirty(/* struct hblk * h  */);
			/* Read retrieved dirty bits.	*/
void GC_write_hint(/* struct hblk * h  */);
			/* h is about to be written.	*/
void GC_dirty_init();

/* Slow/general mark bit manipulation: */
bool GC_is_marked();
void GC_clear_mark_bit();
void GC_set_mark_bit();

/* Stubborn objects: */
void GC_read_changed();	/* Analogous to GC_read_dirty */
bool GC_page_was_changed(/* h */);	/* Analogous to GC_page_was_dirty */
void GC_clean_changing_list();	/* Collect obsolete changing list entries */
void GC_stubborn_init();

/* Debugging print routines: */
void GC_print_block_list();
void GC_print_hblkfreelist();

/* Logging and diagnostic output: 	*/
void GC_printf(/* format, a, b, c, d, e, f */);
			/* A version of printf that doesn't allocate,	*/
			/* is restricted to long arguments, and		*/
			/* (unfortunately) doesn't use varargs for	*/
			/* portability.  Restricted to 6 args and	*/
			/* 1K total output length.			*/
			/* (We use sprintf.  Hopefully that doesn't	*/
			/* allocate for long arguments.)  		*/
# define GC_printf0(f) GC_printf(f, 0l, 0l, 0l, 0l, 0l, 0l)
# define GC_printf1(f,a) GC_printf(f, (long)a, 0l, 0l, 0l, 0l, 0l)
# define GC_printf2(f,a,b) GC_printf(f, (long)a, (long)b, 0l, 0l, 0l, 0l)
# define GC_printf3(f,a,b,c) GC_printf(f, (long)a, (long)b, (long)c, 0l, 0l, 0l)
# define GC_printf4(f,a,b,c,d) GC_printf(f, (long)a, (long)b, (long)c, \
					    (long)d, 0l, 0l)
# define GC_printf5(f,a,b,c,d,e) GC_printf(f, (long)a, (long)b, (long)c, \
					      (long)d, (long)e, 0l)
# define GC_printf6(f,a,b,c,d,e,g) GC_printf(f, (long)a, (long)b, (long)c, \
						(long)d, (long)e, (long)g)

void GC_err_printf(/* format, a, b, c, d, e, f */);
# define GC_err_printf0(f) GC_err_puts(f)
# define GC_err_printf1(f,a) GC_err_printf(f, (long)a, 0l, 0l, 0l, 0l, 0l)
# define GC_err_printf2(f,a,b) GC_err_printf(f, (long)a, (long)b, 0l, 0l, 0l, 0l)
# define GC_err_printf3(f,a,b,c) GC_err_printf(f, (long)a, (long)b, (long)c, \
						  0l, 0l, 0l)
# define GC_err_printf4(f,a,b,c,d) GC_err_printf(f, (long)a, (long)b, \
						    (long)c, (long)d, 0l, 0l)
# define GC_err_printf5(f,a,b,c,d,e) GC_err_printf(f, (long)a, (long)b, \
						      (long)c, (long)d, \
						      (long)e, 0l)
# define GC_err_printf6(f,a,b,c,d,e,g) GC_err_printf(f, (long)a, (long)b, \
							(long)c, (long)d, \
							(long)e, (long)g)
			/* Ditto, writes to stderr.			*/
			
void GC_err_puts(/* char *s */);
			/* Write s to stderr, don't buffer, don't add	*/
			/* newlines, don't ...				*/

# endif /* GC_PRIVATE_H */
