/*
 * Some simple primitives for allocation with explicit type information.
 * Facilities for dynamic type inference may be added later.
 * Should be used only for extremely performance critical applications,
 * or if conservative collector leakage is otherwise a problem (unlikely).
 * Note that this is implemented completely separately from the rest
 * of the collector, and is not linked in unless referenced.
 */
/* Boehm, March 31, 1994 4:43 pm PST */

#ifndef _GC_TYPED_H
# define _GC_TYPED_H
# ifndef _GC_H
#   include "gc.h"
# endif

typedef GC_word * GC_bitmap;
	/* The least significant bit of the first word is one if	*/
	/* the first word in the object may be a pointer.		*/
	
# define GC_get_bit(bm, index) \
		(((bm)[divWORDSZ(index)] >> modWORDSZ(index)) & 1)
# define GC_set_bit(bm, index) \
		(bm)[divWORDSZ(index)] |= (word)1 << modWORDSZ(index)

typedef GC_word GC_descr;

#if defined(__STDC__) || defined(__cplusplus)
  extern GC_descr GC_make_decriptor(GC_bitmap bm, size_t len);
#else
  extern GC_descr GC_make_decriptor(/* GC_bitmap bm, size_t len */);
#endif
		/* Return a type descriptor for the object whose layout	*/
		/* is described by the argument.			*/
		/* The least significant bit of the first word is one	*/
		/* if the first word in the object may be a pointer.	*/
		/* The second argument specifies the number of		*/
		/* meaningful bits in the bitmap.  The actual object 	*/
		/* may be larger (but not smaller).  Any additional	*/
		/* words in the object are assumed not to contain 	*/
		/* pointers.						*/
		/* Returns (GC_descr)(-1) on failure (no memory).	*/

#if defined(__STDC__) || defined(__cplusplus)
  extern void * GC_malloc_explicitly_typed(size_t size_in_bytes, GC_descr d);
#else
  extern char * GC_malloc_explicitly_typed(/* size_in_bytes, descriptor */);
#endif
		/* Allocate an object whose layout is described by d.	*/
		/* The resulting object MAY NOT BE PASSED TO REALLOC.	*/
		
#if defined(__STDC__) || defined(__cplusplus)
  extern void * GC_calloc_explicitly_typed(size_t nelements,
  					   size_t element_size_in_bytes,
  					   GC_descr d);
#else
  char * GC_calloc_explicitly_typed(/* nelements, size_in_bytes, descriptor */);
  	/* Allocate an array of nelements elements, each of the	*/
  	/* given size, and with the given descriptor.		*/
  	/* The elemnt size must be a multiple of the byte	*/
  	/* alignment required for pointers.  E.g. on a 32-bit	*/
  	/* machine with 16-bit aligned pointers, size_in_bytes	*/
  	/* must be a multiple of 2.				*/
#endif

#endif /* _GC_TYPED_H */

