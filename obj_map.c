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
  
/* Routines for maintaining maps describing heap block
 * layouts for various object sizes.  Allows fast pointer validity checks
 * and fast location of object start locations on machines (such as SPARC)
 * with slow division.
 *
 * Boehm, February 6, 1992 1:00:09 pm PST
 */
 
# include "gc_private.h"

char * GC_invalid_map = 0;

/* Invalidate the object map associated with a block.	Free blocks	*/
/* are identified by invalid maps.					*/
void GC_invalidate_map(hhdr)
hdr *hhdr;
{
    register int displ;
    
    if (GC_invalid_map == 0) {
        GC_invalid_map = GC_scratch_alloc(MAP_SIZE);
        for (displ = 0; displ < HBLKSIZE; displ++) {
            MAP_ENTRY(GC_invalid_map, displ) = OBJ_INVALID;
        }
    }
    hhdr -> hb_map = GC_invalid_map;
}

/* Consider pointers that are offset bytes displaced from the beginning */
/* of an object to be valid.                                            */
void GC_register_displacement(offset) 
word offset;
{
# ifndef ALL_INTERIOR_POINTERS
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    GC_register_displacement_inner(offset);
    UNLOCK();
    ENABLE_SIGNALS();
# endif
}

void GC_register_displacement_inner(offset) 
word offset;
{
# ifndef ALL_INTERIOR_POINTERS
    register int i;
    
    if (offset > MAX_OFFSET) {
        ABORT("Bad argument to GC_register_displacement");
    }
    if (!GC_valid_offsets[offset]) {
      GC_valid_offsets[offset] = TRUE;
      GC_modws_valid_offsets[offset % sizeof(word)] = TRUE;
      for (i = 0; i <= MAXOBJSZ; i++) {
          if (GC_obj_map[i] != 0) {
             if (i == 0) {
               GC_obj_map[i][offset + HDR_BYTES] = offset >> 2;
             } else {
               register int j;
               register int lb = WORDS_TO_BYTES(i);
               
               if (offset < lb) {
                 for (j = offset + HDR_BYTES; j < HBLKSIZE; j += lb) {
                   GC_obj_map[i][j] = offset >> 2;
                 }
               }
             }
          }
      }
    }
# endif
}


/* Add a heap block map for objects of size sz to obj_map.  */
void GC_add_map_entry(sz)
word sz;
{
    register int obj_start;
    register int displ;
    register char * new_map;
    
    if (sz > MAXOBJSZ) sz = 0;
    if (GC_obj_map[sz] != 0) {
        return;
    }
    new_map = GC_scratch_alloc(MAP_SIZE);
#   ifdef PRINTSTATS
        GC_printf("Adding block map for size %lu\n", (unsigned long)sz);
#   endif
    for (displ = 0; displ < HBLKSIZE; displ++) {
        MAP_ENTRY(new_map,displ) = OBJ_INVALID;
    }
    if (sz == 0) {
        for(displ = 0; displ <= MAX_OFFSET; displ++) {
            if (OFFSET_VALID(displ)) {
                MAP_ENTRY(new_map,displ+HDR_BYTES) = BYTES_TO_WORDS(displ);
            }
        }
    } else {
        for (obj_start = HDR_BYTES;
             obj_start + WORDS_TO_BYTES(sz) <= HBLKSIZE;
             obj_start += WORDS_TO_BYTES(sz)) {
             for (displ = 0; displ < WORDS_TO_BYTES(sz); displ++) {
                 if (OFFSET_VALID(displ)) {
                     MAP_ENTRY(new_map, obj_start + displ) =
                     				BYTES_TO_WORDS(displ);
                 }
             }
        }
    }
    GC_obj_map[sz] = new_map;
}
