# define SILENT
/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991, 1992 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
 
/* Machine specific parts contributed by various people.  See README file. */

# ifndef GC_PRIVATE_H
# define GC_PRIVATE_H

# ifndef GC_H
#   include "gc.h"
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
			
#ifdef __STDC__
#   if !(defined( sony_news ) )
#       include <stddef.h>
#   endif
    typedef void * extern_ptr_t;
#else
    typedef char * extern_ptr_t;
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

# if defined(sun) && defined(mc68000)
#    define M68K_SUN
#    define mach_type_known
# endif
# if defined(hp9000s300)
#    define M68K_HP
#    define mach_type_known
# endif
# if defined(vax)
#    define VAX
#    ifdef ultrix
#	define ULTRIX
#    else
#	define BSD
#    endif
#    define mach_type_known
# endif
# if defined(mips)
#    define MIPS
#    ifdef ultrix
#	define ULTRIX
#    else
#	define RISCOS
#    endif
#    define mach_type_known
# endif
# if defined(sequent) && defined(i386)
#    define I386
#    define SEQUENT
#    define mach_type_known
# endif
# if defined(__OS2__) && defined(__32BIT__)
#    define I386
#    define OS2
#    define mach_type_known
# endif
# if defined(ibm032)
#   define RT
#   define mach_type_known
# endif
# if defined(sun) && defined(sparc)
#   define SPARC
#   define mach_type_known
# endif
# if defined(_IBMR2)
#   define IBMRS6000
#   define mach_type_known
# endif
# if defined(SCO)
#   define I386
#   define SCO
#   define mach_type_known
/*	--> incompletely implemented */
# endif
# if defined(_AUX_SOURCE)
#   define M68K_SYSV
#   define mach_type_known
# endif
# if defined(_PA_RISC1_0) || defined(_PA_RISC1_1)
#   define HP_PA
#   define mach_type_known
# endif
# if defined(linux) && defined(i386)
#    define I386
#    define LINUX
#    define mach_type_known
# endif

# ifndef OS2
#   include <sys/types.h>
# endif

/* Feel free to add more clauses here */

/* Or manually define the machine type here.  A machine type is 	*/
/* characterized by the architecture and assembler syntax.  Some	*/
/* machine types are further subdivided by OS.  In that case, we use	*/
/* the macros ULTRIX, RISCOS, and BSD to distinguish.			*/
/* Note that SGI IRIX is treated identically to RISCOS.			*/
/* The distinction in these cases is usually the stack starting address */
# ifndef mach_type_known
#   define M68K_SUN /* Guess "Sun" */
		    /* Mapping is: M68K_SUN   ==> Sun3 assembler 	*/
		    /*             M68K_HP    ==> HP9000/300 		*/
		    /*		   M68K_SYSV  ==> A/UX, maybe others	*/
		    /*             I386       ==> Intel 386	 	*/
		    /*		    (SEQUENT, OS2, SCO, LINUX variants)	*/
		    /*		     SCO is incomplete.			*/
                    /*             NS32K      ==> Encore Multimax 	*/
                    /*             MIPS       ==> R2000 or R3000	*/
                    /*			(RISCOS, ULTRIX variants)	*/
                    /*		   VAX	      ==> DEC VAX		*/
                    /*			(BSD, ULTRIX variants)		*/
                    /*		   RS6000     ==> IBM RS/6000 AIX3.1	*/
                    /*		   RT	      ==> IBM PC/RT		*/
                    /*		   HP_PA      ==> HP9000/700 & /800	*/
                    /*				  HP/UX			*/
		    /*		   SPARC      ==> SPARC under SunOS	*/
# endif

# ifdef SPARC
  /* Test for SunOS 5.x */
#   include <errno.h>
#   ifdef ECHRNG
#     define SUNOS5
#   endif
# endif

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


#ifdef SPARC
#   define ALIGN_DOUBLE  /* Align objects of size > 1 word on 2 word   */
			 /* boundaries.  Wasteful of memory, but       */
			 /* apparently required by SPARC architecture. */
#endif

#if defined(SPARC) || defined(M68K_SUN)
# if !defined(PCR) && !defined(SUNOS5)
#   define DYNAMIC_LOADING /* Search dynamic libraries for roots.	*/
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

#if defined(M68K_SUN) || defined(M68K_SYSV)
#  define ALIGNMENT   2   /* Pointers are aligned on 2 byte boundaries */
			  /* by the Sun C compiler.                    */
