/*************************************************************************


Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 
THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 
Permission is hereby granted to copy this code for any purpose,
provided the above notices are retained on all copies.

This implementation module for gc_c++.h provides an implementation of
the global operators "new" and "delete" that calls the Boehm
allocator.  All objects allocated by this implementation will be
non-collectable but part of the root set of the collector.

You should ensure (using implementation-dependent techniques) that the
linker finds this module before the library that defines the default
built-in "new" and "delete".

Authors: Jesse Hull and John Ellis

**************************************************************************/
/* Boehm, June 8, 1994 3:10 pm PDT */

#include "gc_c++.h"

void* operator new( size_t size ) {
    return GC_MALLOC_UNCOLLECTABLE( size ); }
  
void operator delete( void* obj ) {
    GC_FREE( obj ); }
  


