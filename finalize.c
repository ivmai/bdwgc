/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.

 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
/* Boehm, April 5, 1994 1:42 pm PDT */
# define I_HIDE_POINTERS
# include "gc.h"
# include "gc_priv.h"
# include "gc_mark.h"

# define HASH3(addr,size,log_size) \
    ((((word)(addr) >> 3) ^ ((word)(addr) >> (3+(log_size)))) \
    & ((size) - 1))
#define HASH2(addr,log_size) HASH3(addr, 1 << log_size, log_size)

struct hash_chain_entry {
    word hidden_key;
    struct hash_chain_entry * next;
};

static struct disappearing_link {
    struct hash_chain_entry prolog;
#   define dl_hidden_link prolog.hidden_key
				/* Field to be cleared.		*/
#   define dl_next(x) (struct disappearing_link *)((x) -> prolog.next)
#   define dl_set_next(x,y) (x) -> prolog.next = (struct hash_chain_entry *)(y)

    word dl_hidden_obj;		/* Pointer to object base	*/
} **dl_head = 0;

static signed_word log_dl_table_size = -1;
			/* Binary log of				*/
			/* current size of array pointed to by dl_head.	*/
			/* -1 ==> size is 0.				*/

word GC_dl_entries = 0;	/* Number of entries currently in disappearing	*/
			/* link table.					*/

static struct finalizable_object {
    struct hash_chain_entry prolog;
#   define fo_hidden_base prolog.hidden_key
				/* Pointer to object base.	*/
#   define fo_next(x) (struct finalizable_object *)((x) -> prolog.next)
#   define fo_set_next(x,y) (x) -> prolog.next = (struct hash_chain_entry *)(y)
    GC_finalization_proc fo_fn;	/* Finalizer.			*/
    ptr_t fo_client_data;
    word fo_object_size;	/* In bytes.			*/
} **fo_head = 0;

static signed_word log_fo_table_size = -1;

word GC_fo_entries = 0;

# ifdef SRC_M3
void GC_push_finalizer_structures()
{
    GC_push_all((ptr_t)(&dl_head), (ptr_t)(&dl_head) + sizeof(word));
    GC_push_all((ptr_t)(&fo_head), (ptr_t)(&fo_head) + sizeof(word));
}
# endif

# define ALLOC(x, t) t *x = GC_NEW(t)

/* Double the size of a hash table. *size_ptr is the log of its current	*/
/* size.  May be a noop.						*/
/* *table is a pointer to an array of hash headers.  If we succeed, we	*/
/* update both *table and *log_size_ptr.				*/
/* Lock is held.  Signals are disabled.					*/
void GC_grow_table(table, log_size_ptr)
struct hash_chain_entry ***table;
signed_word * log_size_ptr;
{
    register int i;
    register struct hash_chain_entry *p;
    int log_old_size = *log_size_ptr;
    register int log_new_size = log_old_size + 1;
    word old_size = ((log_old_size == -1)? 0: (1 << log_old_size));
    register word new_size = 1 << log_new_size;
    struct hash_chain_entry **new_table = (struct hash_chain_entry **)
    	GC_generic_malloc_inner(
    		new_size * (word)(sizeof(struct hash_chain_entry *)),
    		NORMAL);
    
    if (new_table == 0) return;
    for (i = 0; i < old_size; i++) {
      p = (*table)[i];
      while (p != 0) {
        register ptr_t real_key = (ptr_t)REVEAL_POINTER(p -> hidden_key);
        register struct hash_chain_entry *next = p -> next;
        register int new_hash = HASH3(real_key, new_size, log_new_size);
        
        p -> next = new_table[new_hash];
        new_table[new_hash] = p;
        p = next;
      }
    }
    *log_size_ptr = log_new_size;
    *table = new_table;
}


int GC_register_disappearing_link(link)
extern_ptr_t * link;
{
    ptr_t base;
    
    base = (ptr_t)GC_base((extern_ptr_t)link);
    if (base == 0)
    	ABORT("Bad arg to GC_register_disappearing_link");
    return(GC_general_register_disappearing_link(link, base));
}

