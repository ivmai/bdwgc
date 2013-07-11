/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1996 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996-1999 by Silicon Graphics.  All rights reserved.
 * Copyright (C) 2007 Free Software Foundation, Inc

 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/gc_pmark.h"

#ifndef GC_NO_FINALIZATION

/* Type of mark procedure used for marking from finalizable object.     */
/* This procedure normally does not mark the object, only its           */
/* descendants.                                                         */
typedef void (* finalization_mark_proc)(ptr_t /* finalizable_obj_ptr */);

#define HASH3(addr,size,log_size) \
        ((((word)(addr) >> 3) ^ ((word)(addr) >> (3 + (log_size)))) \
         & ((size) - 1))
#define HASH2(addr,log_size) HASH3(addr, 1 << (log_size), log_size)

struct hash_chain_entry {
    word hidden_key;
    struct hash_chain_entry * next;
};

struct disappearing_link {
    struct hash_chain_entry prolog;
#   define dl_hidden_link prolog.hidden_key
                                /* Field to be cleared.         */
#   define dl_next(x) (struct disappearing_link *)((x) -> prolog.next)
#   define dl_set_next(x, y) \
                (void)((x)->prolog.next = (struct hash_chain_entry *)(y))
    word dl_hidden_obj;         /* Pointer to object base       */
};

struct dl_hashtbl_s {
    struct disappearing_link **head;
    signed_word log_size;
    word entries;
};

STATIC struct dl_hashtbl_s GC_dl_hashtbl = {
    /* head */ NULL, /* log_size */ -1, /* entries */ 0 };
#ifndef GC_LONG_REFS_NOT_NEEDED
  STATIC struct dl_hashtbl_s GC_ll_hashtbl = { NULL, -1, 0 };
#endif

STATIC struct finalizable_object {
    struct hash_chain_entry prolog;
#   define fo_hidden_base prolog.hidden_key
                                /* Pointer to object base.      */
                                /* No longer hidden once object */
                                /* is on finalize_now queue.    */
#   define fo_next(x) (struct finalizable_object *)((x) -> prolog.next)
#   define fo_set_next(x,y) ((x)->prolog.next = (struct hash_chain_entry *)(y))
    GC_finalization_proc fo_fn; /* Finalizer.                   */
    ptr_t fo_client_data;
    word fo_object_size;        /* In bytes.                    */
    finalization_mark_proc fo_mark_proc;        /* Mark-through procedure */
} **GC_fo_head = 0;

STATIC struct finalizable_object * GC_finalize_now = 0;
        /* List of objects that should be finalized now.        */

static signed_word log_fo_table_size = -1;

GC_INNER void GC_push_finalizer_structures(void)
{
    GC_ASSERT((word)&GC_dl_hashtbl.head % sizeof(word) == 0);
    GC_ASSERT((word)&GC_fo_head % sizeof(word) == 0);
    GC_ASSERT((word)&GC_finalize_now % sizeof(word) == 0);

# ifndef GC_LONG_REFS_NOT_NEEDED
    GC_ASSERT((word)&GC_ll_hashtbl.head % sizeof(word) == 0);
    GC_push_all((ptr_t)(&GC_ll_hashtbl.head),
                (ptr_t)(&GC_ll_hashtbl.head) + sizeof(word));
# endif

    GC_push_all((ptr_t)(&GC_dl_hashtbl.head),
                (ptr_t)(&GC_dl_hashtbl.head) + sizeof(word));
    GC_push_all((ptr_t)(&GC_fo_head), (ptr_t)(&GC_fo_head) + sizeof(word));
    GC_push_all((ptr_t)(&GC_finalize_now),
                (ptr_t)(&GC_finalize_now) + sizeof(word));
}

/* Double the size of a hash table. *size_ptr is the log of its current */
/* size.  May be a no-op.                                               */
/* *table is a pointer to an array of hash headers.  If we succeed, we  */
/* update both *table and *log_size_ptr.  Lock is held.                 */
STATIC void GC_grow_table(struct hash_chain_entry ***table,
                          signed_word *log_size_ptr)
{
    register word i;
    register struct hash_chain_entry *p;
    signed_word log_old_size = *log_size_ptr;
    signed_word log_new_size = log_old_size + 1;
    word old_size = ((log_old_size == -1)? 0: (1 << log_old_size));
    word new_size = (word)1 << log_new_size;
    /* FIXME: Power of 2 size often gets rounded up to one more page. */
    struct hash_chain_entry **new_table = (struct hash_chain_entry **)
        GC_INTERNAL_MALLOC_IGNORE_OFF_PAGE(
                (size_t)new_size * sizeof(struct hash_chain_entry *), NORMAL);

    if (new_table == 0) {
        if (*table == 0) {
            ABORT("Insufficient space for initial table allocation");
        } else {
            return;
        }
    }
    for (i = 0; i < old_size; i++) {
      p = (*table)[i];
      while (p != 0) {
        ptr_t real_key = GC_REVEAL_POINTER(p -> hidden_key);
        struct hash_chain_entry *next = p -> next;
        size_t new_hash = HASH3(real_key, new_size, log_new_size);

        p -> next = new_table[new_hash];
        new_table[new_hash] = p;
        p = next;
      }
    }
    *log_size_ptr = log_new_size;
    *table = new_table;
}

GC_API int GC_CALL GC_register_disappearing_link(void * * link)
{
    ptr_t base;

    base = (ptr_t)GC_base(link);
    if (base == 0)
        ABORT("Bad arg to GC_register_disappearing_link");
    return(GC_general_register_disappearing_link(link, base));
}

