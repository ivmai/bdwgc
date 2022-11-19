/*
 * Test at the boundary between small and large objects.
 * Inspired by a test case from Zoltan Varga.
 */
#include "gc.h"
#include <stdio.h>

#define N_TESTS 40000
#define ALLOC_SZ 4096 /* typical page size */

int main (void)
{
  int i;

  GC_set_all_interior_pointers(0);
  GC_INIT();
  if (GC_get_find_leak())
    printf("This test program is not designed for leak detection mode\n");

  for (i = 0; i < N_TESTS; ++i) {
    (void)GC_malloc_atomic(ALLOC_SZ);
    (void)GC_malloc(ALLOC_SZ);
  }

  /* Test delayed start of marker threads, if they are enabled. */
  GC_start_mark_threads();

  for (i = 0; i < N_TESTS; ++i) {
    (void)GC_malloc_atomic(ALLOC_SZ/2);
    (void)GC_malloc(ALLOC_SZ/2);
  }
  printf("Final heap size is %lu\n", (unsigned long)GC_get_heap_size());
  return 0;
}
