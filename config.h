#ifndef CONFIG_H

# define CONFIG_H

/* Machine dependent parameters.  Some tuning parameters can be found	*/
/* near the top of gc_private.h.					*/

/* Machine specific parts contributed by various people.  See README file. */

/* Determine the machine type: */
# if defined(sun) && defined(mc68000)
#    define M68K
#    define SUNOS4
#    define mach_type_known
# endif
# if defined(hp9000s300)
#    define M68K
#    define HP
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
#	ifdef _SYSTYPE_SVR4
#	  define IRIX5
#	else
#	  define RISCOS  /* or IRIX 4.X */
#	endif
#    endif
#    define mach_type_known
# endif
# if defined(sequent) && defined(i386)
#    define I386
#    define SEQUENT
#    define mach_type_known
# endif
# if defined(sun) && defined(i386)
#    define I386
#    define SUNOS5
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
    /* Test for SunOS 5.x */
#     include <errno.h>
#     ifdef ECHRNG
#       define SUNOS5
#     else
#	define SUNOS4
#     endif
#   define mach_type_known
# endif
# if defined(_IBMR2)
#   define RS6000
#   define mach_type_known
# endif
# if defined(SCO)
#   define I386
#   define SCO
#   define mach_type_known
/*	--> incompletely implemented */
# endif
# if defined(_AUX_SOURCE)
#   define M68K
#   define SYSV
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
# if defined(__alpha)
#   define ALPHA
#   define mach_type_known
# endif
# if defined(_AMIGA)
#   define AMIGA
#   define M68K
#   define mach_type_known
# endif
# if defined(NeXT) && defined(mc68000)
#    define M68K
#    define NEXT
#    define mach_type_known
# endif

/* Feel free to add more clauses here */

/* Or manually define the machine type here.  A machine type is 	*/
/* characterized by the architecture.  Some				*/
/* machine types are further subdivided by OS.				*/
/* the macros ULTRIX, RISCOS, and BSD to distinguish.			*/
/* Note that SGI IRIX is treated identically to RISCOS.			*/
/* SYSV on an M68K actually means A/UX.					*/
/* The distinction in these cases is usually the stack starting address */
# ifndef mach_type_known
	--> unknown machine type
# endif
		    /* Mapping is: M68K       ==> Motorola 680X0	*/
		    /*		   (SUNOS4,HP,NEXT, and SYSV (A/UX),	*/
		    /*		   and AMIGA variants)			*/
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
		    /*			(SUNOS4, SUNOS5 variants)	*/
		    /* 		   ALPHA      ==> DEC Alpha OSF/1	*/


/*
 * For each architecture and OS, the following need to be defined:
 *
 * CPP_WORD_SZ is a simple integer constant representing the word size.
 * in bits.  We assume byte addressibility, where a byte has 8 bits.
 * We also assume CPP_WORD_SZ is either 32 or 64.  Only 32 is completely
 * implemented.  (We care about the length of pointers, not hardware
 * bus widths.  Thus a 64 bit processor with a C compiler that uses
 * 32 bit pointers should use CPP_WORD_SZ of 32, not 64.)
 *
 * MACH_TYPE is a string representation of the machine type.
 * OS_TYPE is analogous for the OS.
 *
 * ALIGNMENT is the largest N, such that
 * all pointer are guaranteed to be aligned on N byte boundaries.
 * defining it to be 1 will always work, but perform poorly.
 *
 * DATASTART is the beginning of the data segment.
 * On UNIX systems, the collector will scan the area between DATASTART
 * and &end for root pointers.
 *
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
 *
 *
 * Each architecture may also define the style of virtual dirty bit
 * implementation to be used:
 *   MPROTECT_VDB: Write protect the heap and catch faults.
 *   PROC_VDB: Use the SVR4 /proc primitives to read dirty bits.
 */


# ifdef M68K
#   define MACH_TYPE "M68K"
#   define ALIGNMENT 2
#   ifdef SUNOS4
#	define OS_TYPE "SUNOS4"
	extern char etext;
#	define DATASTART ((ptr_t)((((word) (&etext)) + 0x1ffff) & ~0x1ffff))
#	define HEURISTIC1	/* differs	*/
#   endif
#   ifdef HP
#	define OS_TYPE "HP"
	extern char etext;
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
#       define STACKBOTTOM ((ptr_t) 0xffeffffc)
			      /* empirically determined.  seems to work. */
#   endif
#   ifdef SYSV
#	define OS_TYPE "SYSV"
	extern etext;
#   	define DATASTART ((ptr_t)((((word) (&etext)) + 0x3fffff) \
				   & ~0x3fffff) \
				  +((word)&etext & 0x1fff))
	/* This only works for shared-text binaries with magic number 0413.
	   The other sorts of SysV binaries put the data at the end of the text,
	   in which case the default of &etext would work.  Unfortunately,
	   handling both would require having the magic-number available.
	   	   		-- Parag
	   */
#	define STACKBOTTOM ((ptr_t)0xFFFFFFFE)
			/* The stack starts at the top of memory, but   */
			/* 0x0 cannot be used as setjump_test complains */
			/* that the stack direction is incorrect.  Two  */
			/* bytes down from 0x0 should be safe enough.   */
			/* 		--Parag				*/
#   endif
#   ifdef AMIGA
#	define OS_TYPE "AMIGA"
 	    	/* STACKBOTTOM and DATASTART handled specially	*/
 	    	/* in os_dep.c					*/
#   endif
#   ifdef NEXT
#	define OS_TYPE "NEXT"
#	define DATASTART ((ptr_t) get_etext())
#	define STACKBOTTOM ((ptr_t) 0x4000000)
#   endif
# endif

