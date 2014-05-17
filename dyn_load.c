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
 * Original author: Bill Janssen
 * Heavily modified by Hans Boehm and others
 */
/* Boehm, May 19, 1994 1:57 pm PDT */

/*
 * This is incredibly OS specific code for tracking down data sections in
 * dynamic libraries.  There appears to be no way of doing this quickly
 * without groveling through undocumented data structures.  We would argue
 * that this is a bug in the design of the dlopen interface.  THIS CODE
 * MAY BREAK IN FUTURE OS RELEASES.  If this matters to you, don't hesitate
 * to let your vendor know ...
 *
 * None of this is safe with dlclose and incremental collection.
 * But then not much of anything is safe in the presence of dlclose.
 */
#include <sys/types.h>
#include "gc_priv.h"

#if (defined(DYNAMIC_LOADING) || defined(MSWIN32)) && !defined(PCR)
#if !defined(SUNOS4) && !defined(SUNOS5) && !defined(IRIX5) && !defined(MSWIN32)
 --> We only know how to find data segments of dynamic libraries under SunOS,
 --> IRIX5 and Win32.  Additional SVR4 variants might not be too hard to add.
#endif

#include <stdio.h>
#ifdef SUNOS5
#   include <sys/elf.h>
#   include <dlfcn.h>
#   include <link.h>
#endif
#ifdef SUNOS4
#   include <dlfcn.h>
#   include <link.h>
#   include <a.out.h>
  /* struct link_map field overrides */
#   define l_next	lm_next
#   define l_addr	lm_addr
#   define l_name	lm_name
#endif


#ifdef SUNOS5

#ifdef LINT
    Elf32_Dyn _DYNAMIC;
#endif

static struct link_map *
GC_FirstDLOpenedLinkMap()
{
    extern Elf32_Dyn _DYNAMIC;
    Elf32_Dyn *dp;
    struct r_debug *r;
    static struct link_map * cachedResult = 0;

    if( &_DYNAMIC == 0) {
        return(0);
    }
    if( cachedResult == 0 ) {
        int tag;
        for( dp = ((Elf32_Dyn *)(&_DYNAMIC)); (tag = dp->d_tag) != 0; dp++ ) {
            if( tag == DT_DEBUG ) {
                struct link_map *lm
                        = ((struct r_debug *)(dp->d_un.d_ptr))->r_map;
                if( lm != 0 ) cachedResult = lm->l_next; /* might be NIL */
                break;
            }
        }
    }
    return cachedResult;
}

#endif

#ifdef SUNOS4

#ifdef LINT
    struct link_dynamic _DYNAMIC;
#endif

static struct link_map *
GC_FirstDLOpenedLinkMap()
{
    extern struct link_dynamic _DYNAMIC;

    if( &_DYNAMIC == 0) {
        return(0);
    }
    return(_DYNAMIC.ld_un.ld_1->ld_loaded);
}

/* Return the address of the ld.so allocated common symbol	*/
/* with the least address, or 0 if none.			*/
static ptr_t GC_first_common()
{
    ptr_t result = 0;
    extern struct link_dynamic _DYNAMIC;
    struct rtc_symb * curr_symbol;
    
    if( &_DYNAMIC == 0) {
        return(0);
    }
    curr_symbol = _DYNAMIC.ldd -> ldd_cp;
    for (; curr_symbol != 0; curr_symbol = curr_symbol -> rtc_next) {
        if (result == 0
            || (ptr_t)(curr_symbol -> rtc_sp -> n_value) < result) {
            result = (ptr_t)(curr_symbol -> rtc_sp -> n_value);
        }
    }
    return(result);
}

#endif

# if defined(SUNOS4) || defined(SUNOS5)
/* Add dynamic library data sections to the root set.		*/
# if !defined(PCR) && !defined(SOLARIS_THREADS) && defined(THREADS)
#   ifndef SRC_M3
	--> fix mutual exclusion with dlopen
#   endif  /* We assume M3 programs don't call dlopen for now */
# endif

# ifdef SOLARIS_THREADS
  /* Redefine dlopen to guarantee mutual exclusion with	*/
  /* GC_register_dynamic_libraries.			*/
  /* assumes that dlopen doesn't need to call GC_malloc	*/
  /* and friends.					*/
# include <thread.h>
# include <synch.h>
  
