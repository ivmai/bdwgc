/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this code for any purpose,
 * provided the above notices are retained on all copies.
 */

/*************************************************************************
This implementation module for gc_c++.h provides an implementation of
the global operators "new" and "delete" that calls the Boehm
allocator.  All objects allocated by this implementation will be
uncollectible but part of the root set of the collector.

You should ensure (using implementation-dependent techniques) that the
linker finds this module before the library that defines the default
built-in "new" and "delete".
**************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef GC_BUILD
# define GC_BUILD
#endif

#include "gc_cpp.h"

#if !defined(GC_NEW_DELETE_NEED_THROW) && defined(__GNUC__) \
    && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
# define GC_NEW_DELETE_NEED_THROW
#endif

#ifdef GC_NEW_DELETE_NEED_THROW
# include <new> /* for std::bad_alloc */
# define GC_DECL_NEW_THROW throw(std::bad_alloc)
# define GC_DECL_DELETE_THROW throw()
#else
# define GC_DECL_NEW_THROW /* empty */
# define GC_DECL_DELETE_THROW /* empty */
#endif /* !GC_NEW_DELETE_NEED_THROW */

void* operator new( size_t size ) GC_DECL_NEW_THROW {
  return GC_MALLOC_UNCOLLECTABLE(size);
}

#if !defined(__CYGWIN__)
  void operator delete( void* obj ) GC_DECL_DELETE_THROW {
    GC_FREE(obj);
  }
#endif /* !__CYGWIN__ */

#ifdef GC_OPERATOR_NEW_ARRAY
  void* operator new[]( size_t size ) GC_DECL_NEW_THROW {
    return GC_MALLOC_UNCOLLECTABLE(size);
  }

  void operator delete[]( void* obj ) GC_DECL_DELETE_THROW {
    GC_FREE(obj);
  }
#endif /* GC_OPERATOR_NEW_ARRAY */

#ifdef _MSC_VER

  // This new operator is used by VC++ in case of Debug builds!
  void* operator new( size_t size, int /* nBlockUse */,
                     const char * szFileName, int nLine ) GC_DECL_NEW_THROW
  {
#   ifndef GC_DEBUG
      return GC_malloc_uncollectable(size);
#   else
      return GC_debug_malloc_uncollectable(size, szFileName, nLine);
#   endif
  }

# if _MSC_VER > 1020
    // This new operator is used by VC++ 7.0 and later in Debug builds.
    void* operator new[]( size_t size, int nBlockUse,
                         const char* szFileName, int nLine ) GC_DECL_NEW_THROW
    {
      return operator new(size, nBlockUse, szFileName, nLine);
    }
# endif

#endif /* _MSC_VER */
