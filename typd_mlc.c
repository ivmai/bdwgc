/*
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1999-2000 by Hewlett-Packard Company.  All rights reserved.
 * Copyright (c) 2008-2022 Ivan Maidanski
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */

#include "private/gc_pmark.h"

/*
 * Some simple primitives for allocation with explicit type information.
 * Simple objects are allocated such that they contain a GC_descr at the
 * end (in the last allocated word).  This descriptor may be a procedure
 * which then examines an extended descriptor passed as its environment.
 *
 * Arrays are treated as simple objects if they have sufficiently simple
 * structure.  Otherwise they are allocated from an array kind that supplies
 * a special mark procedure.  These arrays contain a pointer to a
 * complex_descriptor as their last word.
 * This is done because the environment field is too small, and the collector
 * must trace the complex_descriptor.
 *
 * Note that descriptors inside objects may appear cleared, if we encounter a
 * false reference to an object on a free list.  In the GC_descr case, this
 * is OK, since a 0 descriptor corresponds to examining no fields.
 * In the complex_descriptor case, we explicitly check for that case.
 *
 * MAJOR PARTS OF THIS CODE HAVE NOT BEEN TESTED AT ALL and are not testable,
 * since they are not accessible through the current interface.
 */

#include "gc/gc_typed.h"

#define TYPD_EXTRA_BYTES (sizeof(word) - EXTRA_BYTES)

STATIC int GC_explicit_kind = 0;
                        /* Object kind for objects with indirect        */
                        /* (possibly extended) descriptors.             */

STATIC int GC_array_kind = 0;
                        /* Object kind for objects with complex         */
                        /* descriptors and GC_array_mark_proc.          */

#define ED_INITIAL_SIZE 100

STATIC int GC_typed_mark_proc_index = 0; /* Indices of my mark          */
STATIC int GC_array_mark_proc_index = 0; /* procedures.                 */

STATIC void GC_push_typed_structures_proc(void)
{
  GC_PUSH_ALL_SYM(GC_ext_descriptors);
}

/* Add a multiword bitmap to GC_ext_descriptors arrays.         */
/* Returns starting index on success, -1 otherwise.             */
STATIC signed_word GC_add_ext_descriptor(const word * bm, word nbits)
{
    size_t nwords = divWORDSZ(nbits + WORDSZ-1);
    signed_word result;
    size_t i;

    LOCK();
    while (EXPECT(GC_avail_descr + nwords >= GC_ed_size, FALSE)) {
        typed_ext_descr_t *newExtD;
        size_t new_size;
        word ed_size = GC_ed_size;

        if (ed_size == 0) {
            GC_ASSERT((word)(&GC_ext_descriptors) % sizeof(word) == 0);
            GC_push_typed_structures = GC_push_typed_structures_proc;
            UNLOCK();
            new_size = ED_INITIAL_SIZE;
        } else {
            UNLOCK();
            new_size = 2 * ed_size;
            if (new_size > MAX_ENV) return -1;
        }
        newExtD = (typed_ext_descr_t*)GC_malloc_atomic(new_size
                                                * sizeof(typed_ext_descr_t));
        if (NULL == newExtD)
            return -1;
        LOCK();
        if (ed_size == GC_ed_size) {
            if (GC_avail_descr != 0) {
                BCOPY(GC_ext_descriptors, newExtD,
                      GC_avail_descr * sizeof(typed_ext_descr_t));
            }
            GC_ed_size = new_size;
            GC_ext_descriptors = newExtD;
        }  /* else another thread already resized it in the meantime */
    }
    result = GC_avail_descr;
    for (i = 0; i < nwords-1; i++) {
        GC_ext_descriptors[result + i].ed_bitmap = bm[i];
        GC_ext_descriptors[result + i].ed_continued = TRUE;
    }
    /* Clear irrelevant (highest) bits for the last element.    */
    GC_ext_descriptors[result + i].ed_bitmap =
                bm[i] & (GC_WORD_MAX >> (nwords * WORDSZ - nbits));
    GC_ext_descriptors[result + i].ed_continued = FALSE;
    GC_avail_descr += nwords;
    UNLOCK();
    return result;
}

