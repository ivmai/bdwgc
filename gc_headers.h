/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991, 1992 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
# ifndef GC_HEADERS_H
# define GC_HEADERS_H
typedef struct hblkhdr hdr;
 
# define LOG_TOP_SZ 11
# define LOG_BOTTOM_SZ (WORDSZ - LOG_TOP_SZ - LOG_HBLKSIZE)
# define TOP_SZ (1 << LOG_TOP_SZ)
# define BOTTOM_SZ (1 << LOG_BOTTOM_SZ)

# define MAX_JUMP (HBLKSIZE - 1)
 
extern hdr ** GC_top_index [];

# define HDR(p) (GC_top_index \
		[(unsigned long)(p) >> (LOG_BOTTOM_SZ + LOG_HBLKSIZE)] \
		[((unsigned long)(p) >> LOG_HBLKSIZE) & (BOTTOM_SZ - 1)])
			    
/* Is the result a forwarding address to someplace closer to the	*/
/* beginning of the block or NIL?					*/
# define IS_FORWARDING_ADDR_OR_NIL(hhdr) ((unsigned long) (hhdr) <= MAX_JUMP)

/* Get an HBLKSIZE aligned address closer to the beginning of the block */
/* h.  Assumes hhdr == HDR(h) and IS_FORWARDING_ADDR(hhdr).		*/
# define FORWARDED_ADDR(h, hhdr) ((struct hblk *)(h) - (unsigned long)(hhdr))
# endif /*  GC_HEADERS_H */
