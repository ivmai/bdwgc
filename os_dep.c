/*
 * Copyright (c) 1991-1993 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
# include "gc_private.h"
# include <stdio.h>
# include <signal.h>

/* Blatantly OS dependent routines, except for those that are related 	*/
/* dynamic loading.							*/

#ifdef AMIGA
# include <proto/exec.h>
# include <proto/dos.h>
# include <dos/dosextens.h>
# include <workbench/startup.h>
#endif

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

/* Disable and enable signals during nontrivial allocations	*/

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

#  if !defined(PCR) && !defined(AMIGA)

#   ifdef sigmask
	/* Use the traditional BSD interface */
#	define SIGSET_T int
#	define SIG_DEL(set, signal) (set) &= ~(sigmask(signal))
#	define SIG_FILL(set)  (set) = 0x7fffffff
    	  /* Setting the leading bit appears to provoke a bug in some	*/
    	  /* longjmp implementations.  Most systems appear not to have	*/
    	  /* a signal 32.						*/
#	define SIGSETMASK(old, new) (old) = sigsetmask(new)
#   else
	/* Use POSIX/SYSV interface	*/
#	define SIGSET_T sigset_t
#	define SIG_DEL(set, signal) sigdelset(&(set), (signal))
#	define SIG_FILL(set) sigfillset(&set)
#	define SIGSETMASK(old, new) sigprocmask(SIG_SETMASK, &(new), &(old))
#   endif

static bool mask_initialized = FALSE;

static SIGSET_T new_mask;

static SIGSET_T old_mask;

static SIGSET_T dummy;

void GC_disable_signals()
{
    if (!mask_initialized) {
    	SIG_FILL(new_mask);

	SIG_DEL(new_mask, SIGSEGV);
	SIG_DEL(new_mask, SIGILL);
	SIG_DEL(new_mask, SIGQUIT);
#	ifdef SIGBUS
	    SIG_DEL(new_mask, SIGBUS);
#	endif
#	ifdef SIGIOT
	    SIG_DEL(new_mask, SIGIOT);
#	endif
#	ifdef SIGEMT
	    SIG_DEL(new_mask, SIGEMT);
#	endif
#	ifdef SIGTRAP
	    SIG_DEL(new_mask, SIGTRAP);
#	endif 
	mask_initialized = TRUE;
    }     
    SIGSETMASK(old_mask,new_mask);
}

void GC_enable_signals()
{
    SIGSETMASK(dummy,old_mask);
}

#  endif  /* !PCR */

# endif /*!OS/2 */

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
    	GC_err_printf0("DosGetInfoBlocks failed\n");
    	ABORT("DosGetInfoBlocks failed\n");
    }
    return((ptr_t)(ptib -> tib_pstacklimit));
}

# else

# ifdef AMIGA

ptr_t GC_get_stack_base()
{
    extern struct WBStartup *_WBenchMsg;
    extern long __base;
    extern long __stack;
    struct Task *task;
    struct Process *proc;
    struct CommandLineInterface *cli;
    long size;

    if ((task = FindTask(0)) == 0) {
	GC_err_puts("Cannot find own task structure\n");
	ABORT("task missing");
    }
    proc = (struct Process *)task;
    cli = BADDR(proc->pr_CLI);

    if (_WBenchMsg != 0 || cli == 0) {
	size = (char *)task->tc_SPUpper - (char *)task->tc_SPLower;
    } else {
	size = cli->cli_DefaultStack * 4;
    }
    return (ptr_t)(__base + GC_max(size, __stack));
}

# else

# if !defined(THREADS) && !defined(STACKBOTTOM) && defined(HEURISTIC2)
#   define NEED_FIND_LIMIT
# endif

# if defined(SUNOS4) & defined(DYNAMIC_LOADING)
#   define NEED_FIND_LIMIT
# endif

# ifdef NEED_FIND_LIMIT
  /* Some tools to implement HEURISTIC2	*/
#   define MIN_PAGE_SIZE 256	/* Smallest conceivable page size, bytes */
#   include <setjmp.h>
    /* static */ VOLATILE jmp_buf GC_jmp_buf;
    
    /*ARGSUSED*/
    void GC_fault_handler(sig)
    int sig;
    {
        longjmp(GC_jmp_buf, 1);
    }