int GC_general_register_disappearing_link(link, obj)
extern_ptr_t * link;
extern_ptr_t obj;
{
    struct disappearing_link *curr_dl;
    int index;
    /* Allocate before acquiring lock */
      ALLOC(new_dl, struct disappearing_link);
    DCL_LOCK_STATE;
    
    if ((word)link & (ALIGNMENT-1))
    	ABORT("Bad arg to GC_general_register_disappearing_link");
#   ifdef THREADS
    	DISABLE_SIGNALS();
    	LOCK();
#   endif
    if (log_dl_table_size == -1 || GC_dl_entries > (1 << log_dl_table_size)) {
#	ifndef THREADS
	    DISABLE_SIGNALS();
#	endif
    	GC_grow_table((struct hash_chain_entry ***)(&dl_head),
    		      &log_dl_table_size);
#	ifdef PRINTSTATS
	    GC_printf1("Grew dl table to %lu entries\n",
	    		(unsigned long)(1 << log_dl_table_size));
#	endif
#	ifndef THREADS
	    ENABLE_SIGNALS();
#	endif
    }
    index = HASH2(link, log_dl_table_size);
    curr_dl = dl_head[index];
    for (curr_dl = dl_head[index]; curr_dl != 0; curr_dl = dl_next(curr_dl)) {
        if (curr_dl -> dl_hidden_link == HIDE_POINTER(link)) {
            curr_dl -> dl_hidden_obj = HIDE_POINTER(obj);
#	    ifdef THREADS
                UNLOCK();
    	        ENABLE_SIGNALS();
#	    endif
    	    GC_free((extern_ptr_t)new_dl);
            return(1);
        }
    }
    {
        new_dl -> dl_hidden_obj = HIDE_POINTER(obj);
        new_dl -> dl_hidden_link = HIDE_POINTER(link);
        dl_set_next(new_dl, dl_head[index]);
        dl_head[index] = new_dl;
        GC_dl_entries++;
#	ifdef THREADS
            UNLOCK();
            ENABLE_SIGNALS();
#	endif
        return(0);
    }
}

int GC_unregister_disappearing_link(link)
extern_ptr_t * link;
{
    struct disappearing_link *curr_dl, *prev_dl;
    int index;
    DCL_LOCK_STATE;
    
    index = HASH2(link, log_dl_table_size);
    if (((unsigned long)link & (ALIGNMENT-1)))
    	return(0);
    DISABLE_SIGNALS();
    LOCK();
    prev_dl = 0; curr_dl = dl_head[index];
    while (curr_dl != 0) {
        if (curr_dl -> dl_hidden_link == HIDE_POINTER(link)) {
            if (prev_dl == 0) {
                dl_head[index] = dl_next(curr_dl);
            } else {
                dl_set_next(prev_dl, dl_next(curr_dl));
            }
            GC_dl_entries--;
            UNLOCK();
    	    ENABLE_SIGNALS();
            GC_free((extern_ptr_t)curr_dl);
            return(1);
        }
        prev_dl = curr_dl;
        curr_dl = dl_next(curr_dl);
    }
    UNLOCK();
    ENABLE_SIGNALS();
    return(0);
}