/* Table of bitmap descriptors for n word long all pointer objects.     */
STATIC GC_descr GC_bm_table[WORDSZ/2];

/* Return a descriptor for the concatenation of 2 nwords long objects,  */
/* each of which is described by descriptor d.  The result is known     */
/* to be short enough to fit into a bitmap descriptor.                  */
/* d is a GC_DS_LENGTH or GC_DS_BITMAP descriptor.                      */
STATIC GC_descr GC_double_descr(GC_descr d, size_t nwords)
{
    GC_ASSERT(GC_bm_table[0] == GC_DS_BITMAP); /* bm table is initialized */
    if ((d & GC_DS_TAGS) == GC_DS_LENGTH) {
        d = GC_bm_table[BYTES_TO_WORDS((word)d)];
    }
    d |= (d & ~GC_DS_TAGS) >> nwords;
    return d;
}

STATIC mse * GC_typed_mark_proc(word * addr, mse * mark_stack_ptr,
                                mse * mark_stack_limit, word env);

STATIC mse * GC_array_mark_proc(word * addr, mse * mark_stack_ptr,
                                mse * mark_stack_limit, word env);

STATIC void GC_init_explicit_typing(void)
{
    unsigned i;

    /* Set up object kind with simple indirect descriptor.      */
    /* Descriptor is in the last word of the object.            */
    GC_typed_mark_proc_index = GC_new_proc_inner(GC_typed_mark_proc);
    GC_explicit_kind = GC_new_kind_inner(GC_new_free_list_inner(),
                            (WORDS_TO_BYTES((word)-1) | GC_DS_PER_OBJECT),
                            TRUE, TRUE);

    /* Set up object kind with array descriptor. */
    GC_array_mark_proc_index = GC_new_proc_inner(GC_array_mark_proc);
    GC_array_kind = GC_new_kind_inner(GC_new_free_list_inner(),
                            GC_MAKE_PROC(GC_array_mark_proc_index, 0),
                            FALSE, TRUE);

    GC_bm_table[0] = GC_DS_BITMAP;
    for (i = 1; i < WORDSZ/2; i++) {
      GC_bm_table[i] = (((word)-1) << (WORDSZ - i)) | GC_DS_BITMAP;
    }
}

STATIC mse * GC_typed_mark_proc(word * addr, mse * mark_stack_ptr,
                                mse * mark_stack_limit, word env)
{
    word bm;
    ptr_t current_p = (ptr_t)addr;
    ptr_t greatest_ha = (ptr_t)GC_greatest_plausible_heap_addr;
    ptr_t least_ha = (ptr_t)GC_least_plausible_heap_addr;
    DECLARE_HDR_CACHE;

    /* The allocation lock is held by the collection initiating thread. */
    GC_ASSERT(GC_get_parallel() || I_HOLD_LOCK());
    bm = GC_ext_descriptors[env].ed_bitmap;

    INIT_HDR_CACHE;
    for (; bm != 0; bm >>= 1, current_p += sizeof(word)) {
        if (bm & 1) {
            word current;

            LOAD_WORD_OR_CONTINUE(current, current_p);
            FIXUP_POINTER(current);
            if (current >= (word)least_ha && current <= (word)greatest_ha) {
                PUSH_CONTENTS((ptr_t)current, mark_stack_ptr,
                              mark_stack_limit, current_p);
            }
        }
    }
    if (GC_ext_descriptors[env].ed_continued) {
        /* Push an entry with the rest of the descriptor back onto the  */
        /* stack.  Thus we never do too much work at once.  Note that   */
        /* we also can't overflow the mark stack unless we actually     */
        /* mark something.                                              */
        mark_stack_ptr++;
        if ((word)mark_stack_ptr >= (word)mark_stack_limit) {
            mark_stack_ptr = GC_signal_mark_stack_overflow(mark_stack_ptr);
        }
        mark_stack_ptr -> mse_start = (ptr_t)(addr + WORDSZ);
        mark_stack_ptr -> mse_descr.w =
                        GC_MAKE_PROC(GC_typed_mark_proc_index, env + 1);
    }
    return mark_stack_ptr;
}

