/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1997 by Silicon Graphics.  All rights reserved.
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

#include "private/gc_priv.h"

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

#if !defined(MACOS) && !defined(_WIN32_WCE)
# include <sys/types.h>
#endif

/* BTL: avoid circular redefinition of dlopen if GC_SOLARIS_THREADS defined */
# undef GC_MUST_RESTORE_REDEFINED_DLOPEN
# if (defined(GC_PTHREADS) || defined(GC_SOLARIS_THREADS)) \
      && defined(dlopen) && !defined(GC_USE_LD_WRAP)
    /* To support threads in Solaris, gc.h interposes on dlopen by       */
    /* defining "dlopen" to be "GC_dlopen", which is implemented below.  */
    /* However, both GC_FirstDLOpenedLinkMap() and GC_dlopen() use the   */
    /* real system dlopen() in their implementation. We first remove     */
    /* gc.h's dlopen definition and restore it later, after GC_dlopen(). */
#   undef dlopen
#   define GC_MUST_RESTORE_REDEFINED_DLOPEN
# endif

/* A user-supplied routine (custom filter) that might be called to      */
/* determine whether a DSO really needs to be scanned by the GC.        */
/* 0 means no filter installed.  May be unused on some platforms.       */
/* FIXME: Add filter support for more platforms.                        */
STATIC GC_has_static_roots_func GC_has_static_roots = 0;

#if (defined(DYNAMIC_LOADING) || defined(MSWIN32) || defined(MSWINCE)) \
    && !defined(PCR)

#if !defined(SOLARISDL) && !defined(IRIX5) && \
    !defined(MSWIN32) && !defined(MSWINCE) && \
    !(defined(ALPHA) && defined(OSF1)) && \
    !defined(HPUX) && !(defined(LINUX) && defined(__ELF__)) && \
    !defined(AIX) && !defined(SCO_ELF) && !defined(DGUX) && \
    !(defined(FREEBSD) && defined(__ELF__)) && \
    !(defined(OPENBSD) && (defined(__ELF__) || defined(M68K))) && \
    !(defined(NETBSD) && defined(__ELF__)) && !defined(HURD) && \
    !defined(DARWIN) && !defined(CYGWIN32)
 --> We only know how to find data segments of dynamic libraries for the
 --> above.  Additional SVR4 variants might not be too
 --> hard to add.
#endif

#include <stdio.h>
#ifdef SOLARISDL
#   include <sys/elf.h>
#   include <dlfcn.h>
#   include <link.h>
#endif

#if defined(NETBSD)
#   include <machine/elf_machdep.h>
#   define ELFSIZE ARCH_ELFSIZE
#endif

#if defined(SCO_ELF) || defined(DGUX) || defined(HURD) \
    || (defined(__ELF__) && (defined(LINUX) || defined(FREEBSD) \
                             || defined(NETBSD) || defined(OPENBSD)))
# include <stddef.h>
# if !defined(OPENBSD)
    /* FIXME: Why we exclude it for OpenBSD? */
#   include <elf.h>
# endif
# include <link.h>
#endif

/* Newer versions of GNU/Linux define this macro.  We
 * define it similarly for any ELF systems that don't.  */
#  ifndef ElfW
#    if defined(FREEBSD)
#      if __ELF_WORD_SIZE == 32
#        define ElfW(type) Elf32_##type
#      else
#        define ElfW(type) Elf64_##type
#      endif
#    elif defined(NETBSD) || defined(OPENBSD)
#      if ELFSIZE == 32
#        define ElfW(type) Elf32_##type
#      else
#        define ElfW(type) Elf64_##type
#      endif
#    else
#      if !defined(ELF_CLASS) || ELF_CLASS == ELFCLASS32
#        define ElfW(type) Elf32_##type
#      else
#        define ElfW(type) Elf64_##type
#      endif
#    endif
#  endif

#if defined(SOLARISDL) && !defined(USE_PROC_FOR_LIBRARIES)

#ifdef LINT
    Elf32_Dyn _DYNAMIC;
#endif