/* Register a finalization function.  See gc.h for details.	*/
/* in the nonthreads case, we try to avoid disabling signals,	*/
/* since it can be expensive.  Threads packages typically	*/
/* make it cheaper.						*/
void GC_register_finalizer(obj, fn, cd, ofn, ocd)
extern_ptr_t obj;
GC_finalization_proc fn;
extern_ptr_t cd;
GC_finalization_proc * ofn;
extern_ptr_t * ocd;
{
    ptr_t base;
    struct finalizable_object * curr_fo, * prev_fo;
    int index;
    struct finalizable_object *new_fo;
    DCL_LOCK_STATE;

    if (log_fo_table_size == -1 || GC_fo_entries > (1 << log_fo_table_size)) {
    	DISABLE_SIGNALS();
        LOCK();
    	GC_grow_table((struct hash_chain_entry ***)(&fo_head),
    		      &log_fo_table_size);
#	ifdef PRINTSTATS
	    GC_printf1("Grew fo table to %lu entries\n",
	    		(unsigned long)(1 << log_fo_table_size));
#	endif
#	ifndef THREADS
	  UNLOCK();	/* Presumably noop */
	  ENABLE_SIGNALS();
#	endif
    } else {
#     ifdef THREADS
	DISABLE_SIGNALS();
	LOCK();
#     endif
    }
    /* in the THREADS case signals are disabled and we hold allocation	*/
    /* lock; otherwise neither is true.  Proceed carefully.		*/
    base = (ptr_t)obj;
    index = HASH2(base, log_fo_table_size);
    prev_fo = 0; curr_fo = fo_head[index];
    while (curr_fo != 0) {
        if (curr_fo -> fo_hidden_base == HIDE_POINTER(base)) {
            /* Interruption by a signal in the middle of this	*/
            /* should be safe.  The client may see only *ocd	*/
            /* updated, but we'll declare that to be his	*/
            /* problem.						*/
            if (ocd) *ocd = (extern_ptr_t) curr_fo -> fo_client_data;
            if (ofn) *ofn = curr_fo -> fo_fn;
            /* Delete the structure for base. */
                if (prev_fo == 0) {
                  fo_head[index] = fo_next(curr_fo);
                } else {
                  fo_set_next(prev_fo, fo_next(curr_fo));
                }
            if (fn == 0) {
                GC_fo_entries--;
                  /* May not happen if we get a signal.  But a high	*/
                  /* estimate will only make the table larger than	*/
                  /* necessary.						*/
                GC_free((extern_ptr_t)curr_fo);
            } else {
                curr_fo -> fo_fn = fn;
                curr_fo -> fo_client_data = (ptr_t)cd;
		/* Reinsert it.  We deleted it first to maintain	*/
		/* consistency in the event of a signal.		*/
		if (prev_fo == 0) {
                  fo_head[index] = curr_fo;
                } else {
                  fo_set_next(prev_fo, curr_fo);
                }
            }
#	    ifdef THREADS
                UNLOCK();
    	    	ENABLE_SIGNALS();
#	    endif
            return;
        }
        prev_fo = curr_fo;
        curr_fo = fo_next(curr_fo);
    }
    if (ofn) *ofn = 0;
    if (ocd) *ocd = 0;
    if (fn == 0) {
#	ifdef THREADS
            UNLOCK();
    	    ENABLE_SIGNALS();
#	endif
        return;
    }
#   ifdef THREADS
      new_fo = (struct finalizable_object *)
    	GC_generic_malloc_inner(sizeof(struct finalizable_object),NORMAL);
#   else
      new_fo = GC_NEW(struct finalizable_object);
#   endif
    new_fo -> fo_hidden_base = (word)HIDE_POINTER(base);
    new_fo -> fo_fn = fn;
    new_fo -> fo_client_data = (ptr_t)cd;
    new_fo -> fo_object_size = GC_size(base);
    fo_set_next(new_fo, fo_head[index]);
    GC_fo_entries++;
    fo_head[index] = new_fo;
    UNLOCK();
}