GC_API GC_descr GC_CALL GC_make_descriptor(const GC_word * bm, size_t len)
{
    signed_word last_set_bit = (signed_word)len - 1;
    GC_descr d;

#   if defined(AO_HAVE_load_acquire) && defined(AO_HAVE_store_release)
      if (!EXPECT(AO_load_acquire(&GC_explicit_typing_initialized), TRUE)) {
        LOCK();
        if (!GC_explicit_typing_initialized) {
          GC_init_explicit_typing();
          AO_store_release(&GC_explicit_typing_initialized, TRUE);
        }
        UNLOCK();
      }
#   else
      LOCK();
      if (!EXPECT(GC_explicit_typing_initialized, TRUE)) {
        GC_init_explicit_typing();
        GC_explicit_typing_initialized = TRUE;
      }
      UNLOCK();
#   endif

    while (last_set_bit >= 0 && !GC_get_bit(bm, last_set_bit))
      last_set_bit--;
    if (last_set_bit < 0) return 0; /* no pointers */

#   if ALIGNMENT == CPP_WORDSZ/8
    {
      signed_word i;

      for (i = 0; i < last_set_bit; i++) {
        if (!GC_get_bit(bm, i)) {
          break;
        }
      }
      if (i == last_set_bit) {
        /* An initial section contains all pointers.  Use length descriptor. */
        return WORDS_TO_BYTES(last_set_bit + 1) | GC_DS_LENGTH;
      }
    }
#   endif
    if ((word)last_set_bit < BITMAP_BITS) {
        signed_word i;

        /* Hopefully the common case.                   */
        /* Build bitmap descriptor (with bits reversed) */
        d = SIGNB;
        for (i = last_set_bit - 1; i >= 0; i--) {
            d >>= 1;
            if (GC_get_bit(bm, i)) d |= SIGNB;
        }
        d |= GC_DS_BITMAP;
    } else {
        signed_word index = GC_add_ext_descriptor(bm, (word)last_set_bit + 1);

        if (EXPECT(index == -1, FALSE)) {
            /* Out of memory: use a conservative approximation. */
            return WORDS_TO_BYTES(last_set_bit + 1) | GC_DS_LENGTH;
        }
        d = GC_MAKE_PROC(GC_typed_mark_proc_index, (word)index);
    }
    return d;
}

#ifdef AO_HAVE_store_release
# define set_obj_descr(op, nwords, d) \
        AO_store_release((volatile AO_t *)(op) + (nwords) - 1, (AO_t)(d))
#else
# define set_obj_descr(op, nwords, d) \
        (void)(((word *)(op))[(nwords) - 1] = (word)(d))
#endif

GC_API GC_ATTR_MALLOC void * GC_CALL GC_malloc_explicitly_typed(size_t lb,
                                                                GC_descr d)
{
    void *op;
    size_t nwords;

    GC_ASSERT(GC_explicit_typing_initialized);
    if (EXPECT(0 == lb, FALSE)) lb = 1; /* ensure nwords > 1 */
    op = GC_malloc_kind(SIZET_SAT_ADD(lb, TYPD_EXTRA_BYTES), GC_explicit_kind);
    if (EXPECT(NULL == op, FALSE)) return NULL;

    /* It is not safe to use GC_size_map to compute nwords here as      */
    /* the former might be updated asynchronously.                      */
    nwords = GRANULES_TO_WORDS(BYTES_TO_GRANULES(GC_size(op)));
    set_obj_descr(op, nwords, d);
    GC_dirty((word *)op + nwords - 1);
    REACHABLE_AFTER_DIRTY(d);
    return op;
}

