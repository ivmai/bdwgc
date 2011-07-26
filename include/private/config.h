/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
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
 */
/* Boehm, October 3, 1995 6:39 pm PDT */
 
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
# if defined(__NetBSD__) && defined(m68k)
#    define M68K
#    define NETBSD
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
# if defined(mips) || defined(__mips)
#    define MIPS
#    if defined(ultrix) || defined(__ultrix)
#	define ULTRIX
#    else
#	if defined(_SYSTYPE_SVR4) || defined(SYSTYPE_SVR4) || defined(__SYSTYPE_SVR4__)
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
# if (defined(__OS2__) || defined(__EMX__)) && defined(__32BIT__)
#    define I386
#    define OS2
#    define mach_type_known
# endif
# if defined(ibm032)
#   define RT
#   define mach_type_known
# endif
# if defined(sun) && (defined(sparc) || defined(__sparc))
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
# if defined(sparc) && defined(unix) && !defined(sun)
#   define SPARC
#   define DRSNX
#   define mach_type_known
# endif
# if defined(_IBMR2)
#   define RS6000
#   define mach_type_known
# endif
# if defined(_M_XENIX) && defined(_M_SYSV) && defined(_M_I386)
	/* The above test may need refinement	*/
#   define I386
#   define SCO
#   define mach_type_known
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
#   define M68K
#   define AMIGA
#   define mach_type_known
# endif
# if defined(THINK_C) || defined(__MWERKS__) && !defined(__powerc)
#   define M68K
#   define MACOS
#   define mach_type_known
# endif
# if defined(__MWERKS__) && defined(__powerc)
#   define POWERPC
#   define MACOS
#   define mach_type_known
# endif
# if defined(NeXT) && defined(mc68000)
#   define M68K
#   define NEXT
#   define mach_type_known
# endif
# if defined(NeXT) && defined(i386)
#   define I386
#   define NEXT
#   define mach_type_known
# endif
# if defined(__FreeBSD__) && defined(i386)
#   define I386
#   define FREEBSD
#   define mach_type_known
# endif
# if defined(__NetBSD__) && defined(i386)
#   define I386
#   define NETBSD
#   define mach_type_known
# endif
# if defined(bsdi) && defined(i386)
#    define I386
#    define BSDI
#    define mach_type_known
# endif
# if !defined(mach_type_known) && defined(__386BSD__)
#   define I386
#   define THREE86BSD
#   define mach_type_known
# endif
# if defined(_CX_UX) && defined(_M88K)
#   define M88K
#   define CX_UX
#   define mach_type_known
# endif
# if defined(DGUX)
#   define M88K
    /* DGUX defined */
#   define mach_type_known
# endif
# if defined(_MSDOS) && (_M_IX86 == 300) || (_M_IX86 == 400)
#   define I386
#   define MSWIN32	/* or Win32s */
#   define mach_type_known
# endif
# if defined(GO32)
#   define I386
#   define DJGPP  /* MSDOS running the DJGPP port of GCC */
#   define mach_type_known
# endif
# if defined(__BORLANDC__)
#   define I386
#   define MSWIN32
#   define mach_type_known
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
		    /*		   MACOS and AMIGA variants)		*/
		    /*             I386       ==> Intel 386	 	*/
		    /*		    (SEQUENT, OS2, SCO, LINUX, NETBSD,	*/
		    /*		     FREEBSD, THREE86BSD, MSWIN32,	*/
		    /* 		     BSDI, SUNOS5, NEXT	variants)	*/
                    /*             NS32K      ==> Encore Multimax 	*/
                    /*             MIPS       ==> R2000 or R3000	*/
                    /*			(RISCOS, ULTRIX variants)	*/
                    /*		   VAX	      ==> DEC VAX		*/
                    /*			(BSD, ULTRIX variants)		*/
                    /*		   RS6000     ==> IBM RS/6000 AIX3.X	*/
                    /*		   RT	      ==> IBM PC/RT		*/
                    /*		   HP_PA      ==> HP9000/700 & /800	*/
                    /*				  HP/UX			*/
		    /*		   SPARC      ==> SPARC under SunOS	*/
		    /*			(SUNOS4, SUNOS5,		*/
		    /*			 DRSNX variants)		*/
		    /* 		   ALPHA      ==> DEC Alpha OSF/1	*/
		    /* 		   M88K       ==> Motorola 88XX0        */
		    /* 		        (CX_UX and DGUX)		*/


