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
This implementation module for gc_cpp.h provides an implementation of
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

#define GC_DONT_INCL_WINDOWS_H
#include "gc.h"

#include <new> // for bad_alloc, precedes include of gc_cpp.h

#include "gc_cpp.h" // for GC_OPERATOR_NEW_ARRAY, GC_NOEXCEPT

#if defined(GC_NEW_ABORTS_ON_OOM) || defined(_LIBCPP_NO_EXCEPTIONS)
# define GC_ALLOCATOR_THROW_OR_ABORT() GC_abort_on_oom()
#else
# define GC_ALLOCATOR_THROW_OR_ABORT() throw std::bad_alloc()
#endif

GC_API void GC_CALL GC_throw_bad_alloc() {
  GC_ALLOCATOR_THROW_OR_ABORT();
}

#if !(defined(_MSC_VER) || defined(__DMC__)) || defined(GC_NO_INLINE_STD_NEW)

# if !defined(GC_NEW_DELETE_THROW_NOT_NEEDED) \
    && !defined(GC_NEW_DELETE_NEED_THROW) && GC_GNUC_PREREQ(4, 2) \
    && (__cplusplus < 201103L || defined(__clang__))
#   define GC_NEW_DELETE_NEED_THROW
# endif

# ifdef GC_NEW_DELETE_NEED_THROW
#   if __cplusplus >= 201703L || _MSVC_LANG >= 201703L
      // The "dynamic exception" syntax had been deprecated in C++11
      // and was removed in C++17.
#     define GC_DECL_NEW_THROW noexcept(false)
#   else
#     define GC_DECL_NEW_THROW throw(std::bad_alloc)
#   endif
# else
#   define GC_DECL_NEW_THROW /* empty */
# endif

  void* operator new(size_t size) GC_DECL_NEW_THROW {
    void* obj = GC_MALLOC_UNCOLLECTABLE(size);
    if (0 == obj)
      GC_ALLOCATOR_THROW_OR_ABORT();
    return obj;
  }

# ifdef _MSC_VER
    // This new operator is used by VC++ in case of Debug builds.
    void* operator new(size_t size, int /* nBlockUse */,
                       const char* szFileName, int nLine)
    {
#     ifdef GC_DEBUG
        void* obj = GC_debug_malloc_uncollectable(size, szFileName, nLine);
#     else
        void* obj = GC_MALLOC_UNCOLLECTABLE(size);
        (void)szFileName; (void)nLine;
#     endif
      if (0 == obj)
        GC_ALLOCATOR_THROW_OR_ABORT();
      return obj;
    }
# endif // _MSC_VER

  void operator delete(void* obj) GC_NOEXCEPT {
    GC_FREE(obj);
  }

# if defined(GC_OPERATOR_NEW_ARRAY) && !defined(CPPCHECK)
    void* operator new[](size_t size) GC_DECL_NEW_THROW {
      void* obj = GC_MALLOC_UNCOLLECTABLE(size);
      if (0 == obj)
        GC_ALLOCATOR_THROW_OR_ABORT();
      return obj;
    }

#   ifdef _MSC_VER
      // This new operator is used by VC++ 7+ in Debug builds.
      void* operator new[](size_t size, int nBlockUse,
                           const char* szFileName, int nLine)
      {
        return operator new(size, nBlockUse, szFileName, nLine);
      }
#   endif // _MSC_VER

    void operator delete[](void* obj) GC_NOEXCEPT {
      GC_FREE(obj);
    }
# endif // GC_OPERATOR_NEW_ARRAY

# if __cplusplus >= 201402L || _MSVC_LANG >= 201402L // C++14
    void operator delete(void* obj, size_t size) GC_NOEXCEPT {
      (void)size; // size is ignored
      GC_FREE(obj);
    }

#   if defined(GC_OPERATOR_NEW_ARRAY) && !defined(CPPCHECK)
      void operator delete[](void* obj, size_t size) GC_NOEXCEPT {
        (void)size;
        GC_FREE(obj);
      }
#   endif
# endif // C++14

#endif // !_MSC_VER && !__DMC__ || GC_NO_INLINE_STD_NEW