#else
#  ifdef VAX
#    define ALIGNMENT 4   /* Pointers are longword aligned by 4.2 C compiler */
#  else
#    ifdef RT
#      define ALIGNMENT 4
#    else
#      ifdef SPARC
#        define ALIGNMENT 4
#      else
#        ifdef I386
#           define ALIGNMENT 4      /* 32-bit compilers align pointers */
#        else
#          ifdef NS32K
#            define ALIGNMENT 4     /* Pointers are aligned on NS32K */
#          else
#            ifdef MIPS
#              define ALIGNMENT 4   /* MIPS hardware requires pointer */
				    /* alignment                      */
#            else
#              ifdef M68K_HP
#                define ALIGNMENT 2 /* 2 byte alignment inside struct/union, */
				    /* 4 bytes elsewhere 		     */
#              else
#		 ifdef IBMRS6000
#		   define ALIGNMENT 4
#		 else
#		   ifdef HP_PA
#		     define ALIGNMENT 4
#		   else
		     --> specify alignment <--
#		   endif
#		 endif
#              endif
#            endif
#          endif
#        endif
#      endif
#    endif
#  endif
# endif

/*
 * STACKBOTTOM is the cool end of the stack, which is usually the
 * highest address in the stack.
 * Under PCR or OS/2, we have other ways of finding thread stacks.
 * For each machine, the following should:
 * 1) define STACK_GROWS_UP if the stack grows toward higher addresses, and
 * 2) define exactly one of
 *	STACKBOTTOM (should be defined to be an expression)
 *	HEURISTIC1
 *	HEURISTIC2
 * If either of the last two macros are defined, then STACKBOTTOM is computed
 * during collector startup using one of the following two heuristics:
 * HEURISTIC1:  Take an address inside GC_init's frame, and round it up to
 *		the next multiple of 16 MB.
 * HEURISTIC2:  Take an address inside GC_init's frame, increment it repeatedly
 *		in small steps (decrement if STACK_GROWS_UP), and read the value
 *		at each location.  Remember the value when the first
 *		Segmentation violation or Bus error is signalled.  Round that
 *		to the nearest plausible page boundary, and use that instead
 *		of STACKBOTTOM.
 *
 * If no expression for STACKBOTTOM can be found, and neither of the above
 * heuristics are usable, the collector can still be used with all of the above
 * undefined, provided one of the following is done:
 * 1) GC_mark_roots can be changed to somehow mark from the correct stack(s)
 *    without reference to STACKBOTTOM.  This is appropriate for use in
 *    conjunction with thread packages, since there will be multiple stacks.
 *    (Allocating thread stacks in the heap, and treating them as ordinary
 *    heap data objects is also possible as a last resort.  However, this is
 *    likely to introduce significant amounts of excess storage retention
 *    unless the dead parts of the thread stacks are periodically cleared.)
 * 2) Client code may set GC_stackbottom before calling any GC_ routines.
 *    If the author of the client code controls the main program, this is
 *    easily accomplished by introducing a new main program, setting
 *    GC_stackbottom to the address of a local variable, and then calling
 *    the original main program.  The new main program would read something
 *    like:
 *
 *		# include "gc_private.h"
 *
 *		main(argc, argv, envp)
 *		int argc;
 *		char **argv, **envp;
 *		{
 *		    int dummy;
 *
 *		    GC_stackbottom = (ptr_t)(&dummy);
 *		    return(real_main(argc, argv, envp));
 *		}
 */
#ifndef PCR
# ifdef RT
#   define STACKBOTTOM ((ptr_t) 0x1fffd800)
# else
#   ifdef I386
#     ifdef SEQUENT
#       define STACKBOTTOM ((ptr_t) 0x3ffff000)  /* For Sequent */
#     else
#	ifdef SCO
#	  define STACKBOTTOM ((ptr_t) 0x7ffffffc)
#	else
#	  ifdef LINUX
#	    define STACKBOTTOM ((ptr_t)0xc0000000)
#	  else
#	    ifdef OS2
 	    	/* This is handled specially in GC_init_inner.	*/
		/* OS2 actually has the right system call!	*/
#	    else
		 --> Your OS isnt supported yet
#	    endif
#	  endif
#	endif
#     endif
#   else
#     ifdef NS32K
#       define STACKBOTTOM ((ptr_t) 0xfffff000) /* for Encore */
#     else
#       ifdef M68K_SYSV
#	  define STACKBOTTOM ((ptr_t)0xFFFFFFFE)
			/* The stack starts at the top of memory, but   */
			/* 0x0 cannot be used as setjump_test complains */
			/* that the stack direction is incorrect.  Two  */
			/* bytes down from 0x0 should be safe enough.   */
			/* 		--Parag				*/
