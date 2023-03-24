/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 2000 by Hewlett-Packard Company.  All rights reserved.
 * Copyright (c) 2009-2022 Ivan Maidanski
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/gc_priv.h"

/*
 * These are extra allocation routines which are likely to be less
 * frequently used than those in malloc.c.  They are separate in the
 * hope that the .o file will be excluded from statically linked
 * executables.  We should probably break this up further.
 */

#include <string.h>

#ifndef MSWINCE
# include <errno.h>
#endif

/* Some externally visible but unadvertised variables to allow access to */
/* free lists from inlined allocators without including gc_priv.h        */
/* or introducing dependencies on internal data structure layouts.       */
#include "private/gc_alloc_ptrs.h"
void ** const GC_objfreelist_ptr = GC_objfreelist;
void ** const GC_aobjfreelist_ptr = GC_aobjfreelist;
void ** const GC_uobjfreelist_ptr = GC_uobjfreelist;
# ifdef GC_ATOMIC_UNCOLLECTABLE
    void ** const GC_auobjfreelist_ptr = GC_auobjfreelist;
# endif

GC_API int GC_CALL GC_get_kind_and_size(const void * p, size_t * psize)
{
    hdr * hhdr = HDR(p);

    if (psize != NULL) {
        *psize = (size_t)hhdr->hb_sz;
    }
    return hhdr -> hb_obj_kind;
}

GC_API GC_ATTR_MALLOC void * GC_CALL GC_generic_or_special_malloc(size_t lb,
                                                                  int k)
{
    switch (k) {
        case PTRFREE:
        case NORMAL:
            return GC_malloc_kind(lb, k);
        case UNCOLLECTABLE:
#       ifdef GC_ATOMIC_UNCOLLECTABLE
          case AUNCOLLECTABLE:
#       endif
            return GC_generic_malloc_uncollectable(lb, k);
        default:
            return GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0);
    }
}