STATIC int GC_register_disappearing_link_inner(
                        struct dl_hashtbl_s *dl_hashtbl, void **link,
                        const void *obj)
{
    struct disappearing_link *curr_dl;
    size_t index;
    struct disappearing_link * new_dl;
    DCL_LOCK_STATE;

    LOCK();
    GC_ASSERT(obj != NULL && GC_base_C(obj) == obj);
    if (dl_hashtbl -> log_size == -1
        || dl_hashtbl -> entries > ((word)1 << dl_hashtbl -> log_size)) {
        GC_grow_table((struct hash_chain_entry ***)&dl_hashtbl -> head,
                      &dl_hashtbl -> log_size);
        GC_COND_LOG_PRINTF("Grew dl table to %u entries\n",
                           1 << (unsigned)dl_hashtbl -> log_size);
    }
    index = HASH2(link, dl_hashtbl -> log_size);
    for (curr_dl = dl_hashtbl -> head[index]; curr_dl != 0;
         curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == GC_HIDE_POINTER(link)) {
            curr_dl -> dl_hidden_obj = GC_HIDE_POINTER(obj);
            UNLOCK();
            return GC_DUPLICATE;
        }
    }
    new_dl = (struct disappearing_link *)
        GC_INTERNAL_MALLOC(sizeof(struct disappearing_link),NORMAL);
    if (0 == new_dl) {
      GC_oom_func oom_fn = GC_oom_fn;
      UNLOCK();
      new_dl = (struct disappearing_link *)
                (*oom_fn)(sizeof(struct disappearing_link));
      if (0 == new_dl) {
        return GC_NO_MEMORY;
      }
      /* It's not likely we'll make it here, but ... */
      LOCK();
      /* Recalculate index since the table may grow.    */
      index = HASH2(link, dl_hashtbl -> log_size);
      /* Check again that our disappearing link not in the table. */
      for (curr_dl = dl_hashtbl -> head[index]; curr_dl != 0;
           curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == GC_HIDE_POINTER(link)) {
          curr_dl -> dl_hidden_obj = GC_HIDE_POINTER(obj);
          UNLOCK();
#         ifndef DBG_HDRS_ALL
            /* Free unused new_dl returned by GC_oom_fn() */
            GC_free((void *)new_dl);
#         endif
          return GC_DUPLICATE;
        }
      }
    }
    new_dl -> dl_hidden_obj = GC_HIDE_POINTER(obj);
    new_dl -> dl_hidden_link = GC_HIDE_POINTER(link);
    dl_set_next(new_dl, dl_hashtbl -> head[index]);
    dl_hashtbl -> head[index] = new_dl;
    dl_hashtbl -> entries++;
    UNLOCK();
    return GC_SUCCESS;
}

GC_API int GC_CALL GC_general_register_disappearing_link(void * * link,
                                                         const void * obj)
{
    if (((word)link & (ALIGNMENT-1)) != 0 || NULL == link)
        ABORT("Bad arg to GC_general_register_disappearing_link");
    return GC_register_disappearing_link_inner(&GC_dl_hashtbl, link, obj);
}

#ifdef DBG_HDRS_ALL
# define FREE_DL_ENTRY(curr_dl) dl_set_next(curr_dl, NULL)
#else
# define FREE_DL_ENTRY(curr_dl) GC_free(curr_dl)
#endif

/* Unregisters given link and returns the link entry to free.   */
/* Assume the lock is held.                                     */
GC_INLINE struct disappearing_link *GC_unregister_disappearing_link_inner(
                                struct dl_hashtbl_s *dl_hashtbl, void **link)
{
    struct disappearing_link *curr_dl;
    struct disappearing_link *prev_dl = NULL;
    size_t index = HASH2(link, dl_hashtbl->log_size);

    for (curr_dl = dl_hashtbl -> head[index]; curr_dl;
         curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == GC_HIDE_POINTER(link)) {
            /* Remove found entry from the table. */
            if (NULL == prev_dl) {
                dl_hashtbl -> head[index] = dl_next(curr_dl);
            } else {
                dl_set_next(prev_dl, dl_next(curr_dl));
            }
            dl_hashtbl -> entries--;
            break;
        }
        prev_dl = curr_dl;
    }
    return curr_dl;
}

GC_API int GC_CALL GC_unregister_disappearing_link(void * * link)
{
    struct disappearing_link *curr_dl;
    DCL_LOCK_STATE;

    if (((word)link & (ALIGNMENT-1)) != 0) return(0); /* Nothing to do. */

    LOCK();
    curr_dl = GC_unregister_disappearing_link_inner(&GC_dl_hashtbl, link);
    UNLOCK();
    if (NULL == curr_dl) return 0;
    FREE_DL_ENTRY(curr_dl);
    return 1;
}

#ifndef GC_LONG_REFS_NOT_NEEDED
  GC_API int GC_CALL GC_register_long_link(void * * link, const void * obj)
  {
    if (((word)link & (ALIGNMENT-1)) != 0 || NULL == link)
        ABORT("Bad arg to GC_register_long_link");
    return GC_register_disappearing_link_inner(&GC_ll_hashtbl, link, obj);
  }

  GC_API int GC_CALL GC_unregister_long_link(void * * link)
  {
    struct disappearing_link *curr_dl;
    DCL_LOCK_STATE;

    if (((word)link & (ALIGNMENT-1)) != 0) return(0); /* Nothing to do. */

    LOCK();
    curr_dl = GC_unregister_disappearing_link_inner(&GC_ll_hashtbl, link);
    UNLOCK();
    if (NULL == curr_dl) return 0;
    FREE_DL_ENTRY(curr_dl);
    return 1;
  }