void * GC_dlopen(const char *path, int mode)
{
    void * result;
    
    mutex_lock(&GC_allocate_ml);
    result = dlopen(path, mode);
    mutex_unlock(&GC_allocate_ml);
    return(result);
}
# endif

void GC_register_dynamic_libraries()
{
  struct link_map *lm = GC_FirstDLOpenedLinkMap();
  

  for (lm = GC_FirstDLOpenedLinkMap();
       lm != (struct link_map *) 0;  lm = lm->l_next)
    {
#     ifdef SUNOS4
	struct exec *e;
	 
        e = (struct exec *) lm->lm_addr;
        GC_add_roots_inner(
      		    ((char *) (N_DATOFF(*e) + lm->lm_addr)),
		    ((char *) (N_BSSADDR(*e) + e->a_bss + lm->lm_addr)));
#     endif
#     ifdef SUNOS5
	Elf32_Ehdr * e;
        Elf32_Phdr * p;
        unsigned long offset;
        char * start;
        register int i;
        
	e = (Elf32_Ehdr *) lm->l_addr;
        p = ((Elf32_Phdr *)(((char *)(e)) + e->e_phoff));
        offset = ((unsigned long)(lm->l_addr));
        for( i = 0; i < (int)(e->e_phnum); ((i++),(p++)) ) {
          switch( p->p_type ) {
            case PT_LOAD:
              {
                if( !(p->p_flags & PF_W) ) break;
                start = ((char *)(p->p_vaddr)) + offset;
                GC_add_roots_inner(
                  start,
                  start + p->p_memsz
                );
              }
              break;
            default:
              break;
          }
	}
#     endif
    }
#   ifdef SUNOS4
      {
      	static ptr_t common_start = 0;
      	ptr_t common_end;
      	extern ptr_t GC_find_limit();
      	
      	if (common_start == 0) common_start = GC_first_common();
      	if (common_start != 0) {
      	    common_end = GC_find_limit(common_start, TRUE);
      	    GC_add_roots_inner((char *)common_start, (char *)common_end);
      	}
      }
#   endif
}

# endif /* SUNOS */

#ifdef IRIX5

#include <sys/procfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>

extern void * GC_roots_present();

extern ptr_t GC_scratch_end_ptr;   /* End of GC_scratch_alloc arena	*/

/* We use /proc to track down all parts of the address space that are	*/
/* mapped by the process, and throw out regions we know we shouldn't	*/
/* worry about.  This may also work under other SVR4 variants.		*/
void GC_register_dynamic_libraries()
{
    static int fd = -1;
    char buf[30];
    static prmap_t * addr_map = 0;
    static int current_sz = 0;	/* Number of records currently in addr_map */
    static int needed_sz;	/* Required size of addr_map		*/
    register int i;
    register long flags;
    register ptr_t start;
    register ptr_t limit;
    ptr_t heap_end = (ptr_t)DATASTART;

    if (fd < 0) {
      sprintf(buf, "/proc/%d", getpid());
      fd = open(buf, O_RDONLY);
      if (fd < 0) {
    	ABORT("/proc open failed");
      }
    }
    if (ioctl(fd, PIOCNMAP, &needed_sz) < 0) {
    	ABORT("/proc PIOCNMAP ioctl failed");
    }
    if (needed_sz >= current_sz) {
        current_sz = needed_sz * 2 + 1;
        		/* Expansion, plus room for 0 record */
        addr_map = (prmap_t *)GC_scratch_alloc(current_sz * sizeof(prmap_t));
    }
    if (ioctl(fd, PIOCMAP, addr_map) < 0) {
    	ABORT("/proc PIOCMAP ioctl failed");
    };
    if (GC_n_heap_sects > 0) {
    	heap_end = GC_heap_sects[GC_n_heap_sects-1].hs_start
    			+ GC_heap_sects[GC_n_heap_sects-1].hs_bytes;
    	if (heap_end < GC_scratch_end_ptr) heap_end = GC_scratch_end_ptr; 
    }
    for (i = 0; i < needed_sz; i++) {
        flags = addr_map[i].pr_mflags;
        if ((flags & (MA_BREAK | MA_STACK | MA_PHYS)) != 0) goto irrelevant;
        if ((flags & (MA_READ | MA_WRITE)) != (MA_READ | MA_WRITE))
            goto irrelevant;
          /* The latter test is empirically useless.  Other than the	*/
          /* main data and stack segments, everything appears to be	*/
          /* mapped readable, writable, executable, and shared(!!).	*/
          /* This makes no sense to me.	- HB				*/
        start = (ptr_t)(addr_map[i].pr_vaddr);
        if (GC_roots_present(start)) goto irrelevant;
        if (start < heap_end && start >= (ptr_t)DATASTART)
        	goto irrelevant;
        limit = start + addr_map[i].pr_size;
	if (addr_map[i].pr_off == 0 && strncmp(start, ELFMAG, 4) == 0) {
	    /* Discard text segments, i.e. 0-offset mappings against	*/
	    /* executable files which appear to have ELF headers.	*/
	    caddr_t arg;
	    int obj;
#	    define MAP_IRR_SZ 10
	    static ptr_t map_irr[MAP_IRR_SZ];
	    				/* Known irrelevant map entries	*/
	    static int n_irr = 0;
	    struct stat buf;
	    register int i;
	    
	    for (i = 0; i < n_irr; i++) {
	        if (map_irr[i] == start) goto irrelevant;
	    }
	    arg = (caddr_t)start;
	    obj = ioctl(fd, PIOCOPENM, &arg);
	    if (obj >= 0) {
	        fstat(obj, &buf);
	        close(obj);
	        if ((buf.st_mode & 0111) != 0) {
	            if (n_irr < MAP_IRR_SZ) {
	                map_irr[n_irr++] = start;
	            }
	            goto irrelevant;
	        }
	    }
	}
        GC_add_roots_inner(start, limit);
      irrelevant: ;
    }
}

