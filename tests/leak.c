
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
        p[i] = i > 0 ? (char*)malloc(sizeof(int)+i)
                     : strdup("abc");
    }
    CHECK_LEAKS();
    for (i = 3; i < N_TESTS / 2; ++i) {
        p[i] = (char*)((i & 1) != 0 ? reallocarray(p[i], i, 43)
                                    : realloc(p[i], i * 16 + 1));
    }
    CHECK_LEAKS();
    for (i = 2; i < N_TESTS; ++i) {
        free(p[i]);
    }
    for (i = 0; i < N_TESTS / 8; ++i) {
        p[i] = i < 3 || i > 6 ? (char*)malloc(sizeof(int)+i)
                      : strndup("abcd", (unsigned)i);
    }
    CHECK_LEAKS();
    CHECK_LEAKS();
    CHECK_LEAKS();
    printf("SUCCEEDED\n");
    return 0;
}
