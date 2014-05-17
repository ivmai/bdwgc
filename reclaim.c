/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
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
/* Boehm, May 19, 1994 2:00 pm PDT */

#include <stdio.h>
#include "gc_priv.h"

signed_word GC_mem_found = 0;
			/* Number of longwords of memory GC_reclaimed     */

# ifdef FIND_LEAK
static report_leak(p, sz)
ptr_t p;
word sz;
{
    if (HDR(p) -> hb_obj_kind == PTRFREE) {
        GC_err_printf0("Leaked atomic object at ");
    } else {
        GC_err_printf0("Leaked composite object at ");
    }
    if (GC_debugging_started && GC_has_debug_info(p)) {
        GC_print_obj(p);
    } else {
        GC_err_printf2("0x%lx (appr. size = %ld)\n",
       		      (unsigned long)p,
       		      (unsigned long)WORDS_TO_BYTES(sz));
    }
}

#   define FOUND_FREE(hblk, word_no) \
      if (abort_if_found) { \
         report_leak((long)hblk + WORDS_TO_BYTES(word_no), \
         	     HDR(hblk) -> hb_sz); \
      }
# else
#   define FOUND_FREE(hblk, word_no)
# endif

/*
 * reclaim phase
 *
 */


/*
 * Test whether a block is completely empty, i.e. contains no marked
 * objects.  This does not require the block to be in physical
 * memory.
 */
 
bool GC_block_empty(hhdr)
register hdr * hhdr;
{
    register word *p = (word *)(&(hhdr -> hb_marks[0]));
    register word * plim =
	    		(word *)(&(hhdr -> hb_marks[MARK_BITS_SZ]));
    while (p < plim) {
	if (*p++) return(FALSE);
    }
    return(TRUE);
}

# ifdef GATHERSTATS
#   define INCR_WORDS(sz) n_words_found += (sz)
# else
#   define INCR_WORDS(sz)
# endif
/*
 * Restore unmarked small objects in h of size sz to the object
 * free list.  Returns the new list.
 * Clears unmarked objects.
 */