#endif  /* IRIX5 */

# ifdef MSWIN32

# define WIN32_LEAN_AND_MEAN
# define NOSERVICE
# include <windows.h>
# include <stdlib.h>

  /* We traverse the entire address space and register all segments 	*/
  /* that could possibly have been written to.				*/
  DWORD GC_allocation_granularity;
  
  extern bool GC_is_heap_base (ptr_t p);
  
  void GC_cond_add_roots(char *base, char * limit)
  {
    char dummy;
    char * stack_top
           = (char *) ((word)(&dummy) & ~(GC_allocation_granularity-1));
    if (base == limit) return;
    if (limit > stack_top && base < GC_stackbottom) {
    	/* Part of the stack; ignore it. */
    	return;
    }
    GC_add_roots_inner(base, limit);
  }
  
  extern bool GC_win32s;
  
  void GC_register_dynamic_libraries()
  {
    MEMORY_BASIC_INFORMATION buf;
    SYSTEM_INFO sysinfo;
    DWORD result;
    DWORD protect;
    LPVOID p;
    char * base;
    char * limit, * new_limit;
    
    if (GC_win32s) return;
    GetSystemInfo(&sysinfo);
    base = limit = p = sysinfo.lpMinimumApplicationAddress;
    GC_allocation_granularity = sysinfo.dwAllocationGranularity;
    while (p < sysinfo.lpMaximumApplicationAddress) {
        result = VirtualQuery(p, &buf, sizeof(buf));
        if (result != sizeof(buf)) {
            ABORT("Weird VirtualQuery result");
        }
        new_limit = (char *)p + buf.RegionSize;
        protect = buf.Protect;
        if (buf.State == MEM_COMMIT
            && (protect == PAGE_EXECUTE_READWRITE
                || protect == PAGE_READWRITE)
            && !GC_is_heap_base(buf.AllocationBase)) {
            if ((char *)p == limit) {
                limit = new_limit;
            } else {
                GC_cond_add_roots(base, limit);
                base = p;
                limit = new_limit;
            }
        }
        if (p > (LPVOID)new_limit /* overflow */) break;
        p = (LPVOID)new_limit;
    }
    GC_cond_add_roots(base, limit);
  }

#endif /* MSWIN32 */