STATIC struct link_map *
GC_FirstDLOpenedLinkMap(void)
{
    extern ElfW(Dyn) _DYNAMIC;
    ElfW(Dyn) *dp;
    struct r_debug *r;
    static struct link_map * cachedResult = 0;
    static ElfW(Dyn) *dynStructureAddr = 0;
                /* BTL: added to avoid Solaris 5.3 ld.so _DYNAMIC bug   */

#   ifdef SUNOS53_SHARED_LIB
        /* BTL: Avoid the Solaris 5.3 bug that _DYNAMIC isn't being set */
        /* up properly in dynamically linked .so's. This means we have  */
        /* to use its value in the set of original object files loaded  */
        /* at program startup.                                          */
        if( dynStructureAddr == 0 ) {
          void* startupSyms = dlopen(0, RTLD_LAZY);
          dynStructureAddr = (ElfW(Dyn)*)dlsym(startupSyms, "_DYNAMIC");
        }
#   else
        dynStructureAddr = &_DYNAMIC;
#   endif

    if( dynStructureAddr == 0) {
        return(0);
    }
    if( cachedResult == 0 ) {
        int tag;
        for( dp = ((ElfW(Dyn) *)(&_DYNAMIC)); (tag = dp->d_tag) != 0; dp++ ) {
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

#endif /* SOLARISDL ... */

/* BTL: added to fix circular dlopen definition if GC_SOLARIS_THREADS defined */
# ifdef GC_MUST_RESTORE_REDEFINED_DLOPEN
#   define dlopen GC_dlopen
# endif

# if defined(SOLARISDL)
/* Add dynamic library data sections to the root set.           */
# if !defined(PCR) && !defined(GC_SOLARIS_THREADS) && defined(THREADS)
        --> fix mutual exclusion with dlopen
# endif

# ifndef USE_PROC_FOR_LIBRARIES
GC_INNER void GC_register_dynamic_libraries(void)
{
  struct link_map *lm = GC_FirstDLOpenedLinkMap();


  for (lm = GC_FirstDLOpenedLinkMap();
       lm != (struct link_map *) 0;  lm = lm->l_next)
    {
        ElfW(Ehdr) * e;
        ElfW(Phdr) * p;
        unsigned long offset;
        char * start;
        int i;

        e = (ElfW(Ehdr) *) lm->l_addr;
        p = ((ElfW(Phdr) *)(((char *)(e)) + e->e_phoff));
        offset = ((unsigned long)(lm->l_addr));
        for( i = 0; i < (int)(e->e_phnum); ((i++),(p++)) ) {
          switch( p->p_type ) {
            case PT_LOAD:
              {
                if( !(p->p_flags & PF_W) ) break;
                start = ((char *)(p->p_vaddr)) + offset;
                GC_add_roots_inner(
                  start,
                  start + p->p_memsz,
                  TRUE
                );
              }
              break;
            default:
              break;
          }
        }
    }
}

# endif /* !USE_PROC ... */
# endif /* SOLARISDL */

#if defined(SCO_ELF) || defined(DGUX) || defined(HURD) \
    || (defined(__ELF__) && (defined(LINUX) || defined(FREEBSD) \
                             || defined(NETBSD) || defined(OPENBSD)))

#ifdef USE_PROC_FOR_LIBRARIES

#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAPS_BUF_SIZE (32*1024)

GC_INNER char *GC_parse_map_entry(char *buf_ptr, ptr_t *start, ptr_t *end,
                                  char **prot, unsigned int *maj_dev,
                                  char **mapping_name);
GC_INNER char *GC_get_maps(void); /* from os_dep.c */

/* Sort an array of HeapSects by start address.                         */
/* Unfortunately at least some versions of                              */
/* Linux qsort end up calling malloc by way of sysconf, and hence can't */
/* be used in the collector.  Hence we roll our own.  Should be         */
/* reasonably fast if the array is already mostly sorted, as we expect  */
/* it to be.                                                            */
static void sort_heap_sects(struct HeapSect *base, size_t number_of_elements)
{
    signed_word n = (signed_word)number_of_elements;
    signed_word nsorted = 1;
    signed_word i;

    while (nsorted < n) {
      while (nsorted < n &&
             base[nsorted-1].hs_start < base[nsorted].hs_start)
          ++nsorted;
      if (nsorted == n) break;
      GC_ASSERT(base[nsorted-1].hs_start > base[nsorted].hs_start);
      i = nsorted - 1;
      while (i >= 0 && base[i].hs_start > base[i+1].hs_start) {
        struct HeapSect tmp = base[i];
        base[i] = base[i+1];
        base[i+1] = tmp;
        --i;
      }
      GC_ASSERT(base[nsorted-1].hs_start < base[nsorted].hs_start);
      ++nsorted;
    }
}

#ifdef THREADS
  GC_INNER GC_bool GC_segment_is_thread_stack(ptr_t lo, ptr_t hi);
#endif

STATIC word GC_register_map_entries(char *maps)
{
    char *prot;
    char *buf_ptr = maps;
    int count;
    ptr_t start, end;
    unsigned int maj_dev;
    ptr_t least_ha, greatest_ha;
    unsigned i;
    ptr_t datastart = (ptr_t)(DATASTART);

    GC_ASSERT(I_HOLD_LOCK());
    sort_heap_sects(GC_our_memory, GC_n_memory);
    least_ha = GC_our_memory[0].hs_start;
    greatest_ha = GC_our_memory[GC_n_memory-1].hs_start
                  + GC_our_memory[GC_n_memory-1].hs_bytes;

    for (;;) {
        buf_ptr = GC_parse_map_entry(buf_ptr, &start, &end, &prot,
                                     &maj_dev, 0);
        if (buf_ptr == NULL) return 1;
        if (prot[1] == 'w') {
            /* This is a writable mapping.  Add it to           */
            /* the root set unless it is already otherwise      */
            /* accounted for.                                   */
            if (start <= GC_stackbottom && end >= GC_stackbottom) {
                /* Stack mapping; discard       */
                continue;
            }
#           ifdef THREADS
              /* This may fail, since a thread may already be           */
              /* unregistered, but its thread stack may still be there. */
              /* That can fail because the stack may disappear while    */
              /* we're marking.  Thus the marker is, and has to be      */
              /* prepared to recover from segmentation faults.          */

              if (GC_segment_is_thread_stack(start, end)) continue;

              /* FIXME: NPTL squirrels                                  */
              /* away pointers in pieces of the stack segment that we   */
              /* don't scan.  We work around this                       */
              /* by treating anything allocated by libpthread as        */
              /* uncollectable, as we do in some other cases.           */
              /* A specifically identified problem is that              */
              /* thread stacks contain pointers to dynamic thread       */
              /* vectors, which may be reused due to thread caching.    */
              /* They may not be marked if the thread is still live.    */
              /* This specific instance should be addressed by          */
              /* INCLUDE_LINUX_THREAD_DESCR, but that doesn't quite     */
              /* seem to suffice.                                       */
              /* We currently trace entire thread stacks, if they are   */
              /* are currently cached but unused.  This is              */
              /* very suboptimal for performance reasons.               */
#           endif
            /* We no longer exclude the main data segment.              */
            if (end <= least_ha || start >= greatest_ha) {
              /* The easy case; just trace entire segment */
              GC_add_roots_inner((char *)start, (char *)end, TRUE);
              continue;
            }
            /* Add sections that don't belong to us. */
              i = 0;
              while (GC_our_memory[i].hs_start + GC_our_memory[i].hs_bytes
                     < start)
                  ++i;
              GC_ASSERT(i < GC_n_memory);
              if (GC_our_memory[i].hs_start <= start) {
                  start = GC_our_memory[i].hs_start
                          + GC_our_memory[i].hs_bytes;
                  ++i;
              }
              while (i < GC_n_memory && GC_our_memory[i].hs_start < end
                     && start < end) {
                  if ((char *)start < GC_our_memory[i].hs_start)
                    GC_add_roots_inner((char *)start,
                                       GC_our_memory[i].hs_start, TRUE);
                  start = GC_our_memory[i].hs_start
                          + GC_our_memory[i].hs_bytes;
                  ++i;
              }
              if (start < end)
                  GC_add_roots_inner((char *)start, (char *)end, TRUE);
        }
    }
    return 1;
}

GC_INNER void GC_register_dynamic_libraries(void)
{
    if (!GC_register_map_entries(GC_get_maps()))
        ABORT("Failed to read /proc for library registration.");
}

/* We now take care of the main data segment ourselves: */
GC_INNER GC_bool GC_register_main_static_data(void)
{
    return FALSE;
}

# define HAVE_REGISTER_MAIN_STATIC_DATA

#endif /* USE_PROC_FOR_LIBRARIES */

#if !defined(USE_PROC_FOR_LIBRARIES)
/* The following is the preferred way to walk dynamic libraries */
/* For glibc 2.2.4+.  Unfortunately, it doesn't work for older  */
/* versions.  Thanks to Jakub Jelinek for most of the code.     */

# if (defined(LINUX) || defined (__GLIBC__)) /* Are others OK here, too? */ \
     && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ > 2) \
         || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 2 && defined(DT_CONFIG)))
