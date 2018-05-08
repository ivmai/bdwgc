#include <stdio.h>
#include <stdlib.h>

#ifndef GC_DEBUG
# define GC_DEBUG
#endif

#include "gc.h"
#include "gc_backptr.h"

struct treenode {
    struct treenode *x;
    struct treenode *y;
} * root[10];

struct treenode * mktree(int i) {
  struct treenode * r = GC_MALLOC(sizeof(struct treenode));
  struct treenode *x, *y;
  if (0 == i) return 0;
  if (1 == i) r = GC_MALLOC_ATOMIC(sizeof(struct treenode));
  if (r == NULL) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }
  x = mktree(i - 1);
  y = mktree(i - 1);
  r -> x = x;
  r -> y = y;
  if (i != 1) {
    GC_END_STUBBORN_CHANGE(r);
    GC_reachable_here(x);
    GC_reachable_here(y);
  }
  return r;
}

int main(void)
{
  int i;
  GC_INIT();
  for (i = 0; i < 10; ++i) {
    root[i] = mktree(12);
  }
  GC_generate_random_backtrace();
  GC_generate_random_backtrace();
  GC_generate_random_backtrace();
  GC_generate_random_backtrace();
  return 0;
}
