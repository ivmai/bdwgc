# ifdef CHECKSUMS

# include "gc_private.h"

/* This is debugging code intended to verify the results of dirty bit	*/
/* computations. Works only in a single threaded environment.		*/
/* We assume that stubborn objects are changed only when they are 	*/
/* enabled for writing.  (Certain kinds of writing are actually		*/
/* safe under other conditions.)					*/
# define NSUMS 2000

# define OFFSET 100000

typedef struct {
	bool new_valid;
	word old_sum;
	word new_sum;
	struct hblk * block;	/* Block to which this refers + OFFSET  */
				/* to hide it from colector.		*/
} page_entry;

page_entry GC_sums [NSUMS];

word GC_checksum(h)
struct hblk *h;
{
    register word *p = (word *)h;
    register word *lim = (word *)(h+1);
    register word result = 0;
    
    while (p < lim) {
        result += *p++;
    }
    return(result);
}

# ifdef STUBBORN_ALLOC
/* Check whether a stubborn object from the given block appears on	*/
/* the appropriate free list.						*/
bool GC_on_free_list(h)
struct hblk *h;
{
    register hdr * hhdr = HDR(h);
    register int sz = hhdr -> hb_sz;
    ptr_t p;
    
    if (sz > MAXOBJSZ) return(FALSE);
    for (p = GC_sobjfreelist[sz]; p != 0; p = obj_link(p)) {
        if (HBLKPTR(p) == h) return(TRUE);
    }
    return(FALSE);
}
# endif
 
int GC_n_dirty_errors;
int GC_n_changed_errors;
int GC_n_clean;
int GC_n_dirty;

void GC_update_check_page(h, index)
struct hblk *h;
int index;
{
    page_entry *pe = GC_sums + index;
    register hdr * hhdr = HDR(h);
    
    if (pe -> block != 0 && pe -> block != h + OFFSET) ABORT("goofed");
    pe -> old_sum = pe -> new_sum;
    pe -> new_sum = GC_checksum(h);
    if (GC_page_was_dirty(h)) {
    	GC_n_dirty++;
    } else {
    	GC_n_clean++;
    }
    if (pe -> new_valid && pe -> old_sum != pe -> new_sum) {
    	if (!GC_page_was_dirty(h)) {
    	    /* Set breakpoint here */GC_n_dirty_errors++;
    	}
#	ifdef STUBBORN_ALLOC
    	  if (!IS_FORWARDING_ADDR_OR_NIL(hhdr)
    	    && hhdr -> hb_map != GC_invalid_map
    	    && hhdr -> hb_obj_kind == STUBBORN
    	    && !GC_page_was_changed(h)
    	    && !GC_on_free_list(h)) {
    	    /* if GC_on_free_list(h) then reclaim may have touched it	*/
    	    /* without any allocations taking place.			*/
    	    /* Set breakpoint here */GC_n_changed_errors++;
    	  }
#	endif
    }
    pe -> new_valid = TRUE;
    pe -> block = h + OFFSET;
}

/* Should be called immediately after GC_read_dirty and GC_read_changed. */
void GC_check_dirty()
{
    register int index;
    register int i;
    register struct hblk *h;
    register ptr_t start;
    
    GC_n_dirty_errors = 0;
    GC_n_changed_errors = 0;
    GC_n_clean = 0;
    GC_n_dirty = 0;
    
    index = 0;
    for (i = 0; i < GC_n_heap_sects; i++) {
    	start = GC_heap_sects[i].hs_start;
        for (h = (struct hblk *)start;
             h < (struct hblk *)(start + GC_heap_sects[i].hs_bytes);
             h++) {
             GC_update_check_page(h, index);
             index++;
             if (index >= NSUMS) goto out;
        }
    }
out:
    GC_printf2("Checked %lu clean and %lu dirty pages\n",
    	      (unsigned long) GC_n_clean, (unsigned long) GC_n_dirty);
    if (GC_n_dirty_errors > 0) {
        GC_printf1("Found %lu dirty bit errors\n",
        	   (unsigned long)GC_n_dirty_errors);
    }
    if (GC_n_changed_errors > 0) {
    	GC_printf1("Found %lu changed bit errors\n",
        	   (unsigned long)GC_n_changed_errors);
    }
}

# else

extern int GC_quiet;
	/* ANSI C doesn't allow translation units to be empty.	*/
	/* So we guarantee this one is nonempty.		*/

# endif /* CHECKSUMS */
