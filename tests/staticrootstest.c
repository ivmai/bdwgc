
#include <stdio.h>
#include <string.h>

#ifndef GC_DEBUG
# define GC_DEBUG
#endif

#include "gc.h"
#include "gc_backptr.h"

#ifndef GC_TEST_IMPORT_API
# define GC_TEST_IMPORT_API extern
#endif

/* Should match that in staticrootslib.c.       */
struct treenode {
    struct treenode *x;
    struct treenode *y;
};

struct treenode *root[10] = { NULL };

static char *staticroot = 0;

GC_TEST_IMPORT_API struct treenode * libsrl_mktree(int i);
GC_TEST_IMPORT_API void * libsrl_init(void);

int main(void)
{
  int i, j;

  staticroot = libsrl_init();
  if (NULL == staticroot) {
    fprintf(stderr, "GC_malloc returned NULL\n");
    return 2;
  }
  memset(staticroot, 0x42, sizeof(struct treenode));
  GC_gcollect();
  for (j = 0; j < 2; j++) {
      for (i = 0; i < 10; ++i) {
        root[i] = libsrl_mktree(12);
        GC_gcollect();
      }
      for (i = 0; i < (int)sizeof(struct treenode); ++i) {
        if (staticroot[i] != 0x42) {
          fprintf(stderr, "Memory check failed\n");
          return -1;
        }
      }
  }
  return 0;
}