/* We have the header files for a glibc that includes dl_iterate_phdr.  */
/* It may still not be available in the library on the target system.   */
/* Thus we also treat it as a weak symbol.                              */
#define HAVE_DL_ITERATE_PHDR
#pragma weak dl_iterate_phdr
#endif

# if (defined(FREEBSD) && __FreeBSD__ >= 7)
/* On the FreeBSD system, any target system at major version 7 shall    */
/* have dl_iterate_phdr; therefore, we need not make it weak as above.  */
#define HAVE_DL_ITERATE_PHDR
#endif

#if defined(HAVE_DL_ITERATE_PHDR)

# ifdef PT_GNU_RELRO

/* Instead of registering PT_LOAD sections directly, we keep them       */
/* in a temporary list, and filter them by excluding PT_GNU_RELRO       */
/* segments.  Processing PT_GNU_RELRO sections with                     */
/* GC_exclude_static_roots instead would be superficially cleaner.  But */
/* it runs into trouble if a client registers an overlapping segment,   */
/* which unfortunately seems quite possible.                            */

#define MAX_LOAD_SEGS MAX_ROOT_SETS

static struct load_segment {
  ptr_t start;
  ptr_t end;
  /* Room for a second segment if we remove a RELRO segment */
  /* from the middle.                                       */
  ptr_t start2;
  ptr_t end2;
} load_segs[MAX_LOAD_SEGS];

static int n_load_segs;

# endif /* PT_GNU_RELRO */

STATIC int GC_register_dynlib_callback(struct dl_phdr_info * info,
                                       size_t size, void * ptr)
{
  const ElfW(Phdr) * p;
  ptr_t start, end;
  int i;

  /* Make sure struct dl_phdr_info is at least as big as we need.  */
  if (size < offsetof (struct dl_phdr_info, dlpi_phnum)
      + sizeof (info->dlpi_phnum))
    return -1;

  p = info->dlpi_phdr;
  for( i = 0; i < (int)(info->dlpi_phnum); ((i++),(p++)) ) {
    switch( p->p_type ) {
#     ifdef PT_GNU_RELRO
        case PT_GNU_RELRO:
        /* This entry is known to be constant and will eventually be remapped
           read-only.  However, the address range covered by this entry is
           typically a subset of a previously encountered `LOAD' segment, so
           we need to exclude it.  */
        {
            int j;

            start = ((ptr_t)(p->p_vaddr)) + info->dlpi_addr;
            end = start + p->p_memsz;
            for (j = n_load_segs; --j >= 0; ) {
              if (start >= load_segs[j].start && start < load_segs[j].end) {
                if (load_segs[j].start2 != 0) {
                  WARN("More than one GNU_RELRO segment per load seg\n",0);
                } else {
                  GC_ASSERT(end <= load_segs[j].end);
                  /* Remove from the existing load segment */
                  load_segs[j].end2 = load_segs[j].end;
                  load_segs[j].end = start;
                  load_segs[j].start2 = end;
                }
                break;
              }
              if (j == 0) WARN("Failed to find PT_GNU_RELRO segment"
                               " inside PT_LOAD region", 0);
            }
        }

        break;
#     endif

      case PT_LOAD:
        {
          GC_has_static_roots_func callback = GC_has_static_roots;
          if( !(p->p_flags & PF_W) ) break;
          start = ((char *)(p->p_vaddr)) + info->dlpi_addr;
          end = start + p->p_memsz;

          if (callback != 0 && !callback(info->dlpi_name, start, p->p_memsz))
            break;
#         ifdef PT_GNU_RELRO
            if (n_load_segs >= MAX_LOAD_SEGS) ABORT("Too many PT_LOAD segs");
            load_segs[n_load_segs].start = start;
            load_segs[n_load_segs].end = end;
            load_segs[n_load_segs].start2 = 0;
            load_segs[n_load_segs].end2 = 0;
            ++n_load_segs;
#         else
            GC_add_roots_inner(start, end, TRUE);
#         endif /* PT_GNU_RELRO */
        }
      break;
      default:
        break;
    }
  }

  * (int *)ptr = 1;     /* Signal that we were called */
  return 0;
}

