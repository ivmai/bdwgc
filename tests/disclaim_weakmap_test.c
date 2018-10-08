/*
 * Copyright (c) 2018 Petter A. Urkedal
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/* This tests a case where disclaim notifiers sometimes return non-zero */
/* in order to protect objects from collection.                         */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
  /* For GC_[P]THREADS */
# include "config.h"
#endif

#include "private/dbg_mlc.h" /* for oh type */
#include "private/gc_atomic_ops.h"
#include "gc.h"
#include "gc_disclaim.h"
#include "gc_mark.h"

#define THREAD_CNT 8
#define POP_SIZE 200
#define MUTATE_CNT (5000000/THREAD_CNT)
#define GROW_LIMIT (MUTATE_CNT/10)

#define WEAKMAP_CAPACITY 256
#define WEAKMAP_MUTEX_COUNT 32

void
dief(int ec, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, "\n");
    exit(ec);
}

#define my_assert(e) \
    if (!(e)) { \
        fflush(stdout); \
        fprintf(stderr, "Assertion failure, line %d: %s\n", __LINE__, #e); \
        exit(70); \
    }

void out_of_memory() { dief(69, "Out of memory."); }

unsigned int
memhash(void *src, size_t len)
{
    unsigned int acc = 0;
    size_t i;
    my_assert(len % sizeof(GC_word) == 0);
    for (i = 0; i < len / sizeof(GC_word); ++i)
        acc = (2003*acc + ((GC_word *)src)[i]) / 3;
    return acc;
}

static void **_weakobj_free_list;
static unsigned int _weakobj_kind;

static volatile AO_t stat_added = 0;
static volatile AO_t stat_found = 0;
static volatile AO_t stat_removed = 0;
static volatile AO_t stat_skip_locked = 0;
static volatile AO_t stat_skip_marked = 0;

struct weakmap_link {
    GC_hidden_pointer obj;
    struct weakmap_link *next;
};

struct weakmap {
    pthread_mutex_t mutex[WEAKMAP_MUTEX_COUNT];
    size_t key_size, obj_size, capacity;
    struct weakmap_link **links;
};

void
weakmap_lock(struct weakmap *wm, unsigned int h)
{
    int err = pthread_mutex_lock(&wm->mutex[h % WEAKMAP_MUTEX_COUNT]);
    if (err != 0)
        dief(69, "pthread_mutex_lock: %s", strerror(err));
}

int
weakmap_trylock(struct weakmap *wm, unsigned int h)
{
    int err = pthread_mutex_trylock(&wm->mutex[h % WEAKMAP_MUTEX_COUNT]);
    if (err != 0 && err != EBUSY)
        dief(69, "pthread_mutex_trylock: %s", strerror(err));
    return err;
}

void
weakmap_unlock(struct weakmap *wm, unsigned int h)
{
    int err = pthread_mutex_unlock(&wm->mutex[h % WEAKMAP_MUTEX_COUNT]);
    if (err != 0)
        dief(69, "pthread_mutex_unlock: %s", strerror(err));
}

static void *set_mark_bit(void *obj) { GC_set_mark_bit(obj); return NULL; }

void *
weakmap_add(struct weakmap *wm, void *obj)
{
    struct weakmap_link *link, *new_link, **first;
    GC_word *new_base;
    void *new_obj;
    unsigned int h;

    /* Lock and look for an existing entry. */
    h = memhash(obj, wm->key_size);
    first = &wm->links[h % wm->capacity];
    weakmap_lock(wm, h);
    for (link = *first; link != NULL; link = link->next) {
        void *old_obj = GC_REVEAL_POINTER(link->obj);
        if (memcmp(old_obj, obj, wm->key_size) == 0) {
            GC_call_with_alloc_lock(set_mark_bit, (GC_word *)old_obj - 1);
            /* Pointers in the key part may have been freed and reused, */
            /* changing the keys without memcmp noticing.  This is okay */
            /* as long as we update the mapped value.                   */
            if (memcmp((char *)old_obj + wm->key_size,
                       (char *)obj + wm->key_size,
                       wm->obj_size - wm->key_size) != 0)
                memcpy((char *)old_obj + wm->key_size,
                       (char *)obj + wm->key_size,
                       wm->obj_size - wm->key_size);
            weakmap_unlock(wm, h);
#           ifdef DEBUG_DISCLAIM_WEAKMAP
              printf("Found %p %#x.\n", old_obj, h);
#           endif
            AO_fetch_and_add1(&stat_found);
            return old_obj;
        }
    }

    /* Create new object. */
    new_base = (GC_word *)GC_generic_malloc(sizeof(GC_word) + wm->obj_size,
                                            _weakobj_kind);
    if (!new_base) out_of_memory();
    *new_base = (GC_word)wm | 1;
    new_obj = (void *)(new_base + 1);
    memcpy(new_obj, obj, wm->obj_size);

    /* Add the object to the map. */
    new_link = GC_NEW(struct weakmap_link);
    if (!new_link) out_of_memory();
    new_link->obj = GC_HIDE_POINTER(new_obj);
    new_link->next = *first;
    *first = new_link;

    weakmap_unlock(wm, h);
#   ifdef DEBUG_DISCLAIM_WEAKMAP
      printf("Added %p %#x.\n", new_obj, h);
#   endif
    AO_fetch_and_add1(&stat_added);
    return new_obj;
}

int
weakmap_disclaim(void *obj_base)
{
    struct weakmap *wm;
    struct weakmap_link **link;
    GC_word hdr;
    void *obj;
    unsigned int h;

    /* Decode header word. */
    hdr = *(GC_word *)obj_base;
    if ((hdr & 1) == 0) return 0;        /* on GC free list, ignore */
    my_assert((hdr & 2) == 0);           /* assert not invalidated */
    wm = (struct weakmap *)(hdr & ~(GC_word)1);
    obj = (GC_word *)obj_base + 1;

    /* Lock and check for mark. */
    h = memhash(obj, wm->key_size);
    if (weakmap_trylock(wm, h) != 0) {
#       ifdef DEBUG_DISCLAIM_WEAKMAP
          printf("Skipping locked %p %#x.\n", obj, h);
#       endif
        AO_fetch_and_add1(&stat_skip_locked);
        return 1;
    }
    if (GC_is_marked(obj_base)) {
#       ifdef DEBUG_DISCLAIM_WEAKMAP
          printf("Skipping marked %p %#x.\n", obj, h);
#       endif
        AO_fetch_and_add1(&stat_skip_marked);
        weakmap_unlock(wm, h);
        return 1;
    }

    /* Remove obj from wm. */
#   ifdef DEBUG_DISCLAIM_WEAKMAP
      printf("Removing %p %#x.\n", obj, h);
#   endif
    AO_fetch_and_add1(&stat_removed);
    *(GC_word *)obj_base |= 2;          /* invalidate */
    link = &wm->links[h % wm->capacity];
    while (*link != NULL) {
        void *old_obj = GC_REVEAL_POINTER((*link)->obj);
        if (old_obj == obj) {
            *link = (*link)->next;
            weakmap_unlock(wm, h);
            return 0;
        }
        else {
            my_assert(memcmp(old_obj, obj, wm->key_size) != 0);
            link = &(*link)->next;
        }
    }
    dief(70, "Did not find %p.", obj);
    weakmap_unlock(wm, h);
    return 0;
}

struct weakmap *
weakmap_new(size_t capacity, size_t key_size, size_t obj_size)
{
    int i;
    struct weakmap *wm = GC_NEW(struct weakmap);
    if (!wm) out_of_memory();
    for (i = 0; i < WEAKMAP_MUTEX_COUNT; ++i)
        pthread_mutex_init(&wm->mutex[i], NULL);
    wm->key_size = key_size;
    wm->obj_size = obj_size;
    wm->capacity = capacity;
    wm->links = (struct weakmap_link **)GC_malloc(sizeof(struct weakmap_link *)
                                                  * capacity);
    if (!wm->links) out_of_memory();
    memset(wm->links, 0, sizeof(struct weakmap_link *) * capacity);
    return wm;
}

static struct weakmap *_pair_hcset;

#define PAIR_MAGIC_SIZE 16

struct pair_key {
    struct pair *car, *cdr;
};
struct pair {
    struct pair *car, *cdr;
    char magic[PAIR_MAGIC_SIZE];
    int checksum;
};
static const char * const pair_magic = "PAIR_MAGIC_BYTES";

struct pair *
pair_new(struct pair *car, struct pair *cdr)
{
    struct pair tmpl;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.car = car;
    tmpl.cdr = cdr;
    memcpy(tmpl.magic, pair_magic, PAIR_MAGIC_SIZE);
    tmpl.checksum = 782 + (car? car->checksum : 0) + (cdr? cdr->checksum : 0);
    return (struct pair *)weakmap_add(_pair_hcset, &tmpl);
}

void
pair_check_rec(struct pair *p, int line)
{
    while (p) {
        int checksum = 782;
        if (memcmp(p->magic, pair_magic, PAIR_MAGIC_SIZE) != 0)
            dief(70, "Magic bytes wrong for %p at %d.", (void *)p, line);
        if (p->car) checksum += p->car->checksum;
        if (p->cdr) checksum += p->cdr->checksum;
        if (p->checksum != checksum)
            dief(70, "Checksum failure for %p = (%p, %p) at %d.",
                 (void *)p, (void *)p->car, (void *)p->cdr, line);
        switch (rand() % 2) {
          case 0: p = p->car; break;
          case 1: p = p->cdr; break;
        }
    }
}

void *test(void *data)
{
    int i;
    struct pair *pop[POP_SIZE], *p0, *p1;
    memset(pop, 0, sizeof(pop));
    for (i = 0; i < MUTATE_CNT; ++i) {
        int bits = rand();
        int t = (bits >> 3) % POP_SIZE;
        switch (bits % (i > GROW_LIMIT? 5 : 3)) {
          case 0: case 3:
            if (pop[t])
                pop[t] = pop[t]->car;
            break;
          case 1: case 4:
            if (pop[t])
                pop[t] = pop[t]->cdr;
            break;
          case 2:
            p0 = pop[rand() % POP_SIZE];
            p1 = pop[rand() % POP_SIZE];
            pop[t] = pair_new(p0, p1);
            my_assert(pop[t] == pair_new(p0, p1));
            my_assert(pop[t]->car == p0);
            my_assert(pop[t]->cdr == p1);
            break;
        }
        pair_check_rec(pop[rand() % POP_SIZE], __LINE__);
    }
    return data;
}

int main()
{
    int i;
    pthread_t th[THREAD_CNT];
    GC_set_all_interior_pointers(0);
    GC_register_displacement(sizeof(GC_word));
    GC_register_displacement(sizeof(oh) + sizeof(GC_word));
    GC_register_displacement(1);
    GC_register_displacement(sizeof(oh) + 1);
    GC_INIT();

    _weakobj_free_list = GC_new_free_list();
    if (!_weakobj_free_list) out_of_memory();
    _weakobj_kind = GC_new_kind(_weakobj_free_list, 0 | GC_DS_LENGTH, 1, 1);
    GC_register_disclaim_proc(_weakobj_kind, weakmap_disclaim, 1);
    _pair_hcset = weakmap_new(WEAKMAP_CAPACITY,
                              sizeof(struct pair_key), sizeof(struct pair));

    for (i = 0; i < THREAD_CNT; ++i) {
        int err = GC_pthread_create(&th[i], NULL, test, NULL);
        if (err)
            dief(69, "Failed to create thread # %d: %s", i, strerror(err));
    }
    for (i = 0; i < THREAD_CNT; ++i) {
        int err = GC_pthread_join(th[i], NULL);
        if (err)
            dief(69, "Failed to join thread # %d: %s", i, strerror(err));
    }
    printf("%5d added, %6d found; %5d removed, %5d locked, %d marked; "
           "%d remains\n",
           (int)stat_added, (int)stat_found, (int)stat_removed,
           (int)stat_skip_locked, (int)stat_skip_marked,
           (int)(stat_added - stat_removed));
    return 0;
}