#   ifdef __STDC__
	typedef void (*handler)(int);
#   else
	typedef void (*handler)();
#   endif

    /* Return the first nonaddressible location > p (up) or 	*/
    /* the smallest location q s.t. [q,p] is addressible (!up).	*/
    ptr_t GC_find_limit(p, up)
    ptr_t p;
    bool up;
    {
        static VOLATILE ptr_t result;
    		/* Needs to be static, since otherwise it may not be	*/
    		/* preserved across the longjmp.  Can safely be 	*/
    		/* static since it's only called once, with the		*/
    		/* allocation lock held.				*/

        static handler old_segv_handler, old_bus_handler;
      		/* See above for static declaration.			*/

    	old_segv_handler = signal(SIGSEGV, GC_fault_handler);
#	ifdef SIGBUS
	   old_bus_handler = signal(SIGBUS, GC_fault_handler);
#	endif
	if (setjmp(GC_jmp_buf) == 0) {
	    result = (ptr_t)(((word)(p))
			      & ~(MIN_PAGE_SIZE-1));
	    for (;;) {
 	        if (up) {
		    result += MIN_PAGE_SIZE;
 	        } else {
		    result -= MIN_PAGE_SIZE;
 	        }
		GC_noop(*result);
	    }
	}
	(void) signal(SIGSEGV, old_segv_handler);
#	ifdef SIGBUS
	    (void) signal(SIGBUS, old_bus_handler);
#	endif
 	if (!up) {
	    result += MIN_PAGE_SIZE;
 	}
	return(result);
    }
# endif


ptr_t GC_get_stack_base()
{
    word dummy;
    ptr_t result;

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
#	    ifdef STACK_GROWS_DOWN
		result = GC_find_limit((ptr_t)(&dummy), TRUE);
#	    else
		result = GC_find_limit((ptr_t)(&dummy), FALSE);
#	    endif
#	endif /* HEURISTIC2 */
    	return(result);
#   endif /* STACKBOTTOM */
}

# endif /* ! AMIGA */
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
    	GC_err_printf0("DosGetInfoBlocks failed\n");
    	ABORT("DosGetInfoBlocks failed\n");
    }
    module_handle = ppib -> pib_hmte;
    if (DosQueryModuleName(module_handle, PBUFSIZ, path) != NO_ERROR) {
    	GC_err_printf0("DosQueryModuleName failed\n");
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
          GC_err_printf0("Object with invalid pages?\n");
          continue;
      } 
      GC_add_roots_inner(O32_BASE(seg), O32_BASE(seg)+O32_SIZE(seg));
    }
}

# else
# ifdef AMIGA

  void GC_register_data_segments()
  {
    extern struct WBStartup *_WBenchMsg;
    struct Process	*proc;
    struct CommandLineInterface *cli;
    BPTR myseglist;
    ULONG *data;

    if ( _WBenchMsg != 0 ) {
	if ((myseglist = _WBenchMsg->sm_Segment) == 0) {
	    GC_err_puts("No seglist from workbench\n");
	    return;
	}
    } else {
	if ((proc = (struct Process *)FindTask(0)) == 0) {
	    GC_err_puts("Cannot find process structure\n");
	    return;
	}
	if ((cli = BADDR(proc->pr_CLI)) == 0) {
	    GC_err_puts("No CLI\n");
	    return;
	}
	if ((myseglist = cli->cli_Module) == 0) {
	    GC_err_puts("No seglist from CLI\n");
	    return;
	}
    }

    for (data = (ULONG *)BADDR(myseglist); data != 0;
         data = (ULONG *)BADDR(data[0])) {
	GC_add_roots_inner((char *)&data[1], ((char *)&data[1]) + data[-1]);
    }
  }


# else

void GC_register_data_segments()
{
#   ifndef NEXT
        extern int end;
#   endif
 
#   if !defined(PCR) && !defined(SRC_M3) && !defined(NEXT) 
      GC_add_roots_inner(DATASTART, (char *)(&end));
#   endif
#   if !defined(PCR) && defined(NEXT)
      GC_add_roots_inner(DATASTART, (char *) get_end());
#   endif
    /* Dynamic libraries are added at every collection, since they may  */
    /* change.								*/
}

