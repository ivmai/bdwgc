/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991, 1992 by Xerox Corporation.  All rights reserved.

 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
# define I_HIDE_POINTERS
# include "gc.h"
# include "gc_private.h"
# ifdef __STDC__
    typedef void * void_star;
# else
    typedef char * void_star;
# endif

# define LOG_TSIZE 7
# define TSIZE (1 << LOG_TSIZE)
# define HASH(addr) \
    ((((word)(addr) >> 3) ^ ((word)(addr) >> (3+LOG_TSIZE))) \
    & (TSIZE - 1))
    
static struct disappearing_link {
    word dl_hidden_obj;		/* Pointer to object base	*/
    word dl_hidden_link;	/* Field to be cleared.		*/
    struct disappearing_link * dl_next;
} * dl_head[TSIZE] = {0};

static struct finalizable_object {
    word fo_hidden_base;	/* Pointer to object base	*/
    GC_finalization_proc fo_fn;	/* Finalizer.			*/
    ptr_t fo_client_data;
    word fo_object_size;	/* In bytes.			*/
    struct finalizable_object * fo_next;
} * fo_head[TSIZE] = {0};

# ifdef SRC_M3
void GC_push_finalizer_structures()
{
    GC_push_all((ptr_t)dl_head, (ptr_t)(dl_head + TSIZE));
    GC_push_all((ptr_t)fo_head, (ptr_t)(fo_head + TSIZE));
}
# endif

# define ALLOC(x, t) t *x = (t *)GC_malloc(sizeof (t))

int GC_register_disappearing_link(link)
void_star * link;
{
    ptr_t base;
    
    base = (ptr_t)GC_base((void_star)link);
    if (base == 0)
    	ABORT("Bad arg to GC_register_disappearing_link");
    return(GC_general_register_disappearing_link(link, base));
}

int GC_general_register_disappearing_link(link, obj)
void_star * link;
void_star obj;
{
    struct disappearing_link *curr_dl;
    int index;
    /* Allocate before acquiring lock */
      ALLOC(new_dl, struct disappearing_link);
    DCL_LOCK_STATE;
    
    index = HASH(link);
    if ((word)link & (ALIGNMENT-1))
    	ABORT("Bad arg to GC_general_register_disappearing_link");
    DISABLE_SIGNALS();
    LOCK();
    curr_dl = dl_head[index];
    for (curr_dl = dl_head[index]; curr_dl != 0; curr_dl = curr_dl -> dl_next) {
        if (curr_dl -> dl_hidden_link == HIDE_POINTER(link)) {
            curr_dl -> dl_hidden_obj = HIDE_POINTER(obj);
            UNLOCK();
    	    ENABLE_SIGNALS();
    	    GC_free((extern_ptr_t)new_dl);
            return(1);
        }
    }
    {
        new_dl -> dl_hidden_obj = HIDE_POINTER(obj);
        new_dl -> dl_hidden_link = HIDE_POINTER(link);
        new_dl -> dl_next = dl_head[index];
        dl_head[index] = new_dl;
        UNLOCK();
        ENABLE_SIGNALS();
        return(0);
    }
}

int GC_unregister_disappearing_link(link)
void_star * link;
{
    struct disappearing_link *curr_dl, *prev_dl;
    int index;
    DCL_LOCK_STATE;
    
    index = HASH(link);
    if (((unsigned long)link & (ALIGNMENT-1)))
    	return(0);
    DISABLE_SIGNALS();
    LOCK();
    prev_dl = 0; curr_dl = dl_head[index];
    while (curr_dl != 0) {
        if (curr_dl -> dl_hidden_link == HIDE_POINTER(link)) {
            if (prev_dl == 0) {
                dl_head[index] = curr_dl -> dl_next;
            } else {
               prev_dl -> dl_next = curr_dl -> dl_next;
            }
            UNLOCK();
    	    ENABLE_SIGNALS();
            GC_free((extern_ptr_t)curr_dl);
            return(1);
        }
        prev_dl = curr_dl;
        curr_dl = curr_dl -> dl_next;
    }
    UNLOCK();
    ENABLE_SIGNALS();
    return(0);
}