/* Return TRUE if we succeed, FALSE if dl_iterate_phdr wasn't there. */

STATIC GC_bool GC_register_dynamic_libraries_dl_iterate_phdr(void)
{
  if (dl_iterate_phdr) {
    int did_something = 0;

#   ifdef PT_GNU_RELRO
        static GC_bool excluded_segs = FALSE;
        n_load_segs = 0;
        if (!excluded_segs) {
          GC_exclude_static_roots_inner((ptr_t)load_segs,
                                        (ptr_t)load_segs + sizeof(load_segs));
          excluded_segs = TRUE;
        }
#   endif
    dl_iterate_phdr(GC_register_dynlib_callback, &did_something);
    if (did_something) {
#     ifdef PT_GNU_RELRO
        size_t i;

        for (i = 0; i < n_load_segs; ++i) {
          if (load_segs[i].end > load_segs[i].start) {
            GC_add_roots_inner(load_segs[i].start, load_segs[i].end, TRUE);
          }
          if (load_segs[i].end2 > load_segs[i].start2) {
            GC_add_roots_inner(load_segs[i].start2, load_segs[i].end2, TRUE);
          }
        }
#     endif
    } else {
        /* dl_iterate_phdr may forget the static data segment in        */
        /* statically linked executables.                               */
        GC_add_roots_inner(DATASTART, (char *)(DATAEND), TRUE);
#       if defined(DATASTART2)
          GC_add_roots_inner(DATASTART2, (char *)(DATAEND2), TRUE);
#       endif
    }

    return TRUE;
  } else {
    return FALSE;
  }
}

/* Do we need to separately register the main static data segment? */
GC_INNER GC_bool GC_register_main_static_data(void)
{
  return (dl_iterate_phdr == 0);
}

#define HAVE_REGISTER_MAIN_STATIC_DATA

# else /* !LINUX || version(glibc) < 2.2.4 */

/* Dynamic loading code for Linux running ELF. Somewhat tested on
 * Linux/x86, untested but hopefully should work on Linux/Alpha.
 * This code was derived from the Solaris/ELF support. Thanks to
 * whatever kind soul wrote that.  - Patrick Bridges */

/* This doesn't necessarily work in all cases, e.g. with preloaded
 * dynamic libraries.                                           */

#if defined(NETBSD) || defined(OPENBSD)
#  include <sys/exec_elf.h>
/* for compatibility with 1.4.x */
#  ifndef DT_DEBUG
#  define DT_DEBUG     21
#  endif
#  ifndef PT_LOAD
#  define PT_LOAD      1
#  endif
#  ifndef PF_W
#  define PF_W         2
#  endif
#else
#  include <elf.h>
#endif
#include <link.h>

# endif

#ifdef __GNUC__
# pragma weak _DYNAMIC
#endif
extern ElfW(Dyn) _DYNAMIC[];

