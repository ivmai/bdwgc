/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1995 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1997 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1999 by Hewlett-Packard Company.  All rights reserved.
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
 * This is mostly an internal header file.  Typical clients should
 * not use it.  Clients that define their own object kinds with
 * debugging allocators will probably want to include this, however.
 * No attempt is made to keep the namespace clean.  This should not be
 * included from header files that are frequently included by clients.
 */

#ifndef GC_DBG_MLC_H
#define GC_DBG_MLC_H

#include "gc_priv.h"
#ifdef KEEP_BACK_PTRS
# include "gc/gc_backptr.h"
#endif

EXTERN_C_BEGIN

#ifndef GC_FREED_MEM_MARKER
# if CPP_WORDSZ == 32
#   define GC_FREED_MEM_MARKER (GC_uintptr_t)0xdeadbeef
# else
#   define GC_FREED_MEM_MARKER (GC_uintptr_t)GC_WORD_C(0xEFBEADDEdeadbeef)
# endif
#endif /* !GC_FREED_MEM_MARKER */

#if CPP_WORDSZ == 32
# define START_FLAG (GC_uintptr_t)0xfedcedcb
# define END_FLAG (GC_uintptr_t)0xbcdecdef
#else
# define START_FLAG (GC_uintptr_t)GC_WORD_C(0xFEDCEDCBfedcedcb)
# define END_FLAG (GC_uintptr_t)GC_WORD_C(0xBCDECDEFbcdecdef)
#endif
        /* Stored both one past the end of user object, and one before  */
        /* the end of the object as seen by the allocator.              */

#if defined(KEEP_BACK_PTRS) || defined(PRINT_BLACK_LIST)
  /* Pointer "source"s that aren't real locations.      */
  /* Used in oh_back_ptr fields and as "source"         */
  /* argument to some marking functions.                */
# define MARKED_FOR_FINALIZATION ((ptr_t)(GC_uintptr_t)2)
                /* Object was marked because it is finalizable. */
# define MARKED_FROM_REGISTER ((ptr_t)(GC_uintptr_t)4)
                /* Object was marked from a register.  Hence the        */
                /* source of the reference doesn't have an address.     */
# define NOT_MARKED ((ptr_t)(GC_uintptr_t)8)
#endif /* KEEP_BACK_PTRS || PRINT_BLACK_LIST */

/* Object debug header. */
typedef struct {
# if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
    /* We potentially keep two different kinds of back          */
    /* pointers.  KEEP_BACK_PTRS stores a single back           */
    /* pointer in each reachable object to allow reporting      */
    /* of why an object was retained.  MAKE_BACK_GRAPH          */
    /* builds a graph containing the inverse of all             */
    /* "points-to" edges including those involving              */
    /* objects that have just become unreachable. This          */
    /* allows detection of growing chains of unreachable        */
    /* objects.  It may be possible to eventually combine       */
    /* both, but for now we keep them separate.  Both           */
    /* kinds of back pointers are hidden using the              */
    /* following macros.  In both cases, the plain version      */
    /* is constrained to have the least significant bit of 1,   */
    /* to allow it to be distinguished from a free-list         */
    /* link.  This means the plain version must have the least  */
    /* significant bit of zero.  Note that blocks dropped by    */
    /* black-listing will also have the the least significant   */
    /* bit clear once debugging has started; we are careful     */
    /* never to overwrite such a value.                         */
#   if ALIGNMENT == 1
      /* Fudge back pointer to be even. */
#     define HIDE_BACK_PTR(p) \
                GC_HIDE_POINTER((ptr_t)(~(GC_uintptr_t)1 & (GC_uintptr_t)(p)))
#   else
#     define HIDE_BACK_PTR(p) GC_HIDE_POINTER(p)
#   endif
    GC_hidden_pointer oh_back_ptr;
    GC_hidden_pointer oh_bg_ptr;
                        /* Always define either none or both of the     */
                        /* fields to ensure double-pointer alignment.   */
# endif
  const char * oh_string;       /* object descriptor string (file name)    */
  signed_word oh_int;           /* object descriptor integer (line number) */
# ifdef NEED_CALLINFO
    struct callinfo oh_ci[NFRAMES];
# endif
# ifndef SHORT_DBG_HDRS
    GC_uintptr_t oh_sz;                 /* Original malloc argument.    */
    GC_uintptr_t oh_sf;                 /* Start flag.                  */
# endif
} oh;
/* The size of the above structure is assumed not to de-align things,   */
/* and to be a multiple of a double-pointer length.                     */

