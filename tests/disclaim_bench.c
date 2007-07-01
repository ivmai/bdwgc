#include "gc_disclaim.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int nf = 0;

typedef struct testobj_s *testobj_t;
struct testobj_s {
    testobj_t keep_link;
    int i;
};

void testobj_finalize(void *obj, void *carg)
{
#define obj ((testobj_t)obj)
    ++*(int *)carg;
    assert(obj->i++ == 109);
#undef obj
}
static struct GC_finalizer_closure fclos = {
    testobj_finalize,
    &nf
};

testobj_t testobj_new(int model)
{
    testobj_t obj;
    switch (model) {
	case 0:
	    obj = GC_malloc(sizeof(struct testobj_s));
	    GC_register_finalizer_no_order(obj, testobj_finalize, &nf,
					   NULL, NULL);
	    break;
	case 1:
	    obj = GC_finalized_malloc(sizeof(struct testobj_s), &fclos);
	    break;
	case 2:
	    obj = GC_malloc(sizeof(struct testobj_s));
	    break;
	default:
	    abort();
    }
    obj->i = 109;
    obj->keep_link = NULL;
    return obj;
}


#define ALLOC_CNT (4*1024*1024)
#define KEEP_CNT (32*1024)

int main(int argc, char **argv)
{
    int i;
    int repeat;
    int model;
    testobj_t *keep_arr;
    double t;
    static char const *model_str[3] = {
	"regular finalization",
	"finalize on reclaim",
	"no finalization"
    };

    GC_init();
    GC_init_finalized_malloc();

    /* Seed with time for distict usage patters over repeated runs. */
    srand48(time(NULL));

    keep_arr = GC_malloc(sizeof(void *)*KEEP_CNT);

    if (argc == 1) {
	char *buf = GC_malloc(strlen(argv[0]) + 3);
	printf("\t\t\tfin. ratio       time/s    time/fin.\n");
	for (i = 0; i < 3; ++i) {
	    int st;
	    sprintf(buf, "%s %d", argv[0], i);
	    st = system(buf);
	    if (st != 0)
		return st;
	}
	return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
	fprintf(stderr,
		"Usage: %s FINALIZATION_MODEL\n"
		"\t0 -- original finalization\n"
		"\t1 -- finalization on reclaim\n"
		"\t2 -- no finalization\n", argv[0]);
	return 1;
    }
    model = atoi(argv[1]);
    if (model < 0 || model > 2)
	exit(2);
    t = -clock();
    for (i = 0; i < ALLOC_CNT; ++i) {
	int k = lrand48() % KEEP_CNT;
	keep_arr[k] = testobj_new(model);
    }
    GC_gcollect();
    t += clock();
    t /= CLOCKS_PER_SEC;
    if (model < 2)
	printf("%20s: %12.4lf %12lg %12lg\n", model_str[model],
	       nf/(double)ALLOC_CNT, t, t/nf);
    else
	printf("%20s:            0 %12lg          N/A\n",
	       model_str[model], t);

    return 0;
}