/* Called with world stopped.  Cause disappearing links to disappear,	*/
/* and invoke finalizers.						*/
void GC_finalize()
{
    struct disappearing_link * curr_dl, * prev_dl, * next_dl;
    struct finalizable_object * curr_fo, * prev_fo, * next_fo;
    ptr_t real_ptr, real_link;
    register int i;
    int dl_size = 1 << log_dl_table_size;
    int fo_size = 1 << log_fo_table_size;
    
  /* Make disappearing links disappear */
    for (i = 0; i < dl_size; i++) {
      curr_dl = dl_head[i];
      prev_dl = 0;
      while (curr_dl != 0) {
        real_ptr = (ptr_t)REVEAL_POINTER(curr_dl -> dl_hidden_obj);
        real_link = (ptr_t)REVEAL_POINTER(curr_dl -> dl_hidden_link);
        if (!GC_is_marked(real_ptr)) {
            *(word *)real_link = 0;
            next_dl = dl_next(curr_dl);
            if (prev_dl == 0) {
                dl_head[i] = next_dl;
            } else {
                dl_set_next(prev_dl, next_dl);
            }
            GC_clear_mark_bit((ptr_t)curr_dl);
            GC_dl_entries--;
            curr_dl = next_dl;
        } else {
            prev_dl = curr_dl;
            curr_dl = dl_next(curr_dl);
        }
      }
    }
  /* Mark all objects reachable via chains of 1 or more pointers	*/
  /* from finalizable objects.						*/
#   ifdef PRINTSTATS
        if (GC_mark_state != MS_NONE) ABORT("Bad mark state");
#   endif
    for (i = 0; i < fo_size; i++) {
      for (curr_fo = fo_head[i]; curr_fo != 0; curr_fo = fo_next(curr_fo)) {
        real_ptr = (ptr_t)REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!GC_is_marked(real_ptr)) {
            hdr * hhdr = HDR(real_ptr);
            
            PUSH_OBJ((word *)real_ptr, hhdr, GC_mark_stack_top,
	             &(GC_mark_stack[GC_mark_stack_size]));
            while (!GC_mark_stack_empty()) GC_mark_from_mark_stack();
            if (GC_mark_state != MS_NONE) {
                /* Mark stack overflowed. Very unlikely. */
#		ifdef PRINTSTATS
		    if (GC_mark_state != MS_INVALID) ABORT("Bad mark state");
		    GC_printf0("Mark stack overflowed in finalization!!\n");
#		endif
		/* Make mark bits consistent again.  Forget about	*/
		/* finalizing this object for now.			*/
		    GC_set_mark_bit(real_ptr);
		    while (!GC_mark_some());
            }
            /* 
            if (GC_is_marked(real_ptr)) {
                --> Report finalization cycle here, if desired
            }
            */
        }
        
      }
    }
  /* Invoke finalization code for all objects that are still		*/
  /* unreachable.							*/
    for (i = 0; i < fo_size; i++) {
      curr_fo = fo_head[i];
      prev_fo = 0;
      while (curr_fo != 0) {
        real_ptr = (ptr_t)REVEAL_POINTER(curr_fo -> fo_hidden_base);
        if (!GC_is_marked(real_ptr)) {
            (*(curr_fo -> fo_fn))(real_ptr, curr_fo -> fo_client_data);
            GC_set_mark_bit(real_ptr);
            next_fo = fo_next(curr_fo);
            if (prev_fo == 0) {
                fo_head[i] = next_fo;
            } else {
                fo_set_next(prev_fo, next_fo);
            }
#	    ifdef PRINTSTATS
              if (!GC_is_marked((ptr_t)curr_fo)) {
                ABORT("GC_finalize: found accessible unmarked object\n");
              }
#	    endif
            GC_clear_mark_bit((ptr_t)curr_fo);
            GC_fo_entries--;
            curr_fo = next_fo;
        } else {
            prev_fo = curr_fo;
            curr_fo = fo_next(curr_fo);
        }
      }
    }
  /* Remove dangling disappearing links. */
    for (i = 0; i < dl_size; i++) {
      curr_dl = dl_head[i];
      prev_dl = 0;
      while (curr_dl != 0) {
        real_link = GC_base((ptr_t)REVEAL_POINTER(curr_dl -> dl_hidden_link));
        if (real_link != 0 && !GC_is_marked(real_link)) {
            next_dl = dl_next(curr_dl);
            if (prev_dl == 0) {
                dl_head[i] = next_dl;
            } else {
                dl_set_next(prev_dl, next_dl);
            }
            GC_clear_mark_bit((ptr_t)curr_dl);
            GC_dl_entries--;
            curr_dl = next_dl;
        } else {
            prev_dl = curr_dl;
            curr_dl = dl_next(curr_dl);
        }
      }
    }
}

# ifdef __STDC__
    extern_ptr_t GC_call_with_alloc_lock(GC_fn_type fn, extern_ptr_t client_data)
# else
    extern_ptr_t GC_call_with_alloc_lock(fn, client_data)
    GC_fn_type fn;
    extern_ptr_t client_data;
# endif
{
    extern_ptr_t result;
    DCL_LOCK_STATE;
    
    DISABLE_SIGNALS();
    LOCK();
    result = (*fn)(client_data);
    UNLOCK();
    ENABLE_SIGNALS();
    return(result);
}