#ifdef SHORT_DBG_HDRS
# define DEBUG_BYTES sizeof(oh)
# define UNCOLLECTABLE_DEBUG_BYTES DEBUG_BYTES
#else
  /* Add space for END_FLAG, but use any extra space that was already   */
  /* added to catch off-the-end pointers.                               */
  /* For uncollectible objects, the extra byte is not added.            */
# define UNCOLLECTABLE_DEBUG_BYTES (sizeof(oh) + sizeof(word))
# define DEBUG_BYTES (UNCOLLECTABLE_DEBUG_BYTES - EXTRA_BYTES)
#endif

/* Round bytes to words without adding extra byte at end.       */
#define SIMPLE_ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1) - 1)

/* ADD_CALL_CHAIN stores a (partial) call chain into an object  */
/* header; it should be called with the allocator lock held.    */
/* PRINT_CALL_CHAIN prints the call chain stored in an object   */
/* to stderr; it requires we do not hold the allocator lock.    */
#if defined(SAVE_CALL_CHAIN)
# define ADD_CALL_CHAIN(base, ra) GC_save_callers(((oh *)(base)) -> oh_ci)
# if defined(REDIRECT_MALLOC) && defined(THREADS) && defined(DBG_HDRS_ALL) \
     && NARGS == 0 && NFRAMES % 2 == 0 && defined(GC_HAVE_BUILTIN_BACKTRACE)
    GC_INNER void GC_save_callers_no_unlock(struct callinfo info[NFRAMES]);
#   define ADD_CALL_CHAIN_INNER(base) \
                    GC_save_callers_no_unlock(((oh *)(base)) -> oh_ci)
# endif
# define PRINT_CALL_CHAIN(base) GC_print_callers(((oh *)(base)) -> oh_ci)
#elif defined(GC_ADD_CALLER)
# define ADD_CALL_CHAIN(base, ra) ((oh *)(base)) -> oh_ci[0].ci_pc = (ra)
# define PRINT_CALL_CHAIN(base) GC_print_callers(((oh *)(base)) -> oh_ci)
#else
# define ADD_CALL_CHAIN(base, ra)
# define PRINT_CALL_CHAIN(base)
#endif

#if !defined(ADD_CALL_CHAIN_INNER) && defined(DBG_HDRS_ALL)
  /* A variant of ADD_CALL_CHAIN() used for internal allocations.   */
# define ADD_CALL_CHAIN_INNER(base) ADD_CALL_CHAIN(base, GC_RETURN_ADDR)
#endif

#ifdef GC_ADD_CALLER
# define OPT_RA ra,
#else
# define OPT_RA
#endif

/* Check whether object with base pointer p has debugging info  */
/* p is assumed to point to a legitimate object in our part     */
/* of the heap.                                                 */
#ifdef SHORT_DBG_HDRS
# define GC_has_other_debug_info(base) 1
#else
  GC_INNER int GC_has_other_debug_info(ptr_t base);
#endif

#if defined(KEEP_BACK_PTRS) || defined(MAKE_BACK_GRAPH)
# if defined(SHORT_DBG_HDRS) && !defined(CPPCHECK)
#   error Non-ptr stored in object results in GC_HAS_DEBUG_INFO malfunction
    /* We may mistakenly conclude that base has a debugging wrapper.    */
# endif
# if defined(PARALLEL_MARK) && defined(KEEP_BACK_PTRS)
#   define GC_HAS_DEBUG_INFO(base) \
                ((AO_load((volatile AO_t *)(base)) & 1) != 0 \
                 && GC_has_other_debug_info(base) > 0)
                        /* Atomic load is used as GC_store_back_pointer */
                        /* stores oh_back_ptr atomically (base might    */
                        /* point to the field); this prevents a TSan    */
                        /* warning.                                     */
# else
#   define GC_HAS_DEBUG_INFO(base) \
                ((*(GC_uintptr_t *)(base) & 1) != 0 \
                 && GC_has_other_debug_info(base) > 0)
# endif
#else
# define GC_HAS_DEBUG_INFO(base) (GC_has_other_debug_info(base) > 0)
#endif /* !KEEP_BACK_PTRS && !MAKE_BACK_GRAPH */

EXTERN_C_END

#endif /* GC_DBG_MLC_H */