/* Change the size of the block pointed to by p to contain at least   */
/* lb bytes.  The object may be (and quite likely will be) moved.     */
/* The kind (e.g. atomic) is the same as that of the old.             */
/* Shrinking of large blocks is not implemented well.                 */
GC_API void * GC_CALL GC_realloc(void * p, size_t lb)
{
    struct hblk * h;
    hdr * hhdr;
    void * result;
#   if defined(_FORTIFY_SOURCE) && defined(__GNUC__) && !defined(__clang__)
      volatile  /* Use cleared_p instead of p as a workaround to avoid  */
                /* passing alloc_size(lb) attribute associated with p   */
                /* to memset (including memset call inside GC_free).    */
#   endif
      word cleared_p = (word)p;
    size_t sz;      /* Current size in bytes    */
    size_t orig_sz; /* Original sz in bytes     */
    int obj_kind;

    if (NULL == p) return GC_malloc(lb);  /* Required by ANSI */
    if (0 == lb) /* and p != NULL */ {
#     ifndef IGNORE_FREE
        GC_free(p);
#     endif
      return NULL;
    }
    h = HBLKPTR(p);
    hhdr = HDR(h);
    sz = (size_t)hhdr->hb_sz;
    obj_kind = hhdr -> hb_obj_kind;
    orig_sz = sz;

    if (sz > MAXOBJBYTES) {
        /* Round it up to the next whole heap block */
        word descr = GC_obj_kinds[obj_kind].ok_descriptor;

        sz = (sz + HBLKSIZE-1) & ~(HBLKSIZE-1);
        if (GC_obj_kinds[obj_kind].ok_relocate_descr)
          descr += sz;
        /* GC_realloc might be changing the block size while            */
        /* GC_reclaim_block or GC_clear_hdr_marks is examining it.      */
        /* The change to the size field is benign, in that GC_reclaim   */
        /* (and GC_clear_hdr_marks) would work correctly with either    */
        /* value, since we are not changing the number of objects in    */
        /* the block.  But seeing a half-updated value (though unlikely */
        /* to occur in practice) could be probably bad.                 */
        /* Using unordered atomic accesses on the size and hb_descr     */
        /* fields would solve the issue.  (The alternate solution might */
        /* be to initially overallocate large objects, so we do not     */
        /* have to adjust the size in GC_realloc, if they still fit.    */
        /* But that is probably more expensive, since we may end up     */
        /* scanning a bunch of zeros during GC.)                        */
#       ifdef AO_HAVE_store
          GC_STATIC_ASSERT(sizeof(hhdr->hb_sz) == sizeof(AO_t));
          AO_store((volatile AO_t *)&hhdr->hb_sz, (AO_t)sz);
          AO_store((volatile AO_t *)&hhdr->hb_descr, (AO_t)descr);
#       else
          {
            LOCK();
            hhdr -> hb_sz = sz;
            hhdr -> hb_descr = descr;
            UNLOCK();
          }
#       endif

#         ifdef MARK_BIT_PER_OBJ
            GC_ASSERT(hhdr -> hb_inv_sz == LARGE_INV_SZ);
#         endif
#         ifdef MARK_BIT_PER_GRANULE
            GC_ASSERT((hhdr -> hb_flags & LARGE_BLOCK) != 0
                        && hhdr -> hb_map[ANY_INDEX] == 1);
#         endif
          if (IS_UNCOLLECTABLE(obj_kind)) GC_non_gc_bytes += (sz - orig_sz);
          /* Extra area is already cleared by GC_alloc_large_and_clear. */
    }
    if (ADD_EXTRA_BYTES(lb) <= sz) {
        if (lb >= (sz >> 1)) {
            if (orig_sz > lb) {
              /* Clear unneeded part of object to avoid bogus pointer */
              /* tracing.                                             */
                BZERO((ptr_t)cleared_p + lb, orig_sz - lb);
            }
            return p;
        }
        /* shrink */
        sz = lb;
    }
    result = GC_generic_or_special_malloc((word)lb, obj_kind);
    if (EXPECT(result != NULL, TRUE)) {
      /* In case of shrink, it could also return original object.       */
      /* But this gives the client warning of imminent disaster.        */
      BCOPY(p, result, sz);
#     ifndef IGNORE_FREE
        GC_free((ptr_t)cleared_p);
#     endif
    }
    return result;
}

# if defined(REDIRECT_MALLOC) && !defined(REDIRECT_REALLOC)
#   define REDIRECT_REALLOC GC_realloc
# endif

# ifdef REDIRECT_REALLOC

/* As with malloc, avoid two levels of extra calls here.        */
# define GC_debug_realloc_replacement(p, lb) \
        GC_debug_realloc(p, lb, GC_DBG_EXTRAS)

# if !defined(REDIRECT_MALLOC_IN_HEADER)
    void * realloc(void * p, size_t lb)
    {
      return REDIRECT_REALLOC(p, lb);
    }
# endif

# undef GC_debug_realloc_replacement
# endif /* REDIRECT_REALLOC */

/* Allocate memory such that only pointers to near the          */
/* beginning of the object are considered.                      */
/* We avoid holding allocation lock while we clear the memory.  */
GC_API GC_ATTR_MALLOC void * GC_CALL
    GC_generic_malloc_ignore_off_page(size_t lb, int k)
{
  return GC_generic_malloc_aligned(lb, k, IGNORE_OFF_PAGE, 0 /* align_m1 */);
}

GC_API GC_ATTR_MALLOC void * GC_CALL GC_malloc_ignore_off_page(size_t lb)
{
    return GC_generic_malloc_aligned(lb, NORMAL, IGNORE_OFF_PAGE, 0);
}

GC_API GC_ATTR_MALLOC void * GC_CALL
    GC_malloc_atomic_ignore_off_page(size_t lb)
{
    return GC_generic_malloc_aligned(lb, PTRFREE, IGNORE_OFF_PAGE, 0);
}

