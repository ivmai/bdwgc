
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#ifndef GC_IGNORE_WARN
  /* Ignore misleading "Out of Memory!" warning (which is printed on    */
  /* every GC_MALLOC call below) by defining this macro before "gc.h"   */
  /* inclusion.                                                         */
# define GC_IGNORE_WARN
#endif

#include "gc.h"

/*
 * Check that very large allocation requests fail.  "Success" would usually
 * indicate that the size was somehow converted to a negative
 * number.  Clients shouldn't do this, but we should fail in the
 * expected manner.
 */

#define GC_SWORD_MAX ((GC_signed_word)(((GC_word)-1) >> 1))

int main(void)
{
    void *r;
    GC_INIT();

    GC_set_max_heap_size(100*1024*1024);
        /* Otherwise heap expansion aborts when deallocating large block. */
        /* That's OK.  We test this corner case mostly to make sure that  */
        /* it fails predictably.                                          */
    GC_expand_hp(1024*1024*5);
    r = GC_MALLOC(GC_SWORD_MAX - 1024);
    if (NULL != r) {
        fprintf(stderr,
                "Size SWORD_MAX-1024 allocation unexpectedly succeeded\n");
        exit(1);
    }
    r = GC_MALLOC(GC_SWORD_MAX);
    if (NULL != r) {
        fprintf(stderr,
                "Size SWORD_MAX allocation unexpectedly succeeded\n");
        exit(1);
    }
    r = GC_MALLOC((GC_word)GC_SWORD_MAX + 1024);
    if (NULL != r) {
        fprintf(stderr,
                "Size SWORD_MAX+1024 allocation unexpectedly succeeded\n");
        exit(1);
    }
    return 0;
}
