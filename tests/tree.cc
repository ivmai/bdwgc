
// The program tests part of the C++ functionality provided by gc_cpp.h (and
// gctba library) by creating balanced trees of various depth and arity.

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#undef GC_BUILD

#define GC_DONT_INCL_WINDOWS_H
#define GC_NAMESPACE
#include "gc_cpp.h"

#include <stdio.h>
#include <stdlib.h>

#define USE_GC GC_NS_QUALIFY(UseGC)

class Tree;
typedef Tree *PTree;

class Tree : public GC_NS_QUALIFY(gc)
{
public:
  GC_ATTR_EXPLICIT
  Tree(int a, int d);
  void verify();

private:
  void verify(int a, int d);
  void verify_node(int a, int d);
  int arity, depth;
  PTree *m_nodes;
};

Tree::Tree(int a, int d) : arity(a), depth(d)
{
  PTree *nodes = 0;
  if (depth > 0) {
#ifdef GC_OPERATOR_NEW_ARRAY
    nodes = new (USE_GC) PTree[arity];
#else
    nodes = reinterpret_cast<PTree *>(
        GC_MALLOC(sizeof(PTree) * (unsigned)arity));
    GC_OP_NEW_OOM_CHECK(nodes);
#endif
    for (int i = 0; i < arity; i++) {
      GC_PTR_STORE_AND_DIRTY(&nodes[i], new (USE_GC) Tree(arity, depth - 1));
    }
  }
  this->m_nodes = nodes;
  GC_END_STUBBORN_CHANGE(this);
  GC_reachable_here(nodes);
}

void
Tree::verify()
{
  verify(arity, depth);
}

void
Tree::verify_node(int a, int d)
{
  if (a != arity || d != depth) {
    fprintf(stderr, "Wrong stored arity or depth! arity=%d, depth=%d\n", arity,
            depth);
    exit(1);
  }
  if (0 == depth) {
    if (m_nodes != 0) {
      fprintf(stderr, "Found non-empty node!\n");
      exit(1);
    }
  } else if (0 == m_nodes) {
    fprintf(stderr, "Found empty node! depth=%d\n", depth);
    exit(1);
  }
}

void
Tree::verify(int a, int d)
{
  verify_node(a, d);
  if (0 == depth)
    return;
  for (int i = 0; i < arity; i++) {
    m_nodes[i]->verify(a, d - 1);
  }
}

#ifndef MAX_ARITY
#  define MAX_ARITY 5
#endif

#ifndef MAX_DEPTH
#  define MAX_DEPTH 9
#endif

int
main(void)
{
#ifdef TEST_MANUAL_VDB
  GC_set_manual_vdb_allowed(1);
#endif
#ifndef CPPCHECK
  GC_INIT();
#endif
  if (GC_get_find_leak())
    printf("This test program is not designed for leak detection mode\n");
#ifndef NO_INCREMENTAL
  GC_enable_incremental();
#endif

  Tree *keep_tree = new (USE_GC) Tree(MAX_ARITY, MAX_DEPTH);
  keep_tree->verify();
  for (int arity = 2; arity <= MAX_ARITY; arity++) {
    for (int depth = 1; depth <= MAX_DEPTH; depth++) {
      Tree *tree = new (USE_GC) Tree(arity, depth);
      tree->verify();
    }
  }
  keep_tree->verify(); // recheck
  printf("SUCCEEDED\n");
  return 0;
}
