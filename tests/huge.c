
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef GC_IGNORE_WARN
/* Ignore misleading "Out of Memory!" warning (which is printed on    */
/* every GC_MALLOC call below) by defining this macro before "gc.h"   */
/* inclusion.                                                         */
#  define GC_IGNORE_WARN
#endif

#ifndef GC_MAXIMUM_HEAP_SIZE
#  define GC_MAXIMUM_HEAP_SIZE (100 * 1024 * 1024)
#  define GC_INITIAL_HEAP_SIZE (GC_MAXIMUM_HEAP_SIZE / 20)
/* Otherwise heap expansion aborts when deallocating large block.   */
/* That's OK.  We test this corner case mostly to make sure that    */
/* it fails predictably.                                            */
#endif

#ifndef GC_ATTR_ALLOC_SIZE
/* Omit alloc_size attribute to avoid compiler warnings about         */
/* exceeding maximum object size when values close to GC_SWORD_MAX    */
/* are passed to GC_MALLOC.                                           */
#  define GC_ATTR_ALLOC_SIZE(argnum) /* empty */
#endif

#include "gc.h"

/*
 * Check that very large allocation requests fail.  "Success" would usually
 * indicate that the size was somehow converted to a negative
 * number.  Clients shouldn't do this, but we should fail in the
 * expected manner.
 */

#define CHECK_ALLOC_FAILED(r, sz_str)                                         \
  do {                                                                        \
    if (NULL != (r)) {                                                        \
      fprintf(stderr, "Size " sz_str " allocation unexpectedly succeeded\n"); \
      exit(1);                                                                \
    }                                                                         \
  } while (0)

#undef SIZE_MAX
#define SIZE_MAX (~(size_t)0)

#define U_SSIZE_MAX (SIZE_MAX >> 1) /* unsigned */

int
main(void)
{
  GC_INIT();

  CHECK_ALLOC_FAILED(GC_MALLOC(U_SSIZE_MAX - 1024), "SSIZE_MAX-1024");
  CHECK_ALLOC_FAILED(GC_MALLOC(U_SSIZE_MAX), "SSIZE_MAX");
  /* Skip other checks to avoid "exceeds maximum object size" gcc warning. */
#if !defined(_FORTIFY_SOURCE)
  CHECK_ALLOC_FAILED(GC_MALLOC(U_SSIZE_MAX + 1), "SSIZE_MAX+1");
  CHECK_ALLOC_FAILED(GC_MALLOC(U_SSIZE_MAX + 1024), "SSIZE_MAX+1024");
  CHECK_ALLOC_FAILED(GC_MALLOC(SIZE_MAX - 1024), "SIZE_MAX-1024");
  CHECK_ALLOC_FAILED(GC_MALLOC(SIZE_MAX - 16), "SIZE_MAX-16");
  CHECK_ALLOC_FAILED(GC_MALLOC(SIZE_MAX - 8), "SIZE_MAX-8");
  CHECK_ALLOC_FAILED(GC_MALLOC(SIZE_MAX - 4), "SIZE_MAX-4");
  CHECK_ALLOC_FAILED(GC_MALLOC(SIZE_MAX), "SIZE_MAX");
#endif
  printf("SUCCEEDED\n");
  return 0;
}