#endif /* !GC_LONG_REFS_NOT_NEEDED */

#ifndef GC_MOVE_DISAPPEARING_LINK_NOT_NEEDED
  /* Moves a link.  Assume the lock is held.    */
  STATIC int GC_move_disappearing_link_inner(
                                struct dl_hashtbl_s *dl_hashtbl,
                                void **link, void **new_link)
  {
    struct disappearing_link *curr_dl, *prev_dl, *new_dl;
    size_t curr_index, new_index;
    word curr_hidden_link;
    word new_hidden_link;

    /* Find current link.       */
    curr_index = HASH2(link, dl_hashtbl -> log_size);
    curr_hidden_link = GC_HIDE_POINTER(link);
    prev_dl = NULL;
    for (curr_dl = dl_hashtbl -> head[curr_index]; curr_dl;
         curr_dl = dl_next(curr_dl)) {
      if (curr_dl -> dl_hidden_link == curr_hidden_link)
        break;
      prev_dl = curr_dl;
    }

    if (NULL == curr_dl) {
      return GC_NOT_FOUND;
    }

    if (link == new_link) {
      return GC_SUCCESS; /* Nothing to do.      */
    }

    /* link found; now check new_link not present.      */
    new_index = HASH2(new_link, dl_hashtbl -> log_size);
    new_hidden_link = GC_HIDE_POINTER(new_link);
    for (new_dl = dl_hashtbl -> head[new_index]; new_dl;
         new_dl = dl_next(new_dl)) {
      if (new_dl -> dl_hidden_link == new_hidden_link) {
        /* Target already registered; bail.     */
        return GC_DUPLICATE;
      }
    }

    /* Remove from old, add to new, update link.        */
    if (NULL == prev_dl) {
      dl_hashtbl -> head[curr_index] = dl_next(curr_dl);
    } else {
      dl_set_next(prev_dl, dl_next(curr_dl));
    }
    curr_dl -> dl_hidden_link = new_hidden_link;
    dl_set_next(curr_dl, dl_hashtbl -> head[new_index]);
    dl_hashtbl -> head[new_index] = curr_dl;
    return GC_SUCCESS;
  }

  GC_API int GC_CALL GC_move_disappearing_link(void **link, void **new_link)
  {
    int result;
    DCL_LOCK_STATE;

    if (((word)new_link & (ALIGNMENT-1)) != 0 || new_link == NULL)
      ABORT("Bad new_link arg to GC_move_disappearing_link");
    if (((word)link & (ALIGNMENT-1)) != 0)
      return GC_NOT_FOUND; /* Nothing to do. */

    LOCK();
    result = GC_move_disappearing_link_inner(&GC_dl_hashtbl, link, new_link);
    UNLOCK();
    return result;
  }

# ifndef GC_LONG_REFS_NOT_NEEDED
    GC_API int GC_CALL GC_move_long_link(void **link, void **new_link)
    {
      int result;
      DCL_LOCK_STATE;

      if (((word)new_link & (ALIGNMENT-1)) != 0 || new_link == NULL)
        ABORT("Bad new_link arg to GC_move_disappearing_link");
      if (((word)link & (ALIGNMENT-1)) != 0)
        return GC_NOT_FOUND; /* Nothing to do. */

      LOCK();
      result = GC_move_disappearing_link_inner(&GC_ll_hashtbl, link, new_link);
      UNLOCK();
      return result;
    }
# endif /* !GC_LONG_REFS_NOT_NEEDED */
#endif /* !GC_MOVE_DISAPPEARING_LINK_NOT_NEEDED */

/* Possible finalization_marker procedures.  Note that mark stack       */
/* overflow is handled by the caller, and is not a disaster.            */
STATIC void GC_normal_finalize_mark_proc(ptr_t p)
{
    hdr * hhdr = HDR(p);

    PUSH_OBJ(p, hhdr, GC_mark_stack_top,
             &(GC_mark_stack[GC_mark_stack_size]));
}

/* This only pays very partial attention to the mark descriptor.        */
/* It does the right thing for normal and atomic objects, and treats    */
/* most others as normal.                                               */
STATIC void GC_ignore_self_finalize_mark_proc(ptr_t p)
{
    hdr * hhdr = HDR(p);
    word descr = hhdr -> hb_descr;
    ptr_t q;
    word r;
    ptr_t scan_limit;
    ptr_t target_limit = p + hhdr -> hb_sz - 1;

    if ((descr & GC_DS_TAGS) == GC_DS_LENGTH) {
       scan_limit = p + descr - sizeof(word);
    } else {
       scan_limit = target_limit + 1 - sizeof(word);
    }
    for (q = p; (word)q <= (word)scan_limit; q += ALIGNMENT) {
        r = *(word *)q;
        if (r < (word)p || r > (word)target_limit) {
            GC_PUSH_ONE_HEAP(r, q, GC_mark_stack_top);
        }
    }
}

STATIC void GC_null_finalize_mark_proc(ptr_t p GC_ATTR_UNUSED) {}

/* Possible finalization_marker procedures.  Note that mark stack       */
/* overflow is handled by the caller, and is not a disaster.            */

