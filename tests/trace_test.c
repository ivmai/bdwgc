#include <stdio.h>

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
  if (0 == i) return 0;
  if (1 == i) r = GC_MALLOC_ATOMIC(sizeof(struct treenode));
  r -> x = mktree(i-1);
  r -> y = mktree(i-1);
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
