
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#ifndef GC_IGNORE_WARN
  /* Ignore misleading "Out of Memory!" warning (which is printed on    */
  /* every GC_MALLOC call below) by defining this macro before "gc.h"   */
  /* inclusion.                                                         */
# define GC_IGNORE_WARN
#endif

#ifndef GC_MAXIMUM_HEAP_SIZE
# define GC_MAXIMUM_HEAP_SIZE 100 * 1024 * 1024
# define GC_INITIAL_HEAP_SIZE GC_MAXIMUM_HEAP_SIZE / 20
    /* Otherwise heap expansion aborts when deallocating large block.   */
    /* That's OK.  We test this corner case mostly to make sure that    */
    /* it fails predictably.                                            */
#endif

#include "gc.h"

/*
 * Check that very large allocation requests fail.  "Success" would usually
 * indicate that the size was somehow converted to a negative
 * number.  Clients shouldn't do this, but we should fail in the
 * expected manner.
 */

#define CHECK_ALLOC_FAILED(r, sz_str) \
  do { \
    if (NULL != (r)) { \
        fprintf(stderr, \
                "Size " sz_str " allocation unexpectedly succeeded\n"); \
        exit(1); \
    } \
  } while (0)

#define GC_SWORD_MAX ((GC_signed_word)(((GC_word)-1) >> 1))

int main(void)
{
    GC_INIT();

    CHECK_ALLOC_FAILED(GC_MALLOC(GC_SWORD_MAX - 1024), "SWORD_MAX-1024");
    CHECK_ALLOC_FAILED(GC_MALLOC(GC_SWORD_MAX), "SWORD_MAX");
    CHECK_ALLOC_FAILED(GC_MALLOC((GC_word)GC_SWORD_MAX + 1024),
                       "SWORD_MAX+1024");
    return 0;
}
