
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

/* Same as "root" variable but initialized to some non-zero value (to   */
/* be placed to .data section instead of .bss).                         */
struct treenode *root_nz[10] = { (void *)(GC_word)1 };

static char *staticroot = 0;

GC_TEST_IMPORT_API struct treenode * libsrl_mktree(int i);
GC_TEST_IMPORT_API void * libsrl_init(void);
GC_TEST_IMPORT_API struct treenode ** libsrl_getpelem(int i, int j);

GC_TEST_IMPORT_API struct treenode ** libsrl_getpelem2(int i, int j);

int main(void)
{
  int i, j;

# ifdef STATICROOTSLIB_INIT_IN_MAIN
    GC_INIT();
# endif
  staticroot = libsrl_init();
  if (NULL == staticroot) {
    fprintf(stderr, "GC_malloc returned NULL\n");
    return 2;
  }
  memset(staticroot, 0x42, sizeof(struct treenode));
  GC_gcollect();
  for (j = 0; j < 4; j++) {
      for (i = 0; i < (int)(sizeof(root) / sizeof(root[0])); ++i) {
#       ifdef STATICROOTSLIB2
          *libsrl_getpelem2(i, j) = libsrl_mktree(12);
#       endif
        *libsrl_getpelem(i, j) = libsrl_mktree(12);
        ((j & 1) != 0 ? root_nz : root)[i] = libsrl_mktree(12);
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
