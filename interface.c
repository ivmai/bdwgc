#include "gc.h"
/* These are some additional routines to interface the collector to C     */
/* They were contributed by David Chase (chase@orc.olivetti.com)          */
/* They illustrates the use of non_gc_bytes, and provide an interface to  */
/* the storage allocator's size information.  Note that there is a        */
/* check to guard against 0 length allocations.                           */
/* Hacked by H. Boehm (11/16/89) to accomodate gc_realloc.                */

initialize_allocator() {
  non_gc_bytes = 0;
  gc_init();
}


/* Use of gc_gasp to report errors reduces risk of bizarre
   interactions with I/O system in desperate situations.  */

gc_gasp(s) char * s;
{
  write(2,s,strlen(s));
}


/* This reports how many bytes are actually available to an object.
   It is a fatal error to request the size of memory addressed by a
   pointer not obtained from the storage allocator. */

size_of_obj_in_bytes(p)
     struct obj * p;
{
  register struct hblk * h;
  register int size;
  
  h = HBLKPTR(p);
  
  if (is_hblk(h)) {
    return (HB_SIZE(h))<<2;
  }
  gc_gasp("GC/size_of_obj_in_bytes: requested byte size of non-pointer!\n");
  exit(1);
}


/* This free routine is merely advisory -- it reduces the estimate of
   storage that won't be reclaimed in the next collection, thus
   making it more likely that the collector will run next time more
   memory is needed. */

void free(p) {
  int inc = size_of_obj_in_bytes(p);
  non_gc_bytes -= inc;
}

/* This free routine adjusts the collector estimates of space in use,
   but also actually releases the memory for reuse.  It is thus "unsafe"
   if the programmer "frees" memory that is actually still in use.  */

void unsafe_free(p) {
  int inc = size_of_obj_in_bytes(p);
  non_gc_bytes -= inc;
  gc_free(p);
}


/* malloc and malloc_atomic are obvious substitutes for the C library
   malloc.  Note that the storage so allocated is regarded as not likely
   to be reclaimed by the collector (until explicitly freed), and thus
   its size is added to non_gc_bytes.
*/

word malloc(bytesize) {
word result;
if (bytesize == 0) bytesize = 4;
result = (word) gc_malloc (bytesize);
non_gc_bytes += (bytesize + 3) & ~3;
return result;
}

word malloc_atomic(bytesize) {
word result;
if (bytesize == 0) bytesize = 4;
result = (word) gc_malloc_atomic (bytesize);
non_gc_bytes += (bytesize + 3) & ~3;
return result;
}

word realloc(old,size) word old,size; {
    int inc = size_of_obj_in_bytes(old);

    non_gc_bytes += ((size + 3) & ~3) - inc;
    return(gc_realloc(old, size);
    }