STATIC struct link_map *
GC_FirstDLOpenedLinkMap(void)
{
    ElfW(Dyn) *dp;
    static struct link_map *cachedResult = 0;

    if( _DYNAMIC == 0) {
        return(0);
    }
    if( cachedResult == 0 ) {
        int tag;
        for( dp = _DYNAMIC; (tag = dp->d_tag) != 0; dp++ ) {
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

GC_INNER void GC_register_dynamic_libraries(void)
{
  struct link_map *lm;

# ifdef HAVE_DL_ITERATE_PHDR
    if (GC_register_dynamic_libraries_dl_iterate_phdr()) {
        return;
    }
# endif
  lm = GC_FirstDLOpenedLinkMap();
  for (lm = GC_FirstDLOpenedLinkMap();
       lm != (struct link_map *) 0;  lm = lm->l_next)
    {
        ElfW(Ehdr) * e;
        ElfW(Phdr) * p;
        unsigned long offset;
        char * start;
        int i;

        e = (ElfW(Ehdr) *) lm->l_addr;
        p = ((ElfW(Phdr) *)(((char *)(e)) + e->e_phoff));
        offset = ((unsigned long)(lm->l_addr));
        for( i = 0; i < (int)(e->e_phnum); ((i++),(p++)) ) {
          switch( p->p_type ) {
            case PT_LOAD:
              {
                if( !(p->p_flags & PF_W) ) break;
                start = ((char *)(p->p_vaddr)) + offset;
                GC_add_roots_inner(start, start + p->p_memsz, TRUE);
              }
              break;
            default:
              break;
          }
        }
    }
}

#endif /* !USE_PROC_FOR_LIBRARIES */

#endif /* LINUX */

#if defined(IRIX5) || (defined(USE_PROC_FOR_LIBRARIES) && !defined(LINUX))

#include <sys/procfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <errno.h>
#include <signal.h>  /* Only for the following test. */
#ifndef _sigargs
# define IRIX6
#endif

GC_INNER void * GC_roots_present(ptr_t);
        /* The type is a lie, since the real type doesn't make sense here, */
        /* and we only test for NULL.                                      */

/* We use /proc to track down all parts of the address space that are   */
/* mapped by the process, and throw out regions we know we shouldn't    */
/* worry about.  This may also work under other SVR4 variants.          */
GC_INNER void GC_register_dynamic_libraries(void)
{
    static int fd = -1;
    char buf[30];
    static prmap_t * addr_map = 0;
    static int current_sz = 0;  /* Number of records currently in addr_map */
    static int needed_sz;       /* Required size of addr_map            */
    int i;
    long flags;
    ptr_t start;
    ptr_t limit;
    ptr_t heap_start = HEAP_START;
    ptr_t heap_end = heap_start;

#   ifdef SOLARISDL
#     define MA_PHYS 0
#   endif /* SOLARISDL */

    if (fd < 0) {
      sprintf(buf, "/proc/%ld", (long)getpid());
        /* The above generates a lint complaint, since pid_t varies.    */
        /* It's unclear how to improve this.                            */
      fd = open(buf, O_RDONLY);
      if (fd < 0) {
        ABORT("/proc open failed");
      }
    }
    if (ioctl(fd, PIOCNMAP, &needed_sz) < 0) {
        GC_err_printf("fd = %d, errno = %d\n", fd, errno);
        ABORT("/proc PIOCNMAP ioctl failed");
    }
    if (needed_sz >= current_sz) {
        current_sz = needed_sz * 2 + 1;
                        /* Expansion, plus room for 0 record */
        addr_map = (prmap_t *)GC_scratch_alloc(
                                (word)current_sz * sizeof(prmap_t));
    }
    if (ioctl(fd, PIOCMAP, addr_map) < 0) {
        GC_err_printf("fd = %d, errno = %d, needed_sz = %d, addr_map = %p\n",
                        fd, errno, needed_sz, addr_map);
        ABORT("/proc PIOCMAP ioctl failed");
    };
    if (GC_n_heap_sects > 0) {
        heap_end = GC_heap_sects[GC_n_heap_sects-1].hs_start
                        + GC_heap_sects[GC_n_heap_sects-1].hs_bytes;
        if (heap_end < GC_scratch_last_end_ptr) heap_end = GC_scratch_last_end_ptr;
    }
    for (i = 0; i < needed_sz; i++) {
        flags = addr_map[i].pr_mflags;
        if ((flags & (MA_BREAK | MA_STACK | MA_PHYS
                      | MA_FETCHOP | MA_NOTCACHED)) != 0) goto irrelevant;
        if ((flags & (MA_READ | MA_WRITE)) != (MA_READ | MA_WRITE))
            goto irrelevant;
          /* The latter test is empirically useless in very old Irix    */
          /* versions.  Other than the                                  */
          /* main data and stack segments, everything appears to be     */
          /* mapped readable, writable, executable, and shared(!!).     */
          /* This makes no sense to me. - HB                            */
        start = (ptr_t)(addr_map[i].pr_vaddr);
        if (GC_roots_present(start)) goto irrelevant;
        if (start < heap_end && start >= heap_start)
                goto irrelevant;
#       ifdef MMAP_STACKS
          if (GC_is_thread_stack(start)) goto irrelevant;
#       endif /* MMAP_STACKS */

        limit = start + addr_map[i].pr_size;
        /* The following seemed to be necessary for very old versions   */
        /* of Irix, but it has been reported to discard relevant        */
        /* segments under Irix 6.5.                                     */
#       ifndef IRIX6
          if (addr_map[i].pr_off == 0 && strncmp(start, ELFMAG, 4) == 0) {
            /* Discard text segments, i.e. 0-offset mappings against    */
            /* executable files which appear to have ELF headers.       */
            caddr_t arg;
            int obj;
#           define MAP_IRR_SZ 10
            static ptr_t map_irr[MAP_IRR_SZ];
                                        /* Known irrelevant map entries */
            static int n_irr = 0;
            struct stat buf;
            register int j;

            for (j = 0; j < n_irr; j++) {
                if (map_irr[j] == start) goto irrelevant;
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
#       endif /* !IRIX6 */
        GC_add_roots_inner(start, limit, TRUE);
      irrelevant: ;
    }
    /* Don't keep cached descriptor, for now.  Some kernels don't like us */
    /* to keep a /proc file descriptor around during kill -9.             */
        if (close(fd) < 0) ABORT("Couldn't close /proc file");
        fd = -1;
}

# endif /* USE_PROC || IRIX5 */

# if defined(MSWIN32) || defined(MSWINCE) || defined(CYGWIN32)

# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
# endif
# define NOSERVICE
# include <windows.h>
# include <stdlib.h>

  /* We traverse the entire address space and register all segments     */
  /* that could possibly have been written to.                          */

  GC_INNER GC_bool GC_is_heap_base(ptr_t p);

# ifdef GC_WIN32_THREADS
    GC_INNER void GC_get_next_stack(char *start, char * limit, char **lo,
                                    char **hi);

    STATIC void GC_cond_add_roots(char *base, char * limit)
    {
      char * curr_base = base;
      char * next_stack_lo;
      char * next_stack_hi;

      if (base == limit) return;
      for(;;) {
          GC_get_next_stack(curr_base, limit, &next_stack_lo, &next_stack_hi);
          if (next_stack_lo >= limit) break;
          if (next_stack_lo > curr_base)
            GC_add_roots_inner(curr_base, next_stack_lo, TRUE);
          curr_base = next_stack_hi;
      }
      if (curr_base < limit) GC_add_roots_inner(curr_base, limit, TRUE);
    }
# else
    STATIC void GC_cond_add_roots(char *base, char * limit)
    {
      char dummy;
      char * stack_top
         = (char *) ((word)(&dummy) & ~(GC_sysinfo.dwAllocationGranularity-1));
      if (base == limit) return;
      if (limit > stack_top && base < GC_stackbottom) {
          /* Part of the stack; ignore it. */
          return;
      }
      GC_add_roots_inner(base, limit, TRUE);
    }
# endif

#ifdef DYNAMIC_LOADING
  /* GC_register_main_static_data is not needed unless DYNAMIC_LOADING. */
  GC_INNER GC_bool GC_register_main_static_data(void)
  {
#   ifdef MSWINCE
      /* Do we need to separately register the main static data segment? */
      return FALSE;
#   else
      return GC_no_win32_dlls;
#   endif
  }
#endif /* DYNAMIC_LOADING */

#define HAVE_REGISTER_MAIN_STATIC_DATA

# ifdef DEBUG_VIRTUALQUERY
  void GC_dump_meminfo(MEMORY_BASIC_INFORMATION *buf)
  {
    GC_printf("BaseAddress = 0x%lx, AllocationBase = 0x%lx,"
              " RegionSize = 0x%lx(%lu)\n", buf -> BaseAddress,
              buf -> AllocationBase, buf -> RegionSize, buf -> RegionSize);
    GC_printf("\tAllocationProtect = 0x%lx, State = 0x%lx, Protect = 0x%lx, "
              "Type = 0x%lx\n", buf -> AllocationProtect, buf -> State,
              buf -> Protect, buf -> Type);
  }
# endif /* DEBUG_VIRTUALQUERY */

# ifdef MSWINCE
    /* FIXME: Should we really need to scan MEM_PRIVATE sections?       */
    /* For now, we don't add MEM_PRIVATE sections to the data roots for */
    /* WinCE because otherwise SEGV fault sometimes happens to occur in */
    /* GC_mark_from() (and, even if we use WRAP_MARK_SOME, WinCE prints */
    /* a "Data Abort" message to the debugging console).                */
    /* To workaround that, use -DGC_REGISTER_MEM_PRIVATE.               */
#   define GC_wnt TRUE
# endif

  GC_INNER void GC_register_dynamic_libraries(void)
  {
    MEMORY_BASIC_INFORMATION buf;
    size_t result;
    DWORD protect;
    LPVOID p;
    char * base;
    char * limit, * new_limit;

#   ifdef MSWIN32
      if (GC_no_win32_dlls) return;
#   endif
    base = limit = p = GC_sysinfo.lpMinimumApplicationAddress;
    while (p < GC_sysinfo.lpMaximumApplicationAddress) {
        result = VirtualQuery(p, &buf, sizeof(buf));
#       ifdef MSWINCE
          if (result == 0) {
            /* Page is free; advance to the next possible allocation base */
            new_limit = (char *)
                (((DWORD) p + GC_sysinfo.dwAllocationGranularity)
                 & ~(GC_sysinfo.dwAllocationGranularity-1));
          } else
#       endif
        /* else */ {
            if (result != sizeof(buf)) {
                ABORT("Weird VirtualQuery result");
            }
            new_limit = (char *)p + buf.RegionSize;
            protect = buf.Protect;
            if (buf.State == MEM_COMMIT
                && (protect == PAGE_EXECUTE_READWRITE
                    || protect == PAGE_READWRITE)
                && (buf.Type == MEM_IMAGE
#                   ifdef GC_REGISTER_MEM_PRIVATE
                      || (protect == PAGE_READWRITE && buf.Type == MEM_PRIVATE)
#                   else
                      /* There is some evidence that we cannot always   */
                      /* ignore MEM_PRIVATE sections under Windows ME   */
                      /* and predecessors.  Hence we now also check for */
                      /* that case.                                     */
                      || (!GC_wnt && buf.Type == MEM_PRIVATE)
#                   endif
                   )
                && !GC_is_heap_base(buf.AllocationBase)) {
#               ifdef DEBUG_VIRTUALQUERY
                  GC_dump_meminfo(&buf);
#               endif
                if ((char *)p != limit) {
                    GC_cond_add_roots(base, limit);
                    base = p;
                }
                limit = new_limit;
            }
        }
        if (p > (LPVOID)new_limit /* overflow */) break;
        p = (LPVOID)new_limit;
    }
    GC_cond_add_roots(base, limit);
  }

#endif /* MSWIN32 || MSWINCE || CYGWIN32 */

#if defined(ALPHA) && defined(OSF1)

#include <loader.h>

GC_INNER void GC_register_dynamic_libraries(void)
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
            GC_printf("dynamic_load: status = %d\n", status);
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

#     ifdef DL_VERBOSE
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

#         ifdef DL_VERBOSE
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
                (char *)regioninfo.lri_mapaddr + regioninfo.lri_size,
                TRUE);

        }
    }
}
#endif

#if defined(HPUX)

#include <errno.h>
#include <dl.h>

extern char *sys_errlist[];
extern int sys_nerr;

GC_INNER void GC_register_dynamic_libraries(void)
{
  int status;
  int index = 1; /* Ordinal position in shared library search list */
  struct shl_descriptor *shl_desc; /* Shared library info, see dl.h */

  /* For each dynamic library loaded */
    while (TRUE) {

      /* Get info about next shared library */
        status = shl_get(index, &shl_desc);

      /* Check if this is the end of the list or if some error occured */
        if (status != 0) {
#        ifdef GC_HPUX_THREADS
           /* I've seen errno values of 0.  The man page is not clear   */
           /* as to whether errno should get set on a -1 return.        */
           break;
#        else
          if (errno == EINVAL) {
              break; /* Moved past end of shared library list --> finished */
          } else {
              if (errno <= sys_nerr) {
                    GC_printf("dynamic_load: %s\n", sys_errlist[errno]);
              } else {
                    GC_printf("dynamic_load: %d\n", errno);
              }
              ABORT("shl_get failed");
          }
#        endif
        }

#     ifdef DL_VERBOSE
          GC_printf("---Shared library---\n");
          GC_printf("\tfilename        = \"%s\"\n", shl_desc->filename);
          GC_printf("\tindex           = %d\n", index);
          GC_printf("\thandle          = %08x\n",
                                        (unsigned long) shl_desc->handle);
          GC_printf("\ttext seg. start = %08x\n", shl_desc->tstart);
          GC_printf("\ttext seg. end   = %08x\n", shl_desc->tend);
          GC_printf("\tdata seg. start = %08x\n", shl_desc->dstart);
          GC_printf("\tdata seg. end   = %08x\n", shl_desc->dend);
          GC_printf("\tref. count      = %lu\n", shl_desc->ref_count);
#     endif

      /* register shared library's data segment as a garbage collection root */
        GC_add_roots_inner((char *) shl_desc->dstart,
                           (char *) shl_desc->dend, TRUE);

        index++;
    }
}
#endif /* HPUX */

#ifdef AIX
# pragma alloca
# include <sys/ldr.h>
# include <sys/errno.h>
  GC_INNER void GC_register_dynamic_libraries(void)
  {
        int len;
        char *ldibuf;
        int ldibuflen;
        struct ld_info *ldi;

        ldibuf = alloca(ldibuflen = 8192);

        while ( (len = loadquery(L_GETINFO,ldibuf,ldibuflen)) < 0) {
                if (errno != ENOMEM) {
                        ABORT("loadquery failed");
                }
                ldibuf = alloca(ldibuflen *= 2);
        }

        ldi = (struct ld_info *)ldibuf;
        while (ldi) {
                len = ldi->ldinfo_next;
                GC_add_roots_inner(
                                ldi->ldinfo_dataorg,
                                (ptr_t)(unsigned long)ldi->ldinfo_dataorg
                                + ldi->ldinfo_datasize,
                                TRUE);
                ldi = len ? (struct ld_info *)((char *)ldi + len) : 0;
        }
  }
#endif /* AIX */

#ifdef DARWIN

/* __private_extern__ hack required for pre-3.4 gcc versions.   */
#ifndef __private_extern__
# define __private_extern__ extern
# include <mach-o/dyld.h>
# undef __private_extern__
#else
# include <mach-o/dyld.h>
#endif
#include <mach-o/getsect.h>

/*#define DARWIN_DEBUG*/

STATIC const struct {
        const char *seg;
        const char *sect;
} GC_dyld_sections[] = {
        { SEG_DATA, SECT_DATA },
        { SEG_DATA, SECT_BSS },
        { SEG_DATA, SECT_COMMON }
};

STATIC const char *GC_dyld_name_for_hdr(const struct GC_MACH_HEADER *hdr)
{
    unsigned long i, c;
    c = _dyld_image_count();
    for (i = 0; i < c; i++)
      if ((const struct GC_MACH_HEADER *)_dyld_get_image_header(i) == hdr)
        return _dyld_get_image_name(i);
    return NULL;
}

/* This should never be called by a thread holding the lock */
STATIC void GC_dyld_image_add(const struct GC_MACH_HEADER *hdr, intptr_t slide)
{
    unsigned long start,end,i;
    const struct GC_MACH_SECTION *sec;
    const char *name;
    GC_has_static_roots_func callback = GC_has_static_roots;
    DCL_LOCK_STATE;
    if (GC_no_dls) return;
#   ifdef DARWIN_DEBUG
      name = GC_dyld_name_for_hdr(hdr);
#   else
      name = callback != 0 ? GC_dyld_name_for_hdr(hdr) : NULL;
#   endif
    for(i=0;i<sizeof(GC_dyld_sections)/sizeof(GC_dyld_sections[0]);i++) {
      sec = GC_GETSECTBYNAME(hdr, GC_dyld_sections[i].seg,
                             GC_dyld_sections[i].sect);
      if(sec == NULL || sec->size < sizeof(word)) continue;
      start = slide + sec->addr;
      end = start + sec->size;
      LOCK();
      /* The user callback is called holding the lock   */
      if (callback == 0 || callback(name, (void*)start, (size_t)sec->size)) {
#       ifdef DARWIN_DEBUG
          GC_printf("Adding section at %p-%p (%lu bytes) from image %s\n",
                    start,end,sec->size,name);
#       endif
        GC_add_roots_inner((ptr_t)start, (ptr_t)end, FALSE);
      }
      UNLOCK();
    }
#   ifdef DARWIN_DEBUG
       GC_print_static_roots();
#   endif
}

/* This should never be called by a thread holding the lock */
STATIC void GC_dyld_image_remove(const struct GC_MACH_HEADER *hdr,
                                 intptr_t slide)
{
    unsigned long start,end,i;
    const struct GC_MACH_SECTION *sec;
    for(i=0;i<sizeof(GC_dyld_sections)/sizeof(GC_dyld_sections[0]);i++) {
      sec = GC_GETSECTBYNAME(hdr, GC_dyld_sections[i].seg,
                             GC_dyld_sections[i].sect);
      if(sec == NULL || sec->size == 0) continue;
      start = slide + sec->addr;
      end = start + sec->size;
#   ifdef DARWIN_DEBUG
      GC_printf("Removing section at %p-%p (%lu bytes) from image %s\n",
                start,end,sec->size,GC_dyld_name_for_hdr(hdr));
#   endif
      GC_remove_roots((char*)start,(char*)end);
    }
#   ifdef DARWIN_DEBUG
        GC_print_static_roots();
#   endif
}

GC_INNER void GC_register_dynamic_libraries(void)
{
    /* Currently does nothing. The callbacks are setup by GC_init_dyld()
    The dyld library takes it from there. */
}

/* The _dyld_* functions have an internal lock so no _dyld functions
   can be called while the world is stopped without the risk of a deadlock.
   Because of this we MUST setup callbacks BEFORE we ever stop the world.
   This should be called BEFORE any thread in created and WITHOUT the
   allocation lock held. */

GC_INNER void GC_init_dyld(void)
{
  static GC_bool initialized = FALSE;

  if(initialized) return;

#   ifdef DARWIN_DEBUG
      GC_printf("Registering dyld callbacks...\n");
#   endif

  /* Apple's Documentation:
     When you call _dyld_register_func_for_add_image, the dynamic linker
     runtime calls the specified callback (func) once for each of the images
     that is currently loaded into the program. When a new image is added to
     the program, your callback is called again with the mach_header for the
     new image, and the virtual memory slide amount of the new image.

     This WILL properly register already linked libraries and libraries
     linked in the future.
  */

    _dyld_register_func_for_add_image(GC_dyld_image_add);
    _dyld_register_func_for_remove_image(GC_dyld_image_remove);
        /* Ignore 2 compiler warnings here: passing argument 1 of       */
        /* '_dyld_register_func_for_add/remove_image' from incompatible */
        /* pointer type.                                                */


    /* Set this early to avoid reentrancy issues. */
    initialized = TRUE;

    if (GETENV("DYLD_BIND_AT_LAUNCH") == 0) {
#     ifdef DARWIN_DEBUG
        GC_printf("Forcing full bind of GC code...\n");
#     endif
      /* FIXME: '_dyld_bind_fully_image_containing_address' is deprecated. */
      if(!_dyld_bind_fully_image_containing_address((unsigned long*)GC_malloc))
        ABORT("_dyld_bind_fully_image_containing_address failed");
    }

}

#define HAVE_REGISTER_MAIN_STATIC_DATA
GC_INNER GC_bool GC_register_main_static_data(void)
{
  /* Already done through dyld callbacks */
  return FALSE;
}

#endif /* DARWIN */

#else /* !DYNAMIC_LOADING */

#ifdef PCR

# include "il/PCR_IL.h"
# include "th/PCR_ThCtl.h"
# include "mm/PCR_MM.h"

  GC_INNER void GC_register_dynamic_libraries(void)
  {
    /* Add new static data areas of dynamically loaded modules. */
        {
          PCR_IL_LoadedFile * p = PCR_IL_GetLastLoadedFile();
          PCR_IL_LoadedSegment * q;

          /* Skip uncommitted files */
          while (p != NIL && !(p -> lf_commitPoint)) {
              /* The loading of this file has not yet been committed    */
              /* Hence its description could be inconsistent.           */
              /* Furthermore, it hasn't yet been run.  Hence its data   */
              /* segments can't possibly reference heap allocated       */
              /* objects.                                               */
              p = p -> lf_prev;
          }
          for (; p != NIL; p = p -> lf_prev) {
            for (q = p -> lf_ls; q != NIL; q = q -> ls_next) {
              if ((q -> ls_flags & PCR_IL_SegFlags_Traced_MASK)
                  == PCR_IL_SegFlags_Traced_on) {
                GC_add_roots_inner
                        ((char *)(q -> ls_addr),
                         (char *)(q -> ls_addr) + q -> ls_bytes,
                         TRUE);
              }
            }
          }
        }
  }

#else /* !PCR */

GC_INNER void GC_register_dynamic_libraries(void) {}

#endif /* !PCR */

#endif /* !DYNAMIC_LOADING */

#ifndef HAVE_REGISTER_MAIN_STATIC_DATA
  /* Do we need to separately register the main static data segment? */
  GC_INNER GC_bool GC_register_main_static_data(void)
  {
    return TRUE;
  }
#endif /* HAVE_REGISTER_MAIN_STATIC_DATA */

/* Register a routine to filter dynamic library registration.  */
GC_API void GC_CALL GC_register_has_static_roots_callback(
                                        GC_has_static_roots_func callback)
{
    GC_has_static_roots = callback;
}
