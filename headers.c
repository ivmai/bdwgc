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
 
/*
 * This implements:
 * 1. allocation of heap block headers
 * 2. A map from addresses to heap block addresses to heap block headers
 *
 * Access speed is crucial.  We implement an index structure based on a 2
 * level tree.
 * For 64 bit machines this will have to be rewritten.  We expect that the
 * winning strategy there is to use a hash table as a cache, with
 * collisions resolved through a 4 or 5 level tree.
 */
 
# include "gc_private.h"

# if CPP_WORDSZ != 32
#   if CPP_WORDSZ > 32
 	--> This needs to be reimplemented.  See above.
#   else
	--> Get a real machine.
#   endif
# endif
 
hdr ** GC_top_index [TOP_SZ];
 
typedef hdr * bottom_index[BOTTOM_SZ];
 
/*
 * The bottom level index contains one of three kinds of values:
 * 0 means we're not responsible for this block.
 * 1 < (long)X <= MAX_JUMP means the block starts at least
 *        X * HBLKSIZE bytes before the current address.
 * A valid pointer points to a hdr structure. (The above can't be
 * valid pointers due to the GET_MEM return convention.)
 */
 
static bottom_index all_nils = { 0 };
 
/* Non-macro version of header location routine */
hdr * GC_find_header(h)
ptr_t h;
{
   return(HDR(h));
}
 
/* Routines to dynamically allocate collector data structures that will */
/* never be freed.							 */
 
static char * scratch_free_ptr = 0;
 
static char * scratch_end_ptr = 0;
 
ptr_t GC_scratch_alloc(bytes)
register word bytes;
{
    register char * result = scratch_free_ptr;
    scratch_free_ptr += bytes;
    if (scratch_free_ptr <= scratch_end_ptr) {
        return(result);
    }
    {
        long bytes_to_get = ((HINCR+1) * HBLKSIZE + bytes) & ~(HBLKSIZE - 1);
         
        scratch_free_ptr = (char *)GET_MEM(bytes_to_get);
        if (scratch_free_ptr == 0) {
            GC_printf("Out of memory - trying to allocate less\n");
            result = (char *)GET_MEM(bytes);
            if (result == 0) {
                GC_printf("Out of memory - giving up\n");
            } else {
                scratch_free_ptr -= bytes;
                return(result);
            }
        }
        scratch_end_ptr = scratch_free_ptr + bytes_to_get;
        return(GC_scratch_alloc(bytes));
    }
}

static hdr * hdr_free_list = 0;

/* Return an uninitialized header */
static hdr * alloc_hdr()
{
    register hdr * result;
    
    if (hdr_free_list == 0) {
        result = (hdr *) GC_scratch_alloc((word)(sizeof(hdr)));
    } else {
        result = hdr_free_list;
        hdr_free_list = (hdr *) (result -> hb_next);
    }
    return(result);
}

static void free_hdr(hhdr)
hdr * hhdr;
{
    hhdr -> hb_next = (struct hblk *) hdr_free_list;
    hdr_free_list = hhdr;
}
 
GC_init_headers()
{
    register int i;
     
    for (i = 0; i < TOP_SZ; i++) {
        GC_top_index[i] = all_nils;
    }
}

/* Make sure that there is a bottom level index block for address addr  */
static void get_index(addr)
register word addr;
{
    register word indx =
    		(word)(addr) >> (LOG_BOTTOM_SZ + LOG_HBLKSIZE);
    		
    if (GC_top_index[indx] == all_nils) {
        GC_top_index[indx] = (hdr **)
        		GC_scratch_alloc((word)(sizeof (bottom_index)));
        bzero((char *)(GC_top_index[indx]), (int)(sizeof (bottom_index)));
    }
}

/* Install a header for block h.  */
/* The header is uninitialized.	  */
void GC_install_header(h)
register struct hblk * h;
{
    get_index((word) h);
    HDR(h) = alloc_hdr();
}

/* Set up forwarding counts for block h of size sz */
void GC_install_counts(h, sz)
register struct hblk * h;
register word sz; /* bytes */
{
    register struct hblk * hbp;
    register int i;
    
    for (hbp = h; (char *)hbp < (char *)h + sz; hbp += BOTTOM_SZ) {
        get_index((word) hbp);
    }
    get_index((word)h + sz - 1);
    for (hbp = h + 1; (char *)hbp < (char *)h + sz; hbp += 1) {
        i = hbp - h;
        HDR(hbp) = (hdr *)(i > MAX_JUMP? MAX_JUMP : i);
    }
}

/* Remove the header for block h */
void GC_remove_header(h)
register struct hblk * h;
{
    free_hdr(HDR(h));
    HDR(h) = 0;
}

/* Remove forwarding counts for h */
void GC_remove_counts(h, sz)
register struct hblk * h;
register word sz; /* bytes */
{
    register struct hblk * hbp;
    
    for (hbp = h+1; (char *)hbp < (char *)h + sz; hbp += 1) {
        HDR(hbp) = 0;
    }
}

/* Apply fn to all allocated blocks */
/*VARARGS1*/
void GC_apply_to_all_blocks(fn, client_data)
void (*fn)(/* struct hblk *h, word client_data */);
word client_data;
{
    register int i, j;
    register hdr ** index_p;
    
    for (i = 0; i < TOP_SZ; i++) {
        index_p = GC_top_index[i];
        if (index_p != all_nils) {
            for (j = BOTTOM_SZ-1; j >= 0;) {
                if (!IS_FORWARDING_ADDR_OR_NIL(index_p[j])) {
                  if (index_p[j]->hb_map != GC_invalid_map) {
                    (*fn)(((struct hblk *)
                  	      (((i << LOG_BOTTOM_SZ) + j) << LOG_HBLKSIZE)),
                          client_data);
                  }
                  j--;
                } else if (index_p[j] == 0) {
                  j--;
                } else {
                  j -= (int)(index_p[j]);
                }
            }
        }
    }
}
