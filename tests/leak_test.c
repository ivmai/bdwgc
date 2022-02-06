
#include <stdio.h>

#include "gc/leak_detector.h"

#define N_TESTS 100

int main(void) {
    char *p[N_TESTS];
    int i;
    GC_set_find_leak(1); /* for new collect versions not compiled       */
                         /* with -DFIND_LEAK.                           */

    GC_INIT();  /* Needed if thread-local allocation is enabled.        */
                /* FIXME: This is not ideal.                            */
    for (i = 0; i < N_TESTS; ++i) {
        p[i] = (char*)malloc(sizeof(int)+i);
    }
    CHECK_LEAKS();
    for (i = 1; i < N_TESTS; ++i) {
        free(p[i]);
    }
    for (i = 0; i < N_TESTS / 2; ++i) {
        p[i] = (char*)malloc(sizeof(int)+i);
    }
    CHECK_LEAKS();
    CHECK_LEAKS();
    CHECK_LEAKS();
    printf("SUCCEEDED\n");
    return 0;
}
