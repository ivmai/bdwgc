# include <stdio.h>
# include "gc_private.h"
# define MAX_ROOT_SETS 50
# ifdef PCR
#   include "pcr/il/PCR_IL.h"
#   include "pcr/th/PCR_ThCtl.h"
#   include "pcr/mm/PCR_MM.h"
# endif

struct roots {
	ptr_t r_start;
	ptr_t r_end;
};

static struct roots static_roots[MAX_ROOT_SETS];
static n_root_sets = 0;

word GC_root_size = 0;

void GC_add_roots(b, e)
char * b; char * e;
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    GC_add_roots_inner(b, e);
    UNLOCK();
    ENABLE_SIGNALS();
}



void GC_add_roots_inner(b, e)
char * b; char * e;
{
    /* We exclude GC data structures from root sets.  It's usually safe	*/
    /* to mark from those, but it is a waste of time.			*/
    if ( (ptr_t)b < beginGC_arrays && (ptr_t)e > beginGC_arrays) {
        if ((ptr_t)e <= endGC_arrays) {
            e = (char *)beginGC_arrays;
        } else {
            GC_add_roots_inner(b, (char *)beginGC_arrays);
            GC_add_roots_inner((char *)endGC_arrays, e);
            return;
        }
    } else if ((ptr_t)b < endGC_arrays && (ptr_t)e > endGC_arrays) {
        b = (char *)endGC_arrays;
    }
    if (n_root_sets == MAX_ROOT_SETS) {
        ABORT("Too many root sets\n");
    }
    static_roots[n_root_sets].r_start = (ptr_t)b;
    static_roots[n_root_sets].r_end = (ptr_t)e;
    GC_root_size += (ptr_t)e - (ptr_t)b;
    n_root_sets++;
}

void GC_clear_roots()
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    n_root_sets = 0;
    GC_root_size = 0;
    UNLOCK();
    ENABLE_SIGNALS();
}

# ifdef PCR
PCR_ERes GC_mark_thread_stack(PCR_Th_T *t, PCR_Any dummy)
{
    struct PCR_ThCtl_TInfoRep info;
    PCR_ERes result;
    
    info.ti_stkLow = info.ti_stkHi = 0;
    result = PCR_ThCtl_GetInfo(t, &info);
    GC_mark_all_stack((ptr_t)(info.ti_stkLow), (ptr_t)(info.ti_stkHi));
    return(result);
}

PCR_ERes GC_mark_old_obj(void *p, size_t size, PCR_Any data)
{
    GC_mark_all((ptr_t)p, (ptr_t)p + size);
    return(PCR_ERes_okay);
}

# else
ptr_t GC_approx_sp()
{
    word dummy;
    
    return((ptr_t)(&dummy));
}
# endif
/*
 * Call the mark routines (GC_tl_mark for a single pointer, GC_mark_all
 * on groups of pointers) on every top level accessible pointer.
 * This is source language specific.  The following works for C.
 */

GC_mark_roots()
{
    register int i;

    /*
     * mark from registers - i.e., call GC_tl_mark(i) for each
     * register i
     */
        GC_mark_regs(); /* usually defined in machine_dep.c */


#   ifdef PCR
	/* Traverse data allocated by previous memory managers.		*/
	{
	  extern struct PCR_MM_ProcsRep * GC_old_allocator;
	  
	  if ((*(GC_old_allocator->mmp_enumerate))(PCR_Bool_false,
	  					   GC_mark_old_obj, 0)
	      != PCR_ERes_okay) {
	      ABORT("Old object enumeration failed");
	  }
	}
	
        /* Add new static data areas of dynamically loaded modules.	*/
        {
          PCR_IL_LoadedFile * p = PCR_IL_GetLoadedFiles();
          static PCR_IL_LoadedFile * last_already_added = NIL;
          	/* Last file that was already added to the list of roots. */
          PCR_IL_LoadedFile * last_committed;
          PCR_IL_LoadedSegment * q;
          
          if (p != NIL && last_already_added == NIL) {
            /* Switch to obtaining roots from the dynamic loader. */
              /* Make sure the loader is properly initialized and that	*/
              /* it has a correct description of PCR static data.	*/
                PCR_IL_Lock(PCR_Bool_false,
                	    PCR_allSigsBlocked, PCR_waitForever);
	        PCR_IL_Unlock();
	      /* Discard old root sets. */
	        n_root_sets = 0;
	        GC_root_size = 0;
	      /* We claim there are no dynamic libraries, or they	*/
	      /* don't contain roots, since they allocate using the	*/
	      /* system malloc, and they can't retain our pointers.	*/
          }
          /* Skip uncommited files */
          while (p != NIL && !(p -> lf_commitPoint)) {
              /* The loading of this file has not yet been committed	*/
              /* Hence its description could be inconsistent.  		*/
              /* Furthermore, it hasn't yet been run.  Hence it's data  */
              /* segments can possibly reference heap allocated objects.*/
              p = p -> lf_prev;
          }
          last_committed = p;
          for (; p != last_already_added; p = p -> lf_prev) {
            for (q = p -> lf_ls; q != NIL; q = q -> ls_next) {
              if ((q -> ls_flags & PCR_IL_SegFlags_Traced_MASK)
                  == PCR_IL_SegFlags_Traced_on) {
                GC_add_roots_inner
                	((char *)(q -> ls_addr), 
                	 (char *)(q -> ls_addr) + q -> ls_bytes);
              }
            }
          }
          last_already_added = last_committed;
        }
        
        
        /* Traverse all thread stacks. */
          if (PCR_ERes_IsErr(
                PCR_ThCtl_ApplyToAllOtherThreads(GC_mark_thread_stack,0))
              || PCR_ERes_IsErr(GC_mark_thread_stack(PCR_Th_CurrThread(), 0))) {
              ABORT("Thread stack marking failed\n");
          }
#   else
        /* Mark everything on the stack.           */
#   	  ifdef STACK_GROWS_DOWN
	    GC_mark_all_stack( GC_approx_sp(), GC_stackbottom );
#	  else
	    GC_mark_all_stack( GC_stackbottom, GC_approx_sp() );
#	  endif
#   endif

    /* Mark everything in static data areas                             */
    for (i = 0; i < n_root_sets; i++) {
        GC_mark_all(static_roots[i].r_start, static_roots[i].r_end);
    }
}

/*
 * Top level GC_mark routine. Mark from the object pointed to by p.
 * GC_tl_mark is normally called by GC_mark_regs, and thus must be defined.
 */
void GC_tl_mark(p)
word p;
{
    word q;

    q = p;
    GC_mark_all_stack((ptr_t)(&q), (ptr_t)((&q)+1));
}
