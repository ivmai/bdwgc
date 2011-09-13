/*
 * Copyright (c) 2011 by Hewlett-Packard Company.  All rights reserved.
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
 */

// FIXME: Move this code to another existing file (which is appropriate),
// and remove this file (adding a new file is a bit complex due to numerous
// scripts present).

#include "private/gc_priv.h"

// FIXME: add #ifdef ENABLE_DISCLAIM

#include "private/thread_local_alloc.h"
         // FIXME: we can include it only if THREAD_LOCAL_ALLOC.

#include "gc_disclaim.h"

/* Low level interface for reclaim callbacks. */

// FIXME: Use declared type for proc.
// FIXME: Use GC_API and GC_CALL.
void GC_register_disclaim_proc(int kind,
                               int (*proc)(void *obj, void *cd), void *cd,
                               int mark_unconditionally)
{
    GC_obj_kinds[kind].ok_disclaim_proc = proc;
    GC_obj_kinds[kind].ok_disclaim_cd = cd;
    GC_obj_kinds[kind].ok_mark_unconditionally = mark_unconditionally;
}

/* High level interface for finalization. */

STATIC int GC_finalized_kind;

STATIC ptr_t * GC_finalized_objfreelist = 0;
STATIC ptr_t * GC_finalized_debugobjfreelist = 0;


STATIC int GC_finalized_disclaim(void *obj, void *cd) // FIXME: Add CALLBACK
{
    struct GC_finalizer_closure *fc = *(void **)obj;
    if (((word)fc & 1) != 0) {
       /* The disclaim function may be passed fragments from the free-list, on
        * which it should not run finalization.  To recognize this case, we use
        * the fact that the first word on such fragments are always even (a link
        * to the next fragment, or NULL).  If it is desirable to have a finalizer
        * which does not use the first word for storing finalization info,
        * GC_reclaim_with_finalization must be extended to clear fragments so
        * that the assumption holds for the selected word. */
        fc = (void *)((word)fc & ~(word)1);
        (*fc->proc)((void **)obj + 1, fc->cd);
    }
    return 0;
}

static int done_init = 0;

// FIXME: GC_API
void GC_init_finalized_malloc(void)
{
    DCL_LOCK_STATE;

    if (done_init) // FIXME: Is race possible here?
        return;
    GC_init(); // FIXME: Portable client should always do GC_INIT() itself

    LOCK();
    if (done_init)
        goto done; // FIXME: avoid "goto" if possible
    done_init = 1;

    GC_finalized_objfreelist = (ptr_t *)GC_new_free_list_inner();
    GC_finalized_kind =
        GC_new_kind_inner((void **)GC_finalized_objfreelist,
                          0 | GC_DS_LENGTH,
                          TRUE, TRUE);
    GC_register_disclaim_proc(GC_finalized_kind, GC_finalized_disclaim, 0, 1);

done:
    UNLOCK();
}

void * GC_clear_stack(); // FIXME: remove as declared in gc_priv.h

#ifdef THREAD_LOCAL_ALLOC
  STATIC void * GC_core_finalized_malloc(size_t lb,
                                         struct GC_finalizer_closure *fclos)
#else
// FIXME: add GC_API, GC_CALL
  void * GC_finalized_malloc(size_t lb, struct GC_finalizer_closure *fclos)
#endif
{
    register ptr_t op;
    register ptr_t *opp;
    DCL_LOCK_STATE;

    lb += sizeof(void *);
    if (!done_init) // FIXME: Probably GC_ASSERT is adequate here?
        ABORT("You must call GC_init_finalize_malloc before using "
              "GC_malloc_with_finalizer.");
    if (EXPECT(SMALL_OBJ(lb), 1)) {
        register word lg;
        lg = GC_size_map[lb];
        opp = &GC_finalized_objfreelist[lg];
        LOCK();
        if (EXPECT((op = *opp) == 0, 0)) {
            UNLOCK();
            op = GC_generic_malloc((word)lb, GC_finalized_kind);
        } else {
            *opp = obj_link(op);
            obj_link(op) = 0;
            GC_bytes_allocd += GRANULES_TO_BYTES(lg);
            UNLOCK();
        }
    } else {
        op = GC_generic_malloc((word)lb, GC_finalized_kind);
    }
    *(void **)op = (ptr_t)fclos + 1; /* See [1] */
    return GC_clear_stack(op + sizeof(void *));
}

#ifdef THREAD_LOCAL_ALLOC
  // FIXME: GC_API, GC_CALL
  void * GC_finalized_malloc(size_t client_lb,
                             struct GC_finalizer_closure *fclos)
  {
    size_t lb = client_lb + sizeof(void *);
    size_t lg = ROUNDED_UP_GRANULES(lb);
    GC_tlfs tsd;
    void *result;
    void **tiny_fl, **my_fl, *my_entry;
    void *next;

    if (GC_EXPECT(lg >= GC_TINY_FREELISTS, 0))
        return GC_core_finalized_malloc(client_lb, fclos);

    tsd = GC_getspecific(GC_thread_key);
    tiny_fl = tsd->finalized_freelists;
    my_fl = tiny_fl + lg;
    my_entry = *my_fl;
    while (GC_EXPECT((word)my_entry
                     <= DIRECT_GRANULES + GC_TINY_FREELISTS + 1, 0)) {
        if ((word)my_entry - 1 < DIRECT_GRANULES) {
            *my_fl = (ptr_t)my_entry + lg + 1;
            return GC_core_finalized_malloc(client_lb, fclos);
        } else {
            GC_generic_malloc_many(GC_RAW_BYTES_FROM_INDEX(lg),
                                   GC_finalized_kind, my_fl);
            my_entry = *my_fl;
            if (my_entry == 0)
                return GC_oom_fn(lb);
        }
    }
    next = obj_link(my_entry);
    result = (void *)my_entry;
    *my_fl = next;
    *(void **)result = (ptr_t)fclos + 1;
    PREFETCH_FOR_WRITE(next);
    return (void **)result + 1;
  }
#endif /* THREAD_LOCAL_ALLOC */
