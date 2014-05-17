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
/* Boehm, May 19, 1994 2:12 pm PDT */
 
# ifndef GC_PRIVATE_H
#   include "gc_priv.h"
# endif

/* Allocate n words (NOT BYTES).  X is made to point to the result.	*/
/* It is assumed that n < MAXOBJSZ, and					*/
/* that n > 0.  On machines requiring double word alignment of some	*/
/* data, we also assume that n is 1 or even.  This bypasses the		*/
/* MERGE_SIZES mechanism.  In order to minimize the number of distinct	*/
/* free lists that are maintained, the caller should ensure that a 	*/
/* small number of distinct values of n are used.  (The MERGE_SIZES	*/
/* mechanism normally does this by ensuring that only the leading three	*/
/* bits of n may be nonzero.  See misc.c for details.)  We really 	*/
/* recommend this only in cases in which n is a constant, and no	*/
/* locking is required.							*/
/* In that case it may allow the compiler to perform substantial	*/
/* additional optimizations.						*/
# define GC_MALLOC_WORDS(result,n) \
{	\
    register ptr_t op;	\
    register ptr_t *opp;	\
    DCL_LOCK_STATE;	\
	\
    opp = &(GC_objfreelist[n]);	\
    FASTLOCK();	\
    if( !FASTLOCK_SUCCEEDED() || (op = *opp) == 0 ) {	\
        FASTUNLOCK();	\
        (result) = GC_generic_malloc_words_small((n), NORMAL);	\
    } else { 	\
        *opp = obj_link(op);	\
        obj_link(op) = 0;	\
        GC_words_allocd += (n);	\
        FASTUNLOCK();	\
        (result) = (extern_ptr_t) op;	\
    }	\
}


/* The same for atomic objects:	*/
# define GC_MALLOC_ATOMIC_WORDS(result,n) \
{	\
    register ptr_t op;	\
    register ptr_t *opp;	\
    DCL_LOCK_STATE;	\
	\
    opp = &(GC_aobjfreelist[n]);	\
    FASTLOCK();	\
    if( !FASTLOCK_SUCCEEDED() || (op = *opp) == 0 ) {	\
        FASTUNLOCK();	\
        (result) = GC_generic_malloc_words_small((n), PTRFREE);	\
    } else { 	\
        *opp = obj_link(op);	\
        obj_link(op) = 0;	\
        GC_words_allocd += (n);	\
        FASTUNLOCK();	\
        (result) = (extern_ptr_t) op;	\
    }	\
}

/* And once more for two word initialized objects: */
# define GC_CONS(result, first, second) \
{	\
    register ptr_t op;	\
    register ptr_t *opp;	\
    DCL_LOCK_STATE;	\
	\
    opp = &(GC_objfreelist[2]);	\
    FASTLOCK();	\
    if( !FASTLOCK_SUCCEEDED() || (op = *opp) == 0 ) {	\
        FASTUNLOCK();	\
        op = GC_generic_malloc_words_small(2, NORMAL);	\
    } else {	\
        *opp = obj_link(op);	\
        GC_words_allocd += 2;	\
        FASTUNLOCK();	\
    } \
    ((word *)op)[0] = (word)(first);	\
    ((word *)op)[1] = (word)(second);	\
    (result) = (extern_ptr_t) op;	\
}
