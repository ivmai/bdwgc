/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 2000 by Hewlett-Packard Company.  All rights reserved.
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

/* This file contains the functions:    */
/* - ptr_t GC_build_flXXX(h, old_fl);   */
/* - void GC_new_hblk(size, kind);      */

#ifndef SMALL_CONFIG
/* Build a free list for two-pointer cleared objects inside the given */
/* block.  Set the last link to be ofl.  Return a pointer to the      */
/* first free-list entry.                                             */
STATIC ptr_t
GC_build_fl_clear2(struct hblk *h, ptr_t ofl)
{
  ptr_t *p = (ptr_t *)h->hb_body;
  ptr_t plim = (ptr_t)(h + 1);

  p[0] = ofl;
  p[1] = NULL;
  p[2] = (ptr_t)p;
  p[3] = NULL;
  for (p += 4; ADDR_LT((ptr_t)p, plim); p += 4) {
    p[0] = (ptr_t)(p - 2);
    p[1] = NULL;
    p[2] = (ptr_t)p;
    p[3] = NULL;
  }
  return (ptr_t)(p - 2);
}

/* The same as above but uncleared objects.   */
STATIC ptr_t
GC_build_fl2(struct hblk *h, ptr_t ofl)
{
  ptr_t *p = (ptr_t *)h->hb_body;
  ptr_t plim = (ptr_t)(h + 1);

  p[0] = ofl;
  p[2] = (ptr_t)p;
  for (p += 4; ADDR_LT((ptr_t)p, plim); p += 4) {
    p[0] = (ptr_t)(p - 2);
    p[2] = (ptr_t)p;
  }
  return (ptr_t)(p - 2);
}

/* The same as above but for four-pointer cleared objects.        */
STATIC ptr_t
GC_build_fl_clear4(struct hblk *h, ptr_t ofl)
{
  ptr_t *p = (ptr_t *)h->hb_body;
  ptr_t plim = (ptr_t)(h + 1);

  p[0] = ofl;
  p[1] = NULL;
  p[2] = NULL;
  p[3] = NULL;
  for (p += 4; ADDR_LT((ptr_t)p, plim); p += 4) {
    GC_PREFETCH_FOR_WRITE((ptr_t)(p + 64));
    p[0] = (ptr_t)(p - 4);
    p[1] = NULL;
    CLEAR_DOUBLE(p + 2);
  }
  return (ptr_t)(p - 4);
}

/* The same as GC_build_fl_clear4() but uncleared objects.            */
STATIC ptr_t
GC_build_fl4(struct hblk *h, ptr_t ofl)
{
  ptr_t *p = (ptr_t *)h->hb_body;
  ptr_t plim = (ptr_t)(h + 1);

  p[0] = ofl;
  p[4] = (ptr_t)p;
  /* Unroll the loop by 2.    */
  for (p += 8; ADDR_LT((ptr_t)p, plim); p += 8) {
    GC_PREFETCH_FOR_WRITE((ptr_t)(p + 64));
    p[0] = (ptr_t)(p - 4);
    p[4] = (ptr_t)p;
  }
  return (ptr_t)(p - 4);
}
#endif /* !SMALL_CONFIG */

GC_INNER ptr_t
GC_build_fl(struct hblk *h, ptr_t list, size_t lg, GC_bool clear)
{
  ptr_t *p, *prev;
  ptr_t plim; /* points to last object in new hblk */
  size_t lpw = GRANULES_TO_PTRS(lg);

  /* Do a few prefetches here, just because it's cheap.         */
  /* If we were more serious about it, these should go inside   */
  /* the loops.  But write prefetches usually don't seem to     */
  /* matter much.                                               */
  GC_PREFETCH_FOR_WRITE((ptr_t)h);
  GC_PREFETCH_FOR_WRITE((ptr_t)h + 128);
  GC_PREFETCH_FOR_WRITE((ptr_t)h + 256);
  GC_PREFETCH_FOR_WRITE((ptr_t)h + 378);
#ifndef SMALL_CONFIG
  /* Handle small objects sizes more efficiently.  For larger objects */
  /* the difference is less significant.                              */
  switch (lpw) {
  case 2:
    if (clear) {
      return GC_build_fl_clear2(h, list);
    } else {
      return GC_build_fl2(h, list);
    }
  case 4:
    if (clear) {
      return GC_build_fl_clear4(h, list);
    } else {
      return GC_build_fl4(h, list);
    }
  default:
    break;
  }
#endif /* !SMALL_CONFIG */

  /* Clear the page if necessary. */
  if (clear)
    BZERO(h, HBLKSIZE);

  /* Add objects to free list. */
  prev = (ptr_t *)h->hb_body; /* one object behind p */

  /* The last place for the last object to start.       */
  plim = (ptr_t)h + HBLKSIZE - lpw * sizeof(ptr_t);

  /* Make a list of all objects in *h with head as last object. */
  for (p = prev + lpw; ADDR_GE(plim, (ptr_t)p); p += lpw) {
    /* current object's link points to last object */
    obj_link(p) = (ptr_t)prev;
    prev = p;
  }
  p -= lpw;
  /* p now points to the last object.   */

  /* Put p (which is now head of list of objects in *h) as first    */
  /* pointer in the appropriate free list for this size.            */
  *(ptr_t *)h = list;
  return (ptr_t)p;
}

GC_INNER void
GC_new_hblk(size_t lg, int k)
{
  struct hblk *h; /* the new heap block */
  size_t lb_adjusted = GRANULES_TO_BYTES(lg);

  GC_STATIC_ASSERT(sizeof(struct hblk) == HBLKSIZE);
  GC_ASSERT(I_HOLD_LOCK());
  /* Allocate a new heap block. */
  h = GC_allochblk(lb_adjusted, k, 0 /* flags */, 0 /* align_m1 */);
  if (EXPECT(NULL == h, FALSE)) {
    /* Out of memory.   */
    return;
  }

  /* Mark all objects if appropriate. */
  if (IS_UNCOLLECTABLE(k))
    GC_set_hdr_marks(HDR(h));

  /* Build the free list.       */
  GC_obj_kinds[k].ok_freelist[lg]
      = GC_build_fl(h, (ptr_t)GC_obj_kinds[k].ok_freelist[lg], lg,
                    GC_debugging_started || GC_obj_kinds[k].ok_init);
}
