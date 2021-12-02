#include "gc.h"
#include <assert.h>
#include <cheriintrin.h>
#include <stdio.h>
#include <string.h>

#define TOTAL_ITERATIONS 32 * 1024

int main(void)
{
	int i;
	int iter_total = 16 * 1024;

	GC_INIT();
	for (i = 0; i < iter_total; ++i)
	{
		int **p = (int **) GC_MALLOC(sizeof(int *));
		int *q = (int *) GC_MALLOC_ATOMIC(sizeof(int));
		assert(*p == 0);
		*p = (int *) GC_REALLOC(q, 4 * sizeof(int));
		if (i % 1024 == 0)
			printf("Heap size = %lu bytes\n", (unsigned long) GC_get_heap_size());
	}
	return 0;
}