# ifdef VAX
#   define MACH_TYPE "VAX"
#   define ALIGNMENT 4	/* Pointers are longword aligned by 4.2 C compiler */
    extern char etext;
#   define DATASTART ((ptr_t)(&etext))
#   ifdef BSD
#	define OS_TYPE "BSD"
#	define HEURISTIC1
			/* HEURISTIC2 may be OK, but it's hard to test. */
#   endif
#   ifdef ULTRIX
#	define OS_TYPE "ULTRIX"
#	define STACKBOTTOM ((ptr_t) 0x7fffc800)
#   endif
# endif

# ifdef RT
#   define MACH_TYPE "RT"
#   define ALIGNMENT 4
#   define DATASTART ((ptr_t) 0x10000000)
#   define STACKBOTTOM ((ptr_t) 0x1fffd800)
# endif

# ifdef SPARC
#   define MACH_TYPE "SPARC"
#   define ALIGNMENT 4	/* Required by hardware	*/
    extern int etext;
#   ifdef SUNOS5
#	define OS_TYPE "SUNOS5"
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0x10003) & ~0x3))
		/* Experimentally determined.  			*/
		/* Inconsistent with man a.out, which appears	*/
		/* to be wrong.					*/
#	define PROC_VDB
#   endif
#   ifdef SUNOS4
#	define OS_TYPE "SUNOS4"
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
		/* On very old SPARCs this is too conservative. */
#	define MPROTECT_VDB
#   endif
#   define HEURISTIC1
# endif

# ifdef I386
#   define MACH_TYPE "I386"
#   define ALIGNMENT 4	/* 32-bit compilers align pointers */
#   ifdef SEQUENT
#	define OS_TYPE "SEQUENT"
	extern int etext;
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
#       define STACKBOTTOM ((ptr_t) 0x3ffff000) 
#   endif
#   ifdef SUNOS5
#	define OS_TYPE "SUNOS5"
  	extern int etext;
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0x1003) & ~0x3))
	extern int _start();
#	define STACKBOTTOM ((ptr_t)(&_start))
#	define PROC_VDB
#   endif
#   ifdef SCO
#	define OS_TYPE "SCO"
#   	define DATASTART ((ptr_t)((((word) (&etext)) + 0x3fffff) \
				  & ~0x3fffff) \
				 +((word)&etext & 0xfff))
#	define STACKBOTTOM ((ptr_t) 0x7ffffffc)
#   endif
#   ifdef LINUX
#	define OS_TYPE "LINUX"
	extern int etext;
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
#	define STACKBOTTOM ((ptr_t)0xc0000000)
#   endif
#   ifdef OS2
#	define OS_TYPE "OS2"
#   	define DATASTART ((ptr_t)((((word) (&etext)) + 0x3fffff) \
				  & ~0x3fffff) \
				 +((word)&etext & 0xfff))
 	    	/* STACKBOTTOM is handled specially in GC_init_inner.	*/
		/* OS2 actually has the right system call!		*/
#   endif
# endif

# ifdef NS32K
#   define MACH_TYPE "NS32K"
#   define ALIGNMENT 4
    extern char **environ;
#   define DATASTART ((ptr_t)(&environ))
			      /* hideous kludge: environ is the first   */
			      /* word in crt0.o, and delimits the start */
			      /* of the data segment, no matter which   */
			      /* ld options were passed through.        */
#   define STACKBOTTOM ((ptr_t) 0xfffff000) /* for Encore */
# endif

# ifdef MIPS
#   define MACH_TYPE "MIPS"
#   define ALIGNMENT 4	/* Required by hardware	*/
#   define DATASTART 0x10000000
			      /* Could probably be slightly higher since */
			      /* startup code allocates lots of junk     */
#   define HEURISTIC2
#   ifdef ULTRIX
#	define OS_TYPE "ULTRIX"
#   endif
#   ifdef RISCOS
#	define OS_TYPE "RISCOS"
#   endif
#   ifdef IRIX5
#	define OS_TYPE "IRIX5"
#	define MPROTECT_VDB
#   endif
# endif

# ifdef RS6000
#   define MACH_TYPE "RS6000"
#   define ALIGNMENT 4
#   define DATASTART ((ptr_t)0x20000000)
#   define STACKBOTTOM ((ptr_t)0x2ff80000)
# endif

# ifdef HP_PA
#   define MACH_TYPE "HP_PA"
#   define ALIGNMENT 4
    extern int __data_start;
#   define DATASTART ((ptr_t)(&__data_start))
#   define HEURISTIC2
#   define STACK_GROWS_UP
# endif

# ifdef ALPHA
#   define MACH_TYPE "ALPHA"
#   define ALIGNMENT 8
#   define DATASTART ((ptr_t) 0x140000000)
#   define HEURISTIC2
#   define CPP_WORDSZ 64
#   define MPROTECT_VDB
# endif

# ifndef STACK_GROWS_UP
#   define STACK_GROWS_DOWN
# endif

# ifndef CPP_WORDSZ
#   define CPP_WORDSZ 32
# endif

# ifndef OS_TYPE
#   define OS_TYPE ""
# endif

# if CPP_WORDSZ != 32 && CPP_WORDSZ != 64
   -> bad word size
# endif

# ifdef PCR
#   undef STACKBOTTOM
#   undef HEURISTIC1
#   undef HEURISTIC2
#   undef PROC_VDB
#   undef MPROTECT_VDB
#   define PCR_VDB
# endif

# ifdef SRC_M3
/* Postponed for now. */
#   undef PROC_VDB
#   undef MPROTECT_VDB
# endif

# if !defined(PCR_VDB) && !defined(PROC_VDB) && !defined(MPROTECT_VDB)
#   define DEFAULT_VDB
# endif

# endif
