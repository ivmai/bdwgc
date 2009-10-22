/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/gc_priv.h"

#include <stdio.h>

/* Data structure for list of root sets.                                */
/* We keep a hash table, so that we can filter out duplicate additions. */
/* Under Win32, we need to do a better job of filtering overlaps, so    */
/* we resort to sequential search, and pay the price.                   */
/* This is really declared in gc_priv.h:
struct roots {
        ptr_t r_start;
        ptr_t r_end;
#       if !defined(MSWIN32) && !defined(MSWINCE)
          struct roots * r_next;
#       endif
        GC_bool r_tmp;
                -- Delete before registering new dynamic libraries
};

struct roots GC_static_roots[MAX_ROOT_SETS];
*/

int GC_no_dls = 0;      /* Register dynamic library data segments.      */

static int n_root_sets = 0;
        /* GC_static_roots[0..n_root_sets) contains the valid root sets. */

#if !defined(NO_DEBUGGING)
  /* For debugging:     */
  void GC_print_static_roots(void)
  {
    int i;
    size_t total = 0;

    for (i = 0; i < n_root_sets; i++) {
        GC_printf("From %p to %p%s\n",
                  GC_static_roots[i].r_start,
                  GC_static_roots[i].r_end,
                  GC_static_roots[i].r_tmp ? " (temporary)" : "");
        total += GC_static_roots[i].r_end - GC_static_roots[i].r_start;
    }
    GC_printf("Total size: %ld\n", (unsigned long) total);
    if (GC_root_size != total) {
        GC_err_printf("GC_root_size incorrect: %ld!!\n",
                      (long) GC_root_size);
    }
  }
#endif /* !NO_DEBUGGING */

#ifndef THREADS
  /* Primarily for debugging support:     */
  /* Is the address p in one of the registered static root sections?      */
  GC_INNER GC_bool GC_is_static_root(ptr_t p)
  {
    static int last_root_set = MAX_ROOT_SETS;
    int i;

    if (last_root_set < n_root_sets
        && p >= GC_static_roots[last_root_set].r_start
        && p < GC_static_roots[last_root_set].r_end) return(TRUE);
    for (i = 0; i < n_root_sets; i++) {
        if (p >= GC_static_roots[i].r_start
            && p < GC_static_roots[i].r_end) {
            last_root_set = i;
            return(TRUE);
        }
    }
    return(FALSE);
  }
#endif /* !THREADS */

#if !defined(MSWIN32) && !defined(MSWINCE)
/*
#   define LOG_RT_SIZE 6
#   define RT_SIZE (1 << LOG_RT_SIZE)  -- Power of 2, may be != MAX_ROOT_SETS

    struct roots * GC_root_index[RT_SIZE];
        -- Hash table header.  Used only to check whether a range is
        -- already present.
        -- really defined in gc_priv.h
*/

  GC_INLINE int rt_hash(ptr_t addr)
  {
    word result = (word) addr;
#   if CPP_WORDSZ > 8*LOG_RT_SIZE
        result ^= result >> 8*LOG_RT_SIZE;
#   endif
#   if CPP_WORDSZ > 4*LOG_RT_SIZE
        result ^= result >> 4*LOG_RT_SIZE;
#   endif
    result ^= result >> 2*LOG_RT_SIZE;
    result ^= result >> LOG_RT_SIZE;
    result &= (RT_SIZE-1);
    return(result);
  }

  /* Is a range starting at b already in the table? If so return a        */
  /* pointer to it, else NIL.                                             */
  GC_INNER struct roots * GC_roots_present(ptr_t b)
  {
    int h = rt_hash(b);
    struct roots *p = GC_root_index[h];

    while (p != 0) {
        if (p -> r_start == (ptr_t)b) return(p);
        p = p -> r_next;
    }
    return(FALSE);
  }

  /* Add the given root structure to the index. */
  GC_INLINE void add_roots_to_index(struct roots *p)
  {
    int h = rt_hash(p -> r_start);

    p -> r_next = GC_root_index[h];
    GC_root_index[h] = p;
  }
