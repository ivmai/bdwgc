# include "gc_private.h"
# include <stdio.h>
# include <signal.h>

/* Disable and enable signals during nontrivial allocations	*/

# ifdef OS2

# define INCL_DOSEXCEPTIONS
# define INCL_DOSPROCESS
# define INCL_DOSERRORS
# define INCL_DOSMODULEMGR
# include <os2.h>

/* A kludge to get around what appears to be a header file bug */
# ifndef WORD
#   define WORD unsigned short
# endif
# ifndef DWORD
#   define DWORD unsigned long
# endif

# define EXE386 1
# include <newexe.h>
# include <exe386.h>

void GC_disable_signals(void)
{
    ULONG nest;
    
    DosEnterMustComplete(&nest);
    if (nest != 1) ABORT("nested GC_disable_signals");
}

void GC_enable_signals(void)
{
    ULONG nest;
    
    DosExitMustComplete(&nest);
    if (nest != 0) ABORT("GC_enable_signals");
}


# else

static int old_mask;

void GC_disable_signals()
{
    int mask = 0x7fffffff;
    	/* Setting the leading bit appears to provoke a bug in some	*/
    	/* longjmp implementations.  Most systems appear not to have    */
    	/* a signal 32.							*/

    mask &= ~(1<<(SIGSEGV-1));
    mask &= ~(1<<(SIGILL-1));
    mask &= ~(1<<(SIGQUIT-1));
#   ifdef SIGBUS
        mask &= ~(1<<(SIGBUS-1));
#   endif
#   ifdef SIGIOT
        mask &= ~(1<<(SIGIOT-1));
#   endif
#   ifdef SIGEMT
        mask &= ~(1<<(SIGEMT-1));
#   endif
#   ifdef SIGTRAP
        mask &= ~(1<<(SIGTRAP-1));
#   endif      
    old_mask = sigsetmask(mask);
}

void GC_enable_signals()
{
    (void)sigsetmask(old_mask);
}


# endif

/*
 * Find the base of the stack.
 * Used only in single-threaded environment.
 * With threads, GC_mark_roots needs to know how to do this.
 * Called with allocator lock held.
 */

# ifdef OS2

ptr_t GC_get_stack_base()
{
    PTIB ptib;
    PPIB ppib;
    
    if (DosGetInfoBlocks(&ptib, &ppib) != NO_ERROR) {
    	fprintf(stderr, "DosGetInfoBlocks failed\n");
    	ABORT("DosGetInfoBlocks failed\n");
    }
    return((ptr_t)(ptib -> tib_pstacklimit));
}

# else

# if !defined(THREADS) && !defined(STACKBOTTOM) && defined(HEURISTIC2)
  /* Some tools to implement HEURISTIC2	*/
#   define MIN_PAGE_SIZE 256	/* Smallest conceivable page size, bytes */
#   include <setjmp.h>
    /* static */ jmp_buf GC_jmp_buf;
    
    /*ARGSUSED*/
    void GC_fault_handler(sig)
    int sig;
    {
        longjmp(GC_jmp_buf, 1);
    }
# endif

ptr_t GC_get_stack_base()
{
    word dummy;
    static ptr_t result;
    		/* Needs to be static, since otherwise it may not be	*/
    		/* preserved across the longjmp.  Can safely be 	*/
    		/* static since it's only called once, with the		*/
    		/* allocation lock held.				*/
#   ifdef __STDC__
	typedef void (*handler)(int);
#   else
	typedef void (*handler)();
#   endif
#   ifdef HEURISTIC2
      static handler old_segv_handler, old_bus_handler;
      		/* See above for static declaration.			*/
#   endif
#   define STACKBOTTOM_ALIGNMENT_M1 0xffffff

#   ifdef STACKBOTTOM
	return(STACKBOTTOM);
#   else
#	ifdef HEURISTIC1
#	   ifdef STACK_GROWS_DOWN
	     result = (ptr_t)((((word)(&dummy))
	     		       + STACKBOTTOM_ALIGNMENT_M1)
			      & ~STACKBOTTOM_ALIGNMENT_M1);
#	   else
	     result = (ptr_t)(((word)(&dummy))
			      & ~STACKBOTTOM_ALIGNMENT_M1);
#	   endif
#	endif /* HEURISTIC1 */
#	ifdef HEURISTIC2
	   old_segv_handler = signal(SIGSEGV, GC_fault_handler);
#	   ifdef SIGBUS
	     old_bus_handler = signal(SIGBUS, GC_fault_handler);
#	   endif
	   if (setjmp(GC_jmp_buf) == 0) {
	     result = (ptr_t)(((word)(&dummy))
			      & ~(MIN_PAGE_SIZE-1));
	     for (;;) {
#	         ifdef STACK_GROWS_DOWN
		   result += MIN_PAGE_SIZE;
#	         else
		   result -= MIN_PAGE_SIZE;
#	         endif
		 GC_noop(*result);
	     }
	   }
	   (void) signal(SIGSEGV, old_segv_handler);
#	   ifdef SIGBUS
	       (void) signal(SIGBUS, old_bus_handler);
#	   endif
#	   ifdef STACK_GROWS_UP
	      result += MIN_PAGE_SIZE;
#	   endif
#	endif /* HEURISTIC2 */
#   endif /* STACKBOTTOM */
    return(result);
}

