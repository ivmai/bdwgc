/* 
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
/* Boehm, March 28, 1994 1:58 pm PST */
# include "gc_priv.h"

# ifdef PCR
/*
 * Note that POSIX PCR requires an ANSI C compiler.  Hence we are allowed
 * to make the same assumption here.
 * We wrap all of the allocator functions to avoid questions of
 * compatibility between the prototyped and nonprototyped versions of the f
 */
# include "mm/PCR_MM.h"

# define MY_MAGIC 17L

void * GC_AllocProc(size_t size, PCR_Bool ptrFree, PCR_Bool clear )
{
    if (ptrFree) {
        void * result = (void *)GC_malloc_atomic(size);
        if (clear && result != 0) BZERO(result, size);
        return(result);
    } else {
        return((void *)GC_malloc(size));
    }
}

# define GC_ReallocProc GC_realloc

# define GC_FreeProc GC_free

typedef struct {
  PCR_ERes (*ed_proc)(void *p, size_t size, PCR_Any data);
  bool ed_pointerfree;
  PCR_ERes ed_fail_code;
  PCR_Any ed_client_data;
} enumerate_data;

void GC_enumerate_block(h, ed)
register struct hblk *h;
enumerate_data * ed;
{
    register hdr * hhdr;
    register int sz;
    word *p;
    word * lim;
    
    hhdr = HDR(h);
    sz = hhdr -> hb_sz;
    if (sz >= 0 && ed -> ed_pointerfree
    	|| sz <= 0 && !(ed -> ed_pointerfree)) return;
    if (sz < 0) sz = -sz;
    lim = (word *)(h+1) - sz;
    p = (word *)h;
    do {
        if (PCR_ERes_IsErr(ed -> ed_fail_code)) return;
        ed -> ed_fail_code =
            (*(ed -> ed_proc))(p, WORDS_TO_BYTES(sz), ed -> ed_client_data);
        p+= sz;
    } while (p <= lim);
}

struct PCR_MM_ProcsRep * GC_old_allocator = 0;

PCR_ERes GC_EnumerateProc(
    PCR_Bool ptrFree,
    PCR_ERes (*proc)(void *p, size_t size, PCR_Any data),
    PCR_Any data
)
{
    enumerate_data ed;
    
    ed.ed_proc = proc;
    ed.ed_pointerfree = ptrFree;
    ed.ed_fail_code = PCR_ERes_okay;
    ed.ed_client_data = data;
    GC_apply_to_all_blocks(GC_enumerate_block, &ed);
    if (ed.ed_fail_code != PCR_ERes_okay) {
        return(ed.ed_fail_code);
    } else {
    	/* Also enumerate objects allocated by my predecessors */
    	return((*(GC_old_allocator->mmp_enumerate))(ptrFree, proc, data));
    }
}

void GC_DummyFreeProc(void *p) {};

void GC_DummyShutdownProc(void) {};

struct PCR_MM_ProcsRep GC_Rep = {
	MY_MAGIC,
	GC_AllocProc,
	GC_ReallocProc,
	GC_DummyFreeProc,  	/* mmp_free */
	GC_FreeProc,  		/* mmp_unsafeFree */
	GC_EnumerateProc,
	GC_DummyShutdownProc	/* mmp_shutdown */
};

void GC_pcr_install()
{
    PCR_MM_Install(&GC_Rep, &GC_old_allocator);
}
# endif