# endif  /* ! AMIGA */
# endif  /* ! OS2 */

# if !defined(OS2) && !defined(PCR) && !defined(AMIGA)

extern caddr_t sbrk();
# ifdef __STDC__
#   define SBRK_ARG_T size_t
# else
#   define SBRK_ARG_T int
# endif

ptr_t GC_unix_get_mem(bytes)
word bytes;
{
    caddr_t cur_brk = sbrk(0);
    caddr_t result;
    SBRK_ARG_T lsbs = (word)cur_brk & (HBLKSIZE-1);
    
    if (lsbs != 0) {
        if(sbrk(HBLKSIZE - lsbs) == (caddr_t)(-1)) return(0);
    }
    result = sbrk((SBRK_ARG_T)bytes);
    if (result == (caddr_t)(-1)) return(0);
    return((ptr_t)result);
}

# endif

/*
 * Routines for accessing dirty  bits on virtual pages.
 * We plan to eventaually implement four strategies for doing so:
 * DEFAULT_VDB:	A simple dummy implementation that treats every page
 *		as possibly dirty.  This makes incremental collection
 *		useless, but the implementation is still correct.
 * PCR_VDB:	Use PPCRs virtual dirty bit facility.
 * PROC_VDB:	Use the /proc facility for reading dirty bits.  Only
 *		works under some SVR4 variants.  Even then, it may be
 *		too slow to be entirely satisfactory.  Requires reading
 *		dirty bits for entire address space.  Implementations tend
 *		to assume that the client is a (slow) debugger.
 * MPROTECT_VDB:Protect pages and then catch the faults to keep track of
 *		dirtied pages.  The implementation (and implementability)
 *		is highly system dependent.  This usually fails when system
 *		calls write to a protected page.  We prevent the read system
 *		call from doing so.  It is the clients responsibility to
 *		make sure that other system calls are similarly protected
 *		or write only to the stack.
 */
 
# ifdef DEFAULT_VDB

/* All of the following assume the allocation lock is held, and	*/
/* signals are disabled.					*/

/* The client asserts that unallocated pages in the heap are never	*/
/* written.								*/

/* Initialize virtual dirty bit implementation.			*/
void GC_dirty_init()
{
}

/* Retrieve system dirty bits for heap to a local buffer.	*/
/* Restore the systems notion of which pages are dirty.		*/
void GC_read_dirty()
{}

/* Is the HBLKSIZE sized page at h marked dirty in the local buffer?	*/
/* If the actual page size is different, this returns TRUE if any	*/
/* of the pages overlapping h are dirty.  This routine may err on the	*/
/* side of labelling pages as dirty (and this implementation does).	*/
/*ARGSUSED*/
bool GC_page_was_dirty(h)
struct hblk *h;
{
    return(TRUE);
}

/* A call hints that h is about to be written	*/
/*ARGSUSED*/
void GC_write_hint(h)
struct hblk *h;
{
}

# endif /* DEFAULT_VDB */


# ifdef MPROTECT_VDB

/*
 * See DEFAULT_VDB for interface descriptions.
 */

/*
 * This implementation maintains dirty bits itself by catching write
 * faults and keeping track of them.  We assume nobody else catches
 * SIGBUS or SIGSEGV.  We assume no write faults occur in system calls
 * except as a result of a read system call.  This means clients must
 * either ensure that system calls do not touch the heap, or must
 * provide their own wrappers analogous to the one for read.
 * This implementation is currently SunOS 4.X and IRIX 5.X specific, though we
 * tried to use portable code where easily possible.  It is known
 * not to work under a number of other systems.
 */

# include <sys/mman.h>
# include <signal.h>
# include <sys/syscall.h>

VOLATILE page_hash_table GC_dirty_pages;
				/* Pages dirtied since last GC_read_dirty. */

word GC_page_size;

/*ARGSUSED*/
# ifdef SUNOS4
    void GC_write_fault_handler(sig, code, scp, addr)
    int sig, code;
    struct sigcontext *scp;
    char * addr;
