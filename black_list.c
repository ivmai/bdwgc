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
# include "gc_private.h"

/*
 * We maintain several hash tables of hblks that have had false hits.
 * Each contains one bit per hash bucket;  If any page in the bucket
 * has had a false hit, we assume that all of them have.
 * False hits from the stack(s) are much more dangerous than false hits
 * from elsewhere, since the former can pin a large object that spans the
 * block, eventhough it does not start on the dangerous block.
 */
 
/*
 * Externally callable routines are:
 
 * GC_add_to_black_list_normal
 * GC_add_to_black_list_stack
 * GC_promote_black_lists
 * GC_is_black_listed
 *
 * All require that the allocator lock is held.
 */

# define LOG_HT_ENTRIES  14	/* Collisions are likely if heap grows	*/
				/* to more than 16K hblks = 64MB.	*/
				/* Each hash table occupies 2K bytes.   */
# define HT_ENTRIES ((word)1 << LOG_HT_ENTRIES)
# define HT_SIZE (HT_ENTRIES >> LOGWL)
typedef word black_list_t[HT_SIZE];

# define HASH(addr) (((addr) >> LOG_HBLKSIZE) & (HT_ENTRIES - 1))

/* Pointers to individual tables.  We replace one table by another by 	*/
/* switching these pointers.  GC_black_lists is not used directly.	*/
word * GC_new_normal_bl;
		/* Nonstack false references seen at last complete	*/
		/* collection.						*/
word * GC_old_normal_bl;
		/* Nonstack false references seen at preceding		*/
		/* collection.						*/
word * GC_incomplete_normal_bl;
		/* Nonstack false references seen at current,		*/
		/* not yet completed collection.			*/
word * GC_new_stack_bl;
word * GC_old_stack_bl;
word * GC_incomplete_stack_bl;

# define get_bl_entry_from_index(bl, index) \
		(((bl)[divWORDSZ(index)] >> modWORDSZ(index)) & 1)
# define set_bl_entry_from_index(bl, index) \
		(bl)[divWORDSZ(index)] |= 1 << modWORDSZ(index)
# define clear_bl_entry_from_index(bl, index) \
		(bl)[divWORDSZ(index)] &= ~(1 << modWORDSZ(index))
		
GC_bl_init()
{
# ifndef ALL_INTERIOR_POINTERS
    GC_new_normal_bl = (word *)GC_scratch_alloc((word)(sizeof(black_list_t)));
    GC_old_normal_bl = (word *)GC_scratch_alloc((word)(sizeof (black_list_t)));
    GC_incomplete_normal_bl = (word *)GC_scratch_alloc
    					((word)(sizeof(black_list_t)));
# endif
    GC_new_stack_bl = (word *)GC_scratch_alloc((word)(sizeof(black_list_t)));
    GC_old_stack_bl = (word *)GC_scratch_alloc((word)(sizeof(black_list_t)));
    GC_incomplete_stack_bl = (word *)GC_scratch_alloc
    					((word)(sizeof(black_list_t)));
}
		
void GC_clear_bl(doomed)
word *doomed;
{
    bzero((char *)doomed, (int)HT_SIZE*sizeof(word));
}

/* Signal the completion of a collection.  Turn the incomplete black	*/
/* lists into new black lists, etc.					*/			 
void GC_promote_black_lists()
{
    word * very_old_normal_bl = GC_old_normal_bl;
    word * very_old_stack_bl = GC_old_stack_bl;
    
    GC_old_normal_bl = GC_new_normal_bl;
    GC_new_normal_bl = GC_incomplete_normal_bl;
    GC_old_stack_bl = GC_new_stack_bl;
    GC_new_stack_bl = GC_incomplete_stack_bl;
#   ifndef ALL_INTERIOR_POINTERS
      GC_clear_bl(very_old_normal_bl);
#   endif
    GC_clear_bl(very_old_stack_bl);
    GC_incomplete_normal_bl = very_old_normal_bl;
    GC_incomplete_stack_bl = very_old_stack_bl;
}

# ifndef ALL_INTERIOR_POINTERS
/* P is not a valid pointer reference, but it falls inside	*/
/* the plausible heap bounds.					*/
/* Add it to the normal incomplete black list if appropriate.	*/
void GC_add_to_black_list_normal(p)
word p;
{
    if (!(GC_modws_valid_offsets[p & (sizeof(word)-1)])) return;
    {
        register int index = HASH(p);
        
        if (HDR(p) == 0 || get_bl_entry_from_index(GC_new_normal_bl, index)) {
#   	    ifdef PRINTBLACKLIST
		if (!get_bl_entry_from_index(GC_incomplete_normal_bl, index)) {
	    	  GC_printf("Black listing (normal) 0x%lx\n",
	    	  	    (unsigned long) p);
	    	}
#           endif
            set_bl_entry_from_index(GC_incomplete_normal_bl, index);
        } /* else this is probably just an interior pointer to an allocated */
          /* object, and isn't worth black listing.			    */
    }
}
# endif

/* And the same for false pointers from the stack. */
void GC_add_to_black_list_stack(p)
word p;
{
    register int index = HASH(p);
        
    if (HDR(p) == 0 || get_bl_entry_from_index(GC_new_stack_bl, index)) {
#   	ifdef PRINTBLACKLIST
	    if (!get_bl_entry_from_index(GC_incomplete_stack_bl, index)) {
	    	  GC_printf("Black listing (stack) 0x%lx\n",
	    	            (unsigned long)p);
	    }
#       endif
	set_bl_entry_from_index(GC_incomplete_stack_bl, index);
    }
}

/*
 * Is the block starting at h of size len bytes black listed?   If so,
 * return the address of the next plausible r such that (r, len) might not
 * be black listed.  (R may not actually be in the heap.  We guarantee only
 * that every smaller value of r after h is also black listed.)
 * If (h,len) is not black listed, return 0.
 * Knows about the structure of the black list hash tables.
 */
struct hblk * GC_is_black_listed(h, len)
struct hblk * h;
word len;
{
    register int index = HASH((word)h);
    register word i;
    word nblocks = divHBLKSZ(len);

#   ifndef ALL_INTERIOR_POINTERS
      if (get_bl_entry_from_index(GC_new_normal_bl, index)
        && get_bl_entry_from_index(GC_old_normal_bl, index)) {
        return(h+1);
      }
#   endif
    
    for (i = 0; ; ) {
        if (GC_new_stack_bl[divWORDSZ(index)] == 0) {
            /* An easy case */
            i += WORDSZ - modWORDSZ(index);
        } else {
          if (get_bl_entry_from_index(GC_new_stack_bl, index)
            && get_bl_entry_from_index(GC_old_stack_bl, index)) {
            return(h+i+1);
          }
          i++;
        }
        if (i >= nblocks) break;
        index = HASH((word)(h+i));
    }
    return(0);
}

