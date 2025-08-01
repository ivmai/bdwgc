/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright 1996 Silicon Graphics.  All rights reserved.
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

/*
 * Some simple primitives for allocation with explicit type information.
 * Facilities for dynamic type inference may be added later.
 * Should be used only for extremely performance critical applications,
 * or if conservative collector leakage is otherwise a problem (unlikely).
 * Note that this is implemented completely separately from the rest
 * of the collector, and is not linked in unless referenced.
 * This does not currently support `GC_DEBUG` in any interesting way.
 *
 * Note for cris and m68k architectures (and for bcc and wcc compilers):
 * this API expects all pointers in the type (`T`) are aligned by the size
 * of a pointer (which might differ from `ALIGNMENT`).
 */

#ifndef GC_TYPED_H
#define GC_TYPED_H

#ifndef GC_H
#  include "gc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** The size of `GC_word` (not a pointer) type in bits. */
#define GC_WORDSZ (8 * sizeof(GC_word))

/** The size of a type `t` in pointers.  The value is rounded down. */
#define GC_SIZEOF_IN_PTRS(t) (sizeof(t) / sizeof(void *))

/** The offset of a field in pointers. */
#define GC_OFFSETOF_IN_PTRS(t, f) (offsetof(t, f) / sizeof(void *))

/**
 * The bitmap type.  The least significant bit of the first `GC_word` is one
 * if the first "pointer-sized" word in the object may be a pointer.
 */
typedef GC_word *GC_bitmap;

/**
 * The number of elements (of `GC_word` type) of a bitmap array to
 * create for a given type `t`.  The bitmap is intended to be passed to
 * `GC_make_descriptor()`.
 */
#define GC_BITMAP_SIZE(t) ((GC_SIZEOF_IN_PTRS(t) + GC_WORDSZ - 1) / GC_WORDSZ)

/**
 * The setter and getter of the bitmap.  The `bm` argument should be of
 * `GC_bitmap` type; `i` argument should be of some `unsigned` type and
 * should not have side effects.
 */
#define GC_set_bit(bm, i) \
  ((bm)[(i) / GC_WORDSZ] |= (GC_word)1 << ((i) % GC_WORDSZ))
#define GC_get_bit(bm, i) (((bm)[(i) / GC_WORDSZ] >> ((i) % GC_WORDSZ)) & 1)

typedef GC_word GC_descr;

/**
 * Return a type descriptor for the object whose layout is described
 * by the first argument.  The least significant bit of the first
 * `GC_word` is one if the first "pointer-sized" word in the object may
 * be a pointer.  The second argument (`number_of_bits_in_bitmap`)
 * specifies the number of meaningful bits in the bitmap.
 * The actual object may be larger (but not smaller); any additional
 * "pointer-sized" words in the object are assumed not to contain
 * pointers.  Returns a conservative approximation in the (unlikely)
 * case of insufficient memory to build the descriptor.  Calls to
 * `GC_make_descriptor()` may consume some amount of a finite resource.
 * This is intended to be called once per a type, not once per an
 * allocation.
 *
 * It is possible to generate a descriptor for a client type `T` with
 * aligned pointer fields `f1`, `f2`, ... as follows:
 * ```
 *   GC_descr t_descr;
 *   {
 *     GC_word bm[GC_BITMAP_SIZE(T)] = { 0 };
 *     GC_set_bit(bm, GC_OFFSETOF_IN_PTRS(T, f1));
 *     GC_set_bit(bm, GC_OFFSETOF_IN_PTRS(T, f2));
 *     ...
 *     t_descr = GC_make_descriptor(bm, GC_SIZEOF_IN_PTRS(T));
 *   }
 * ```
 */
GC_API GC_descr GC_CALL
GC_make_descriptor(const GC_word * /* `GC_bitmap` `bm` */,
                   size_t /* `number_of_bits_in_bitmap` */);

/**
 * Allocate an object whose layout is described by `d`.  The size
 * (`size_in_bytes`) may *not* be less than the number of meaningful
 * bits in the bitmap of `d` multiplied by the size of a pointer.
 * The returned object is cleared.  The returned object may *not* be
 * passed to `GC_realloc()`.
 */
GC_API GC_ATTR_MALLOC GC_ATTR_ALLOC_SIZE(1) void *GC_CALL
    GC_malloc_explicitly_typed(size_t /* `size_in_bytes` */,
                               GC_descr /* `d` */);

