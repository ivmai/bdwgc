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

#include "private/gc_priv.h"

#ifdef ENABLE_DISCLAIM

#include "gc_disclaim.h"

#ifdef THREAD_LOCAL_ALLOC
# include "private/thread_local_alloc.h"
#else
  STATIC ptr_t * GC_finalized_objfreelist = NULL;
#endif /* !THREAD_LOCAL_ALLOC */

STATIC int GC_finalized_kind = 0;

STATIC int GC_CALLBACK GC_finalized_disclaim(void *obj,
                                             void *cd GC_ATTR_UNUSED)
{
    struct GC_finalizer_closure *fc = *(void **)obj;
    if (((word)fc & 1) != 0) {
       /* [1] The disclaim function may be passed fragments from the    */
       /* free-list, on which it should not run finalization.           */
       /* To recognize this case, we use the fact that the first word   */
       /* on such fragments are always even (a link to the next         */
       /* fragment, or NULL).  If it is desirable to have a finalizer   */
       /* which does not use the first word for storing finalization    */
       /* info, GC_reclaim_with_finalization must be extended to clear  */
       /* fragments so that the assumption holds for the selected word. */
        fc = (void *)((word)fc & ~(word)1);
        (*fc->proc)((void **)obj + 1, fc->cd);
    }
    return 0;
}

static GC_bool done_init = FALSE;

GC_API void GC_CALL GC_init_finalized_malloc(void)
{
    DCL_LOCK_STATE;

    GC_init();  /* In case it's not already done.       */
    LOCK();
    if (done_init) {
        UNLOCK();
        return;
    }
    done_init = TRUE;

    GC_finalized_objfreelist = (ptr_t *)GC_new_free_list_inner();
    GC_finalized_kind = GC_new_kind_inner((void **)GC_finalized_objfreelist,
                                          GC_DS_LENGTH, TRUE, TRUE);
    GC_register_disclaim_proc(GC_finalized_kind, GC_finalized_disclaim,
                              NULL, TRUE);
    UNLOCK();
}

#ifdef THREAD_LOCAL_ALLOC
  STATIC void * GC_core_finalized_malloc(size_t lb,
                                         struct GC_finalizer_closure *fclos)
#else
  GC_API void * GC_CALL GC_finalized_malloc(size_t lb,
                                         struct GC_finalizer_closure *fclos)
#endif
{
    ptr_t op;
    ptr_t *opp;
    word lg;
    DCL_LOCK_STATE;

    lb += sizeof(void *);
    GC_ASSERT(done_init);
    if (EXPECT(SMALL_OBJ(lb), 1)) {
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
  GC_API void * GC_CALL GC_finalized_malloc(size_t client_lb,
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
            if (my_entry == 0) {
                GC_oom_func oom_fn;

                LOCK();
                oom_fn = GC_oom_fn;
                UNLOCK();
                return((*oom_fn)(lb));
            }
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

#endif /* ENABLE_DISCLAIM */
