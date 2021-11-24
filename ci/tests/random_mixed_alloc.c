#include "gc.h"
#include <assert.h>
#include <cheriintrin.h>
#include <stdio.h>
#include <string.h>

#define INIT_SEED 42
#define TOTAL_ITERATIONS (32 * 1024)

int main(void)
{
	int i, j;

	srand(INIT_SEED);
	GC_INIT();
	for (i = 0; i < TOTAL_ITERATIONS; ++i)
	{
		unsigned int random_size = rand() % 1024;
		size_t sz = random_size * sizeof(int *);
		int **p = (int **) GC_MALLOC(sz);
		assert(*p == 0);
		if (i % 1024 == 0)
			printf("Heap size = %lu bytes\n", (unsigned long) GC_get_heap_size());
	}
	return 0;
}