/* Increment GC_bytes_allocd from code that doesn't have direct access  */
/* to GC_arrays.                                                        */
void GC_CALL GC_incr_bytes_allocd(size_t n)
{
    GC_bytes_allocd += n;
}

/* The same for GC_bytes_freed.                         */
void GC_CALL GC_incr_bytes_freed(size_t n)
{
    GC_bytes_freed += n;
}

GC_API size_t GC_CALL GC_get_expl_freed_bytes_since_gc(void)
{
    return (size_t)GC_bytes_freed;
}

# ifdef PARALLEL_MARK
    STATIC volatile AO_t GC_bytes_allocd_tmp = 0;
                        /* Number of bytes of memory allocated since    */
                        /* we released the GC lock.  Instead of         */
                        /* reacquiring the GC lock just to add this in, */
                        /* we add it in the next time we reacquire      */
                        /* the lock.  (Atomically adding it doesn't     */
                        /* work, since we would have to atomically      */
                        /* update it in GC_malloc, which is too         */
                        /* expensive.)                                  */
# endif /* PARALLEL_MARK */

/* Return a list of 1 or more objects of the indicated size, linked     */
/* through the first word in the object.  This has the advantage that   */
/* it acquires the allocation lock only once, and may greatly reduce    */
/* time wasted contending for the allocation lock.  Typical usage would */
/* be in a thread that requires many items of the same size.  It would  */
/* keep its own free list in thread-local storage, and call             */
/* GC_malloc_many or friends to replenish it.  (We do not round up      */
/* object sizes, since a call indicates the intention to consume many   */
/* objects of exactly this size.)                                       */
/* We assume that the size is a multiple of GRANULE_BYTES.              */
/* We return the free-list by assigning it to *result, since it is      */
/* not safe to return, e.g. a linked list of pointer-free objects,      */
/* since the collector would not retain the entire list if it were      */
/* invoked just as we were returning.                                   */
/* Note that the client should usually clear the link field.            */
GC_API void GC_CALL GC_generic_malloc_many(size_t lb, int k, void **result)
{
    void *op;
    void *p;
    void **opp;
    size_t lw;      /* Length in words.     */
    size_t lg;      /* Length in granules.  */
    signed_word my_bytes_allocd = 0;
    struct obj_kind * ok = &(GC_obj_kinds[k]);
    struct hblk ** rlh;

    GC_ASSERT(lb != 0 && (lb & (GRANULE_BYTES-1)) == 0);
    /* Currently a single object is always allocated if manual VDB. */
    /* TODO: GC_dirty should be called for each linked object (but  */
    /* the last one) to support multiple objects allocation.        */
    if (!SMALL_OBJ(lb) || GC_manual_vdb) {
        op = GC_generic_malloc_aligned(lb, k, 0 /* flags */, 0 /* align_m1 */);
        if (EXPECT(0 != op, TRUE))
            obj_link(op) = 0;
        *result = op;
#       ifndef GC_DISABLE_INCREMENTAL
          if (GC_manual_vdb && GC_is_heap_ptr(result)) {
            GC_dirty_inner(result);
            REACHABLE_AFTER_DIRTY(op);
          }
#       endif
        return;
    }
    GC_ASSERT(k < MAXOBJKINDS);
    lw = BYTES_TO_WORDS(lb);
    lg = BYTES_TO_GRANULES(lb);
    if (EXPECT(get_have_errors(), FALSE))
      GC_print_all_errors();
    GC_INVOKE_FINALIZERS();
    GC_DBG_COLLECT_AT_MALLOC(lb);
    if (!EXPECT(GC_is_initialized, TRUE)) GC_init();
    LOCK();
    /* Do our share of marking work */
      if (GC_incremental && !GC_dont_gc) {
        ENTER_GC();
        GC_collect_a_little_inner(1);
        EXIT_GC();
      }
    /* First see if we can reclaim a page of objects waiting to be */
    /* reclaimed.                                                  */
    rlh = ok -> ok_reclaim_list;
    if (rlh != NULL) {
        struct hblk * hbp;
        hdr * hhdr;

        while ((hbp = rlh[lg]) != NULL) {
            hhdr = HDR(hbp);
            rlh[lg] = hhdr -> hb_next;
            GC_ASSERT(hhdr -> hb_sz == lb);
            hhdr -> hb_last_reclaimed = (unsigned short) GC_gc_no;
#           ifdef PARALLEL_MARK
              if (GC_parallel) {
                  signed_word my_bytes_allocd_tmp =
                                (signed_word)AO_load(&GC_bytes_allocd_tmp);
                  GC_ASSERT(my_bytes_allocd_tmp >= 0);
                  /* We only decrement it while holding the GC lock.    */
                  /* Thus we can't accidentally adjust it down in more  */
                  /* than one thread simultaneously.                    */

                  if (my_bytes_allocd_tmp != 0) {
                    (void)AO_fetch_and_add(&GC_bytes_allocd_tmp,
                                           (AO_t)(-my_bytes_allocd_tmp));
                    GC_bytes_allocd += my_bytes_allocd_tmp;
                  }
                  GC_acquire_mark_lock();
                  ++ GC_fl_builder_count;
                  UNLOCK();
                  GC_release_mark_lock();
              }
#           endif
            op = GC_reclaim_generic(hbp, hhdr, lb,
                                    ok -> ok_init, 0, &my_bytes_allocd);
            if (op != 0) {
#             ifdef PARALLEL_MARK
                if (GC_parallel) {
                  *result = op;
                  (void)AO_fetch_and_add(&GC_bytes_allocd_tmp,
                                         (AO_t)my_bytes_allocd);
                  GC_acquire_mark_lock();
                  -- GC_fl_builder_count;
                  if (GC_fl_builder_count == 0) GC_notify_all_builder();
#                 ifdef THREAD_SANITIZER
                    GC_release_mark_lock();
                    LOCK();
                    GC_bytes_found += my_bytes_allocd;
                    UNLOCK();
#                 else
                    GC_bytes_found += my_bytes_allocd;
                                        /* The result may be inaccurate. */
                    GC_release_mark_lock();
#                 endif
                  (void) GC_clear_stack(0);
                  return;
                }
#             endif
              /* We also reclaimed memory, so we need to adjust       */
              /* that count.                                          */
              GC_bytes_found += my_bytes_allocd;
              GC_bytes_allocd += my_bytes_allocd;
              goto out;
            }
#           ifdef PARALLEL_MARK
              if (GC_parallel) {
                GC_acquire_mark_lock();
                -- GC_fl_builder_count;
                if (GC_fl_builder_count == 0) GC_notify_all_builder();
                GC_release_mark_lock();
                LOCK();
                /* The GC lock is needed for reclaim list access.  We   */
                /* must decrement fl_builder_count before reacquiring   */
                /* the lock.  Hopefully this path is rare.              */

                rlh = ok -> ok_reclaim_list; /* reload rlh after locking */
                if (NULL == rlh) break;
              }
#           endif
        }
    }
    /* Next try to use prefix of global free list if there is one.      */
    /* We don't refill it, but we need to use it up before allocating   */
    /* a new block ourselves.                                           */
      opp = &(GC_obj_kinds[k].ok_freelist[lg]);
      if ( (op = *opp) != 0 ) {
        *opp = 0;
        my_bytes_allocd = 0;
        for (p = op; p != 0; p = obj_link(p)) {
          my_bytes_allocd += lb;
          if ((word)my_bytes_allocd >= HBLKSIZE) {
            *opp = obj_link(p);
            obj_link(p) = 0;
            break;
          }
        }
        GC_bytes_allocd += my_bytes_allocd;
        goto out;
      }
    /* Next try to allocate a new block worth of objects of this size.  */
    {
        struct hblk *h = GC_allochblk(lb, k, 0 /* flags */, 0 /* align_m1 */);

        if (h /* != NULL */) { /* CPPCHECK */
          if (IS_UNCOLLECTABLE(k)) GC_set_hdr_marks(HDR(h));
          GC_bytes_allocd += HBLKSIZE - HBLKSIZE % lb;
#         ifdef PARALLEL_MARK
            if (GC_parallel) {
              GC_acquire_mark_lock();
              ++ GC_fl_builder_count;
              UNLOCK();
              GC_release_mark_lock();

              op = GC_build_fl(h, lw,
                        (ok -> ok_init || GC_debugging_started), 0);

              *result = op;
              GC_acquire_mark_lock();
              -- GC_fl_builder_count;
              if (GC_fl_builder_count == 0) GC_notify_all_builder();
              GC_release_mark_lock();
              (void) GC_clear_stack(0);
              return;
            }
#         endif
          op = GC_build_fl(h, lw, (ok -> ok_init || GC_debugging_started), 0);
          goto out;
        }
    }

    /* As a last attempt, try allocating a single object.  Note that    */
    /* this may trigger a collection or expand the heap.                */
      op = GC_generic_malloc_inner(lb, k, 0 /* flags */);
      if (op != NULL) obj_link(op) = NULL;

  out:
    *result = op;
    UNLOCK();
    (void) GC_clear_stack(0);
}