/* GC_unreachable_finalize_mark_proc is an alias for normal marking,    */
/* but it is explicitly tested for, and triggers different              */
/* behavior.  Objects registered in this way are not finalized          */
/* if they are reachable by other finalizable objects, even if those    */
/* other objects specify no ordering.                                   */
STATIC void GC_unreachable_finalize_mark_proc(ptr_t p)
{
    GC_normal_finalize_mark_proc(p);
}

/* Register a finalization function.  See gc.h for details.     */
/* The last parameter is a procedure that determines            */
/* marking for finalization ordering.  Any objects marked       */
/* by that procedure will be guaranteed to not have been        */
/* finalized when this finalizer is invoked.                    */
STATIC void GC_register_finalizer_inner(void * obj,
                                        GC_finalization_proc fn, void *cd,
                                        GC_finalization_proc *ofn, void **ocd,
                                        finalization_mark_proc mp)
{
    ptr_t base;
    struct finalizable_object * curr_fo, * prev_fo;
    size_t index;
    struct finalizable_object *new_fo = 0;
    hdr *hhdr = NULL; /* initialized to prevent warning. */
    GC_oom_func oom_fn;
    DCL_LOCK_STATE;

    LOCK();
    if (log_fo_table_size == -1
        || GC_fo_entries > ((word)1 << log_fo_table_size)) {
        GC_grow_table((struct hash_chain_entry ***)&GC_fo_head,
                      &log_fo_table_size);
        GC_COND_LOG_PRINTF("Grew fo table to %u entries\n",
                           1 << (unsigned)log_fo_table_size);
    }
    /* in the THREADS case we hold allocation lock.             */
    base = (ptr_t)obj;
    for (;;) {
      index = HASH2(base, log_fo_table_size);
      prev_fo = 0;
      curr_fo = GC_fo_head[index];
      while (curr_fo != 0) {
        GC_ASSERT(GC_size(curr_fo) >= sizeof(struct finalizable_object));
        if (curr_fo -> fo_hidden_base == GC_HIDE_POINTER(base)) {
          /* Interruption by a signal in the middle of this     */
          /* should be safe.  The client may see only *ocd      */
          /* updated, but we'll declare that to be his problem. */
          if (ocd) *ocd = (void *) (curr_fo -> fo_client_data);
          if (ofn) *ofn = curr_fo -> fo_fn;
          /* Delete the structure for base. */
          if (prev_fo == 0) {
            GC_fo_head[index] = fo_next(curr_fo);
          } else {
            fo_set_next(prev_fo, fo_next(curr_fo));
          }
          if (fn == 0) {
            GC_fo_entries--;
            /* May not happen if we get a signal.  But a high   */
            /* estimate will only make the table larger than    */
            /* necessary.                                       */
#           if !defined(THREADS) && !defined(DBG_HDRS_ALL)
              GC_free((void *)curr_fo);
#           endif
          } else {
            curr_fo -> fo_fn = fn;
            curr_fo -> fo_client_data = (ptr_t)cd;
            curr_fo -> fo_mark_proc = mp;
            /* Reinsert it.  We deleted it first to maintain    */
            /* consistency in the event of a signal.            */
            if (prev_fo == 0) {
              GC_fo_head[index] = curr_fo;
            } else {
              fo_set_next(prev_fo, curr_fo);
            }
          }
          UNLOCK();
#         ifndef DBG_HDRS_ALL
            if (EXPECT(new_fo != 0, FALSE)) {
              /* Free unused new_fo returned by GC_oom_fn() */
              GC_free((void *)new_fo);
            }
#         endif
          return;
        }
        prev_fo = curr_fo;
        curr_fo = fo_next(curr_fo);
      }
      if (EXPECT(new_fo != 0, FALSE)) {
        /* new_fo is returned by GC_oom_fn(), so fn != 0 and hhdr != 0. */
        break;
      }
      if (fn == 0) {
        if (ocd) *ocd = 0;
        if (ofn) *ofn = 0;
        UNLOCK();
        return;
      }
      GET_HDR(base, hhdr);
      if (EXPECT(0 == hhdr, FALSE)) {
        /* We won't collect it, hence finalizer wouldn't be run. */
        if (ocd) *ocd = 0;
        if (ofn) *ofn = 0;
        UNLOCK();
        return;
      }
      new_fo = (struct finalizable_object *)
        GC_INTERNAL_MALLOC(sizeof(struct finalizable_object),NORMAL);
      if (EXPECT(new_fo != 0, TRUE))
        break;
      oom_fn = GC_oom_fn;
      UNLOCK();
      new_fo = (struct finalizable_object *)
                (*oom_fn)(sizeof(struct finalizable_object));
      if (0 == new_fo) {
        /* No enough memory.  *ocd and *ofn remains unchanged.  */
        return;
      }
      /* It's not likely we'll make it here, but ... */
      LOCK();
      /* Recalculate index since the table may grow and         */
      /* check again that our finalizer is not in the table.    */
    }
    GC_ASSERT(GC_size(new_fo) >= sizeof(struct finalizable_object));
    if (ocd) *ocd = 0;
    if (ofn) *ofn = 0;
    new_fo -> fo_hidden_base = GC_HIDE_POINTER(base);
    new_fo -> fo_fn = fn;
    new_fo -> fo_client_data = (ptr_t)cd;
    new_fo -> fo_object_size = hhdr -> hb_sz;
    new_fo -> fo_mark_proc = mp;
    fo_set_next(new_fo, GC_fo_head[index]);
    GC_fo_entries++;
    GC_fo_head[index] = new_fo;
    UNLOCK();
}