# endif /* ! OS2 */

/*
 * Register static data segment(s) as roots.
 * If more data segments are added later then they need to be registered
 * add that point (as we do with SunOS dynamic loading),
 * or GC_mark_roots needs to check for them (as we do with PCR).
 * Called with allocator lock held.
 */

# ifdef OS2

void GC_register_data_segments()
{
    PTIB ptib;
    PPIB ppib;
    HMODULE module_handle;
#   define PBUFSIZ 512
    UCHAR path[PBUFSIZ];
    FILE * myexefile;
    struct exe_hdr hdrdos;	/* MSDOS header.	*/
    struct e32_exe hdr386;	/* Real header for my executable */
    struct o32_obj seg;	/* Currrent segment */
    int nsegs;
    
    
    if (DosGetInfoBlocks(&ptib, &ppib) != NO_ERROR) {
    	fprintf(stderr, "DosGetInfoBlocks failed\n");
    	ABORT("DosGetInfoBlocks failed\n");
    }
    module_handle = ppib -> pib_hmte;
    if (DosQueryModuleName(module_handle, PBUFSIZ, path) != NO_ERROR) {
    	fprintf(stderr, "DosQueryModuleName failed\n");
    	ABORT("DosGetInfoBlocks failed\n");
    }
    myexefile = fopen(path, "rb");
    if (myexefile == 0) {
        GC_err_puts("Couldn't open executable ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Failed to open executable\n");
    }
    if (fread((char *)(&hdrdos), 1, sizeof hdrdos, myexefile) < sizeof hdrdos) {
        GC_err_puts("Couldn't read MSDOS header from ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Couldn't read MSDOS header");
    }
    if (E_MAGIC(hdrdos) != EMAGIC) {
        GC_err_puts("Executable has wrong DOS magic number: ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Bad DOS magic number");
    }
    if (fseek(myexefile, E_LFANEW(hdrdos), SEEK_SET) != 0) {
        GC_err_puts("Seek to new header failed in ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Bad DOS magic number");
    }
    if (fread((char *)(&hdr386), 1, sizeof hdr386, myexefile) < sizeof hdr386) {
        GC_err_puts("Couldn't read MSDOS header from ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Couldn't read OS/2 header");
    }
    if (E32_MAGIC1(hdr386) != E32MAGIC1 || E32_MAGIC2(hdr386) != E32MAGIC2) {
        GC_err_puts("Executable has wrong OS/2 magic number:");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Bad OS/2 magic number");
    }
    if ( E32_BORDER(hdr386) != E32LEBO || E32_WORDER(hdr386) != E32LEWO) {
        GC_err_puts("Executable %s has wrong byte order: ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Bad byte order");
    }
    if ( E32_CPU(hdr386) == E32CPU286) {
        GC_err_puts("GC can't handle 80286 executables: ");
        GC_err_puts(path); GC_err_puts("\n");
        EXIT();
    }
    if (fseek(myexefile, E_LFANEW(hdrdos) + E32_OBJTAB(hdr386),
    	      SEEK_SET) != 0) {
        GC_err_puts("Seek to object table failed: ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Seek to object table failed");
    }
    for (nsegs = E32_OBJCNT(hdr386); nsegs > 0; nsegs--) {
      int flags;
      if (fread((char *)(&seg), 1, sizeof seg, myexefile) < sizeof seg) {
        GC_err_puts("Couldn't read obj table entry from ");
        GC_err_puts(path); GC_err_puts("\n");
        ABORT("Couldn't read obj table entry");
      }
      flags = O32_FLAGS(seg);
      if (!(flags & OBJWRITE)) continue;
      if (!(flags & OBJREAD)) continue;
      if (flags & OBJINVALID) {
          fprintf(stderr, "Object with invalid pages?\n");
          continue;
      } 
      GC_add_roots_inner(O32_BASE(seg), O32_BASE(seg)+O32_SIZE(seg));
    }
}

# else

void GC_register_data_segments()
{
    extern int end;
        
    GC_add_roots_inner(DATASTART, (char *)(&end));
#   ifdef DYNAMIC_LOADING
	{
	   extern void GC_setup_dynamic_loading();
	       
	   GC_setup_dynamic_loading();
	}
#   endif
}

# endif  /* ! OS2 */
