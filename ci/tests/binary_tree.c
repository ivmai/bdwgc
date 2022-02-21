/*! 
This is a specific instance of the Open Source Initiative (OSI) BSD license template
http://www.opensource.org/licenses/bsd-license.php

Copyright © 2004-2008 Brent Fulgham, 2005-2019 Isaac Gouy
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

 contributed by Kevin Carson
 compilation:
     gcc -O3 -fomit-frame-pointer -funroll-loops -static binary-trees.c -lm
     icc -O3 -ip -unroll -static binary-trees.c -lm

 *reset*
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#if defined(__CHERI_PURE_CAPABILITY__)
#include <cheriintrin.h>
#endif
#include "gc.h"

#define DEFAULT_DEPTH 16

typedef struct tn {
  struct tn *left;
  struct tn *right;
} treeNode;


treeNode* NewTreeNode(treeNode* left, treeNode* right)
{
  treeNode *new;
  static int alloc_count = 0;

  new = (treeNode *)GC_MALLOC(sizeof(treeNode));

  new->left = left;
  new->right = right;
  return new;
} /* NewTreeNode() */


long ItemCheck(treeNode* tree)
{
  if (tree->left == NULL) {
    return 1;
  } else {
    return 1 + ItemCheck(tree->left) + ItemCheck(tree->right);
  }
} /* ItemCheck() */


treeNode* BottomUpTree(unsigned depth)
{
  if (depth > 0)
      return NewTreeNode(BottomUpTree(depth - 1), BottomUpTree(depth - 1));
  else
      return NewTreeNode(NULL, NULL);
} /* BottomUpTree() */


void DeleteTree(treeNode* tree)
{
  if (tree->left != NULL)
  {
      DeleteTree(tree->left);
      DeleteTree(tree->right);
  }

  // To be performed by GC
  // free(tree);
} /* DeleteTree() */

int main(int argc, char* argv[])
{
  unsigned   N, depth, minDepth, maxDepth, stretchDepth;
  treeNode   *stretchTree, *longLivedTree, *tempTree;

  GC_INIT();

  /* check we have an argument */
  if (argc < 2) {
    printf("Usage: %s <depth-of-tree>\ndefaulting to depth %u\n " , argv[0], DEFAULT_DEPTH);
    N = DEFAULT_DEPTH;
  } else {
    N = atol(argv[1]);
  }

  minDepth = 4;
  maxDepth = ((minDepth + 2) > N) ?  minDepth + 2 : N;
  stretchDepth = maxDepth + 1;

  stretchTree = BottomUpTree(stretchDepth);
  printf("stretch tree of depth %u\t check: %li\n", stretchDepth, ItemCheck(stretchTree));

  DeleteTree(stretchTree);

  longLivedTree = BottomUpTree(maxDepth);

  for (depth = minDepth; depth <= maxDepth; depth += 2)
  {
    long    i, iterations, check;
    iterations = pow(2, maxDepth - depth + minDepth);
    check = 0;

    for (i = 1; i <= iterations; i++) {
      tempTree = BottomUpTree(depth);
      check += ItemCheck(tempTree);
      DeleteTree(tempTree);
    } /* for(i = 1...) */

    printf("%li\t trees of depth %u\t check: %li\n", iterations, depth, check);
  } /* for(depth = minDepth...) */

  printf("long lived tree of depth %u\t check: %li\n", maxDepth, ItemCheck(longLivedTree));
  return 0;
} /* main() */
