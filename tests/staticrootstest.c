
#include <stdio.h>
#include <string.h>

#ifndef GC_DEBUG
# define GC_DEBUG
#endif

#include "gc.h"
#include "gc_backptr.h"

struct treenode {
    struct treenode *x;
    struct treenode *y;
} * root[10];

static char *staticroot = 0;

extern struct treenode * libsrl_mktree(int i);
extern void * libsrl_init(void);

/*
struct treenode * mktree(int i) {
  struct treenode * r = GC_MALLOC(sizeof(struct treenode));
  if (0 == i) return 0;
  if (1 == i) r = GC_MALLOC_ATOMIC(sizeof(struct treenode));
  r -> x = mktree(i-1);
  r -> y = mktree(i-1);
  return r;
}*/

int main(void)
{
  int i;
  /*GC_INIT();
  staticroot = GC_MALLOC(sizeof(struct treenode));*/
  staticroot = libsrl_init();
  memset(staticroot, 0x42, sizeof(struct treenode));
  GC_gcollect();
  for (i = 0; i < 10; ++i) {
    root[i] = libsrl_mktree(12);
    GC_gcollect();
  }
  for (i = 0; i < (int)sizeof(struct treenode); ++i) {
    if (staticroot[i] != 0x42)
      return -1;
  }
  for (i = 0; i < 10; ++i) {
    root[i] = libsrl_mktree(12);
    GC_gcollect();
  }
  for (i = 0; i < (int)sizeof(struct treenode); ++i) {
    if (staticroot[i] != 0x42)
      return -1;
  }
  return 0;
}