GC_API GC_ATTR_MALLOC void * GC_CALL
    GC_malloc_explicitly_typed_ignore_off_page(size_t lb, GC_descr d)
{
    void *op;
    size_t lb_adjusted = SIZET_SAT_ADD(lb, TYPD_EXTRA_BYTES);
    size_t nwords;

    if (SMALL_OBJ(lb_adjusted))
      return GC_malloc_explicitly_typed(lb, d);

    GC_ASSERT(GC_explicit_typing_initialized);
    op = GC_clear_stack(GC_generic_malloc_aligned(lb_adjusted,
                                                  GC_explicit_kind,
                                                  IGNORE_OFF_PAGE, 0));
    if (EXPECT(NULL == op, FALSE)) return NULL;

    nwords = GRANULES_TO_WORDS(BYTES_TO_GRANULES(GC_size(op)));
    set_obj_descr(op, nwords, d);
    GC_dirty((word *)op + nwords - 1);
    REACHABLE_AFTER_DIRTY(d);
    return op;
}

/* Array descriptors.  GC_array_mark_proc understands these.    */
/* We may eventually need to add provisions for headers and     */
/* trailers.  Hence we provide for tree structured descriptors, */
/* though we don't really use them currently.                   */

struct LeafDescriptor {         /* Describes simple array.      */
  word ld_tag;
# define LEAF_TAG 1
  word ld_size;                 /* Bytes per element; non-zero, */
                                /* multiple of ALIGNMENT.       */
  word ld_nelements;            /* Number of elements.          */
  GC_descr ld_descriptor;       /* A simple length, bitmap,     */
                                /* or procedure descriptor.     */
};

struct ComplexArrayDescriptor {
  word ad_tag;
# define ARRAY_TAG 2
  word ad_nelements;
  union ComplexDescriptor *ad_element_descr;
};

struct SequenceDescriptor {
  word sd_tag;
# define SEQUENCE_TAG 3
  union ComplexDescriptor *sd_first;
  union ComplexDescriptor *sd_second;
};

typedef union ComplexDescriptor {
  struct LeafDescriptor ld;
  struct ComplexArrayDescriptor ad;
  struct SequenceDescriptor sd;
} complex_descriptor;

STATIC complex_descriptor *GC_make_leaf_descriptor(word size, word nelements,
                                                   GC_descr d)
{
  complex_descriptor *result = (complex_descriptor *)
                GC_malloc_atomic(sizeof(struct LeafDescriptor));

  GC_ASSERT(size != 0);
  if (EXPECT(NULL == result, FALSE)) return NULL;

  result -> ld.ld_tag = LEAF_TAG;
  result -> ld.ld_size = size;
  result -> ld.ld_nelements = nelements;
  result -> ld.ld_descriptor = d;
  return result;
}

STATIC complex_descriptor *GC_make_sequence_descriptor(
                                                complex_descriptor *first,
                                                complex_descriptor *second)
{
  struct SequenceDescriptor *result = (struct SequenceDescriptor *)
                GC_malloc(sizeof(struct SequenceDescriptor));
                /* Note: for a reason, the sanitizer runtime complains  */
                /* of insufficient space for complex_descriptor if the  */
                /* pointer type of result variable is changed to.       */

  if (EXPECT(NULL == result, FALSE)) return NULL;

  /* Can't result in overly conservative marking, since tags are        */
  /* very small integers. Probably faster than maintaining type info.   */
  result -> sd_tag = SEQUENCE_TAG;
  result -> sd_first = first;
  result -> sd_second = second;
  GC_dirty(result);
  REACHABLE_AFTER_DIRTY(first);
  REACHABLE_AFTER_DIRTY(second);
  return (complex_descriptor *)result;
}

#define NO_MEM  (-1)
#define SIMPLE  0
#define LEAF    1
#define COMPLEX 2