/** The ignore-off-page variant of `GC_malloc_explicitly_typed()`. */
GC_API GC_ATTR_MALLOC GC_ATTR_ALLOC_SIZE(1) void *GC_CALL
    GC_malloc_explicitly_typed_ignore_off_page(size_t /* `size_in_bytes` */,
                                               GC_descr /* `d` */);

/**
 * Allocate an array of `nelements` elements, each of the given size
 * (`element_size_in_bytes`), and with the given descriptor (`d`).
 * The element size must be a multiple of the byte alignment required
 * for pointers.  (E.g. on a 32-bit machine with 16-bit aligned
 * pointers, `element_size_in_bytes` must be a multiple of 2.)
 * The element size may *not* be less than the number of meaningful
 * bits in the bitmap of `d` multiplied by the size of a pointer.
 * The returned object is cleared.
 */
GC_API GC_ATTR_MALLOC GC_ATTR_CALLOC_SIZE(1, 2) void *GC_CALL
    GC_calloc_explicitly_typed(size_t /* `nelements` */,
                               size_t /* `element_size_in_bytes` */,
                               GC_descr /* d */);

#define GC_CALLOC_TYPED_DESCR_PTRS 1
#define GC_CALLOC_TYPED_DESCR_WORDS 8 /*< includes space for pointers */

#ifdef GC_BUILD
struct GC_calloc_typed_descr_s;
#  define GC_calloc_typed_descr_s GC_calloc_typed_descr_opaque_s
#endif

struct GC_calloc_typed_descr_s {
  GC_uintptr_t opaque_p[GC_CALLOC_TYPED_DESCR_PTRS];
  GC_word opaque[GC_CALLOC_TYPED_DESCR_WORDS - GC_CALLOC_TYPED_DESCR_PTRS];
};
#undef GC_calloc_typed_descr_s

/**
 * This is same as `GC_calloc_explicitly_typed` but more optimal in
 * terms of the performance and memory usage if the client needs to
 * allocate multiple typed object arrays with the same layout and
 * number of elements.  The client should call it to be prepared for
 * the subsequent allocations by `GC_calloc_do_explicitly_typed()`, one
 * or many.  The result of the preparation is stored to `*pctd`, even
 * in case of a failure.  The prepared structure could be just dropped
 * when no longer needed.  Returns 0 on failure, 1 on success; the
 * result could be ignored (as it is also stored to `*pctd` and checked
 * later by `GC_calloc_do_explicitly_typed()`).
 */
GC_API int GC_CALL GC_calloc_prepare_explicitly_typed(
    struct GC_calloc_typed_descr_s * /* `pctd` */, size_t /* `sizeof_ctd` */,
    size_t /* `nelements` */, size_t /* `element_size_in_bytes` */, GC_descr);

/** The actual object allocation for the prepared description. */
GC_API GC_ATTR_MALLOC void *GC_CALL GC_calloc_do_explicitly_typed(
    const struct GC_calloc_typed_descr_s * /* `pctd` */,
    size_t /* `sizeof_ctd` */);

#ifdef GC_DEBUG
#  define GC_MALLOC_EXPLICITLY_TYPED(bytes, d) ((void)(d), GC_MALLOC(bytes))
#  define GC_MALLOC_EXPLICITLY_TYPED_IGNORE_OFF_PAGE(bytes, d) \
    GC_MALLOC_EXPLICITLY_TYPED(bytes, d)
#  define GC_CALLOC_EXPLICITLY_TYPED(n, bytes, d) \
    ((void)(d), GC_MALLOC((n) * (bytes)))
#else
#  define GC_MALLOC_EXPLICITLY_TYPED(bytes, d) \
    GC_malloc_explicitly_typed(bytes, d)
#  define GC_MALLOC_EXPLICITLY_TYPED_IGNORE_OFF_PAGE(bytes, d) \
    GC_malloc_explicitly_typed_ignore_off_page(bytes, d)
#  define GC_CALLOC_EXPLICITLY_TYPED(n, bytes, d) \
    GC_calloc_explicitly_typed(n, bytes, d)
#endif

/* Deprecated.  Use `GC_SIZEOF_IN_PTRS()` instead. */
#define GC_WORD_LEN(t) GC_SIZEOF_IN_PTRS(t)

/* Deprecated.  Use `GC_OFFSETOF_IN_PTRS()` instead. */
#define GC_WORD_OFFSET(t, f) GC_OFFSETOF_IN_PTRS(t, f)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GC_TYPED_H */
