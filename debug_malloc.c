# include "gc_private.h"
# define START_FLAG ((word)0xfedcedcb)
# define END_FLAG ((word)0xbcdecdef)
	/* Stored both one past the end of user object, and one before	*/
	/* the end of the object as seen by the allocator.		*/

/* Object header */
typedef struct {
    char * oh_string;	/* object descriptor string	*/
    word oh_int;		/* object descriptor integers	*/
    word oh_sz;		/* Original malloc arg.		*/
    word oh_sf;			/* start flag */
} oh;
/* The size of the above structure is assumed not to dealign things,	*/
/* and to be a multiple of the word length.				*/

#define DEBUG_BYTES (sizeof (oh) + sizeof (word))
#define ROUNDED_UP_WORDS(n) BYTES_TO_WORDS((n) + WORDS_TO_BYTES(1) - 1)

bool GC_debugging_started = FALSE;

/* Check whether object with base pointer p has debugging info	*/ 
/* p is assumed to point to a legitimate object in our part	*/
/* of the heap.							*/
bool GC_has_debug_info(p)
ptr_t p;
{
    register oh * ohdr = (oh *)p;
    register ptr_t body = (ptr_t)(ohdr + 1);
    register word sz = GC_size((ptr_t) ohdr);
    
    if (HBLKPTR((ptr_t)ohdr) != HBLKPTR((ptr_t)body)
        || sz < sizeof (oh)) {
        return(FALSE);
    }
    if (ohdr -> oh_sf == (START_FLAG ^ (word)body)) return(TRUE);
    if (((word *)ohdr)[BYTES_TO_WORDS(sz)-1] == (END_FLAG ^ (word)body)) {
        return(TRUE);
    }
    return(FALSE);
}

/* Store debugging info into p.  Return displaced pointer. */
/* Assumes we don't hold allocation lock.		   */
ptr_t GC_store_debug_info(p, sz, string, integer)
register ptr_t p;	/* base pointer */
word sz; 	/* bytes */
char * string;
word integer;
{
    register word * result = (word *)((oh *)p + 1);
    DCL_LOCK_STATE;
    
    /* There is some argument that we should dissble signals here.	*/
    /* But that's expensive.  And this way things should only appear	*/
    /* inconsistent while we're in the handler.				*/
    LOCK();
    ((oh *)p) -> oh_string = string;
    ((oh *)p) -> oh_int = integer;
    ((oh *)p) -> oh_sz = sz;
    ((oh *)p) -> oh_sf = START_FLAG ^ (word)result;
    ((word *)p)[BYTES_TO_WORDS(GC_size(p))-1] =
         result[ROUNDED_UP_WORDS(sz)] = END_FLAG ^ (word)result;
    UNLOCK();
    return((ptr_t)result);
}

/* Check the object with debugging info at p 		*/
/* return NIL if it's OK.  Else return clobbered	*/
/* address.						*/
ptr_t GC_check_annotated_obj(ohdr)
register oh * ohdr;
{
    register ptr_t body = (ptr_t)(ohdr + 1);
    register word gc_sz = GC_size((ptr_t)ohdr);
    if (ohdr -> oh_sz + DEBUG_BYTES > gc_sz) {
        return((ptr_t)(&(ohdr -> oh_sz)));
    }
    if (ohdr -> oh_sf != (START_FLAG ^ (word)body)) {
        return((ptr_t)(&(ohdr -> oh_sf)));
    }
    if (((word *)ohdr)[BYTES_TO_WORDS(gc_sz)-1] != (END_FLAG ^ (word)body)) {
        return((ptr_t)((word *)ohdr + BYTES_TO_WORDS(gc_sz)-1));
    }
    if (((word *)body)[ROUNDED_UP_WORDS(ohdr -> oh_sz)]
        != (END_FLAG ^ (word)body)) {
        return((ptr_t)((word *)body + ROUNDED_UP_WORDS(ohdr -> oh_sz)));
    }
    return(0);
}

void GC_print_obj(p)
ptr_t p;
{
    register oh * ohdr = (oh *)GC_base(p);
    
    GC_err_printf("0x%lx (", (unsigned long)ohdr + sizeof(oh));
    GC_err_puts(ohdr -> oh_string);
    GC_err_printf(":%ld, sz=%ld)\n", (unsigned long)(ohdr -> oh_int),
        			     (unsigned long)(ohdr -> oh_sz));
}
void GC_print_smashed_obj(p, clobbered_addr)
ptr_t p, clobbered_addr;
{
    register oh * ohdr = (oh *)GC_base(p);
    
    GC_err_printf("0x%lx in object at 0x%lx(", (unsigned long)clobbered_addr,
    					       (unsigned long)p);
    if (clobbered_addr <= (ptr_t)(&(ohdr -> oh_sz))) {
        GC_err_printf("<smashed>, appr. sz = %ld)\n",
        	      BYTES_TO_WORDS(GC_size((ptr_t)ohdr)));
    } else {
        GC_err_puts(ohdr -> oh_string);
        GC_err_printf(":%ld, sz=%ld)\n", (unsigned long)(ohdr -> oh_int),
        			         (unsigned long)(ohdr -> oh_sz));
    }
}

# ifdef __STDC__
    extern_ptr_t GC_debug_malloc(size_t lb, char * s, int i)
# else
    extern_ptr_t GC_debug_malloc(lb, s, i)
    size_t lb;
    char * s;
    int i;