/* Note that the "atomic" version of this would be unsafe, since the    */
/* links would not be seen by the collector.                            */
GC_API GC_ATTR_MALLOC void * GC_CALL GC_malloc_many(size_t lb)
{
    void *result;
    size_t lg = ALLOC_REQUEST_GRANS(lb);

    GC_generic_malloc_many(GRANULES_TO_BYTES(lg), NORMAL, &result);
    return result;
}

/* TODO: The debugging version of GC_memalign and friends is tricky     */
/* and currently missing.  There are 2 major difficulties:              */
/* - GC_base() should always point to the beginning of the allocated    */
/* block (thus, for small objects allocation we should probably         */
/* iterate over the list of free objects to find the one with the       */
/* suitable alignment);                                                 */
/* - store_debug_info() should return the pointer of the object with    */
/* the requested alignment (unlike the object header).                  */

GC_API GC_ATTR_MALLOC void * GC_CALL GC_memalign(size_t align, size_t lb)
{
    size_t offset;
    ptr_t result;
    size_t align_m1 = align - 1;

    /* Check the alignment argument.    */
    if (align < sizeof(void *) || (align & align_m1) != 0) return NULL;

    if (align <= GRANULE_BYTES) return GC_malloc(lb);

    if (align >= HBLKSIZE/2 || lb >= HBLKSIZE/2) {
      return GC_clear_stack(GC_generic_malloc_aligned(lb, NORMAL,
                                        0 /* flags */, align_m1));
    }

    /* We could also try to make sure that the real rounded-up object size */
    /* is a multiple of align.  That would be correct up to HBLKSIZE.      */
    /* TODO: Not space efficient for big align values. */
    result = (ptr_t)GC_malloc(SIZET_SAT_ADD(lb, align_m1));
            /* It is OK not to check result for NULL as in that case    */
            /* GC_memalign returns NULL too since (0 + 0 % align) is 0. */
    offset = (size_t)(word)result & align_m1;
    if (offset != 0) {
        offset = align - offset;
        if (!GC_all_interior_pointers) {
            GC_STATIC_ASSERT(VALID_OFFSET_SZ <= HBLKSIZE);
            GC_ASSERT(offset < VALID_OFFSET_SZ);
            GC_register_displacement(offset);
        }
        result += offset;
    }
    GC_ASSERT(((word)result & align_m1) == 0);
    return result;
}