/*
 * For each architecture and OS, the following need to be defined:
 *
 * CPP_WORD_SZ is a simple integer constant representing the word size.
 * in bits.  We assume byte addressibility, where a byte has 8 bits.
 * We also assume CPP_WORD_SZ is either 32 or 64.
 * (We care about the length of pointers, not hardware
 * bus widths.  Thus a 64 bit processor with a C compiler that uses
 * 32 bit pointers should use CPP_WORD_SZ of 32, not 64. Default is 32.)
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
 *		the next multiple of STACK_GRAN.
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
 *
 * An architecture may define DYNAMIC_LOADING if dynamic_load.c
 * defined GC_register_dynamic_libraries() for the architecture.
 */


# define STACK_GRAN 0x1000000
# ifdef M68K
#   define MACH_TYPE "M68K"
#   define ALIGNMENT 2
#   ifdef NETBSD
#	define OS_TYPE "NETBSD"
#	define HEURISTIC2
	extern char etext;
#	define DATASTART ((ptr_t)(&etext))
#   endif
#   ifdef SUNOS4
#	define OS_TYPE "SUNOS4"
	extern char etext;
#	define DATASTART ((ptr_t)((((word) (&etext)) + 0x1ffff) & ~0x1ffff))
#	define HEURISTIC1	/* differs	*/
#	define DYNAMIC_LOADING
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
# 	define DATAEND	/* not needed */
#   endif
#   ifdef MACOS
#     ifndef __LOWMEM__
#     include <LowMem.h>
#     endif
#     define OS_TYPE "MACOS"
			/* see os_dep.c for details of global data segments. */
#     define STACKBOTTOM ((ptr_t) LMGetCurStackBase())
#     define DATAEND	/* not needed */
#   endif
#   ifdef NEXT
#	define OS_TYPE "NEXT"
#	define DATASTART ((ptr_t) get_etext())
#	define STACKBOTTOM ((ptr_t) 0x4000000)
#	define DATAEND	/* not needed */
#   endif
# endif

# ifdef POWERPC
#   define MACH_TYPE "POWERPC"
#   define ALIGNMENT 2
#   ifdef MACOS
#     ifndef __LOWMEM__
#     include <LowMem.h>
#     endif
#     define OS_TYPE "MACOS"
			/* see os_dep.c for details of global data segments. */
#     define STACKBOTTOM ((ptr_t) LMGetCurStackBase())
#     define DATAEND  /* not needed */
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
	extern int _etext;
	extern int _end;
	extern char * GC_SysVGetDataStart();
#       define DATASTART (ptr_t)GC_SysVGetDataStart(0x10000, &_etext)
#	define DATAEND (&_end)
#	define PROC_VDB
#	define HEURISTIC1
#   endif
#   ifdef SUNOS4
#	define OS_TYPE "SUNOS4"
	/* [If you have a weak stomach, don't read this.]		*/
	/* We would like to use:					*/
/* #       define DATASTART ((ptr_t)((((word) (&etext)) + 0x1fff) & ~0x1fff)) */
	/* This fails occasionally, due to an ancient, but very 	*/
	/* persistent ld bug.  &etext is set 32 bytes too high.		*/
	/* We instead read the text segment size from the a.out		*/
	/* header, which happens to be mapped into our address space	*/
	/* at the start of the text segment.  The detective work here	*/
	/* was done by Robert Ehrlich, Manuel Serrano, and Bernard	*/
	/* Serpette of INRIA.						*/
	/* This assumes ZMAGIC, i.e. demand-loadable executables.	*/
#	define TEXTSTART 0x2000
#       define DATASTART ((ptr_t)(*(int *)(TEXTSTART+0x4)+TEXTSTART))
#	define MPROTECT_VDB
#	define HEURISTIC1
#   endif
#   ifdef DRSNX
#       define CPP_WORDSZ 32
#	define OS_TYPE "DRSNX"
	extern char * GC_SysVGetDataStart();
	extern int etext;
#       define DATASTART (ptr_t)GC_SysVGetDataStart(0x10000, &etext)
#	define MPROTECT_VDB
#       define STACKBOTTOM ((ptr_t) 0xdfff0000)
#   endif
#   define DYNAMIC_LOADING
# endif

# ifdef I386
#   define MACH_TYPE "I386"
#   define ALIGNMENT 4	/* Appears to hold for all "32 bit" compilers	*/
			/* except Borland.  The -a4 option fixes 	*/
			/* Borland.					*/
#   ifdef SEQUENT
#	define OS_TYPE "SEQUENT"
	extern int etext;
#       define DATASTART ((ptr_t)((((word) (&etext)) + 0xfff) & ~0xfff))
#       define STACKBOTTOM ((ptr_t) 0x3ffff000) 
#   endif
#   ifdef SUNOS5
#	define OS_TYPE "SUNOS5"
  	extern int etext, _start;
  	extern char * GC_SysVGetDataStart();
