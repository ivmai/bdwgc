/*
 * Copyright (c) 2011 by Hewlett-Packard Company.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */

/* Test that objects reachable from an object allocated with            */
/* GC_malloc_with_finalizer is not reclaimable before the finalizer     */
/* is called.                                                           */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "gc_disclaim.h"

typedef struct pair_s *pair_t;

struct pair_s {
    int is_valid;
    int checksum;
    pair_t car;
    pair_t cdr;
};

void
pair_dct(void *obj, void *cd)
{
    pair_t p = obj;
    int checksum;

    /* Check that obj and its car and cdr are not trashed. */
#   ifdef DEBUG_DISCLAIM_DESTRUCT
      printf("Destruct %p = (%p, %p)\n", p, p->car, p->cdr);
#   endif
    assert(GC_base(obj));
    assert(p->is_valid);
    assert(!p->car || p->car->is_valid);
    assert(!p->cdr || p->cdr->is_valid);
    checksum = 782;
    if (p->car) checksum += p->car->checksum;
    if (p->cdr) checksum += p->cdr->checksum;
    assert(p->checksum == checksum);

    /* Invalidate it. */
    p->is_valid = 0;
    p->checksum = 0;
    p->car = NULL;
    p->cdr = NULL;
}

pair_t
pair_new(pair_t car, pair_t cdr)
{
    pair_t p;
    static struct GC_finalizer_closure fc = { pair_dct, NULL };

    p = GC_finalized_malloc(sizeof(struct pair_s), &fc);
    p->is_valid = 1;
    p->checksum = 782 + (car? car->checksum : 0) + (cdr? cdr->checksum : 0);
    p->car = car;
    p->cdr = cdr;
#   ifdef DEBUG_DISCLAIM_DESTRUCT
      printf("Construct %p = (%p, %p)\n", p, p->car, p->cdr);
#   endif
    return p;
}

void
pair_check_rec(pair_t p)
{
    while (p) {
        int checksum = 782;
        if (p->car) checksum += p->car->checksum;
        if (p->cdr) checksum += p->cdr->checksum;
        assert(p->checksum == checksum);
        if (rand() % 2)
            p = p->car;
        else
            p = p->cdr;
    }
}

#ifdef GC_PTHREADS
#  define THREAD_CNT 6
#else
#  define THREAD_CNT 1
#endif

#define POP_SIZE 1000
#if THREAD_CNT > 1
#  define MUTATE_CNT 2000000/THREAD_CNT
#else
#  define MUTATE_CNT 10000000
#endif
#define GROW_LIMIT 10000000

void *test(void *data)
{
    int i;
    pair_t pop[POP_SIZE];
    memset(pop, 0, sizeof(pop));
    for (i = 0; i < MUTATE_CNT; ++i) {
        int t = rand() % POP_SIZE;
        switch (rand() % (i > GROW_LIMIT? 5 : 3)) {
        case 0: case 3:
            if (pop[t])
                pop[t] = pop[t]->car;
            break;
        case 1: case 4:
            if (pop[t])
                pop[t] = pop[t]->cdr;
            break;
        case 2:
            pop[t] = pair_new(pop[rand() % POP_SIZE],
                              pop[rand() % POP_SIZE]);
            break;
        }
        if (rand() % 8 == 1)
            pair_check_rec(pop[rand() % POP_SIZE]);
    }
    return 0;
}

int main(void)
{
#if THREAD_CNT > 1
    pthread_t th[THREAD_CNT];
    int i;
#endif

    GC_INIT();
    GC_init_finalized_malloc();

#if THREAD_CNT > 1
    printf("Threaded disclaim test.\n");
    for (i = 0; i < THREAD_CNT; ++i) {
        // FIXME: this is not available on Win32 without pthreads
        // FIXME: Should GC_ suffix be used?
        int err = pthread_create(&th[i], NULL, test, NULL);
        if (err) {
            fprintf(stderr, "Failed to create thread # %d: %s\n", i,
                    strerror(err));
            exit(1);
        }
    }
    for (i = 0; i < THREAD_CNT; ++i) {
        // FIXME: Should GC_ suffix be used?
        // FIXME: Check error code.
        pthread_join(th[i], NULL);
    }
#else
    printf("Unthreaded disclaim test.\n");
    test(NULL);
#endif
    return 0;
}
