/*
 * Copyright (c) 2011 by Hewlett-Packard Company.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gc/gc_disclaim.h"

#define NOT_GCBUILD
#include "private/gc_priv.h"

#include <string.h>

#undef rand
static GC_RAND_STATE_T seed;
#define rand() GC_RAND_NEXT(&seed)

#define my_assert(e)                                                   \
  if (!(e)) {                                                          \
    fprintf(stderr, "Assertion failure, line %d: " #e "\n", __LINE__); \
    exit(-1);                                                          \
  }

#define CHECK_OUT_OF_MEMORY(p)            \
  do {                                    \
    if (NULL == (p)) {                    \
      fprintf(stderr, "Out of memory\n"); \
      exit(69);                           \
    }                                     \
  } while (0)

static int free_count = 0;

struct testobj_s {
  struct testobj_s *keep_link;
  int i;
};

typedef struct testobj_s *testobj_t;

static void GC_CALLBACK
testobj_finalize(void *obj, void *carg)
{
  ++*(int *)carg;
  my_assert(((testobj_t)obj)->i == 109);
  ((testobj_t)obj)->i = 110;
}

static const struct GC_finalizer_closure fclos
    = { testobj_finalize, &free_count };

static testobj_t
testobj_new(int model)
{
  testobj_t obj;
  switch (model) {
#ifndef GC_NO_FINALIZATION
  case 0:
    obj = (struct testobj_s *)GC_malloc(sizeof(struct testobj_s));
    if (obj != NULL)
      GC_register_finalizer_no_order(obj, testobj_finalize, &free_count, NULL,
                                     NULL);
    break;
#endif
  case 1:
    obj = (testobj_t)GC_finalized_malloc(sizeof(struct testobj_s), &fclos);
    break;
  case 2:
    obj = (struct testobj_s *)GC_malloc(sizeof(struct testobj_s));
    break;
  default:
    exit(-1);
  }
  CHECK_OUT_OF_MEMORY(obj);
  my_assert(obj->i == 0 && obj->keep_link == NULL);
  obj->i = 109;
  return obj;
}

#define ALLOC_CNT (2 * 1024 * 1024)
#define KEEP_CNT (32 * 1024)

static char const *model_str[3]
    = { "regular finalization", "finalize on reclaim", "no finalization" };

int
main(int argc, const char *argv[])
{
  int i;
  int model, model_min, model_max;
  testobj_t *keep_arr;

  GC_INIT();
  GC_init_finalized_malloc();
  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    fprintf(stderr,
            "Usage: %s [FINALIZATION_MODEL]\n"
            "\t0 -- original finalization\n"
            "\t1 -- finalization on reclaim\n"
            "\t2 -- no finalization\n",
            argv[0]);
    return 1;
  }
  if (argc == 2) {
    model_min = model_max = (int)COVERT_DATAFLOW(atoi(argv[1]));
    if (model_min < 0 || model_max > 2)
      exit(2);
  } else {
#ifndef GC_NO_FINALIZATION
    model_min = 0;
#else
    model_min = 1;
#endif
    model_max = 2;
  }
  if (GC_get_find_leak())
    printf("This test program is not designed for leak detection mode\n");

  keep_arr = (testobj_t *)GC_malloc(sizeof(void *) * KEEP_CNT);
  CHECK_OUT_OF_MEMORY(keep_arr);
  printf("\t\t\tfin. ratio       time/s    time/fin.\n");
  for (model = model_min; model <= model_max; ++model) {
    double t = 0.0;
#ifndef NO_CLOCK
    CLOCK_TYPE tI, tF;

    GET_TIME(tI);
#endif
    free_count = 0;
    for (i = 0; i < ALLOC_CNT; ++i) {
      int k = rand() % KEEP_CNT;
      keep_arr[k] = testobj_new(model);
    }
    GC_gcollect();
#ifndef NO_CLOCK
    GET_TIME(tF);
    t = (double)MS_TIME_DIFF(tF, tI) * 1e-3;
#endif

#ifdef EMBOX
    /* Workaround some issue with %g processing in Embox libc.  */
#  define PRINTF_SPEC_12g "%12f"
#else
#  define PRINTF_SPEC_12g "%12g"
#endif
    if (model < 2 && free_count > 0) {
      printf("%20s: %12.4f " PRINTF_SPEC_12g " " PRINTF_SPEC_12g "\n",
             model_str[model], free_count / (double)ALLOC_CNT, t,
             t / free_count);
    } else {
      printf("%20s: %12.4f " PRINTF_SPEC_12g " %12s\n", model_str[model], 0.0,
             t, "N/A");
    }
  }
  return 0;
}