/*ARGSUSED*/
ptr_t GC_reclaim_clear(hbp, hhdr, sz, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
register hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
register word sz;
{
    register int word_no;
    register word *p, *q, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif        
    
    p = (word *)(hbp->hb_body);
    word_no = HDR_WORDS;
    plim = (word *)((((word)hbp) + HBLKSIZE)
		   - WORDS_TO_BYTES(sz));

    /* go through all words in block */
	while( p <= plim )  {
	    if( mark_bit_from_hdr(hhdr, word_no) ) {
		p += sz;
	    } else {
		FOUND_FREE(hbp, word_no);
		INCR_WORDS(sz);
		/* object is available - put on list */
		    obj_link(p) = list;
		    list = ((ptr_t)p);
		/* Clear object, advance p to next object in the process */
		    q = p + sz;
                    p++; /* Skip link field */
                    while (p < q) {
			*p++ = 0;
		    }
	    }
	    word_no += sz;
	}
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
}

#ifndef SMALL_CONFIG

/*
 * A special case for 2 word composite objects (e.g. cons cells):
 */
/*ARGSUSED*/
ptr_t GC_reclaim_clear2(hbp, hhdr, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
{
    register word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif
    register word mark_word;
    register int i;
#   define DO_OBJ(start_displ) \
	if (!(mark_word & ((word)1 << start_displ))) { \
	    FOUND_FREE(hbp, p - (word *)hbp + start_displ); \
	    p[start_displ] = (word)list; \
	    list = (ptr_t)(p+start_displ); \
	    p[start_displ+1] = 0; \
	    INCR_WORDS(2); \
	}
    
    p = (word *)(hbp->hb_body);
    plim = (word *)(((word)hbp) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    for (i = 0; i < WORDSZ; i += 8) {
		DO_OBJ(0);
		DO_OBJ(2);
		DO_OBJ(4);
		DO_OBJ(6);
		p += 8;
		mark_word >>= 8;
	    }
	}	        
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
#   undef DO_OBJ
}

/*
 * Another special case for 4 word composite objects:
 */
/*ARGSUSED*/
ptr_t GC_reclaim_clear4(hbp, hhdr, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
{
    register word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif
    register word mark_word;
#   define DO_OBJ(start_displ) \
	if (!(mark_word & ((word)1 << start_displ))) { \
	    FOUND_FREE(hbp, p - (word *)hbp + start_displ); \
	    p[start_displ] = (word)list; \
	    list = (ptr_t)(p+start_displ); \
	    p[start_displ+1] = 0; \
	    p[start_displ+2] = 0; \
	    p[start_displ+3] = 0; \
	    INCR_WORDS(4); \
	}
    
    p = (word *)(hbp->hb_body);
    plim = (word *)(((word)hbp) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    DO_OBJ(0);
	    DO_OBJ(4);
	    DO_OBJ(8);
	    DO_OBJ(12);
	    DO_OBJ(16);
	    DO_OBJ(20);
	    DO_OBJ(24);
	    DO_OBJ(28);
#	    if CPP_WORDSZ == 64
	      DO_OBJ(32);
	      DO_OBJ(36);
	      DO_OBJ(40);
	      DO_OBJ(44);
	      DO_OBJ(48);
	      DO_OBJ(52);
	      DO_OBJ(56);
	      DO_OBJ(60);
#	    endif
	    p += WORDSZ;
	}	        
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
#   undef DO_OBJ
}

#endif /* !SMALL_CONFIG */

/* The same thing, but don't clear objects: */
/*ARGSUSED*/
ptr_t GC_reclaim_uninit(hbp, hhdr, sz, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
register hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
register word sz;
{
    register int word_no;
    register word *p, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif
    
    p = (word *)(hbp->hb_body);
    word_no = HDR_WORDS;
    plim = (word *)((((word)hbp) + HBLKSIZE)
		   - WORDS_TO_BYTES(sz));

    /* go through all words in block */
	while( p <= plim )  {
	    if( !mark_bit_from_hdr(hhdr, word_no) ) {
		FOUND_FREE(hbp, word_no);
		INCR_WORDS(sz);
		/* object is available - put on list */
		    obj_link(p) = list;
		    list = ((ptr_t)p);
	    }
	    p += sz;
	    word_no += sz;
	}
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
}

#ifndef SMALL_CONFIG
/*
 * Another special case for 2 word atomic objects:
 */
/*ARGSUSED*/
ptr_t GC_reclaim_uninit2(hbp, hhdr, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
{
    register word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif
    register word mark_word;
    register int i;
#   define DO_OBJ(start_displ) \
	if (!(mark_word & ((word)1 << start_displ))) { \
	    FOUND_FREE(hbp, p - (word *)hbp + start_displ); \
	    p[start_displ] = (word)list; \
	    list = (ptr_t)(p+start_displ); \
	    INCR_WORDS(2); \
	}
    
    p = (word *)(hbp->hb_body);
    plim = (word *)(((word)hbp) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    for (i = 0; i < WORDSZ; i += 8) {
		DO_OBJ(0);
		DO_OBJ(2);
		DO_OBJ(4);
		DO_OBJ(6);
		p += 8;
		mark_word >>= 8;
	    }
	}	        
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
#   undef DO_OBJ
}

/*
 * Another special case for 4 word atomic objects:
 */
/*ARGSUSED*/
ptr_t GC_reclaim_uninit4(hbp, hhdr, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
{
    register word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif
    register word mark_word;
#   define DO_OBJ(start_displ) \
	if (!(mark_word & ((word)1 << start_displ))) { \
	    FOUND_FREE(hbp, p - (word *)hbp + start_displ); \
	    p[start_displ] = (word)list; \
	    list = (ptr_t)(p+start_displ); \
	    INCR_WORDS(4); \
	}
    
    p = (word *)(hbp->hb_body);
    plim = (word *)(((word)hbp) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    DO_OBJ(0);
	    DO_OBJ(4);
	    DO_OBJ(8);
	    DO_OBJ(12);
	    DO_OBJ(16);
	    DO_OBJ(20);
	    DO_OBJ(24);
	    DO_OBJ(28);
#	    if CPP_WORDSZ == 64
	      DO_OBJ(32);
	      DO_OBJ(36);
	      DO_OBJ(40);
	      DO_OBJ(44);
	      DO_OBJ(48);
	      DO_OBJ(52);
	      DO_OBJ(56);
	      DO_OBJ(60);
#	    endif
	    p += WORDSZ;
	}	        
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
#   undef DO_OBJ
}

/* Finally the one word case, which never requires any clearing: */
/*ARGSUSED*/
ptr_t GC_reclaim1(hbp, hhdr, list, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
hdr * hhdr;
bool abort_if_found;		/* Abort if a reclaimable object is found */
register ptr_t list;
{
    register word * mark_word_addr = &(hhdr->hb_marks[divWORDSZ(HDR_WORDS)]);
    register word *p, *plim;
#   ifdef GATHERSTATS
        register int n_words_found = 0;
#   endif
    register word mark_word;
    register int i;
#   define DO_OBJ(start_displ) \
	if (!(mark_word & ((word)1 << start_displ))) { \
	    FOUND_FREE(hbp, p - (word *)hbp + start_displ); \
	    p[start_displ] = (word)list; \
	    list = (ptr_t)(p+start_displ); \
	    INCR_WORDS(1); \
	}
    
    p = (word *)(hbp->hb_body);
    plim = (word *)(((word)hbp) + HBLKSIZE);

    /* go through all words in block */
	while( p < plim )  {
	    mark_word = *mark_word_addr++;
	    for (i = 0; i < WORDSZ; i += 4) {
		DO_OBJ(0);
		DO_OBJ(1);
		DO_OBJ(2);
		DO_OBJ(3);
		p += 4;
		mark_word >>= 4;
	    }
	}	        
#   ifdef GATHERSTATS
	GC_mem_found += n_words_found;
#   endif
    return(list);
#   undef DO_OBJ
}

#endif /* !SMALL_CONFIG */

/*
 * Restore unmarked small objects in the block pointed to by hbp
 * to the appropriate object free list.
 * If entirely empty blocks are to be completely deallocated, then
 * caller should perform that check.
 */
void GC_reclaim_small_nonempty_block(hbp, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
int abort_if_found;		/* Abort if a reclaimable object is found */
{
    hdr * hhdr;
    register word sz;		/* size of objects in current block	*/
    register struct obj_kind * ok;
    register ptr_t * flh;
    
    hhdr = HDR(hbp);
    sz = hhdr -> hb_sz;
    hhdr -> hb_last_reclaimed = (unsigned short) GC_gc_no;
    ok = &GC_obj_kinds[hhdr -> hb_obj_kind];
    flh = &(ok -> ok_freelist[sz]);
    GC_write_hint(hbp);

    if (ok -> ok_init) {
      switch(sz) {
#      ifndef SMALL_CONFIG
        case 1:
            *flh = GC_reclaim1(hbp, hhdr, *flh, abort_if_found);
            break;
        case 2:
            *flh = GC_reclaim_clear2(hbp, hhdr, *flh, abort_if_found);
            break;
        case 4:
            *flh = GC_reclaim_clear4(hbp, hhdr, *flh, abort_if_found);
            break;
#      endif
        default:
            *flh = GC_reclaim_clear(hbp, hhdr, sz, *flh, abort_if_found);
            break;
      }
    } else {
      switch(sz) {
#      ifndef SMALL_CONFIG
        case 1:
            *flh = GC_reclaim1(hbp, hhdr, *flh, abort_if_found);
            break;
        case 2:
            *flh = GC_reclaim_uninit2(hbp, hhdr, *flh, abort_if_found);
            break;
        case 4:
            *flh = GC_reclaim_uninit4(hbp, hhdr, *flh, abort_if_found);
            break;
#      endif
        default:
            *flh = GC_reclaim_uninit(hbp, hhdr, sz, *flh, abort_if_found);
            break;
      }
    } 
}

/*
 * Restore an unmarked large object or an entirely empty blocks of small objects
 * to the heap block free list.
 * Otherwise enqueue the block for later processing
 * by GC_reclaim_small_nonempty_block.
 * If abort_if_found is TRUE, then process any block immediately.
 */
void GC_reclaim_block(hbp, abort_if_found)
register struct hblk *hbp;	/* ptr to current heap block		*/
word abort_if_found;		/* Abort if a reclaimable object is found */
{
    register hdr * hhdr;
    register word sz;		/* size of objects in current block	*/
    register struct obj_kind * ok;
    struct hblk ** rlh;

    hhdr = HDR(hbp);
    sz = hhdr -> hb_sz;
    ok = &GC_obj_kinds[hhdr -> hb_obj_kind];

    if( sz > MAXOBJSZ ) {  /* 1 big object */
        if( !mark_bit_from_hdr(hhdr, HDR_WORDS) ) {
	    FOUND_FREE(hbp, HDR_WORDS);
#	    ifdef GATHERSTATS
	        GC_mem_found += sz;
#	    endif
	    GC_freehblk(hbp);
	}
    } else {
        bool empty = GC_block_empty(hhdr);
        if (abort_if_found) {
    	  GC_reclaim_small_nonempty_block(hbp, (int)abort_if_found);
        } else if (empty) {
#	  ifdef GATHERSTATS
            GC_mem_found += BYTES_TO_WORDS(HBLKSIZE);
#	  endif
          GC_freehblk(hbp);
        } else {
          /* group of smaller objects, enqueue the real work */
          rlh = &(ok -> ok_reclaim_list[sz]);
          hhdr -> hb_next = *rlh;
          *rlh = hbp;
        }
    }
}

/* Routines to gather and print heap block info 	*/
/* intended for debugging.  Otherwise should be called	*/
/* with lock.						*/
static number_of_blocks;
static total_bytes;

/* Number of set bits in a word.  Not performance critical.	*/
static int set_bits(n)
word n;
{
    register word m = n;
    register int result = 0;
    
    while (m > 0) {
    	if (m & 1) result++;
    	m >>= 1;
    }
    return(result);
}

/* Return the number of set mark bits in the given header	*/
int GC_n_set_marks(hhdr)
hdr * hhdr;
{
    register int result = 0;
    register int i;
    
    for (i = 0; i < MARK_BITS_SZ; i++) {
        result += set_bits(hhdr -> hb_marks[i]);
    }
    return(result);
}

/*ARGSUSED*/
void GC_print_block_descr(h, dummy)
struct hblk *h;
word dummy;
{
    register hdr * hhdr = HDR(h);
    register bytes = WORDS_TO_BYTES(hhdr -> hb_sz);
    
    GC_printf3("(%lu:%lu,%lu)", (unsigned long)(hhdr -> hb_obj_kind),
    			        (unsigned long)bytes,
    			        (unsigned long)(GC_n_set_marks(hhdr)));
    bytes += HDR_BYTES + HBLKSIZE-1;
    bytes &= ~(HBLKSIZE-1);
    total_bytes += bytes;
    number_of_blocks++;
}

void GC_print_block_list()
{
    GC_printf0("(kind(0=ptrfree,1=normal,2=unc.,3=stubborn):size_in_bytes, #_marks_set)\n");
    number_of_blocks = 0;
    total_bytes = 0;
    GC_apply_to_all_blocks(GC_print_block_descr, (word)0);
    GC_printf2("\nblocks = %lu, bytes = %lu\n",
    	       (unsigned long)number_of_blocks,
    	       (unsigned long)total_bytes);
}

/*
 * Do the same thing on the entire heap, after first clearing small object
 * free lists (if we are not just looking for leaks).
 */
void GC_start_reclaim(abort_if_found)
int abort_if_found;		/* Abort if a GC_reclaimable object is found */
{
    int kind;
    
    /* Clear reclaim- and free-lists */
      for (kind = 0; kind < GC_n_kinds; kind++) {
        register ptr_t *fop;
        register ptr_t *lim;
        register struct hblk ** hbpp;
        register struct hblk ** hlim;
          
        if (!abort_if_found) {
            lim = &(GC_obj_kinds[kind].ok_freelist[MAXOBJSZ+1]);
	    for( fop = GC_obj_kinds[kind].ok_freelist; fop < lim; fop++ ) {
	      *fop = 0;
	    }
	} /* otherwise free list objects are marked, 	*/
	  /* and its safe to leave them			*/
	hlim = &(GC_obj_kinds[kind].ok_reclaim_list[MAXOBJSZ+1]);
	for( hbpp = GC_obj_kinds[kind].ok_reclaim_list;
	    hbpp < hlim; hbpp++ ) {
	    *hbpp = 0;
	}
      }
    
#   ifdef PRINTBLOCKS
        GC_printf0("GC_reclaim: current block sizes:\n");
        GC_print_block_list();
#   endif

  /* Go through all heap blocks (in hblklist) and reclaim unmarked objects */
  /* or enqueue the block for later processing.				   */
    GC_apply_to_all_blocks(GC_reclaim_block, (word)abort_if_found);
    
}

/*
 * Sweep blocks of the indicated object size and kind until either the
 * appropriate free list is nonempty, or there are no more blocks to
 * sweep.
 */
void GC_continue_reclaim(sz, kind)
word sz;	/* words */
int kind;
{
    register hdr * hhdr;
    register struct hblk * hbp;
    register struct obj_kind * ok = &(GC_obj_kinds[kind]);
    struct hblk ** rlh = &(ok -> ok_reclaim_list[sz]);
    ptr_t *flh = &(ok -> ok_freelist[sz]);
    
    
    while ((hbp = *rlh) != 0) {
        hhdr = HDR(hbp);
        *rlh = hhdr -> hb_next;
        GC_reclaim_small_nonempty_block(hbp, FALSE);
        if (*flh != 0) break;
    }
}

/*
 * Reclaim all blocks that have been recently reclaimed.
 * Clear lists of blocks waiting to be reclaimed.
 * Must be done before clearing mark bits with the world running,
 * since otherwise a subsequent reclamation of block would see
 * the wrong mark bits.
 * SHOULD PROBABLY BE INCREMENTAL
 */
void GC_reclaim_or_delete_all()
{
    register word sz;
    register int kind;
    register hdr * hhdr;
    register struct hblk * hbp;
    register struct obj_kind * ok;
    struct hblk ** rlh;
#   ifdef PRINTTIMES
	CLOCK_TYPE start_time;
	CLOCK_TYPE done_time;
	
	GET_TIME(start_time);
#   endif
    
    for (kind = 0; kind < GC_n_kinds; kind++) {
    	ok = &(GC_obj_kinds[kind]);
    	for (sz = 1; sz <= MAXOBJSZ; sz++) {
    	    rlh = &(ok -> ok_reclaim_list[sz]);
    	    while ((hbp = *rlh) != 0) {
        	hhdr = HDR(hbp);
        	*rlh = hhdr -> hb_next;
        	if (hhdr -> hb_last_reclaimed == GC_gc_no - 1) {
        	    /* It's likely we'll need it this time, too	*/
        	    /* It's been touched recently, so this	*/
        	    /* shouldn't trigger paging.		*/
        	    GC_reclaim_small_nonempty_block(hbp, FALSE);
        	}
            }
        }
    }
#   ifdef PRINTTIMES
	GET_TIME(done_time);
	GC_printf1("Disposing of reclaim lists took %lu msecs\n",
	           MS_TIME_DIFF(done_time,start_time));
#   endif
}