/* Build a descriptor for an array with nelements elements, each of     */
/* which can be described by a simple descriptor d.  We try to optimize */
/* some common cases.  If the result is COMPLEX, a complex_descriptor*  */
/* value is returned in *pcomplex_d.  If the result is LEAF, then a     */
/* LeafDescriptor value is built in the structure pointed to by pleaf.  */
/* The tag in the *pleaf structure is not set.  If the result is        */
/* SIMPLE, then a GC_descr value is returned in *psimple_d.  If the     */
/* result is NO_MEM, then we failed to allocate the descriptor.         */
/* The implementation assumes GC_DS_LENGTH is 0.  *pleaf, *pcomplex_d   */
/* and *psimple_d may be used as temporaries during the construction.   */
STATIC int GC_make_array_descriptor(size_t nelements, size_t size,
                                    GC_descr d, GC_descr *psimple_d,
                                    complex_descriptor **pcomplex_d,
                                    struct LeafDescriptor *pleaf)
{
# define OPT_THRESHOLD 50
        /* For larger arrays, we try to combine descriptors of adjacent */
        /* descriptors to speed up marking, and to reduce the amount    */
        /* of space needed on the mark stack.                           */

  GC_ASSERT(size != 0);
  if ((d & GC_DS_TAGS) == GC_DS_LENGTH) {
    if (d == (GC_descr)size) {
      *psimple_d = nelements * d; /* no overflow guaranteed by caller */
      return SIMPLE;
    } else if (0 == d) {
      *psimple_d = 0;
      return SIMPLE;
    }
  }

  if (nelements <= OPT_THRESHOLD) {
    if (nelements <= 1) {
      *psimple_d = nelements == 1 ? d : 0;
      return SIMPLE;
    }
  } else if (size <= BITMAP_BITS/2
             && (d & GC_DS_TAGS) != GC_DS_PROC
             && (size & (sizeof(word)-1)) == 0) {
    complex_descriptor *one_element, *beginning;
    int result = GC_make_array_descriptor(nelements / 2, 2 * size,
                                GC_double_descr(d, BYTES_TO_WORDS(size)),
                                psimple_d, pcomplex_d, pleaf);

    if ((nelements & 1) == 0 || EXPECT(NO_MEM == result, FALSE))
      return result;

    one_element = GC_make_leaf_descriptor(size, 1, d);
    if (EXPECT(NULL == one_element, FALSE)) return NO_MEM;

    if (COMPLEX == result) {
      beginning = *pcomplex_d;
    } else {
      beginning = SIMPLE == result ?
                        GC_make_leaf_descriptor(size, 1, *psimple_d) :
                        GC_make_leaf_descriptor(pleaf -> ld_size,
                                                pleaf -> ld_nelements,
                                                pleaf -> ld_descriptor);
      if (EXPECT(NULL == beginning, FALSE)) return NO_MEM;
    }
    *pcomplex_d = GC_make_sequence_descriptor(beginning, one_element);
    if (EXPECT(NULL == *pcomplex_d, FALSE)) return NO_MEM;

    return COMPLEX;
  }

  pleaf -> ld_size = size;
  pleaf -> ld_nelements = nelements;
  pleaf -> ld_descriptor = d;
  return LEAF;
}

