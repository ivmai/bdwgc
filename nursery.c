/* 
 * Copyright (c) 1999 by Silicon Graphics.  All rights reserved.
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


??? This implementation is incomplete.  If you are trying to
??? compile this you are doing something wrong.

#include "nursery.h"

struct copy_obj {
    ptr_t forward;	/* Forwarding link for copied objects.	*/
    GC_copy_descriptor descr; /* Object descriptor	*/
    word data[1];
}

ptr_t GC_nursery_start;	/* Start of nursery area.	*/
			/* Must be NURSERY_BLOCK_SIZE	*/
			/* aligned.			*/
ptr_t GC_nursery_end;	/* End of nursery area.		*/
unsigned char * GC_nursery_map;
			/* GC_nursery_map[i] != 0 if an object	*/
			/* starts on the ith 64-bit "word" of 	*/
			/* nursery.  This simple structure has	*/
			/* the advantage that 			*/
			/* allocation is cheap.  Lookup is 	*/
			/* cheap for pointers to the head of	*/
			/* an object, which should be the	*/
			/* usual case.				*/
#   define NURSERY_MAP_NOT_START	0  /* Not start of object. */
#   define NURSERY_MAP_START		1  /* Start of object.	   */
#   define NURSERY_MAP_PINNED		2  /* Start of pinned obj. */

# ifdef ALIGN_DOUBLE
#   define NURSERY_WORD_SIZE (2 * sizeof(word))
# else
#   define NURSERY_WORD_SIZE sizeof(word)
# endif

# define NURSERY_BLOCK_SIZE (HBLKSIZE/2)	
	/* HBLKSIZE must be a multiple of NURSERY_BLOCK_SIZE */
# define NURSERY_SIZE (1024 * NURSERY_BLOCK_SIZE)

size_t GC_nursery_size = NURSERY_SIZE;
			/* Must be multiple of NURSERY_BLOCK_SIZE	*/

size_t GC_nursery_blocks; /* Number of blocks in the nursery.	*/

unsigned GC_next_nursery_block; /* index of next block we will attempt 	*/
				/* allocate from during this cycle.	*/
				/* If it is pinned, we won't actually	*/
				/* use it.				*/

unsigned short *GC_pinned;	/* Number of pinned objects in ith	*/
				/* nursery block.			*/
				/* GC_pinned[i] != 0 if the ith nursery */
				/* block is pinned, and thus not used	*/
				/* for allocation.			*/

GC_copy_alloc_state global_alloc_state = (ptr_t)(-1);	/* will overflow. */

/* Should be called with allocator lock held.	*/
GC_nursery_init() {
    GC_nursery_start = GET_MEM(GC_nursery_size);
    GC_nursery_end = GC_nursery_start + GC_nursery_size;
    GC_next_nursery_block = 0;
    if (GC_nursery_start < GC_least_plausible_heap_addr) { 
        GC_least_plausible_heap_addr = GC_nursery_start;   
    }
    if (GC_nursery_end > GC_greatest_plausible_heap_addr) {
        GC_greatest_plausible_heap_addr = GC_nursery_end;  
    }
    if (GC_nursery_start & (NURSERY_BLOCK_SIZE-1)) {
	GC_err_printf1("Nursery area is misaligned!!");
	/* This should be impossible, since GET_MEM returns HBLKSIZE */
	/* aligned chunks, and that should be a multiple of 	     */
	/* NURSERY_BLOCK_SIZE					     */
	ABORT("misaligned nursery");
    }
    GC_nursery_map = GET_MEM(GC_nursery_size/NURSERY_WORD_SIZE);
    /* Map is cleared a block at a time when we allocate from the block. */
    /* BZERO(GC_nursery_map, GC_nursery_size/NURSERY_WORD_SIZE);	 */
    GC_nursery_blocks = GC_nursery_size/NURSERY_BLOCK_SIZE;
    GC_pinned = GC_scratch_alloc(GC_nursery_blocks * sizeof(unsigned short));
    BZERO(GC_pinned, GC_nursery_blocks);
}

/* Pin all nursery objects referenced from mark stack. */
void GC_pin_mark_stack_objects(void) {
    for each possible pointer current in a mark stack object
	if (current >= GC_nursery_start && current < GC_nursery_end) {
	    unsigned offset = current - GC_nursery_start;
	    unsigned word_offset = BYTES_TO_WORDS(offset);
	    unsigned blockno = (current - GC_nursery_start)/NURSERY_BLOCK_SIZE;
	    while (GC_nursery_map[word_offset] == NURSERY_MAP_NOT_START) {
		--word_offset;    
	    }
	    if (GC_nursery_map[word_offset] != NURSERY_MAP_PINNED) {
	        GC_nursery_map[word_offset] = NURSERY_MAP_PINNED;
	        ++GC_pinned[blockno];
	        ??Push object at GC_nursery_start + WORDS_TO_BYTES(word_offset)
	        ??onto stack. 
	    }
	}
    }
}

/* Caller holds allocation lock.	*/
void GC_collect_nursery(void) {
    int i;
    ptr_t scan_ptr = 0;
    ?? old_mark_stack_top;
    STOP_WORLD;
    for (i = 0; i < GC_nursery_blocks; ++i) GC_pinned[i] = 0;
    GC_push_all_roots();
    old_mark_stack_top = GC_mark_stack_top();
    GC_pin_mark_stack_objects();
    START_WORLD;
}

/* Initialize an allocation state so that it can be used for 	*/
/* allocation.  This implicitly reserves a small section of the	*/
/* nursery for use with his allocator.				*/
void GC_init_copy_alloc_state(GC_copy_alloc_state *)
    unsigned next_block;
    ptr_t block_addr;
    LOCK();
    next_block = GC_next_nursery_block;
    while(is_pinned[next_block] && next_block < GC_nursery_blocks) {
	++next_block;
    }
    if (next_block < GC_nursery_blocks) {
	block_addr = GC_nursery_start + NURSERY_BLOCK_SIZE * next_block;
   	GC_next_nursery_block = next_block + 1;
	BZERO(GC_nursery_map + next_block *
				(NURSERY_BLOCK_SIZE/NURSERY_WORD_SIZE),
	      NURSERY_BLOCK_SIZE/NURSERY_WORD_SIZE);
	*GC_copy_alloc_state = block_addr;
	UNLOCK();
    } else {
     	GC_collect_nursery();
    	GC_next_nursery_block = 0;
    	UNLOCK();
    	get_new_block(s);
    }
}

GC_PTR GC_copying_malloc2(GC_copy_descriptor *d, GC_copy_alloc_state *s) {
    size_t sz = GC_SIZE_FROM_DESCRIPTOR(d);
    ptrdiff_t offset;
    ptr_t result = *s;
    ptr_t new = result + sz;
    if (new & COPY_BLOCK_MASK <= result & COPY_BLOCK_MASK> {
	GC_init_copy_alloc_state(s);
	result = *s;
	new = result + sz;
        GC_ASSERT(new & COPY_BLOCK_MASK > result & COPY_BLOCK_MASK>
    }
    (struct copy_obj *)result -> descr = d;      
    (struct copy_obj *)result -> forward = 0;      
    offset = (result - GC_nursery_start)/NURSERY_WORD_SIZE;
    GC_nursery_map[offset] = NURSERY_MAP_NOT_START;
}

GC_PTR GC_copying_malloc(GC_copy_descriptor *d) {
}