#   define SIG_OK (sig == SIGSEGV || sig == SIGBUS)
#   define CODE_OK (FC_CODE(code) == FC_PROT \
            	    || (FC_CODE(code) == FC_OBJERR \
                	&& FC_ERRNO(code) == FC_PROT))

# else
#   if defined(IRIX5) || defined(ALPHA) /* OSF1 */
#     include <errno.h>
      void GC_write_fault_handler(int sig, int code, struct sigcontext *scp)
#     define SIG_OK (sig == SIGSEGV)
#     ifdef ALPHA
#	define SIG_PF void (*)(int)
#       define CODE_OK (code == 2 /* experimentally determined */)
#     else
#       define CODE_OK (code == EACCES)
#     endif
#   endif
# endif
{
    register int i;
#   ifdef IRIX5
	char * addr = (char *) (scp -> sc_badvaddr);
#   endif
#   ifdef ALPHA
	char * addr = (char *) (scp -> sc_traparg_a0);
#   endif
    
    if (SIG_OK && CODE_OK) {
        register struct hblk * h =
        		(struct hblk *)((word)addr & ~(GC_page_size-1));
        
        for (i = 0; i < GC_page_size/HBLKSIZE; i++) {
            register int index = PHT_HASH(h+i);
            
            if (HDR(h+i) == 0) {
                ABORT("Unexpected bus error or segmentation fault");
            }
            set_pht_entry_from_index(GC_dirty_pages, index);
        }
        if (mprotect((caddr_t)h, (int)GC_page_size,
            PROT_WRITE | PROT_READ | PROT_EXEC) < 0) {
    	    ABORT("mprotect failed in handler");
    	}
#	if defined(IRIX5) || defined(ALPHA)
	    /* IRIX resets the signal handler each time. */
	    signal(SIGSEGV, (SIG_PF) GC_write_fault_handler);
#	endif
    	/* The write may not take place before dirty bits are read.	*/
    	/* But then we'll fault again ...				*/
    	return;
    }

    ABORT("Unexpected bus error or segmentation fault");
}

void GC_write_hint(h)
struct hblk *h;
{
    register struct hblk * h_trunc =
        		(struct hblk *)((word)h & ~(GC_page_size-1));
    register int i;
    register bool found_clean = FALSE;
    
    for (i = 0; i < divHBLKSZ(GC_page_size); i++) {
        register int index = PHT_HASH(h_trunc+i);
            
        if (!get_pht_entry_from_index(GC_dirty_pages, index)) {
            found_clean = TRUE;
            set_pht_entry_from_index(GC_dirty_pages, index);
        }
    }
    if (found_clean) {
   	if (mprotect((caddr_t)h_trunc, (int)GC_page_size,
            PROT_WRITE | PROT_READ | PROT_EXEC) < 0) {
    	    ABORT("mprotect failed in GC_write_hint");
    	}
    }
}
				 
void GC_dirty_init()
{
    GC_page_size = getpagesize();
    if (GC_page_size % HBLKSIZE != 0) {
        GC_err_printf0("Page size not multiple of HBLKSIZE\n");
        ABORT("Page size not multiple of HBLKSIZE");
    }
#   ifdef SUNOS4
      if (signal(SIGBUS, GC_write_fault_handler) != SIG_DFL) {
        GC_err_printf0("Clobbered other SIGBUS handler\n");
      }
      if (signal(SIGSEGV, GC_write_fault_handler) != SIG_DFL) {
        GC_err_printf0("Clobbered other SIGSEGV handler\n");
      }
#   endif
#   if defined(IRIX5) || defined(ALPHA)
      if (signal(SIGSEGV, (SIG_PF)GC_write_fault_handler) != SIG_DFL) {
        GC_err_printf0("Clobbered other SIGSEGV handler\n");
      }
#   endif
}



