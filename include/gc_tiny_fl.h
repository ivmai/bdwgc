/* 
 * Copyright (c) 1999-2004 Hewlett-Packard Development Company, L.P.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#ifndef TINY_FL_H
#define TINY_FL_H
/*
 * Constants and data structures for "tiny" free lists.
 * These are used for thread-local allocation or in-lined allocators.
 * Each global free list also essentially starts with one of these.
 * However, global free lists are known to the GC.  "Tiny" free lists
 * are basically private to the client.  Their contents are viewed as
 * "in use" and marked accordingly by the core of the GC.
 * 
 * Note that inlined code might know about the layout of these and the constants
 * involved.  Thus any change here may invalidate clients, and such changes should
 * be avoided.  Hence we keep this as simple as possible.
 */

/*
 * We always set GRANULE_BYTES to twice the length of a pointer.
 * This means that all allocation requests are rounded up to the next
 * multiple of 16 on 64-bit architectures or 8 on 32-bit architectures.
 * This appears to be a reasonable compromise between fragmentation overhead
 * and space usage for mark bits (usually mark bytes).
 * On many 64-bit architectures some memory references require 16-byte
 * alignment, making this necessary anyway.
 * For a few 32-bit architecture (e.g. 86), we may also need 16-byte alignment
 * for certain memory references.  But currently that does not seem to be the
 * default for all conventional malloc implementations, so we ignore that
 * problem.
 * It would always be safe, and often useful, to be able to allocate very
 * small objects with smaller alignment.  But that would cost us mark bit
 * space, so we no longer do so.
 */
#ifndef GC_GRANULE_BYTES
# if defined(__LP64__) || defined (_LP64) || defined(_WIN64) || defined(__s390x__) \
	|| defined(__x86_64__) || defined(__alpha__) || defined(__powerpc64__) \
	|| defined(__arch64__)
#  define GC_GRANULE_BYTES 16
# else
#  define GC_GRANULE_BYTES 8
# endif
#endif /* !GC_GRANULE_BYTES */

/* A "tiny" free list header contains TINY_FREELISTS pointers to 	*/
/* singly linked lists of objects of different sizes, the ith one	*/
/* containing objects i granules in size.  Note that there is a list	*/
/* of size zero objects.						*/
#ifndef GC_TINY_FREELISTS
# if GC_GRANULE_BYTES == 16
#   define GC_TINY_FREELISTS 25
# else
#   define GC_TINY_FREELISTS 33	/* Up to and including 256 bytes */
# endif
#endif /* !GC_TINY_FREELISTS */

#endif /* TINY_FL_H */