GC_API GC_ATTR_MALLOC void * GC_CALL GC_calloc_explicitly_typed(size_t n,
                                                        size_t lb, GC_descr d)
{
    void *op;
    size_t nwords;
    GC_descr simple_d;
    complex_descriptor *complex_d;
    int descr_type;
    struct LeafDescriptor leaf;

    GC_STATIC_ASSERT(sizeof(struct LeafDescriptor) % sizeof(word) == 0);
    GC_ASSERT(GC_explicit_typing_initialized);
    if (EXPECT(0 == lb || 0 == n, FALSE)) lb = n = 1;
    if (EXPECT((lb | n) > GC_SQRT_SIZE_MAX, FALSE) /* fast initial check */
        && n > GC_SIZE_MAX / lb)
      return (*GC_get_oom_fn())(GC_SIZE_MAX); /* n*lb overflow */

    descr_type = GC_make_array_descriptor((word)n, (word)lb, d,
                                          &simple_d, &complex_d, &leaf);
    lb *= n;
    switch(descr_type) {
        case NO_MEM:
            return NULL;
        case SIMPLE:
            return GC_malloc_explicitly_typed(lb, simple_d);
        case LEAF:
            lb = SIZET_SAT_ADD(lb,
                        sizeof(struct LeafDescriptor) + TYPD_EXTRA_BYTES);
            break;
        case COMPLEX:
            lb = SIZET_SAT_ADD(lb, TYPD_EXTRA_BYTES);
            break;
    }
    op = GC_malloc_kind(lb, GC_array_kind);
    if (EXPECT(NULL == op, FALSE))
        return NULL;
    nwords = GRANULES_TO_WORDS(BYTES_TO_GRANULES(GC_size(op)));
    if (descr_type == LEAF) {
      /* Set up the descriptor inside the object itself.        */
      struct LeafDescriptor *lp =
                (struct LeafDescriptor *)((word *)op + nwords -
                        (BYTES_TO_WORDS(sizeof(struct LeafDescriptor)) + 1));

      lp -> ld_tag = LEAF_TAG;
      lp -> ld_size = leaf.ld_size;
      lp -> ld_nelements = leaf.ld_nelements;
      lp -> ld_descriptor = leaf.ld_descriptor;
      /* Hold the allocation lock while writing the descriptor word     */
      /* to the object to ensure that the descriptor contents are seen  */
      /* by GC_array_mark_proc as expected.                             */
      /* TODO: It should be possible to replace locking with the atomic */
      /* operations (with the release barrier here) but, in this case,  */
      /* avoiding the acquire barrier in GC_array_mark_proc seems to    */
      /* be tricky as GC_mark_some might be invoked with the world      */
      /* running.                                                       */
      LOCK();
      ((word *)op)[nwords - 1] = (word)lp;
      UNLOCK();
    } else {
#     ifndef GC_NO_FINALIZATION
        LOCK();
        ((word *)op)[nwords - 1] = (word)complex_d;
        UNLOCK();

        GC_dirty((word *)op + nwords - 1);
        REACHABLE_AFTER_DIRTY(complex_d);

        /* Make sure the descriptor is cleared once there is any danger */
        /* it may have been collected.                                  */
        if (EXPECT(GC_general_register_disappearing_link(
                        (void **)op + nwords - 1, op) == GC_NO_MEMORY, FALSE))
#     endif
        {
            /* Couldn't register it due to lack of memory.  Punt.       */
            return (*GC_get_oom_fn())(lb);
        }
    }
    return op;
}

/* Return the size of the object described by complex_d.  It would be   */
/* faster to store this directly, or to compute it as part of           */
/* GC_push_complex_descriptor, but hopefully it does not matter.        */
STATIC word GC_descr_obj_size(complex_descriptor *complex_d)
{
  switch(complex_d -> ad.ad_tag) {
  case LEAF_TAG:
    return complex_d -> ld.ld_nelements * complex_d -> ld.ld_size;
  case ARRAY_TAG:
    return complex_d -> ad.ad_nelements
           * GC_descr_obj_size(complex_d -> ad.ad_element_descr);
  case SEQUENCE_TAG:
    return GC_descr_obj_size(complex_d -> sd.sd_first)
           + GC_descr_obj_size(complex_d -> sd.sd_second);
  default:
    ABORT_RET("Bad complex descriptor");
    return 0;
  }
}

