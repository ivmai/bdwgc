/*
	MacOS_config.h
	
	Configuration flags for Macintosh development systems.
	
	by Patrick C. Beard.
 */

#ifdef __MWERKS__
#if defined(__powerc)
#include <MacHeadersPPC>
#else
#include <MacHeaders68K>
#endif
#endif

// these are defined again in gc_priv.h.
#undef TRUE
#undef FALSE

#define ALL_INTERIOR_POINTERS     // follows interior pointers.
#define SILENT                    // no collection messages.
//#define DONT_ADD_BYTE_AT_END      // no padding.
//#define SMALL_CONFIG              // use a smaller heap.
#define USE_TEMPORARY_MEMORY      // use Macintosh temporary memory.

// CFLAGS= -O -DALL_INTERIOR_POINTERS -DSILENT
// Setjmp_test may yield overly optimistic results when compiled
// without optimization.
// -DSILENT disables statistics printing, and improves performance.
// -DCHECKSUMS reports on erroneously clear dirty bits, and unexpectedly
//   altered stubborn objects, at substantial performance cost.
// -DFIND_LEAK causes the collector to assume that all inaccessible
//   objects should have been explicitly deallocated, and reports exceptions
// -DSOLARIS_THREADS enables support for Solaris (thr_) threads.
//   (Clients should also define SOLARIS_THREADS and then include
//   gc.h before performing thr_ or GC_ operations.)
// -DALL_INTERIOR_POINTERS allows all pointers to the interior
//   of objects to be recognized.  (See gc_private.h for consequences.)
// -DSMALL_CONFIG tries to tune the collector for small heap sizes,
//   usually causing it to use less space in such situations.
//   Incremental collection no longer works in this case.
// -DDONT_ADD_BYTE_AT_END is meaningful only with
//   -DALL_INTERIOR_POINTERS.  Normally -DALL_INTERIOR_POINTERS
//   causes all objects to be padded so that pointers just past the end of
//   an object can be recognized.  This can be expensive.  (The padding
//   is normally more than one byte due to alignment constraints.)
//   -DDONT_ADD_BYTE_AT_END disables the padding.