GC_API void GC_CALL GC_register_finalizer(void * obj,
                                  GC_finalization_proc fn, void * cd,
                                  GC_finalization_proc *ofn, void ** ocd)
{
    GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, GC_normal_finalize_mark_proc);
}

GC_API void GC_CALL GC_register_finalizer_ignore_self(void * obj,
                               GC_finalization_proc fn, void * cd,
                               GC_finalization_proc *ofn, void ** ocd)
{
    GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, GC_ignore_self_finalize_mark_proc);
}

GC_API void GC_CALL GC_register_finalizer_no_order(void * obj,
                               GC_finalization_proc fn, void * cd,
                               GC_finalization_proc *ofn, void ** ocd)
{
    GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, GC_null_finalize_mark_proc);
}

static GC_bool need_unreachable_finalization = FALSE;
        /* Avoid the work if this isn't used.   */

GC_API void GC_CALL GC_register_finalizer_unreachable(void * obj,
                               GC_finalization_proc fn, void * cd,
                               GC_finalization_proc *ofn, void ** ocd)
{
    need_unreachable_finalization = TRUE;
    GC_ASSERT(GC_java_finalization);
    GC_register_finalizer_inner(obj, fn, cd, ofn,
                                ocd, GC_unreachable_finalize_mark_proc);
}

#ifndef NO_DEBUGGING
  STATIC void GC_dump_finalization_links(
                                const struct dl_hashtbl_s *dl_hashtbl)
  {
    struct disappearing_link *curr_dl;
    ptr_t real_ptr, real_link;
    size_t dl_size = dl_hashtbl->log_size == -1 ? 0 :
                                1 << dl_hashtbl->log_size;
    size_t i;

    for (i = 0; i < dl_size; i++) {
      for (curr_dl = dl_hashtbl -> head[i]; curr_dl != 0;
           curr_dl = dl_next(curr_dl)) {
        real_ptr = GC_REVEAL_POINTER(curr_dl -> dl_hidden_obj);
        real_link = GC_REVEAL_POINTER(curr_dl -> dl_hidden_link);
        GC_printf("Object: %p, link: %p\n", real_ptr, real_link);
      }
    }
  }

  void GC_dump_finalization(void)
  {
    struct finalizable_object * curr_fo;
    size_t fo_size = log_fo_table_size == -1 ? 0 : 1 << log_fo_table_size;
    ptr_t real_ptr;
    size_t i;

    GC_printf("Disappearing (short) links:\n");
    GC_dump_finalization_links(&GC_dl_hashtbl);
#   ifndef GC_LONG_REFS_NOT_NEEDED
      GC_printf("Disappearing long links:\n");
      GC_dump_finalization_links(&GC_ll_hashtbl);
#   endif
    GC_printf("Finalizers:\n");
    for (i = 0; i < fo_size; i++) {
      for (curr_fo = GC_fo_head[i]; curr_fo != 0;
           curr_fo = fo_next(curr_fo)) {
        real_ptr = GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
        GC_printf("Finalizable object: %p\n", real_ptr);
      }
    }
  }
#endif /* !NO_DEBUGGING */

#ifndef SMALL_CONFIG
  STATIC word GC_old_dl_entries = 0; /* for stats printing */
# ifndef GC_LONG_REFS_NOT_NEEDED
    STATIC word GC_old_ll_entries = 0;
# endif
#endif /* !SMALL_CONFIG */

#ifndef THREADS
  /* Global variables to minimize the level of recursion when a client  */
  /* finalizer allocates memory.                                        */
  STATIC int GC_finalizer_nested = 0;
                        /* Only the lowest byte is used, the rest is    */
                        /* padding for proper global data alignment     */
                        /* required for some compilers (like Watcom).   */
  STATIC unsigned GC_finalizer_skipped = 0;

  /* Checks and updates the level of finalizers recursion.              */
  /* Returns NULL if GC_invoke_finalizers() should not be called by the */
  /* collector (to minimize the risk of a deep finalizers recursion),   */
  /* otherwise returns a pointer to GC_finalizer_nested.                */
  STATIC unsigned char *GC_check_finalizer_nested(void)
  {
    unsigned nesting_level = *(unsigned char *)&GC_finalizer_nested;
    if (nesting_level) {
      /* We are inside another GC_invoke_finalizers().          */
      /* Skip some implicitly-called GC_invoke_finalizers()     */
      /* depending on the nesting (recursion) level.            */
      if (++GC_finalizer_skipped < (1U << nesting_level)) return NULL;
      GC_finalizer_skipped = 0;
    }
    *(char *)&GC_finalizer_nested = (char)(nesting_level + 1);
    return (unsigned char *)&GC_finalizer_nested;
  }
#endif /* THREADS */

#define ITERATE_DL_HASHTBL_BEGIN(dl_hashtbl, curr_dl, prev_dl) \
  { \
    size_t i; \
    size_t dl_size = dl_hashtbl->log_size == -1 ? 0 : \
                                1 << dl_hashtbl->log_size; \
    for (i = 0; i < dl_size; i++) { \
      curr_dl = dl_hashtbl -> head[i]; \
      prev_dl = NULL; \
      while (curr_dl) {

#define ITERATE_DL_HASHTBL_END(curr_dl, prev_dl) \
        prev_dl = curr_dl; \
        curr_dl = dl_next(curr_dl); \
      } \
    } \
  }