#       else
#         ifdef M68K_HP
#           define STACKBOTTOM ((ptr_t) 0xffeffffc)
			      /* empirically determined.  seems to work. */
#	  else
#	    ifdef IBMRS6000
#	      define STACKBOTTOM ((ptr_t) 0x2ff80000)
#           else
#	      if defined(VAX) && defined(ULTRIX)
#		ifdef ULTRIX
#		  define STACKBOTTOM ((ptr_t) 0x7fffc800)
#		else
#		  define HEURISTIC1
			/* HEURISTIC2 may be OK, but it's hard to test. */
#		endif
#	      else
		  /* Sun systems, HP PA systems, and DEC MIPS systems have  */
		  /* STACKBOTTOM values that differ between machines that   */
		  /* are intended to be object code compatible.		    */
#		    if defined(SPARC) || defined(M68K_SUN)
#		      define HEURISTIC1
#		    else
#		      ifdef HP_PA
#			define STACK_GROWS_UP
#		      endif
#		      define HEURISTIC2
#		    endif
#	      endif
#	    endif
#         endif
#       endif
#     endif
#   endif
# endif
#endif /* PCR */


# ifndef STACK_GROWS_UP
#   define STACK_GROWS_DOWN
# endif

/* Start of data segment for each of the above systems.  Note that the */
/* default case works only for contiguous text and data, such as on a  */
/* Vax.                                                                */
# ifdef M68K_SUN
    extern char etext;
#   define DATASTART ((ptr_t)((((word) (&etext)) + 0x1ffff) & ~0x1ffff))
# else
#   ifdef RT
#     define DATASTART ((ptr_t) 0x10000000)
#   else
#     if (defined(I386) && (defined(SEQUENT)||defined(LINUX))) || defined(SPARC)
	extern int etext;
#	ifdef SUNOS5
#         define DATASTART ((ptr_t)((((word) (&etext)) + 0x10003) & ~0x3))
		/* Experimentally determined.  			*/
		/* Inconsistent with man a.out, which appears	*/
		/* to be wrong.					*/
#	else
#         define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
		/* On very old SPARCs this is too conservative. */
#	endif
#     else
#       ifdef NS32K
	  extern char **environ;
#         define DATASTART ((ptr_t)(&environ))
			      /* hideous kludge: environ is the first   */
			      /* word in crt0.o, and delimits the start */
			      /* of the data segment, no matter which   */
			      /* ld options were passed through.        */
#       else
#         ifdef MIPS
#           define DATASTART 0x10000000
			      /* Could probably be slightly higher since */
			      /* startup code allocates lots of junk     */
#         else
#           ifdef M68K_HP
	      extern char etext;
#             define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
#	    else
#             ifdef IBMRS6000
#		define DATASTART ((ptr_t)0x20000000)
#	      else
#		ifdef I386
#		  ifdef SCO
#   		    define DATASTART ((ptr_t)((((word) (&etext)) + 0x3fffff) \
					    & ~0x3fffff) \
					    +((word)&etext & 0xfff))
#		  else
#		    ifdef OS2
#   		      define DATASTART ((ptr_t)((((word) (&etext)) + 0x3fffff) \
					    & ~0x3fffff) \
					    +((word)&etext & 0xfff))
#		    else
			--> Your OS not supported yet
#		    endif
#		  endif
#		else
#		  ifdef M68K_SYSV
	/* This only works for shared-text binaries with magic number 0413.
	   The other sorts of SysV binaries put the data at the end of the text,
	   in which case the default of &etext would work.  Unfortunately,
	   handling both would require having the magic-number available.
	   	   		-- Parag
	   */
	   	    extern etext;
#   		    define DATASTART ((ptr_t)((((word) (&etext)) + 0x3fffff) \
					      & ~0x3fffff) \
					      +((word)&etext & 0x1fff))    
#                 else
#		    ifdef HP_PA
			extern int __data_start;
#			define DATASTART ((ptr_t)(&__data_start))
#		    else
		    	extern char etext;
#                   	define DATASTART ((ptr_t)(&etext))
#		    endif
#		  endif
#		endif
#	      endif
#           endif
#         endif
#       endif
#     endif
#   endif
# endif