void GC_protect_heap()
{
    word ps = GC_page_size;
    word pmask = (ps-1);
    ptr_t start;
    word offset;
    word len;
    int i;
    
    for (i = 0; i < GC_n_heap_sects; i++) {
        offset = (word)(GC_heap_sects[i].hs_start) & pmask;
        start = GC_heap_sects[i].hs_start - offset;
        len = GC_heap_sects[i].hs_bytes + offset;
        len += ps-1; len &= ~pmask;
    	if (mprotect((caddr_t)start, (int)len, PROT_READ | PROT_EXEC) < 0) {
    	    ABORT("mprotect failed");
    	}
    }
}

# ifdef THREADS
--> The following is broken.  We can lose dirty bits.  We would need
--> the signal handler to cooperate, as in PCR.
# endif

void GC_read_dirty()
{
    bcopy((char *)GC_dirty_pages, (char *)GC_grungy_pages,
          (int)(sizeof GC_dirty_pages));
    bzero((char *)GC_dirty_pages, (int)(sizeof GC_dirty_pages));
    GC_protect_heap();
}

bool GC_page_was_dirty(h)
struct hblk * h;
{
    register word index = PHT_HASH(h);
    
    return(HDR(h) == 0 || get_pht_entry_from_index(GC_grungy_pages, index));
}

void GC_begin_syscall()
{
    DISABLE_SIGNALS();
    LOCK();
}

void GC_end_syscall()
{
    UNLOCK();
    ENABLE_SIGNALS();
}

void GC_unprotect_range(addr, len)
ptr_t addr;
word len;
{
    struct hblk * start_block;
    struct hblk * end_block;
    register struct hblk *h;
    ptr_t obj_start;
    
    if (!GC_incremental) return;
    obj_start = GC_base(addr);
    if (obj_start == 0) return;
    if (GC_base(addr + len - 1) != obj_start) {
        ABORT("GC_unprotect_range(range bigger than object)");
    }
    start_block = (struct hblk *)((word)addr & ~(GC_page_size - 1));
    end_block = (struct hblk *)((word)(addr + len - 1) & ~(GC_page_size - 1));
    end_block += GC_page_size/HBLKSIZE - 1;
    for (h = start_block; h <= end_block; h++) {
        register word index = PHT_HASH(h);
        
        set_pht_entry_from_index(GC_dirty_pages, index);
    }
    if (mprotect((caddr_t)start_block,
    	         (int)((ptr_t)end_block - (ptr_t)start_block)
    	         + HBLKSIZE,
    	         PROT_WRITE | PROT_READ | PROT_EXEC) < 0) {
    	ABORT("mprotect failed in GC_unprotect_range");
    }
}

/* Replacement for UNIX system call.	 */
/* Other calls that write to the heap	 */
/* should be handled similarly.		 */
# ifndef LINT
  int read(fd, buf, nbyte)
# else
  int GC_read(fd, buf, nbyte)
# endif
int fd;
char *buf;
int nbyte;
{
    int result;
    
    GC_begin_syscall();
    GC_unprotect_range(buf, (word)nbyte);
    result = syscall(SYS_read, fd, buf, nbyte);
    GC_end_syscall();
    return(result);
}

# endif /* MPROTECT_VDB */

# ifdef PROC_VDB

/*
 * See DEFAULT_VDB for interface descriptions.
 */
 
/*
 * This implementaion assumes a Solaris 2.X like /proc pseudo-file-system
 * from which we can read page modified bits.  This facility is far from
 * optimal (e.g. we would like to get the info for only some of the
 * address space), but it avoids intercepting system calls.
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFSZ 20000
char *GC_proc_buf;

int GC_proc_fd;

void GC_dirty_init()
{
    int fd;
    char buf[20];

    sprintf(buf, "/proc/%d", getpid());
    fd = open(buf, O_RDONLY);
    if (fd < 0) {
    	ABORT("/proc open failed");
    }
    GC_proc_fd = ioctl(fd, PIOCOPENPD, 0);
    if (GC_proc_fd < 0) {
    	ABORT("/proc ioctl failed");
    }
    GC_proc_buf = GC_scratch_alloc(BUFSZ);
}

/* Ignore write hints. They don't help us here.	*/
/*ARGSUSED*/
void GC_write_hint(h)
struct hblk *h;
{
}