#if defined(ALPHA)
void GC_register_dynamic_libraries()
{
  int status;
  ldr_process_t mypid;

  /* module */
    ldr_module_t moduleid = LDR_NULL_MODULE;
    ldr_module_info_t moduleinfo;
    size_t moduleinfosize = sizeof(moduleinfo);
    size_t modulereturnsize;    

  /* region */
    ldr_region_t region; 
    ldr_region_info_t regioninfo;
    size_t regioninfosize = sizeof(regioninfo);
    size_t regionreturnsize;

  /* Obtain id of this process */
    mypid = ldr_my_process();
  
  /* For each module */
    while (TRUE) {

      /* Get the next (first) module */
        status = ldr_next_module(mypid, &moduleid);

      /* Any more modules? */
        if (moduleid == LDR_NULL_MODULE)
            break;    /* No more modules */

      /* Check status AFTER checking moduleid because */
      /* of a bug in the non-shared ldr_next_module stub */
        if (status != 0 ) {
            GC_printf("dynamic_load: status = %ld\n", (long)status);
            {
                extern char *sys_errlist[];
                extern int sys_nerr;
                extern int errno;
                if (errno <= sys_nerr) {
                    GC_printf("dynamic_load: %s\n", sys_errlist[errno]);
               } else {
                    GC_printf("dynamic_load: %d\n", errno);
                }
        }
            ABORT("ldr_next_module failed");
         }

      /* Get the module information */
        status = ldr_inq_module(mypid, moduleid, &moduleinfo,
                                moduleinfosize, &modulereturnsize); 
        if (status != 0 )
            ABORT("ldr_inq_module failed");

      /* is module for the main program (i.e. nonshared portion)? */
          if (moduleinfo.lmi_flags & LDR_MAIN)
              continue;    /* skip the main module */

#     ifdef VERBOSE
          GC_printf("---Module---\n");
          GC_printf("Module ID            = %16ld\n", moduleinfo.lmi_modid);
          GC_printf("Count of regions     = %16d\n", moduleinfo.lmi_nregion);
          GC_printf("flags for module     = %16lx\n", moduleinfo.lmi_flags); 
          GC_printf("pathname of module   = \"%s\"\n", moduleinfo.lmi_name);
#     endif

      /* For each region in this module */
        for (region = 0; region < moduleinfo.lmi_nregion; region++) {

          /* Get the region information */
            status = ldr_inq_region(mypid, moduleid, region, &regioninfo,
                                    regioninfosize, &regionreturnsize);
            if (status != 0 )
                ABORT("ldr_inq_region failed");

          /* only process writable (data) regions */
            if (! (regioninfo.lri_prot & LDR_W))
                continue;

#         ifdef VERBOSE
              GC_printf("--- Region ---\n");
              GC_printf("Region number    = %16ld\n",
              	        regioninfo.lri_region_no);
              GC_printf("Protection flags = %016x\n",  regioninfo.lri_prot);
              GC_printf("Virtual address  = %16p\n",   regioninfo.lri_vaddr);
              GC_printf("Mapped address   = %16p\n",   regioninfo.lri_mapaddr);
              GC_printf("Region size      = %16ld\n",  regioninfo.lri_size);
              GC_printf("Region name      = \"%s\"\n", regioninfo.lri_name);
#         endif

          /* register region as a garbage collection root */
            GC_add_roots_inner (
                (char *)regioninfo.lri_mapaddr,
                (char *)regioninfo.lri_mapaddr + regioninfo.lri_size);

        }
    }
}
#endif


#else /* !DYNAMIC_LOADING */

#ifdef PCR

#   include "il/PCR_IL.h"
#   include "th/PCR_ThCtl.h"
#   include "mm/PCR_MM.h"

void GC_register_dynamic_libraries()
{
    /* Add new static data areas of dynamically loaded modules.	*/
        {
          PCR_IL_LoadedFile * p = PCR_IL_GetLastLoadedFile();
          PCR_IL_LoadedSegment * q;
          
          /* Skip uncommited files */
          while (p != NIL && !(p -> lf_commitPoint)) {
              /* The loading of this file has not yet been committed	*/
              /* Hence its description could be inconsistent.  		*/
              /* Furthermore, it hasn't yet been run.  Hence its data	*/
              /* segments can't possibly reference heap allocated	*/
              /* objects.						*/
              p = p -> lf_prev;
          }
          for (; p != NIL; p = p -> lf_prev) {
            for (q = p -> lf_ls; q != NIL; q = q -> ls_next) {
              if ((q -> ls_flags & PCR_IL_SegFlags_Traced_MASK)
                  == PCR_IL_SegFlags_Traced_on) {
                GC_add_roots_inner
                	((char *)(q -> ls_addr), 
                	 (char *)(q -> ls_addr) + q -> ls_bytes);
              }
            }
          }
        }
}


#else /* !PCR */

void GC_register_dynamic_libraries(){}

int GC_no_dynamic_loading;

#endif /* !PCR */
#endif /* !DYNAMIC_LOADING */