#define DELETE_DL_HASHTBL_ENTRY(dl_hashtbl, curr_dl, prev_dl, next_dl) \
  { \
    next_dl = dl_next(curr_dl); \
    if (NULL == prev_dl) { \
        dl_hashtbl -> head[i] = next_dl; \
    } else { \
        dl_set_next(prev_dl, next_dl); \
    } \
    GC_clear_mark_bit(curr_dl); \
    dl_hashtbl -> entries--; \
    curr_dl = next_dl; \
    continue; \
  }

GC_INLINE void GC_make_disappearing_links_disappear(
                                struct dl_hashtbl_s* dl_hashtbl)
{
    struct disappearing_link *curr, *prev, *next;
    ptr_t real_ptr, real_link;

    ITERATE_DL_HASHTBL_BEGIN(dl_hashtbl, curr, prev)
        real_ptr = GC_REVEAL_POINTER(curr -> dl_hidden_obj);
        real_link = GC_REVEAL_POINTER(curr -> dl_hidden_link);
        if (!GC_is_marked(real_ptr)) {
            *(word *)real_link = 0;
            GC_clear_mark_bit(curr);
            DELETE_DL_HASHTBL_ENTRY(dl_hashtbl, curr, prev, next);
        }
    ITERATE_DL_HASHTBL_END(curr, prev)
}

GC_INLINE void GC_remove_dangling_disappearing_links(
                                struct dl_hashtbl_s* dl_hashtbl)
{
    struct disappearing_link *curr, *prev, *next;
    ptr_t real_link;

    ITERATE_DL_HASHTBL_BEGIN(dl_hashtbl, curr, prev)
        real_link = GC_base(GC_REVEAL_POINTER(curr -> dl_hidden_link));
        if (NULL != real_link && !GC_is_marked(real_link)) {
            GC_clear_mark_bit(curr);
            DELETE_DL_HASHTBL_ENTRY(dl_hashtbl, curr, prev, next);
        }
    ITERATE_DL_HASHTBL_END(curr, prev)
}

/* Called with held lock (but the world is running).                    */
/* Cause disappearing links to disappear and unreachable objects to be  */
/* enqueued for finalization.                                           */
GC_INNER void GC_finalize(void)
{
    struct finalizable_object * curr_fo, * prev_fo, * next_fo;
    ptr_t real_ptr;
    size_t i;
    size_t fo_size = log_fo_table_size == -1 ? 0 : 1 << log_fo_table_size;

#   ifndef SMALL_CONFIG
      /* Save current GC_[dl/ll]_entries value for stats printing */
      GC_old_dl_entries = GC_dl_hashtbl.entries;
#     ifndef GC_LONG_REFS_NOT_NEEDED
        GC_old_ll_entries = GC_ll_hashtbl.entries;
#     endif
#   endif

    GC_make_disappearing_links_disappear(&GC_dl_hashtbl);

  /* Mark all objects reachable via chains of 1 or more pointers        */
  /* from finalizable objects.                                          */
    GC_ASSERT(GC_mark_state == MS_NONE);
    for (i = 0; i < fo_size; i++) {
      for (curr_fo = GC_fo_head[i]; curr_fo != 0;
           curr_fo = fo_next(curr_fo)) {
        GC_ASSERT(GC_size(curr_fo) >= sizeof(struct finalizable_object));
        real_ptr = GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!GC_is_marked(real_ptr)) {
            GC_MARKED_FOR_FINALIZATION(real_ptr);
            GC_MARK_FO(real_ptr, curr_fo -> fo_mark_proc);
            if (GC_is_marked(real_ptr)) {
                WARN("Finalization cycle involving %p\n", real_ptr);
            }
        }
      }
    }
  /* Enqueue for finalization all objects that are still                */
  /* unreachable.                                                       */
    GC_bytes_finalized = 0;
    for (i = 0; i < fo_size; i++) {
      curr_fo = GC_fo_head[i];
      prev_fo = 0;
      while (curr_fo != 0) {
        real_ptr = GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!GC_is_marked(real_ptr)) {
            if (!GC_java_finalization) {
              GC_set_mark_bit(real_ptr);
            }
            /* Delete from hash table */
              next_fo = fo_next(curr_fo);
              if (prev_fo == 0) {
                GC_fo_head[i] = next_fo;
              } else {
                fo_set_next(prev_fo, next_fo);
              }
              GC_fo_entries--;
            /* Add to list of objects awaiting finalization.    */
              fo_set_next(curr_fo, GC_finalize_now);
              GC_finalize_now = curr_fo;
              /* unhide object pointer so any future collections will   */
              /* see it.                                                */
              curr_fo -> fo_hidden_base =
                        (word)GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
              GC_bytes_finalized +=
                        curr_fo -> fo_object_size
                        + sizeof(struct finalizable_object);
            GC_ASSERT(GC_is_marked(GC_base(curr_fo)));
            curr_fo = next_fo;
        } else {
            prev_fo = curr_fo;
            curr_fo = fo_next(curr_fo);
        }
      }
    }

  if (GC_java_finalization) {
    /* make sure we mark everything reachable from objects finalized
       using the no_order mark_proc */
      for (curr_fo = GC_finalize_now;
         curr_fo != NULL; curr_fo = fo_next(curr_fo)) {
        real_ptr = (ptr_t)curr_fo -> fo_hidden_base;
        if (!GC_is_marked(real_ptr)) {
            if (curr_fo -> fo_mark_proc == GC_null_finalize_mark_proc) {
                GC_MARK_FO(real_ptr, GC_normal_finalize_mark_proc);
            }
            if (curr_fo -> fo_mark_proc != GC_unreachable_finalize_mark_proc) {
                GC_set_mark_bit(real_ptr);
            }
        }
      }

    /* now revive finalize-when-unreachable objects reachable from
       other finalizable objects */
      if (need_unreachable_finalization) {
        curr_fo = GC_finalize_now;
        prev_fo = 0;
        while (curr_fo != 0) {
          next_fo = fo_next(curr_fo);
          if (curr_fo -> fo_mark_proc == GC_unreachable_finalize_mark_proc) {
            real_ptr = (ptr_t)curr_fo -> fo_hidden_base;
            if (!GC_is_marked(real_ptr)) {
              GC_set_mark_bit(real_ptr);
            } else {
              if (prev_fo == 0)
                GC_finalize_now = next_fo;
              else
                fo_set_next(prev_fo, next_fo);

              curr_fo -> fo_hidden_base =
                                GC_HIDE_POINTER(curr_fo -> fo_hidden_base);
              GC_bytes_finalized -=
                  curr_fo->fo_object_size + sizeof(struct finalizable_object);

              i = HASH2(real_ptr, log_fo_table_size);
              fo_set_next (curr_fo, GC_fo_head[i]);
              GC_fo_entries++;
              GC_fo_head[i] = curr_fo;
              curr_fo = prev_fo;
            }
          }
          prev_fo = curr_fo;
          curr_fo = next_fo;
        }
      }
  }

  GC_remove_dangling_disappearing_links(&GC_dl_hashtbl);