#endif /* !MSWIN32 */

GC_INNER word GC_root_size = 0;

GC_API void GC_CALL GC_add_roots(void *b, void *e)
{
    DCL_LOCK_STATE;

    if (!GC_is_initialized) GC_init();
    LOCK();
    GC_add_roots_inner((ptr_t)b, (ptr_t)e, FALSE);
    UNLOCK();
}


/* Add [b,e) to the root set.  Adding the same interval a second time   */
/* is a moderately fast no-op, and hence benign.  We do not handle      */
/* different but overlapping intervals efficiently.  (We do handle      */
/* them correctly.)                                                     */
/* Tmp specifies that the interval may be deleted before                */
/* re-registering dynamic libraries.                                    */
void GC_add_roots_inner(ptr_t b, ptr_t e, GC_bool tmp)
{
    struct roots * old;

    /* Adjust and check range boundaries for safety */
    GC_ASSERT((word)b % sizeof(word) == 0);
    e = (ptr_t)((word)e & ~(sizeof(word) - 1));
    GC_ASSERT(b <= e);
    if (b == e) return;  /* nothing to do? */

#   if defined(MSWIN32) || defined(MSWINCE)
      /* Spend the time to ensure that there are no overlapping */
      /* or adjacent intervals.                                 */
      /* This could be done faster with e.g. a                  */
      /* balanced tree.  But the execution time here is         */
      /* virtually guaranteed to be dominated by the time it    */
      /* takes to scan the roots.                               */
      {
        register int i;
        old = 0; /* initialized to prevent warning. */
        for (i = 0; i < n_root_sets; i++) {
            old = GC_static_roots + i;
            if (b <= old -> r_end && e >= old -> r_start) {
                if (b < old -> r_start) {
                    old -> r_start = b;
                    GC_root_size += (old -> r_start - b);
                }
                if (e > old -> r_end) {
                    old -> r_end = e;
                    GC_root_size += (e - old -> r_end);
                }
                old -> r_tmp &= tmp;
                break;
            }
        }
        if (i < n_root_sets) {
          /* merge other overlapping intervals */
            struct roots *other;

            for (i++; i < n_root_sets; i++) {
              other = GC_static_roots + i;
              b = other -> r_start;
              e = other -> r_end;
              if (b <= old -> r_end && e >= old -> r_start) {
                if (b < old -> r_start) {
                    old -> r_start = b;
                    GC_root_size += (old -> r_start - b);
                }
                if (e > old -> r_end) {
                    old -> r_end = e;
                    GC_root_size += (e - old -> r_end);
                }
                old -> r_tmp &= other -> r_tmp;
                /* Delete this entry. */
                  GC_root_size -= (other -> r_end - other -> r_start);
                  other -> r_start = GC_static_roots[n_root_sets-1].r_start;
                  other -> r_end = GC_static_roots[n_root_sets-1].r_end;
                  n_root_sets--;
              }
            }
          return;
        }
      }
#   else
      old = GC_roots_present(b);
      if (old != 0) {
        if (e <= old -> r_end) /* already there */ return;
        /* else extend */
        GC_root_size += e - old -> r_end;
        old -> r_end = e;
        return;
      }
#   endif
    if (n_root_sets == MAX_ROOT_SETS) {
        ABORT("Too many root sets\n");
    }
    GC_static_roots[n_root_sets].r_start = (ptr_t)b;
    GC_static_roots[n_root_sets].r_end = (ptr_t)e;
    GC_static_roots[n_root_sets].r_tmp = tmp;
#   if !defined(MSWIN32) && !defined(MSWINCE)
      GC_static_roots[n_root_sets].r_next = 0;
      add_roots_to_index(GC_static_roots + n_root_sets);
#   endif
    GC_root_size += e - b;
    n_root_sets++;
}

static GC_bool roots_were_cleared = FALSE;