/* This one exists largely to redirect posix_memalign for leaks finding. */
GC_API int GC_CALL GC_posix_memalign(void **memptr, size_t align, size_t lb)
{
  /* Check alignment properly.  */
  size_t align_minus_one = align - 1; /* to workaround a cppcheck warning */
  if (align < sizeof(void *) || (align_minus_one & align) != 0) {
#   ifdef MSWINCE
      return ERROR_INVALID_PARAMETER;
#   else
      return EINVAL;
#   endif
  }

  if ((*memptr = GC_memalign(align, lb)) == NULL) {
#   ifdef MSWINCE
      return ERROR_NOT_ENOUGH_MEMORY;
#   else
      return ENOMEM;
#   endif
  }
  return 0;
}

#ifndef GC_NO_VALLOC
  GC_API GC_ATTR_MALLOC void * GC_CALL GC_valloc(size_t lb)
  {
    if (!EXPECT(GC_is_initialized, TRUE)) GC_init();
    GC_ASSERT(GC_real_page_size != 0);
    return GC_memalign(GC_real_page_size, lb);
  }

  GC_API GC_ATTR_MALLOC void * GC_CALL GC_pvalloc(size_t lb)
  {
    if (!EXPECT(GC_is_initialized, TRUE)) GC_init();
    GC_ASSERT(GC_real_page_size != 0);
    lb = SIZET_SAT_ADD(lb, GC_real_page_size - 1) & ~(GC_real_page_size - 1);
    return GC_memalign(GC_real_page_size, lb);
  }
