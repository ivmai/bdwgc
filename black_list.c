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
 * See the definition of page_hash_table in gc_private.h.
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

/* Pointers to individual tables.  We replace one table by another by 	*/
/* switching these pointers. 						*/
word * GC_old_normal_bl;
		/* Nonstack false references seen at last full		*/
		/* collection.						*/
word * GC_incomplete_normal_bl;
		/* Nonstack false references seen since last		*/
		/* full collection.					*/
word * GC_old_stack_bl;
word * GC_incomplete_stack_bl;

GC_bl_init()
{
# ifndef ALL_INTERIOR_POINTERS
    GC_old_normal_bl = (word *)
    			 GC_scratch_alloc((word)(sizeof (page_hash_table)));
    GC_incomplete_normal_bl = (word *)GC_scratch_alloc
    					((word)(sizeof(page_hash_table)));
    if (GC_old_normal_bl == 0 || GC_incomplete_normal_bl == 0) {
        GC_err_printf0("Insufficient memory for black list\n");
        EXIT();
    }
# endif
    GC_old_stack_bl = (word *)GC_scratch_alloc((word)(sizeof(page_hash_table)));
    GC_incomplete_stack_bl = (word *)GC_scratch_alloc
    					((word)(sizeof(page_hash_table)));
    if (GC_old_stack_bl == 0 || GC_incomplete_stack_bl == 0) {
        GC_err_printf0("Insufficient memory for black list\n");
        EXIT();
    }
}
		
void GC_clear_bl(doomed)
word *doomed;
{
    bzero((char *)doomed, (int)sizeof(page_hash_table));
}

/* Signal the completion of a collection.  Turn the incomplete black	*/
/* lists into new black lists, etc.					*/			 
void GC_promote_black_lists()
{
    word * very_old_normal_bl = GC_old_normal_bl;
    word * very_old_stack_bl = GC_old_stack_bl;
    
    GC_old_normal_bl = GC_incomplete_normal_bl;
    GC_old_stack_bl = GC_incomplete_stack_bl;
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
        register int index = PHT_HASH(p);
        
        if (HDR(p) == 0 || get_pht_entry_from_index(GC_old_normal_bl, index)) {
#   	    ifdef PRINTBLACKLIST
		if (!get_pht_entry_from_index(GC_incomplete_normal_bl, index)) {
	    	  GC_printf1("Black listing (normal) 0x%lx\n",
	    	  	     (unsigned long) p);
	    	}
#           endif
            set_pht_entry_from_index(GC_incomplete_normal_bl, index);
        } /* else this is probably just an interior pointer to an allocated */
          /* object, and isn't worth black listing.			    */
    }
}
# endif

/* And the same for false pointers from the stack. */
void GC_add_to_black_list_stack(p)
word p;
{
    register int index = PHT_HASH(p);
        
    if (HDR(p) == 0 || get_pht_entry_from_index(GC_old_stack_bl, index)) {
#   	ifdef PRINTBLACKLIST
	    if (!get_pht_entry_from_index(GC_incomplete_stack_bl, index)) {
	    	  GC_printf1("Black listing (stack) 0x%lx\n",
	    	             (unsigned long)p);
	    }
#       endif
	set_pht_entry_from_index(GC_incomplete_stack_bl, index);
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
    register int index = PHT_HASH((word)h);
    register word i;
    word nblocks = divHBLKSZ(len);

#   ifndef ALL_INTERIOR_POINTERS
      if (get_pht_entry_from_index(GC_old_normal_bl, index)
          || get_pht_entry_from_index(GC_incomplete_normal_bl, index)) {
        return(h+1);
      }
#   endif
    
    for (i = 0; ; ) {
        if (GC_old_stack_bl[divWORDSZ(index)] == 0
            && GC_incomplete_stack_bl[divWORDSZ(index)] == 0) {
            /* An easy case */
            i += WORDSZ - modWORDSZ(index);
        } else {
          if (get_pht_entry_from_index(GC_old_stack_bl, index)
              || get_pht_entry_from_index(GC_incomplete_stack_bl, index)) {
            return(h+i+1);
          }
          i++;
        }
        if (i >= nblocks) break;
        index = PHT_HASH((word)(h+i));
    }
    return(0);
}