# ifndef GC_LONG_REFS_NOT_NEEDED
    GC_make_disappearing_links_disappear(&GC_ll_hashtbl);
    GC_remove_dangling_disappearing_links(&GC_ll_hashtbl);
# endif

  if (GC_fail_count) {
    /* Don't prevent running finalizers if there has been an allocation */
    /* failure recently.                                                */
#   ifdef THREADS
      GC_reset_finalizer_nested();
#   else
      GC_finalizer_nested = 0;
#   endif
  }
}

#ifndef JAVA_FINALIZATION_NOT_NEEDED

  /* Enqueue all remaining finalizers to be run - Assumes lock is held. */
  STATIC void GC_enqueue_all_finalizers(void)
  {
    struct finalizable_object * curr_fo, * prev_fo, * next_fo;
    ptr_t real_ptr;
    register int i;
    int fo_size;

    fo_size = log_fo_table_size == -1 ? 0 : 1 << log_fo_table_size;
    GC_bytes_finalized = 0;
    for (i = 0; i < fo_size; i++) {
        curr_fo = GC_fo_head[i];
        prev_fo = 0;
      while (curr_fo != 0) {
          real_ptr = GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
          GC_MARK_FO(real_ptr, GC_normal_finalize_mark_proc);
          GC_set_mark_bit(real_ptr);

          /* Delete from hash table */
          next_fo = fo_next(curr_fo);
          if (prev_fo == 0) {
              GC_fo_head[i] = next_fo;
          } else {
              fo_set_next(prev_fo, next_fo);
          }
          GC_fo_entries--;

          /* Add to list of objects awaiting finalization.      */
          fo_set_next(curr_fo, GC_finalize_now);
          GC_finalize_now = curr_fo;

          /* unhide object pointer so any future collections will       */
          /* see it.                                                    */
          curr_fo -> fo_hidden_base =
                        (word)GC_REVEAL_POINTER(curr_fo -> fo_hidden_base);
          GC_bytes_finalized +=
                curr_fo -> fo_object_size + sizeof(struct finalizable_object);
          curr_fo = next_fo;
        }
    }
  }

  /* Invoke all remaining finalizers that haven't yet been run.
   * This is needed for strict compliance with the Java standard,
   * which can make the runtime guarantee that all finalizers are run.
   * Unfortunately, the Java standard implies we have to keep running
   * finalizers until there are no more left, a potential infinite loop.
   * YUCK.
   * Note that this is even more dangerous than the usual Java
   * finalizers, in that objects reachable from static variables
   * may have been finalized when these finalizers are run.
   * Finalizers run at this point must be prepared to deal with a
   * mostly broken world.
   * This routine is externally callable, so is called without
   * the allocation lock.
   */
  GC_API void GC_CALL GC_finalize_all(void)
  {
    DCL_LOCK_STATE;

    LOCK();
    while (GC_fo_entries > 0) {
      GC_enqueue_all_finalizers();
      UNLOCK();
      GC_invoke_finalizers();
      /* Running the finalizers in this thread is arguably not a good   */
      /* idea when we should be notifying another thread to run them.   */
      /* But otherwise we don't have a great way to wait for them to    */
      /* run.                                                           */
      LOCK();
    }
    UNLOCK();
  }

#endif /* !JAVA_FINALIZATION_NOT_NEEDED */

/* Returns true if it is worth calling GC_invoke_finalizers. (Useful if */
/* finalizers can only be called from some kind of "safe state" and     */
/* getting into that safe state is expensive.)                          */
GC_API int GC_CALL GC_should_invoke_finalizers(void)
{
    return GC_finalize_now != 0;
}

