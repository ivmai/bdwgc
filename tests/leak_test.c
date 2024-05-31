
#include <stdio.h>
#include <stdlib.h>

#include "leak_detector.h"

#define CHECK_OUT_OF_MEMORY(p) \
    do { \
        if (NULL == (p)) { \
            fprintf(stderr, "Out of memory\n"); \
            exit(69); \
        } \
    } while (0)

int main(void) {
    char *p[10];
    int i;
    GC_set_find_leak(1); /* for new collect versions not compiled       */
                         /* with -DFIND_LEAK.                           */

    GC_INIT();  /* Needed if thread-local allocation is enabled.        */
                /* FIXME: This is not ideal.                            */
    for (i = 0; i < 10; ++i) {
        p[i] = (char*)malloc(sizeof(int)+i);
        CHECK_OUT_OF_MEMORY(p[i]);
        (void)malloc_usable_size(p[i]);
    }
    CHECK_LEAKS();
    for (i = 1; i < 10; ++i) {
        free(p[i]);
    }
    for (i = 0; i < 9; ++i) {
        p[i] = (char*)malloc(sizeof(int)+i);
        CHECK_OUT_OF_MEMORY(p[i]);
    }
    CHECK_LEAKS();
    CHECK_LEAKS();
    CHECK_LEAKS();
    return 0;
}
