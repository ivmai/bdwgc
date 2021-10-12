/*
  Copyright (c) 2004-2005 Andrei Polushin

  Permission is hereby granted, free of charge,  to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction,  including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef GC_MSVC_DBG_H
#define GC_MSVC_DBG_H

#include <stdlib.h>

#ifdef __cplusplus
  extern "C" {
#endif

#define MSVC_DBG_EXPORT /* empty */

/* Compatibility with <execinfo.h> */
MSVC_DBG_EXPORT int    backtrace(void* addresses[], int count);
MSVC_DBG_EXPORT char** backtrace_symbols(void* const addresses[], int count);

#ifdef __cplusplus
  } /* extern "C" */
#endif

#endif /* GC_MSVC_DBG_H */