/* Invoke finalizers for all objects that are ready to be finalized.    */
/* Should be called without allocation lock.                            */
GC_API int GC_CALL GC_invoke_finalizers(void)
{
    struct finalizable_object * curr_fo;
    int count = 0;
    word bytes_freed_before = 0; /* initialized to prevent warning. */
    DCL_LOCK_STATE;

    while (GC_finalize_now != 0) {
#       ifdef THREADS
            LOCK();
#       endif
        if (count == 0) {
            bytes_freed_before = GC_bytes_freed;
            /* Don't do this outside, since we need the lock. */
        }
        curr_fo = GC_finalize_now;
#       ifdef THREADS
            if (curr_fo != 0) GC_finalize_now = fo_next(curr_fo);
            UNLOCK();
            if (curr_fo == 0) break;
#       else
            GC_finalize_now = fo_next(curr_fo);
#       endif
        fo_set_next(curr_fo, 0);
        (*(curr_fo -> fo_fn))((ptr_t)(curr_fo -> fo_hidden_base),
                              curr_fo -> fo_client_data);
        curr_fo -> fo_client_data = 0;
        ++count;
#       ifdef UNDEFINED
            /* This is probably a bad idea.  It throws off accounting if */
            /* nearly all objects are finalizable.  O.w. it shouldn't    */
            /* matter.                                                   */
            GC_free((void *)curr_fo);
#       endif
    }
    /* bytes_freed_before is initialized whenever count != 0 */
    if (count != 0 && bytes_freed_before != GC_bytes_freed) {
        LOCK();
        GC_finalizer_bytes_freed += (GC_bytes_freed - bytes_freed_before);
        UNLOCK();
    }
    return count;
}

static GC_word last_finalizer_notification = 0;

GC_INNER void GC_notify_or_invoke_finalizers(void)
{
    GC_finalizer_notifier_proc notifier_fn = 0;
#   if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
      static word last_back_trace_gc_no = 1;    /* Skip first one. */
#   endif
    DCL_LOCK_STATE;

#   if defined(THREADS) && !defined(KEEP_BACK_PTRS) \
       && !defined(MAKE_BACK_GRAPH)
      /* Quick check (while unlocked) for an empty finalization queue.  */
      if (GC_finalize_now == 0) return;
#   endif
    LOCK();

    /* This is a convenient place to generate backtraces if appropriate, */
    /* since that code is not callable with the allocation lock.         */
#   if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
      if (GC_gc_no > last_back_trace_gc_no) {
#       ifdef KEEP_BACK_PTRS
          long i;
          /* Stops when GC_gc_no wraps; that's OK.      */
          last_back_trace_gc_no = (word)(-1);  /* disable others. */
          for (i = 0; i < GC_backtraces; ++i) {
              /* FIXME: This tolerates concurrent heap mutation,        */
              /* which may cause occasional mysterious results.         */
              /* We need to release the GC lock, since GC_print_callers */
              /* acquires it.  It probably shouldn't.                   */
              UNLOCK();
              GC_generate_random_backtrace_no_gc();
              LOCK();
          }
          last_back_trace_gc_no = GC_gc_no;
#       endif
#       ifdef MAKE_BACK_GRAPH
          if (GC_print_back_height) {
            UNLOCK();
            GC_print_back_graph_stats();
            LOCK();
          }
#       endif
      }
#   endif
    if (GC_finalize_now == 0) {
      UNLOCK();
      return;
    }

    if (!GC_finalize_on_demand) {
      unsigned char *pnested = GC_check_finalizer_nested();
      UNLOCK();
      /* Skip GC_invoke_finalizers() if nested */
      if (pnested != NULL) {
        (void) GC_invoke_finalizers();
        *pnested = 0; /* Reset since no more finalizers. */
#       ifndef THREADS
          GC_ASSERT(GC_finalize_now == 0);
#       endif   /* Otherwise GC can run concurrently and add more */
      }
      return;
    }

    /* These variables require synchronization to avoid data races.     */
    if (last_finalizer_notification != GC_gc_no) {
        last_finalizer_notification = GC_gc_no;
        notifier_fn = GC_finalizer_notifier;
    }
    UNLOCK();
    if (notifier_fn != 0)
        (*notifier_fn)(); /* Invoke the notifier */
}

#ifndef SMALL_CONFIG
# ifndef GC_LONG_REFS_NOT_NEEDED
#   define IF_LONG_REFS_PRESENT_ELSE(x,y) (x)
# else
#   define IF_LONG_REFS_PRESENT_ELSE(x,y) (y)
# endif

  GC_INNER void GC_print_finalization_stats(void)
  {
    struct finalizable_object *fo;
    unsigned long ready = 0;

    GC_log_printf("%lu finalization entries;"
                  " %lu/%lu short/long disappearing links alive\n",
                  (unsigned long)GC_fo_entries,
                  (unsigned long)GC_dl_hashtbl.entries,
                  (unsigned long)IF_LONG_REFS_PRESENT_ELSE(
                                                GC_ll_hashtbl.entries, 0));

    for (fo = GC_finalize_now; 0 != fo; fo = fo_next(fo))
      ++ready;
    GC_log_printf("%lu finalization-ready objects;"
                  " %ld/%ld short/long links cleared\n",
                  ready,
                  (long)GC_old_dl_entries - (long)GC_dl_hashtbl.entries,
                  (long)IF_LONG_REFS_PRESENT_ELSE(
                              GC_old_ll_entries - GC_ll_hashtbl.entries, 0));
  }
#endif /* !SMALL_CONFIG */

#endif /* !GC_NO_FINALIZATION */