#       define DATASTART GC_SysVGetDataStart(0x1000, &etext)
#	define STACKBOTTOM ((ptr_t)(&_start))
#	define PROC_VDB
#   endif
#   ifdef SCO
#	define OS_TYPE "SCO"
	extern int etext;
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
#	define MPROTECT_VDB
#   endif
#   ifdef OS2
#	define OS_TYPE "OS2"
 	    	/* STACKBOTTOM and DATASTART are handled specially in 	*/
		/* os_dep.c. OS2 actually has the right			*/
		/* system call!						*/
#	define DATAEND	/* not needed */
#   endif
#   ifdef MSWIN32
#	define OS_TYPE "MSWIN32"
		/* STACKBOTTOM and DATASTART are handled specially in 	*/
		/* os_dep.c.						*/
#	define MPROTECT_VDB
#       define DATAEND  /* not needed */
#   endif
#   ifdef DJGPP
#       define OS_TYPE "DJGPP"
        extern int etext;
#       define DATASTART ((ptr_t)(&etext))
#       define STACKBOTTOM ((ptr_t)0x00080000)
#   endif
#   ifdef FREEBSD
#	define OS_TYPE "FREEBSD"
#	define MPROTECT_VDB
#   endif
#   ifdef NETBSD
#	define OS_TYPE "NETBSD"
#   endif
#   ifdef THREE86BSD
#	define OS_TYPE "THREE86BSD"
#   endif
#   ifdef BSDI
#	define OS_TYPE "BSDI"
#   endif
#   if defined(FREEBSD) || defined(NETBSD) \
        || defined(THREE86BSD) || defined(BSDI)
#	define HEURISTIC2
	extern char etext;
#	define DATASTART ((ptr_t)(&etext))
#   endif
#   ifdef NEXT
#	define OS_TYPE "NEXT"
#	define DATASTART ((ptr_t) get_etext())
#	define STACKBOTTOM ((ptr_t)0xc0000000)
#	define DATAEND	/* not needed */
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
		/* The above is dubious.  Mprotect and signals do work,	*/
		/* and dirty bits are implemented under IRIX5.  But,	*/
		/* at least under IRIX5.2, mprotect seems to be so	*/
		/* slow relative to the hardware that incremental	*/
		/* collection is likely to be rarely useful.		*/
#	define DYNAMIC_LOADING
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
	/* Normally HEURISTIC2 is too conervative, since		*/
	/* the text segment immediately follows the stack.		*/
	/* Hence we give an upper pound.				*/
    extern __start;
#   define HEURISTIC2_LIMIT ((ptr_t)((word)(&__start) & ~(getpagesize()-1)))
#   define CPP_WORDSZ 64
#   define MPROTECT_VDB
#   define DYNAMIC_LOADING
# endif

# ifdef M88K
#   define MACH_TYPE "M88K"
#   define ALIGNMENT 4
#   ifdef CX_UX
#       define DATASTART ((((word)&etext + 0x3fffff) & ~0x3fffff) + 0x10000)
#   endif
#   ifdef  DGUX
	extern char * GC_SysVGetDataStart();
#       define DATASTART (ptr_t)GC_SysVGetDataStart(0x10000, &etext)
#   endif
#   define STACKBOTTOM ((char*)0xf0000000) /* determined empirically */
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

# ifndef DATAEND
    extern int end;
#   define DATAEND (&end)
# endif

# if defined(SUNOS5) || defined(DRSNX)
    /* OS has SVR4 generic features.  Probably others also qualify.	*/
#   define SVR4
# endif

# if defined(SUNOS5) || defined(DRSNX)
    /* OS has SUNOS5 style semi-undocumented interface to dynamic 	*/
    /* loader.								*/
#   define SUNOS5DL
    /* OS has SUNOS5 style signal handlers.				*/
#   define SUNOS5SIGS
# endif

# if CPP_WORDSZ != 32 && CPP_WORDSZ != 64
   -> bad word size
# endif

# ifdef PCR
#   undef DYNAMIC_LOADING
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

# ifdef SMALL_CONFIG
/* Presumably not worth the space it takes. */
#   undef PROC_VDB
#   undef MPROTECT_VDB
# endif

# if !defined(PCR_VDB) && !defined(PROC_VDB) && !defined(MPROTECT_VDB)
#   define DEFAULT_VDB
# endif

# if defined(SPARC)
#   define SAVE_CALL_CHAIN
# endif

# endif