# define HINCR 16          /* Initial heap increment, in blocks of 4K        */
# define MAXHINCR 512      /* Maximum heap increment, in blocks              */
# define HINCR_MULT 3      /* After each new allocation, GC_hincr is multiplied */
# define HINCR_DIV 2       /* by HINCR_MULT/HINCR_DIV                        */
# define GC_MULT 3         /* Don't collect if the fraction of   */
			   /* non-collectable memory in the heap */
			   /* exceeds GC_MUL/GC_DIV              */
# define GC_DIV  4

# define NON_GC_HINCR ((word)8)
			   /* Heap increment if most of heap if collection */
			   /* was suppressed because most of heap is not   */
			   /* collectable                                  */

/*********************************/
/*                               */
/* OS interface routines	 */
/*                               */
/*********************************/

#include <time.h>
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
# ifdef OS2
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
#   define THREADS
# else
#   ifdef OS2
      void * os2_alloc(size_t bytes);
#     define GET_MEM(bytes) HBLKPTR((ptr_t)os2_alloc((size_t)bytes + HBLKSIZE) \
                                    + HBLKSIZE-1)
#   else
      caddr_t sbrk();
#     ifdef __STDC__
#       define GET_MEM(bytes) HBLKPTR(sbrk((size_t)(bytes + HBLKSIZE)) \
  				    + HBLKSIZE-1)
#     else
#       define GET_MEM(bytes) HBLKPTR(sbrk((int)(bytes + HBLKSIZE)) \
                                      + HBLKSIZE-1)
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
 * If signals cannot be tolerated with the FASTLOCK held, then
 * FASTLOCK should disable signals.  The code executed under
 * FASTLOCK is otherwise immune to interruption, provided it is
 * not restarted.
 * DCL_LOCK_STATE declares any local variables needed by LOCK and UNLOCK
 * and/or DISABLE_SIGNALS and ENABLE_SIGNALS and/or FASTLOCK.
 * (There is currently no equivalent for FASTLOCK.)
 */  
# ifdef PCR
#    include  "pcr/th/PCR_Th.h"
#    include  "pcr/th/PCR_ThCrSec.h"
     extern struct PCR_Th_MLRep GC_allocate_ml;
#    define DCL_LOCK_STATE  PCR_sigset_t GC_old_sig_mask
#    define LOCK() PCR_Th_ML_Acquire(&GC_allocate_ml) 
#    define UNLOCK() PCR_Th_ML_Release(&GC_allocate_ml)
#    define FASTLOCK() PCR_ThCrSec_EnterSys()
     /* Here we cheat (a lot): */
#        define FASTLOCK_SUCCEEDED() (*(int *)(&GC_allocate_ml) == 0)
		/* TRUE if nobody currently holds the lock */
#    define FASTUNLOCK() PCR_ThCrSec_ExitSys()
# else
#    define DCL_LOCK_STATE
#    define LOCK()
#    define UNLOCK()
#    define FASTLOCK() LOCK()
#    define FASTLOCK_SUCCEEDED() TRUE
#    define FASTUNLOCK() UNLOCK()
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
#   if 0	/* Useful for debugging, and unusually		*/
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
#     include "pcr/th/PCR_ThCtl.h"
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
#   define ABORT(s) abort(s)
# endif

/* Exit abnormally, but without making a mess (e.g. out of memory) */
# ifdef PCR
    void PCR_Base_Exit(int status);
#   define EXIT() PCR_Base_Exit(1)
# else
#   define EXIT() (void)exit(1)
# endif

/* Print warning message, e.g. almost out of memory.	*/
# define WARN(s) GC_printf(s)

/*********************************/
/*                               */
/* Word-size-dependent defines   */
/*                               */
/*********************************/

#define WORDS_TO_BYTES(x)   ((x)<<2)
#define BYTES_TO_WORDS(x)   ((x)>>2)

#define CPP_WORDSZ              32
#define WORDSZ ((word)CPP_WORDSZ)
#define LOGWL               ((word)5)    /* log[2] of above */
#define BYTES_PER_WORD      ((word)(sizeof (word)))
#define ONES                0xffffffff
#define MSBYTE              0xff000000
#define SIGNB               0x80000000
#define MAXSHORT            0x7fff
#define modHALFWORDSZ(n) ((n) & 0xf)    /* mod n by size of half word    */
#define divHALFWORDSZ(n) ((n) >> 4)	/* divide n by size of half word */
#define modWORDSZ(n) ((n) & 0x1f)       /* mod n by size of word         */
#define divWORDSZ(n) ((n) >> 5)         /* divide n by size of word      */
#define twice(n) ((n) << 1)             /* double n                      */