/* Push descriptors for the object at addr with complex descriptor      */
/* onto the mark stack.  Return NULL if the mark stack overflowed.      */
STATIC mse *GC_push_complex_descriptor(word *addr,
                                       complex_descriptor *complex_d,
                                       mse *msp, mse *msl)
{
  ptr_t current = (ptr_t)addr;
  word nelements;
  word sz;
  word i;
  GC_descr d;
  complex_descriptor *element_descr;

  switch(complex_d -> ad.ad_tag) {
  case LEAF_TAG:
    d = complex_d -> ld.ld_descriptor;
    nelements = complex_d -> ld.ld_nelements;
    sz = complex_d -> ld.ld_size;

    if (EXPECT(msl - msp <= (ptrdiff_t)nelements, FALSE)) return NULL;
    GC_ASSERT(sz != 0);
    for (i = 0; i < nelements; i++) {
      msp++;
      msp -> mse_start = current;
      msp -> mse_descr.w = d;
      current += sz;
    }
    break;
  case ARRAY_TAG:
    element_descr = complex_d -> ad.ad_element_descr;
    nelements = complex_d -> ad.ad_nelements;
    sz = GC_descr_obj_size(element_descr);
    GC_ASSERT(sz != 0 || 0 == nelements);
    for (i = 0; i < nelements; i++) {
      msp = GC_push_complex_descriptor((word *)current, element_descr,
                                       msp, msl);
      if (EXPECT(NULL == msp, FALSE)) return NULL;
      current += sz;
    }
    break;
  case SEQUENCE_TAG:
    sz = GC_descr_obj_size(complex_d -> sd.sd_first);
    msp = GC_push_complex_descriptor((word *)current,
                                     complex_d -> sd.sd_first, msp, msl);
    if (EXPECT(NULL == msp, FALSE)) return NULL;
    GC_ASSERT(sz != 0);
    current += sz;
    msp = GC_push_complex_descriptor((word *)current,
                                     complex_d -> sd.sd_second, msp, msl);
    break;
  default:
    ABORT("Bad complex descriptor");
  }
  return msp;
}

GC_ATTR_NO_SANITIZE_THREAD
static complex_descriptor *get_complex_descr(word *addr, size_t nwords)
{
  return (complex_descriptor *)addr[nwords - 1];
}

/* Used by GC_calloc_explicitly_typed via GC_array_kind.        */
STATIC mse *GC_array_mark_proc(word *addr, mse *mark_stack_ptr,
                               mse *mark_stack_limit, word env)
{
  hdr *hhdr = HDR(addr);
  word sz = hhdr -> hb_sz;
  size_t nwords = (size_t)BYTES_TO_WORDS(sz);
  complex_descriptor *complex_d = get_complex_descr(addr, nwords);
  mse *orig_mark_stack_ptr = mark_stack_ptr;
  mse *new_mark_stack_ptr;

  UNUSED_ARG(env);
  if (NULL == complex_d) {
    /* Found a reference to a free list entry.  Ignore it. */
    return orig_mark_stack_ptr;
  }
  /* In use counts were already updated when array descriptor was       */
  /* pushed.  Here we only replace it by subobject descriptors, so      */
  /* no update is necessary.                                            */
  new_mark_stack_ptr = GC_push_complex_descriptor(addr, complex_d,
                                                  mark_stack_ptr,
                                                  mark_stack_limit-1);
  if (new_mark_stack_ptr == 0) {
    /* Explicitly instruct Clang Static Analyzer that ptr is non-null.  */
    if (NULL == mark_stack_ptr) ABORT("Bad mark_stack_ptr");

    /* Does not fit.  Conservatively push the whole array as a unit and */
    /* request a mark stack expansion.  This cannot cause a mark stack  */
    /* overflow, since it replaces the original array entry.            */
#   ifdef PARALLEL_MARK
      /* We might be using a local_mark_stack in parallel mode. */
      if (GC_mark_stack + GC_mark_stack_size == mark_stack_limit)
#   endif
    {
      GC_mark_stack_too_small = TRUE;
    }
    new_mark_stack_ptr = orig_mark_stack_ptr + 1;
    new_mark_stack_ptr -> mse_start = (ptr_t)addr;
    new_mark_stack_ptr -> mse_descr.w = sz | GC_DS_LENGTH;
  } else {
    /* Push descriptor itself.  */
    new_mark_stack_ptr++;
    new_mark_stack_ptr -> mse_start = (ptr_t)(addr + nwords - 1);
    new_mark_stack_ptr -> mse_descr.w = sizeof(word) | GC_DS_LENGTH;
  }
  return new_mark_stack_ptr;
}