# endif
{
    extern_ptr_t result = GC_malloc(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf("GC_debug_malloc(%ld) returning NIL (",
        	      (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_debugging_started = TRUE;
        GC_register_displacement((word)sizeof(oh));
    }
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}

# ifdef __STDC__
    extern_ptr_t GC_debug_malloc_atomic(size_t lb, char * s, int i)
# else
    extern_ptr_t GC_debug_malloc_atomic(lb, s, i)
    size_t lb;
    char * s;
    int i;
# endif
{
    extern_ptr_t result = GC_malloc_atomic(lb + DEBUG_BYTES);
    
    if (result == 0) {
        GC_err_printf("GC_debug_malloc_atomic(%ld) returning NIL (",
        	      (unsigned long) lb);
        GC_err_puts(s);
        GC_err_printf(":%ld)\n", (unsigned long)i);
        return(0);
    }
    if (!GC_debugging_started) {
        GC_debugging_started = TRUE;
        GC_register_displacement((word)sizeof(oh));
    }
    return (GC_store_debug_info(result, (word)lb, s, (word)i));
}
# ifdef __STDC__
    void GC_debug_free(extern_ptr_t p)
# else
    void GC_debug_free(p)
    extern_ptr_t p;
# endif
{
    register extern_ptr_t base = GC_base(p);
    register ptr_t clobbered;
    
    if (base == 0) {
        GC_err_printf("Attempt to free invalid pointer %lx\n",
        	      (unsigned long)p);
        ABORT("free(invalid pointer)");
    }
    if ((ptr_t)p - (ptr_t)base != sizeof(oh)) {
        GC_err_printf("GC_debug_free called on pointer %lx wo debugging info\n",
        	  (unsigned long)p);
    } else {
      clobbered = GC_check_annotated_obj((oh *)base);
      if (clobbered != 0) {
        GC_err_printf("GC_debug_free: found smashed object at ");
        GC_print_smashed_obj(p, clobbered);
      }
    }
    GC_free(GC_base(p));
}

# ifdef __STDC__
    extern_ptr_t GC_debug_realloc(extern_ptr_t p, size_t lb, char *s, int i)
# else
    extern_ptr_t GC_debug_realloc(p, lb, s, i)
    extern_ptr_t p;
    size_t lb;
    char *s;
    int i;
# endif
{
    register extern_ptr_t base = GC_base(p);
    register ptr_t clobbered;
    register extern_ptr_t result = GC_debug_malloc(lb, s, i);
    register size_t copy_sz = lb;
    register size_t old_sz;
    
    if (base == 0) {
        GC_err_printf(
              "Attempt to free invalid pointer %lx\n", (unsigned long)p);
        ABORT("realloc(invalid pointer)");
    }
    if ((ptr_t)p - (ptr_t)base != sizeof(oh)) {
        GC_err_printf(
        	"GC_debug_realloc called on pointer %lx wo debugging info\n",
        	(unsigned long)p);
        return(GC_realloc(p, lb));
    }
    clobbered = GC_check_annotated_obj((oh *)base);
    if (clobbered != 0) {
        GC_err_printf("GC_debug_realloc: found smashed object at ");
        GC_print_smashed_obj(p, clobbered);
    }
    old_sz = ((oh *)base) -> oh_sz;
    if (old_sz < copy_sz) copy_sz = old_sz;
    if (result == 0) return(0);
    bcopy((char *)p, (char *)result, (int) copy_sz);
    return(result);
}

/* Check all marked objects in the given block for validity */
/*ARGSUSED*/
void GC_check_heap_block(hbp, dummy)
register struct hblk *hbp;	/* ptr to current heap block		*/
word dummy;
{
    register struct hblkhdr * hhdr = HDR(hbp);
    register word sz = hhdr -> hb_sz;
    register int word_no;
    register word *p, *plim;
    
    p = (word *)(hbp->hb_body);
    word_no = HDR_WORDS;
    plim = (word *)((((word)hbp) + HBLKSIZE)
		   - WORDS_TO_BYTES(sz));

    /* go through all words in block */
	do {
	    if( mark_bit_from_hdr(hhdr, word_no)
	        && GC_has_debug_info((ptr_t)p)) {
	        ptr_t clobbered = GC_check_annotated_obj((oh *)p);
	        
	        if (clobbered != 0) {
	            GC_err_printf(
	                "GC_check_heap_block: found smashed object at ");
        	    GC_print_smashed_obj((ptr_t)p, clobbered);
	        }
	    }
	    word_no += sz;
	    p += sz;
	} while( p <= plim );
}


/* This assumes that all accessible objects are marked, and that	*/
/* I hold the allocation lock.	Normally called by collector.		*/
void GC_check_heap()
{
    GC_apply_to_all_blocks(GC_check_heap_block, (word)0);
}

struct closure {
    GC_finalization_proc cl_fn;
    extern_ptr_t cl_data;
};

# ifdef __STDC__
    void * GC_make_closure(GC_finalization_proc fn, void * data)
# else
    extern_ptr_t GC_make_closure(fn, data)
    GC_finalization_proc fn;
    extern_ptr_t data;
# endif
{
    struct closure * result =
    		(struct closure *) GC_malloc(sizeof (struct closure));
    
    result -> cl_fn = fn;
    result -> cl_data = data;
    return((extern_ptr_t)result);
}

# ifdef __STDC__
    void GC_debug_invoke_finalizer(void * obj, void * data)
# else
    void GC_debug_invoke_finalizer(obj, data)
    char * obj;
    char * data;
# endif
{
    register struct closure * cl = (struct closure *) data;
    
    (*(cl -> cl_fn))((extern_ptr_t)((char *)obj + sizeof(oh)), cl -> cl_data);
} 