void GC_read_dirty()
{
    unsigned long ps, np;
    int nmaps;
    ptr_t vaddr;
    struct prasmap * map;
    char * bufp;
    ptr_t current_addr, limit;
    int i;

    bzero((char *)GC_grungy_pages, (int)(sizeof GC_grungy_pages));
    
    bufp = GC_proc_buf;
    if (read(GC_proc_fd, bufp, BUFSZ) <= 0) {
        ABORT("/proc read failed: BUFSZ too small?\n");
    }
    /* Copy dirty bits into GC_grungy_pages */
    	nmaps = ((struct prpageheader *)bufp) -> pr_nmap;
	/* printf( "nmaps = %d, PG_REFERENCED = %d, PG_MODIFIED = %d\n",
		     nmaps, PG_REFERENCED, PG_MODIFIED); */
	bufp = bufp + sizeof(struct prpageheader);
	for (i = 0; i < nmaps; i++) {
	    map = (struct prasmap *)bufp;
	    vaddr = (ptr_t)(map -> pr_vaddr);
	    ps = map -> pr_pagesize;
	    np = map -> pr_npage;
	    /* printf("vaddr = 0x%X, ps = 0x%X, np = 0x%X\n", vaddr, ps, np); */
	    limit = vaddr + ps * np;
	    bufp += sizeof (struct prasmap);
	    for (current_addr = vaddr;
	         current_addr < limit; current_addr += ps){
	        if ((*bufp++) & PG_MODIFIED) {
	            register struct hblk * h = (struct hblk *) current_addr;
	            
	            while ((ptr_t)h < current_addr + ps) {
	                register word index = PHT_HASH(h);
	                
	                set_pht_entry_from_index(GC_grungy_pages, index);
	                h++;
	            }
	        }
	    }
	    bufp += sizeof(long) - 1;
	    bufp = (char *)((unsigned long)bufp & ~(sizeof(long)-1));
	}
}

bool GC_page_was_dirty(h)
struct hblk *h;
{
    register word index = PHT_HASH(h);
    
    return(get_pht_entry_from_index(GC_grungy_pages, index));
}

# endif /* PROC_VDB */


# ifdef PCR_VDB

# include "vd/PCR_VD.h"

# define NPAGES (32*1024)	/* 128 MB */

PCR_VD_DB  GC_grungy_bits[NPAGES];

ptr_t GC_vd_base;	/* Address corresponding to GC_grungy_bits[0]	*/
			/* HBLKSIZE aligned.				*/

void GC_dirty_init()
{
    /* For the time being, we assume the heap generally grows up */
    GC_vd_base = GC_heap_sects[0].hs_start;
    if (GC_vd_base == 0) {
   	ABORT("Bad initial heap segment");
    }
    if (PCR_VD_Start(HBLKSIZE, GC_vd_base, NPAGES*HBLKSIZE)
	!= PCR_ERes_okay) {
	ABORT("dirty bit initialization failed");
    }
}

void GC_read_dirty()
{
    /* lazily enable dirty bits on newly added heap sects */
    {
        static int onhs = 0;
        int nhs = GC_n_heap_sects;
        for( ; onhs < nhs; onhs++ ) {
            PCR_VD_WriteProtectEnable(
                    GC_heap_sects[onhs].hs_start,
                    GC_heap_sects[onhs].hs_bytes );
        }
    }


    if (PCR_VD_Clear(GC_vd_base, NPAGES*HBLKSIZE, GC_grungy_bits)
        != PCR_ERes_okay) {
	ABORT("dirty bit read failed");
    }
}

bool GC_page_was_dirty(h)
struct hblk *h;
{
    if((ptr_t)h < GC_vd_base || (ptr_t)h >= GC_vd_base + NPAGES*HBLKSIZE) {
	return(TRUE);
    }
    return(GC_grungy_bits[h - (struct hblk *)GC_vd_base] & PCR_VD_DB_dirtyBit);
}

/*ARGSUSED*/
void GC_write_hint(h)
struct hblk *h;
{
    PCR_VD_WriteProtectDisable(h, HBLKSIZE);
    PCR_VD_WriteProtectEnable(h, HBLKSIZE);
}

# endif /* PCR_VDB */




