#include "gc_private.h"
#include <stddef.h>

/* These are some additional routines to interface the collector to C     */
/* This is a rather special purpose interface that tries to keep down the */
/* number of collections in the presence of explicit deallocations.	  */
/* A call to this malloc effectively declares that the resulting object   */
/* will be explicitly deallocated with very high probability.		  */
/* The reduced collection frequency may interfere with object 		  */
/* coalescing.								  */
/* If you just want to rename GC_malloc and friends, this is NOT	  */
/* the right way to do it.						  */

/* This contributed by David Chase (chase@eng.sun.com) a long time	  */
/* ago. Much of its original functionality has since been absorbed	  */
/* elsewhere.								  */
/* They illustrates the use of GC_non_gc_bytes 				  */
/* Hacked by H. Boehm (11/16/89) to accomodate GC_realloc.                */
/* Further updated (2/20/92) to reflect changes in interfaces and data	  */
/* structures.								  */
/* Further updated (8/25/92) to correct previously introduced bugs and 	  */
/* make it compile with semi-modern compilers.				  */
/* Note that extern_ptr_t is either void * or char *, as appropriate.     */


/* This free routine is merely advisory -- it reduces the estimate of
   storage that won't be reclaimed in the next collection, thus
   making it more likely that the collector will run next time more
   memory is needed. */

void free(p)
extern_ptr_t p;
{
  size_t inc = GC_size(p);
  GC_non_gc_bytes -= inc;
}

/* This free routine adjusts the collector estimates of space in use,
   but also actually releases the memory for reuse.  It is thus "unsafe"
   if the programmer "frees" memory that is actually still in use.  */

void unsafe_free(p)
extern_ptr_t p;
{
  size_t inc = GC_size(p);
  GC_non_gc_bytes -= inc;
  GC_free(p);
}


/* malloc and malloc_atomic are obvious substitutes for the C library
   malloc.  Note that the storage so allocated is regarded as not likely
   to be reclaimed by the collector (until explicitly freed), and thus
   its size is added to non_gc_bytes.
*/

extern_ptr_t malloc(bytesize)
size_t bytesize;
{
  extern_ptr_t result;
  
  result = (extern_ptr_t) GC_malloc (bytesize);
  GC_non_gc_bytes += (bytesize + 3) & ~3;
  return result;
}

extern_ptr_t malloc_atomic(bytesize)
size_t bytesize;
{
  extern_ptr_t result;
  
  result = (extern_ptr_t) GC_malloc_atomic (bytesize);
  GC_non_gc_bytes += (bytesize + 3) & ~3;
  return result;
}

extern_ptr_t realloc(old,size)
extern_ptr_t old;
size_t size;
{
  int inc = GC_size(old);

  GC_non_gc_bytes += ((size + 3) & ~3) - inc;
  return(GC_realloc(old, size));
}