/*********************/
/*                   */
/*  Size Parameters  */
/*                   */
/*********************/

/*  heap block size, bytes. Should be power of 2 */

#define CPP_LOG_HBLKSIZE 12
#define LOG_HBLKSIZE   ((word)CPP_LOG_HBLKSIZE)
#define CPP_HBLKSIZE (1 << CPP_LOG_HBLKSIZE)
#define HBLKSIZE ((word)CPP_HBLKSIZE)


/*  max size objects supported by freelist (larger objects may be   */
/*  allocated, but less efficiently)                                */

#define CPP_MAXOBJSZ    BYTES_TO_WORDS(CPP_HBLKSIZE/2)
#define MAXOBJSZ ((word)CPP_MAXOBJSZ)
		
# define divHBLKSZ(n) ((n) >> LOG_HBLKSIZE)
 
# define modHBLKSZ(n) ((n) & (HBLKSIZE-1))
 
# define HBLKPTR(objptr) ((struct hblk *)(((word) (objptr)) & ~(HBLKSIZE-1)))

# define HBLKDISPL(objptr) (((word) (objptr)) & (HBLKSIZE-1))


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
    int hb_obj_kind;   /* Kind of objects in the block.  Each kind 	*/
    			/* identifies a mark procedure and a set of 	*/
    			/* list headers.  sometimes called regions.	*/
      	
    word hb_marks[MARK_BITS_SZ];
			    /* Bit i in the array refers to the             */
			    /* object starting at the ith word (header      */
			    /* INCLUDED) in the heap block.                 */
};

/*  heap block body */

# define DISCARD_WORDS 0
	/* Number of words to be dropped at the beginning of each block	*/
	/* Must be a multiple of 32.  May reasonably be nonzero		*/
	/* on mcachines that don't guarantee longword alignment of	*/
	/* pointers, so that the number of false hits is minimized.	*/
	/* 0 and 32 are probably the only reasonable values.		*/

# define BODY_SZ ((HBLKSIZE-WORDS_TO_BYTES(DISCARD_WORDS))/sizeof(word))

struct hblk {
#   if (DISCARD_WORDS != 0)
        word garbage[DISCARD_WORDS];
#   endif
    word hb_body[BODY_SZ];
};

# define HDR_WORDS ((word)DISCARD_WORDS)
# define HDR_BYTES ((word)WORDS_TO_BYTES(DISCARD_WORDS))

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
  ptr_t _objfreelist[MAXOBJSZ+1];
			  /* free list for objects */
# ifdef MERGE_SIZES
    unsigned _size_map[WORDS_TO_BYTES(MAXOBJSZ+1)];
    	/* Number of words to allocate for a given allocation request in */
    	/* bytes.							 */
# endif 
  ptr_t _aobjfreelist[MAXOBJSZ+1];
			  /* free list for atomic objs*/
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
};

extern struct _GC_arrays GC_arrays; 

# define GC_objfreelist GC_arrays._objfreelist
# define GC_aobjfreelist GC_arrays._aobjfreelist
# define GC_valid_offsets GC_arrays._valid_offsets
# define GC_modws_valid_offsets GC_arrays._modws_valid_offsets
# define GC_reclaim_list GC_arrays._reclaim_list
# define GC_areclaim_list GC_arrays._areclaim_list
# define GC_obj_map GC_arrays._obj_map
# define GC_last_heap_addr GC_arrays._last_heap_addr
# define GC_prev_heap_addr GC_arrays._prev_heap_addr
# define GC_words_allocd GC_arrays._words_allocd
# define GC_heapsize GC_arrays._heapsize
# define GC_words_allocd_before_gc GC_arrays._words_allocd_before_gc
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

# ifndef PCR
    extern ptr_t GC_stackbottom;	/* Cool end of user stack	*/
# endif

extern word GC_hincr;      	/* current heap increment, in blocks	*/

extern word GC_root_size;	/* Total size of registered root sections */

extern bool GC_debugging_started;	/* GC_debug_malloc has been called. */ 

extern ptr_t GC_least_plausible_heap_addr;
extern ptr_t GC_greatest_plausible_heap_addr;
			/* Bounds on the heap.  Guaranteed valid	*/
			/* Likely to include future heap expansion.	*/
			
/* Operations */
# define update_GC_hincr  GC_hincr = (GC_hincr * HINCR_MULT)/HINCR_DIV; \
		       if (GC_hincr > MAXHINCR) {GC_hincr = MAXHINCR;}