void GC_register_finalizer(obj, fn, cd, ofn, ocd)
void_star obj;
GC_finalization_proc fn;
void_star cd;
GC_finalization_proc * ofn;
void_star * ocd;
{
    ptr_t base;
    struct finalizable_object * curr_fo, * prev_fo;
    int index;
    /* Allocate before acquiring lock */
      ALLOC(new_fo, struct finalizable_object);
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    base = (ptr_t)GC_base((void_star)obj);
    index = HASH(base);
    if (base != obj)
    		ABORT("Bad arg to GC_register_finalizer");
    prev_fo = 0; curr_fo = fo_head[index];
    while (curr_fo != 0) {
        if (curr_fo -> fo_hidden_base == HIDE_POINTER(base)) {
            if (ofn) *ofn = curr_fo -> fo_fn;
            if (ocd) *ocd = (void_star) curr_fo -> fo_client_data;
            if (fn == 0) {
                /* Delete the structure for base. */
                  if (prev_fo == 0) {
                    fo_head[index] = curr_fo -> fo_next;
                  } else {
                    prev_fo -> fo_next = curr_fo -> fo_next;
                  }
                  UNLOCK();
    	    	  ENABLE_SIGNALS();
                  GC_free((extern_ptr_t)curr_fo);
            } else {
                curr_fo -> fo_fn = fn;
                curr_fo -> fo_client_data = (ptr_t)cd;
                UNLOCK();
    	    	ENABLE_SIGNALS();
            }
            GC_free((extern_ptr_t)new_fo);
            return;
        }
        prev_fo = curr_fo;
        curr_fo = curr_fo -> fo_next;
    }
    {
        if (ofn) *ofn = 0;
        if (ocd) *ocd = 0;
        if (fn == 0) {
            UNLOCK();
    	    ENABLE_SIGNALS();
    	    GC_free((extern_ptr_t)new_fo);
            return;
        }
        new_fo -> fo_hidden_base = (word)HIDE_POINTER(base);
        new_fo -> fo_fn = fn;
        new_fo -> fo_client_data = (ptr_t)cd;
        new_fo -> fo_object_size = GC_size(base);
        new_fo -> fo_next = fo_head[index];
        fo_head[index] = new_fo;
    }
    UNLOCK();
    ENABLE_SIGNALS();
}

/* Called with world stopped.  Cause disappearing links to disappear,	*/
/* and invoke finalizers.						*/
void GC_finalize()
{
    struct disappearing_link * curr_dl, * prev_dl, * next_dl;
    struct finalizable_object * curr_fo, * prev_fo, * next_fo;
    ptr_t real_ptr;
    register int i;
    
  /* Make disappearing links disappear */
    for (i = 0; i < TSIZE; i++) {
      curr_dl = dl_head[i];
      prev_dl = 0;
      while (curr_dl != 0) {
        real_ptr = (ptr_t)REVEAL_POINTER(curr_dl -> dl_hidden_obj);
        if (!GC_is_marked(real_ptr)) {
            *(word *)(REVEAL_POINTER(curr_dl -> dl_hidden_link)) = 0;
            next_dl = curr_dl -> dl_next;
            if (prev_dl == 0) {
                dl_head[i] = next_dl;
            } else {
                prev_dl -> dl_next = next_dl;
            }
            GC_clear_mark_bit((ptr_t)curr_dl);
            curr_dl = next_dl;
        } else {
            prev_dl = curr_dl;
            curr_dl = curr_dl -> dl_next;
        }
      }
    }
  /* Mark all objects reachable via chains of 1 or more pointers	*/
  /* from finalizable objects.						*/
    for (i = 0; i < TSIZE; i++) {
      for (curr_fo = fo_head[i]; curr_fo != 0; curr_fo = curr_fo -> fo_next) {
        real_ptr = (ptr_t)REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!GC_is_marked(real_ptr)) {
            GC_push_all(real_ptr, real_ptr + curr_fo -> fo_object_size);
            while (!GC_mark_stack_empty()) GC_mark_from_mark_stack();
        }
        /* 
        if (GC_is_marked(real_ptr)) {
            --> Report finalization cycle here, if desired
        }
        */
      }
    }
  /* Invoke finalization code for all objects that are still		*/
  /* unreachable.							*/
    for (i = 0; i < TSIZE; i++) {
      curr_fo = fo_head[i];
      prev_fo = 0;
      while (curr_fo != 0) {
        real_ptr = (ptr_t)REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!GC_is_marked(real_ptr)) {
            (*(curr_fo -> fo_fn))(real_ptr, curr_fo -> fo_client_data);
            GC_set_mark_bit(real_ptr);
            next_fo = curr_fo -> fo_next;
            if (prev_fo == 0) {
                fo_head[i] = next_fo;
            } else {
                prev_fo -> fo_next = next_fo;
            }
            if (!GC_is_marked((ptr_t)curr_fo)) {
                ABORT("GC_finalize: found accessible unmarked object\n");
            }
            GC_clear_mark_bit((ptr_t)curr_fo);
            curr_fo = next_fo;
        } else {
            prev_fo = curr_fo;
            curr_fo = curr_fo -> fo_next;
        }
      }
    }
}

# ifdef __STDC__
    void_star GC_call_with_alloc_lock(GC_fn_type fn, void_star client_data)
# else
    void_star GC_call_with_alloc_lock(fn, client_data)
    GC_fn_type fn;
    void_star client_data;
# endif
{
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    (*fn)(client_data);
    UNLOCK();
    ENABLE_SIGNALS();
}