GC_API void GC_CALL GC_clear_roots(void)
{
    DCL_LOCK_STATE;

    if (!GC_is_initialized) GC_init();
    LOCK();
    roots_were_cleared = TRUE;
    n_root_sets = 0;
    GC_root_size = 0;
#   if !defined(MSWIN32) && !defined(MSWINCE)
      {
        int i;
        for (i = 0; i < RT_SIZE; i++) GC_root_index[i] = 0;
      }
#   endif
    UNLOCK();
}

/* Internal use only; lock held.        */
STATIC void GC_remove_root_at_pos(int i)
{
    GC_root_size -= (GC_static_roots[i].r_end - GC_static_roots[i].r_start);
    GC_static_roots[i].r_start = GC_static_roots[n_root_sets-1].r_start;
    GC_static_roots[i].r_end = GC_static_roots[n_root_sets-1].r_end;
    GC_static_roots[i].r_tmp = GC_static_roots[n_root_sets-1].r_tmp;
    n_root_sets--;
}

#if !defined(MSWIN32) && !defined(MSWINCE)
  STATIC void GC_rebuild_root_index(void)
  {
    int i;

    for (i = 0; i < RT_SIZE; i++) GC_root_index[i] = 0;
    for (i = 0; i < n_root_sets; i++)
        add_roots_to_index(GC_static_roots + i);
  }
#endif

#if defined(DYNAMIC_LOADING) || defined(MSWIN32) || defined(MSWINCE) \
     || defined(PCR)
/* Internal use only; lock held.        */
STATIC void GC_remove_tmp_roots(void)
{
    int i;

    for (i = 0; i < n_root_sets; ) {
        if (GC_static_roots[i].r_tmp) {
            GC_remove_root_at_pos(i);
        } else {
            i++;
        }
    }
#   if !defined(MSWIN32) && !defined(MSWINCE)
      GC_rebuild_root_index();
#   endif
}
#endif

#if !defined(MSWIN32) && !defined(MSWINCE)
  STATIC void GC_remove_roots_inner(ptr_t b, ptr_t e);

  GC_API void GC_CALL GC_remove_roots(void *b, void *e)
  {
    DCL_LOCK_STATE;

    /* Quick check whether has nothing to do */
    if ((((word)b + (sizeof(word) - 1)) & ~(sizeof(word) - 1)) >=
        ((word)e & ~(sizeof(word) - 1)))
      return;

    LOCK();
    GC_remove_roots_inner((ptr_t)b, (ptr_t)e);
    UNLOCK();
  }

  /* Should only be called when the lock is held */
  STATIC void GC_remove_roots_inner(ptr_t b, ptr_t e)
  {
    int i;
    for (i = 0; i < n_root_sets; ) {
        if (GC_static_roots[i].r_start >= b
            && GC_static_roots[i].r_end <= e) {
            GC_remove_root_at_pos(i);
        } else {
            i++;
        }
    }
    GC_rebuild_root_index();
  }
#endif /* !defined(MSWIN32) && !defined(MSWINCE) */

#if (defined(MSWIN32) || defined(MSWINCE)) && !defined(NO_DEBUGGING)
  /* Not used at present (except for, may be, debugging purpose).       */
  /* Workaround for the OS mapping and unmapping behind our back:       */
  /* Is the address p in one of the temporary static root sections?     */
  GC_bool GC_is_tmp_root(ptr_t p)
  {
    static int last_root_set = MAX_ROOT_SETS;
    register int i;

    if (last_root_set < n_root_sets
        && p >= GC_static_roots[last_root_set].r_start
        && p < GC_static_roots[last_root_set].r_end)
        return GC_static_roots[last_root_set].r_tmp;
    for (i = 0; i < n_root_sets; i++) {
        if (p >= GC_static_roots[i].r_start
            && p < GC_static_roots[i].r_end) {
            last_root_set = i;
            return GC_static_roots[i].r_tmp;
        }
    }
    return(FALSE);
  }
#endif /* MSWIN32 || MSWINCE */

GC_INNER ptr_t GC_approx_sp(void)
{
    volatile word sp;
    sp = (word)&sp;
                /* Also force stack to grow if necessary. Otherwise the */
                /* later accesses might cause the kernel to think we're */
                /* doing something wrong.                               */
    return((ptr_t)sp);
}