# ifndef abs
#   define abs(x)  ((x) < 0? (-(x)) : (x))
# endif

/****************************/
/*                          */
/*   Objects                */
/*                          */
/****************************/


/*  Marks are in a reserved area in                          */
/*  each heap block.  Each word has one mark bit associated  */
/*  with it. Only those corresponding to the beginning of an */
/*  object are used.                                         */


/* Operations */

/*
 * Retrieve, set, clear the mark bit corresponding
 * to the nth word in a given heap block.
 *
 * (Recall that bit n corresponds to object beginning at word n
 * relative to the beginning of the block, including unused words)
 */

# define mark_bit_from_hdr(hhdr,n) (((hhdr)->hb_marks[divWORDSZ(n)] \
			    >> (modWORDSZ(n))) & 1)
# define set_mark_bit_from_hdr(hhdr,n) (hhdr)->hb_marks[divWORDSZ(n)] \
				|= 1 << modWORDSZ(n)

# define clear_mark_bit_from_hdr(hhdr,n) (hhdr)->hb_marks[divWORDSZ(n)] \
				&= ~(1 << modWORDSZ(n))

/* Important internal collector routines */

void GC_apply_to_all_blocks(/*fn, client_data*/);
			/* Invoke fn(hbp, client_data) for each 	*/
			/* allocated heap block.			*/
mse * GC_no_mark_proc(/*addr,hhdr,msp,msl*/);
			/* Mark procedure for PTRFREE objects.	*/
mse * GC_normal_mark_proc(/*addr,hhdr,msp,msl*/);
			/* Mark procedure for NORMAL objects.	*/
void GC_mark_init();
void GC_mark();  		/* Mark from everything on the mark stack. */
void GC_mark_reliable();	/* as above, but fix things up after	*/
				/* a mark stack overflow.		*/
void GC_mark_all(/*b,t*/);	/* Mark from everything in a range.	*/
void GC_mark_all_stack(/*b,t*/);    /* Mark from everything in a range,	    */
				    /* consider interior pointers as valid  */
void GC_remark();	/* Mark from all marked objects.  Used	*/
		 	/* only if we had to drop something.	*/
void GC_tl_mark(/*p*/);	/* Mark from a single root.		*/
void GC_add_roots_inner();

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
void GC_add_map_entry(/*sz*/);
				/* Add a heap block map for objects of	*/
				/* size sz to obj_map.			*/
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
void GC_gcollect_inner();	/* Collect; caller must have acquired	*/
				/* lock and disabled signals.		*/
void GC_init();			/* Initialize collector.		*/

ptr_t GC_generic_malloc(/* bytes, kind */);
				/* Allocate an object of the given	*/
				/* kind.  By default, there are only	*/
				/* two kinds: composite, and atomic.	*/
				/* We claim it's possible for clever	*/
				/* client code that understands GC	*/
				/* internals to add more, e.g. to	*/
				/* communicate object layout info	*/
				/* to the collector.			*/
ptr_t GC_generic_malloc_words_small(/*words, kind*/);
				/* As above, but size in units of words */
				/* Bypasses MERGE_SIZES.  Assumes	*/
				/* words <= MAXOBJSZ.			*/
ptr_t GC_allocobj(/* sz_inn_words, kind */);
				/* Make the indicated 			  */
				/* free list nonempty, and return its	*/
				/* head.				*/
				
void GC_install_header(/*h*/);
				/* Install a header for block h.	*/
void GC_install_counts(/*h, sz*/);
				/* Set up forwarding counts for block	*/
				/* h of size sz.			*/
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
void GC_check_heap();
			/* Check that all objects in the heap with 	*/
			/* debugging info are intact.  Print 		*/
			/* descriptions of any that are not.		*/

void GC_printf(/* format, ... */);
			/* A version of printf that doesn't allocate,	*/
			/* is restricted to long arguments, and		*/
			/* (unfortunately) doesn't use varargs for	*/
			/* portability.  Restricted to 6 args and	*/
			/* 1K total output length.			*/
			/* (We use sprintf.  Hopefully that doesn't	*/
			/* allocate for long arguments.  		*/
void GC_err_printf(/* format, ... */);
			/* Ditto, writes to stderr.			*/
void GC_err_puts(/* char *s */);
			/* Write s to stderr, don't buffer, don't add	*/
			/* newlines, don't ...				*/

# endif /* GC_PRIVATE_H */