#endif /* !GC_NO_VALLOC */

/* Provide a version of strdup() that uses the collector to allocate    */
/* the copy of the string.                                              */
GC_API GC_ATTR_MALLOC char * GC_CALL GC_strdup(const char *s)
{
  char *copy;
  size_t lb;
  if (s == NULL) return NULL;
  lb = strlen(s) + 1;
  copy = (char *)GC_malloc_atomic(lb);
  if (EXPECT(NULL == copy, FALSE)) {
#   ifndef MSWINCE
      errno = ENOMEM;
#   endif
    return NULL;
  }
  BCOPY(s, copy, lb);
  return copy;
}

GC_API GC_ATTR_MALLOC char * GC_CALL GC_strndup(const char *str, size_t size)
{
  char *copy;
  size_t len = strlen(str); /* str is expected to be non-NULL  */
  if (EXPECT(len > size, FALSE))
    len = size;
  copy = (char *)GC_malloc_atomic(len + 1);
  if (EXPECT(NULL == copy, FALSE)) {
#   ifndef MSWINCE
      errno = ENOMEM;
#   endif
    return NULL;
  }
  if (EXPECT(len > 0, TRUE))
    BCOPY(str, copy, len);
  copy[len] = '\0';
  return copy;
}

#ifdef GC_REQUIRE_WCSDUP
# include <wchar.h> /* for wcslen() */

  GC_API GC_ATTR_MALLOC wchar_t * GC_CALL GC_wcsdup(const wchar_t *str)
  {
    size_t lb = (wcslen(str) + 1) * sizeof(wchar_t);
    wchar_t *copy = (wchar_t *)GC_malloc_atomic(lb);

    if (EXPECT(NULL == copy, FALSE)) {
#     ifndef MSWINCE
        errno = ENOMEM;
#     endif
      return NULL;
    }
    BCOPY(str, copy, lb);
    return copy;
  }
#endif /* GC_REQUIRE_WCSDUP */

#ifndef CPPCHECK
  GC_API void * GC_CALL GC_malloc_stubborn(size_t lb)
  {
    return GC_malloc(lb);
  }

  GC_API void GC_CALL GC_change_stubborn(const void *p)
  {
    UNUSED_ARG(p);
  }
#endif /* !CPPCHECK */

GC_API void GC_CALL GC_end_stubborn_change(const void *p)
{
  GC_dirty(p); /* entire object */
}

GC_API void GC_CALL GC_ptr_store_and_dirty(void *p, const void *q)
{
  *(const void **)p = q;
  GC_dirty(p);
  REACHABLE_AFTER_DIRTY(q);
}