/*
 * Data structure for excluded static roots.
 * Real declaration is in gc_priv.h.

struct exclusion {
    ptr_t e_start;
    ptr_t e_end;
};

struct exclusion GC_excl_table[MAX_EXCLUSIONS];
                                        -- Array of exclusions, ascending
                                        -- address order.
*/

STATIC size_t GC_excl_table_entries = 0;/* Number of entries in use.      */

/* Return the first exclusion range that includes an address >= start_addr */
/* Assumes the exclusion table contains at least one entry (namely the     */
/* GC data structures).                                                    */
STATIC struct exclusion * GC_next_exclusion(ptr_t start_addr)
{
    size_t low = 0;
    size_t high = GC_excl_table_entries - 1;
    size_t mid;

    while (high > low) {
        mid = (low + high) >> 1;
        /* low <= mid < high    */
        if ((word) GC_excl_table[mid].e_end <= (word) start_addr) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if ((word) GC_excl_table[low].e_end <= (word) start_addr) return 0;
    return GC_excl_table + low;
}

/* Should only be called when the lock is held.  The range boundaries   */
/* should be properly aligned and valid.                                */
GC_INNER void GC_exclude_static_roots_inner(void *start, void *finish)
{
    struct exclusion * next;
    size_t next_index, i;

    GC_ASSERT((word)start % sizeof(word) == 0);
    GC_ASSERT(start < finish);

    if (0 == GC_excl_table_entries) {
        next = 0;
    } else {
        next = GC_next_exclusion(start);
    }
    if (0 != next) {
      if ((word)(next -> e_start) < (word) finish) {
        /* incomplete error check. */
        ABORT("exclusion ranges overlap");
      }
      if ((word)(next -> e_start) == (word) finish) {
        /* extend old range backwards   */
          next -> e_start = (ptr_t)start;
          return;
      }
      next_index = next - GC_excl_table;
      for (i = GC_excl_table_entries; i > next_index; --i) {
        GC_excl_table[i] = GC_excl_table[i-1];
      }
    } else {
      next_index = GC_excl_table_entries;
    }
    if (GC_excl_table_entries == MAX_EXCLUSIONS) ABORT("Too many exclusions");
    GC_excl_table[next_index].e_start = (ptr_t)start;
    GC_excl_table[next_index].e_end = (ptr_t)finish;
    ++GC_excl_table_entries;
}

GC_API void GC_CALL GC_exclude_static_roots(void *b, void *e)
{
    DCL_LOCK_STATE;

    /* Adjust the upper boundary for safety (round down) */
    e = (void *)((word)e & ~(sizeof(word) - 1));

    if (b == e) return;  /* nothing to exclude? */

    LOCK();
    GC_exclude_static_roots_inner(b, e);
    UNLOCK();
}

/* Invoke push_conditional on ranges that are not excluded. */
/*ARGSUSED*/
STATIC void GC_push_conditional_with_exclusions(ptr_t bottom, ptr_t top,
                                                GC_bool all)
{
    struct exclusion * next;
    ptr_t excl_start;

    while (bottom < top) {
        next = GC_next_exclusion(bottom);
        if (0 == next || (excl_start = next -> e_start) >= top) {
            GC_push_conditional(bottom, top, all);
            return;
        }
        if (excl_start > bottom) GC_push_conditional(bottom, excl_start, all);
        bottom = next -> e_end;
    }
}

#ifdef IA64
  /* Similar to GC_push_all_stack_frames() but for IA-64 registers store. */
  GC_INNER void GC_push_all_register_frames(ptr_t bs_lo, ptr_t bs_hi,
                    int eager, struct GC_activation_frame_s *activation_frame)
  {
    while (activation_frame != NULL) {
        ptr_t frame_bs_lo = activation_frame -> backing_store_end;
        GC_ASSERT(frame_bs_lo <= bs_hi);
        if (eager) {
            GC_push_all_eager(frame_bs_lo, bs_hi);
        } else {
            GC_push_all_stack(frame_bs_lo, bs_hi);
        }
        bs_hi = activation_frame -> saved_backing_store_ptr;
        activation_frame = activation_frame -> prev;
    }
    GC_ASSERT(bs_lo <= bs_hi);
    if (eager) {
        GC_push_all_eager(bs_lo, bs_hi);
    } else {
        GC_push_all_stack(bs_lo, bs_hi);
    }
  }
#endif /* IA64 */

#ifdef THREADS

GC_INNER void GC_push_all_stack_frames(ptr_t lo, ptr_t hi,
                        struct GC_activation_frame_s *activation_frame)
{
    while (activation_frame != NULL) {
        GC_ASSERT(lo HOTTER_THAN (ptr_t)activation_frame);
#       ifdef STACK_GROWS_UP
            GC_push_all_stack((ptr_t)activation_frame, lo);
#       else /* STACK_GROWS_DOWN */
            GC_push_all_stack(lo, (ptr_t)activation_frame);
#       endif
        lo = activation_frame -> saved_stack_ptr;
        GC_ASSERT(lo != NULL);
        activation_frame = activation_frame -> prev;
    }
    GC_ASSERT(!(hi HOTTER_THAN lo));
#   ifdef STACK_GROWS_UP
        /* We got them backwards! */
        GC_push_all_stack(hi, lo);
#   else /* STACK_GROWS_DOWN */
        GC_push_all_stack(lo, hi);
#   endif
}

#else /* !THREADS */

# ifdef TRACE_BUF
    /* Defined in mark.c.       */
    void GC_add_trace_entry(char *kind, word arg1, word arg2);
# endif

                        /* Similar to GC_push_all_eager, but only the   */
                        /* part hotter than cold_gc_frame is scanned    */
                        /* immediately.  Needed to ensure that callee-  */
                        /* save registers are not missed.               */
/*
 * A version of GC_push_all that treats all interior pointers as valid
 * and scans part of the area immediately, to make sure that saved
 * register values are not lost.
 * Cold_gc_frame delimits the stack section that must be scanned
 * eagerly.  A zero value indicates that no eager scanning is needed.
 * We don't need to worry about the MANUAL_VDB case here, since this
 * is only called in the single-threaded case.  We assume that we
 * cannot collect between an assignment and the corresponding
 * GC_dirty() call.
 */
STATIC void GC_push_all_stack_partially_eager(ptr_t bottom, ptr_t top,
                                              ptr_t cold_gc_frame)
{
  if (!NEED_FIXUP_POINTER && GC_all_interior_pointers) {
    /* Push the hot end of the stack eagerly, so that register values   */
    /* saved inside GC frames are marked before they disappear.         */
    /* The rest of the marking can be deferred until later.             */
    if (0 == cold_gc_frame) {
        GC_push_all_stack(bottom, top);
        return;
    }
    GC_ASSERT(bottom <= cold_gc_frame && cold_gc_frame <= top);
#   ifdef STACK_GROWS_DOWN
        GC_push_all(cold_gc_frame - sizeof(ptr_t), top);
        GC_push_all_eager(bottom, cold_gc_frame);
#   else /* STACK_GROWS_UP */
        GC_push_all(bottom, cold_gc_frame + sizeof(ptr_t));
        GC_push_all_eager(cold_gc_frame, top);
#   endif /* STACK_GROWS_UP */
  } else {
    GC_push_all_eager(bottom, top);
  }
# ifdef TRACE_BUF
      GC_add_trace_entry("GC_push_all_stack", bottom, top);
# endif
}

/* Similar to GC_push_all_stack_frames() but also uses cold_gc_frame.   */
STATIC void GC_push_all_stack_part_eager_frames(ptr_t lo, ptr_t hi,
        ptr_t cold_gc_frame, struct GC_activation_frame_s *activation_frame)
{
    GC_ASSERT(activation_frame == NULL || cold_gc_frame == NULL ||
                cold_gc_frame HOTTER_THAN (ptr_t)activation_frame);

    while (activation_frame != NULL) {
        GC_ASSERT(lo HOTTER_THAN (ptr_t)activation_frame);
#       ifdef STACK_GROWS_UP
            GC_push_all_stack_partially_eager((ptr_t)activation_frame, lo,
                                                cold_gc_frame);
#       else /* STACK_GROWS_DOWN */
            GC_push_all_stack_partially_eager(lo, (ptr_t)activation_frame,
                                                cold_gc_frame);
#       endif
        lo = activation_frame -> saved_stack_ptr;
        GC_ASSERT(lo != NULL);
        activation_frame = activation_frame -> prev;
        cold_gc_frame = NULL; /* Use at most once.      */
    }

    GC_ASSERT(!(hi HOTTER_THAN lo));
#   ifdef STACK_GROWS_UP
        /* We got them backwards! */
        GC_push_all_stack_partially_eager(hi, lo, cold_gc_frame);
#   else /* STACK_GROWS_DOWN */
        GC_push_all_stack_partially_eager(lo, hi, cold_gc_frame);
#   endif
}

#endif /* !THREADS */

                        /* Push enough of the current stack eagerly to  */
                        /* ensure that callee-save registers saved in   */
                        /* GC frames are scanned.                       */
                        /* In the non-threads case, schedule entire     */
                        /* stack for scanning.                          */
                        /* The second argument is a pointer to the      */
                        /* (possibly null) thread context, for          */
                        /* (currently hypothetical) more precise        */
                        /* stack scanning.                              */
/*
 * In the absence of threads, push the stack contents.
 * In the presence of threads, push enough of the current stack
 * to ensure that callee-save registers saved in collector frames have been
 * seen.
 * FIXME: Merge with per-thread stuff.
 */
/*ARGSUSED*/
STATIC void GC_push_current_stack(ptr_t cold_gc_frame, void * context)
{
#   if defined(THREADS)
        if (0 == cold_gc_frame) return;
#       ifdef STACK_GROWS_DOWN
          GC_push_all_eager(GC_approx_sp(), cold_gc_frame);
          /* For IA64, the register stack backing store is handled      */
          /* in the thread-specific code.                               */
#       else
          GC_push_all_eager(cold_gc_frame, GC_approx_sp());
#       endif
#   else
        GC_push_all_stack_part_eager_frames(GC_approx_sp(), GC_stackbottom,
                                        cold_gc_frame, GC_activation_frame);
#       ifdef IA64
              /* We also need to push the register stack backing store. */
              /* This should really be done in the same way as the      */
              /* regular stack.  For now we fudge it a bit.             */
              /* Note that the backing store grows up, so we can't use  */
              /* GC_push_all_stack_partially_eager.                     */
              {
                ptr_t bsp = GC_save_regs_ret_val;
                ptr_t cold_gc_bs_pointer = bsp - 2048;
                if (GC_all_interior_pointers &&
                    cold_gc_bs_pointer > BACKING_STORE_BASE) {
                  /* Adjust cold_gc_bs_pointer if below our innermost   */
                  /* "activation frame" in backing store.               */
                  if (GC_activation_frame != NULL && cold_gc_bs_pointer <
                                GC_activation_frame->backing_store_end)
                    cold_gc_bs_pointer =
                                GC_activation_frame->backing_store_end;
                  GC_push_all_register_frames(BACKING_STORE_BASE,
                        cold_gc_bs_pointer, FALSE, GC_activation_frame);
                  GC_push_all_eager(cold_gc_bs_pointer, bsp);
                } else {
                  GC_push_all_register_frames(BACKING_STORE_BASE, bsp,
                                TRUE /* eager */, GC_activation_frame);
                }
                /* All values should be sufficiently aligned that we    */
                /* don't have to worry about the boundary.              */
              }
#       endif
#   endif /* !THREADS */
}

GC_INNER void (*GC_push_typed_structures)(void) = 0;

                        /* Push GC internal roots.  These are normally  */
                        /* included in the static data segment, and     */
                        /* Thus implicitly pushed.  But we must do this */
                        /* explicitly if normal root processing is      */
                        /* disabled.                                    */
/*
 * Push GC internal roots.  Only called if there is some reason to believe
 * these would not otherwise get registered.
 */
STATIC void GC_push_gc_structures(void)
{
    GC_push_finalizer_structures();
#   if defined(THREADS)
      GC_push_thread_structures();
#   endif
    if( GC_push_typed_structures )
      GC_push_typed_structures();
}

#ifdef THREAD_LOCAL_ALLOC
  GC_INNER void GC_mark_thread_local_free_lists(void);
#endif

GC_INNER void GC_cond_register_dynamic_libraries(void)
{
# if defined(DYNAMIC_LOADING) || defined(MSWIN32) || defined(MSWINCE) \
     || defined(PCR)
    GC_remove_tmp_roots();
    if (!GC_no_dls) GC_register_dynamic_libraries();
# else
    GC_no_dls = TRUE;
# endif
}

STATIC void GC_push_regs_and_stack(ptr_t cold_gc_frame)
{
    GC_with_callee_saves_pushed(GC_push_current_stack, cold_gc_frame);
}

/*
 * Call the mark routines (GC_tl_push for a single pointer, GC_push_conditional
 * on groups of pointers) on every top level accessible pointer.
 * If all is FALSE, arrange to push only possibly altered values.
 * Cold_gc_frame is an address inside a GC frame that
 * remains valid until all marking is complete.
 * A zero value indicates that it's OK to miss some
 * register values.
 */
GC_INNER void GC_push_roots(GC_bool all, ptr_t cold_gc_frame)
{
    int i;
    unsigned kind;

    /*
     * Next push static data.  This must happen early on, since it's
     * not robust against mark stack overflow.
     */
     /* Re-register dynamic libraries, in case one got added.           */
     /* There is some argument for doing this as late as possible,      */
     /* especially on win32, where it can change asynchronously.        */
     /* In those cases, we do it here.  But on other platforms, it's    */
     /* not safe with the world stopped, so we do it earlier.           */
#      if !defined(REGISTER_LIBRARIES_EARLY)
         GC_cond_register_dynamic_libraries();
#      endif

     /* Mark everything in static data areas                             */
       for (i = 0; i < n_root_sets; i++) {
         GC_push_conditional_with_exclusions(
                             GC_static_roots[i].r_start,
                             GC_static_roots[i].r_end, all);
       }

     /* Mark all free list header blocks, if those were allocated from  */
     /* the garbage collected heap.  This makes sure they don't         */
     /* disappear if we are not marking from static data.  It also      */
     /* saves us the trouble of scanning them, and possibly that of     */
     /* marking the freelists.                                          */
       for (kind = 0; kind < GC_n_kinds; kind++) {
         void *base = GC_base(GC_obj_kinds[kind].ok_freelist);
         if (0 != base) {
           GC_set_mark_bit(base);
         }
       }

     /* Mark from GC internal roots if those might otherwise have       */
     /* been excluded.                                                  */
       if (GC_no_dls || roots_were_cleared) {
           GC_push_gc_structures();
       }

     /* Mark thread local free lists, even if their mark        */
     /* descriptor excludes the link field.                     */
     /* If the world is not stopped, this is unsafe.  It is     */
     /* also unnecessary, since we will do this again with the  */
     /* world stopped.                                          */
#      if defined(THREAD_LOCAL_ALLOC)
         if (GC_world_stopped) GC_mark_thread_local_free_lists();
#      endif

    /*
     * Now traverse stacks, and mark from register contents.
     * These must be done last, since they can legitimately overflow
     * the mark stack.
     * This is usually done by saving the current context on the
     * stack, and then just tracing from the stack.
     */
      GC_push_regs_and_stack(cold_gc_frame);

    if (GC_push_other_roots != 0) (*GC_push_other_roots)();
        /* In the threads case, this also pushes thread stacks. */
        /* Note that without interior pointer recognition lots  */
        /* of stuff may have been pushed already, and this      */
        /* should be careful about mark stack overflows.        */
}
