/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
 
#ifndef GC_H

# define GC_H

# include <stddef.h>

/* Define word and signed_word to be unsigned and signed types of the 	*/
/* size as char * or void *.  There seems to be no way to do this	*/
/* even semi-portably.  The following is probably no better/worse 	*/
/* than almost anything else.						*/
/* The ANSI standard suggests that size_t and ptr_diff_t might be 	*/
/* better choices.  But those appear to have incorrect definitions	*/
/* on may systems.  Notably "typedef int size_t" seems to be both	*/
/* frequent and WRONG.							*/
typedef unsigned long word;
typedef long signed_word;

/* Public read-only variables */

extern word GC_heapsize;       /* Heap size in bytes */

extern word GC_gc_no;	/* Counter incremented per collection.  	*/
			/* Includes empty GCs at startup.		*/

/* Public R/W variables */

extern int GC_dont_gc;	/* Dont collect unless explicitly requested, e.g. */
			/* beacuse it's not safe.			  */

extern int GC_dont_expand;
			/* Dont expand heap unless explicitly requested */
			/* or forced to.				*/
			
extern word GC_non_gc_bytes;
			/* Bytes not considered candidates for collection. */


extern word GC_free_space_divisor;
			/* We try to make sure that we allocate at 	*/
			/* least N/GC_free_space_divisor bytes between	*/
			/* collections, where N is the heap size plus	*/
			/* a rough estimate of the root set size.	*/
			/* Initially, GC_free_space_divisor = 4.	*/
			/* Increasing its value will use less space	*/
			/* but more collection time.  Decreasing it	*/
			/* will appreciably decrease collection time	*/
			/* at the expens of space.			*/
			/* GC_free_space_divisor = 1 will effectively	*/
			/* disable collections.				*/
			
/* Public procedures */
/*
 * general purpose allocation routines, with roughly malloc calling conv.
 * The atomic versions promise that no relevant pointers are contained
 * in the object.  The nonatomic version guarantees that the new object
 * is cleared.
 */
# ifdef __STDC__
  extern void * GC_malloc(size_t size_in_bytes);
  extern void * GC_malloc_atomic(size_t size_in_bytes);
# else
  extern char * GC_malloc(/* size_in_bytes */);
  extern char * GC_malloc_atomic(/* size_in_bytes */);
# endif

/* Explicitly deallocate an object.  Dangerous if used incorrectly.     */
/* Requires a pointer to the base of an object.				*/
# ifdef __STDC__
  extern void GC_free(void * object_addr);
# else
  extern void GC_free(/* object_addr */);
# endif

/* Return a pointer to the base (lowest address) of an object given	*/
/* a pointer to a location within the object.				*/
/* Return 0 if displaced_pointer doesn't point to within a valid	*/
/* object.								*/
# ifdef __STDC__
  void * GC_base(void * displaced_pointer);
# else
  char * GC_base(/* char * displaced_pointer */);
# endif

/* Given a pointer to the base of an object, return its size in bytes.	*/
/* The returned size may be slightly larger than what was originally	*/
/* requested.								*/
# ifdef __STDC__
  size_t GC_size(void * object_addr);
# else
  size_t GC_size(/* char * object_addr */);
# endif

/* For compatibility with C library.  This is occasionally faster than	*/
/* a malloc followed by a bcopy.  But if you rely on that, either here	*/
/* or with the standard C library, your code is broken.  In my		*/
/* opinion, it shouldn't have been invented, but now we're stuck. -HB	*/
# ifdef __STDC__
    extern void * GC_realloc(void * old_object, size_t new_size_in_bytes);
# else
    extern char * GC_realloc(/* old_object, new_size_in_bytes */);
# endif


/* Explicitly increase the heap size.	*/
/* Returns 0 on failure, 1 on success.  */
extern int GC_expand_hp(/* number_of_4K_blocks */);

/* Clear the set of root segments */
extern void GC_clear_roots();

/* Add a root segment */
extern void GC_add_roots(/* low_address, high_address_plus_1 */);

/* Add a displacement to the set of those considered valid by the	*/
/* collector.  GC_register_displacement(n) means that if p was returned */
/* by GC_malloc, then (char *)p + n will be considered to be a valid	*/
/* pointer to n.  N must be small and less than the size of p.		*/
/* (All pointers to the interior of objects from the stack are		*/
/* considered valid in any case.  This applies to heap objects and	*/
/* static data.)							*/
/* Preferably, this should be called before any other GC procedures.	*/
/* Calling it later adds to the probability of excess memory		*/
/* retention.								*/
void GC_register_displacement(/* n */);

/* Explicitly trigger a collection. 	*/
void GC_gcollect();

/* Debugging (annotated) allocation.  GC_gcollect will check 		*/
/* objects allocated in this way for overwrites, etc.			*/
# ifdef __STDC__
  extern void * GC_debug_malloc(size_t size_in_bytes,
  				char * descr_string, int descr_int);
  extern void * GC_debug_malloc_atomic(size_t size_in_bytes,
  				       char * descr_string, int descr_int);
  extern void GC_debug_free(void * object_addr);
  extern void * GC_debug_realloc(void * old_object,
  			 	 size_t new_size_in_bytes,
  			 	 char * descr_string, int descr_int);
# else
  extern char * GC_debug_malloc(/* size_in_bytes, descr_string, descr_int */);
  extern char * GC_debug_malloc_atomic(/* size_in_bytes, descr_string,
  					  descr_int */);
  extern void GC_debug_free(/* object_addr */);
  extern char * GC_debug_realloc(/* old_object, new_size_in_bytes,
  			            descr_string, descr_int */);
# endif
# ifdef GC_DEBUG
#   define GC_MALLOC(sz) GC_debug_malloc(sz, __FILE__, __LINE__)
#   define GC_MALLOC_ATOMIC(sz) GC_debug_malloc_atomic(sz, __FILE__, __LINE__)
#   define GC_REALLOC(old, sz) GC_debug_realloc(old, sz, __FILE__, \
							       __LINE__)
#   define GC_FREE(p) GC_debug_free(p)
#   define GC_REGISTER_FINALIZER(p, f, d, of, od) \
	GC_register_finalizer(GC_base(p), GC_debug_invoke_finalizer, \
			      GC_make_closure(f,d), of, od)
# else
#   define GC_MALLOC(sz) GC_malloc(sz)
#   define GC_MALLOC_ATOMIC(sz) GC_malloc_atomic(sz)
#   define GC_REALLOC(old, sz) GC_realloc(old, sz)
#   define GC_FREE(p) GC_free(p)
#   define GC_REGISTER_FINALIZER(p, f, d, of, od) \
	GC_register_finalizer(p, f, d, of, od)
# endif

/* Finalization.  Some of these primitives are grossly unsafe.		*/
/* The idea is to make them both cheap, and sufficient to build		*/
/* a safer layer, closer to PCedar finalization.			*/
/* The interface represents my conclusions from a long discussion	*/
/* with Alan Demers, Dan Greene, Carl Hauser, Barry Hayes, 		*/
/* Christian Jacobi, and Russ Atkinson.  It's not perfect, and		*/
/* probably nobody else agrees with it.	    Hans-J. Boehm  3/13/92	*/
# ifdef __STDC__
  typedef void (*GC_finalization_proc)(void * obj, void * client_data);
# else
  typedef void (*GC_finalization_proc)(/* void * obj, void * client_data */);
# endif
	
void GC_register_finalizer(/* void * obj,
			      GC_finalization_proc fn, void * cd,
			      GC_finalization_proc *ofn, void ** ocd */);
	/* When obj is no longer accessible, invoke		*/
	/* (*fn)(obj, cd).  If a and b are inaccessible, and	*/
	/* a points to b (after disappearing links have been	*/
	/* made to disappear), then only a will be		*/
	/* finalized.  (If this does not create any new		*/
	/* pointers to b, then b will be finalized after the	*/
	/* next collection.)  Any finalizable object that	*/
	/* is reachable from itself by following one or more	*/
	/* pointers will not be finalized (or collected).	*/
	/* Thus cycles involving finalizable objects should	*/
	/* be avoided, or broken by disappearing links.		*/
	/* fn is invoked with the allocation lock held.  It may */
	/* not allocate.  (Any storage it might need		*/
	/* should be preallocated and passed as part of cd.) 	*/
	/* fn should terminate as quickly as possible, and	*/
	/* defer extended computation.				*/
	/* All but the last finalizer registered for an object  */
	/* is ignored.						*/
	/* Finalization may be removed by passing 0 as fn.	*/
	/* The old finalizer and client data are stored in	*/
	/* *ofn and *ocd.					*/ 
	/* Fn is never invoked on an accessible object,		*/
	/* provided hidden pointers are converted to real 	*/
	/* pointers only if the allocation lock is held, and	*/
	/* such conversions are not performed by finalization	*/
	/* routines.						*/

/* The following routine may be used to break cycles between	*/
/* finalizable objects, thus causing cyclic finalizable		*/
/* objects to be finalized in the cirrect order.  Standard	*/
/* use involves calling GC_register_disappearing_link(&p),	*/
/* where p is a pointer that is not followed by finalization	*/
/* code, and should not be considered in determining 		*/
/* finalization order.						*/ 
int GC_register_disappearing_link(/* void ** link */);
	/* Link should point to a field of a heap allocated 	*/
	/* object obj.  *link will be cleared when obj is	*/
	/* found to be inaccessible.  This happens BEFORE any	*/
	/* finalization code is invoked, and BEFORE any		*/
	/* decisions about finalization order are made.		*/
	/* This is useful in telling the finalizer that 	*/
	/* some pointers are not essential for proper		*/
	/* finalization.  This may avoid finalization cycles.	*/
	/* Note that obj may be resurrected by another		*/
	/* finalizer, and thus the clearing of *link may	*/
	/* be visible to non-finalization code.  		*/
	/* There's an argument that an arbitrary action should  */
	/* be allowed here, instead of just clearing a pointer. */
	/* But this causes problems if that action alters, or 	*/
	/* examines connectivity.				*/
	/* Returns 1 if link was already registered, 0		*/
	/* otherise.						*/
int GC_unregister_disappearing_link(/* void ** link */);
	/* Returns 0 if link was not actually registered.	*/

/* Auxiliary fns to make finalization work correctly with displaced	*/
/* pointers introduced by the debugging allocators.			*/
# ifdef __STDC__
    void * GC_make_closure(GC_finalization_proc fn, void * data);
    void GC_debug_invoke_finalizer(void * obj, void * data);
# else
    char * GC_make_closure(/* GC_finalization_proc fn, char * data */);
    void GC_debug_invoke_finalizer(/* void * obj, void * data */);
# endif

	
/* The following is intended to be used by a higher level	*/
/* (e.g. cedar-like) finalization facility.  It is expected	*/
/* that finalization code will arrange for hidden pointers to	*/
/* disappear.  Otherwise objects can be accessed after they	*/
/* have been collected.						*/
# ifdef I_HIDE_POINTERS
#   ifdef __STDC__
#     define HIDE_POINTER(p) (~(size_t)(p))
#     define REVEAL_POINTER(p) ((void *)(HIDE_POINTER(p)))
#   else
#     define HIDE_POINTER(p) (~(unsigned long)(p))
#     define REVEAL_POINTER(p) ((char *)(HIDE_POINTER(p)))
#   endif
    /* Converting a hidden pointer to a real pointer requires verifying	*/
    /* that the object still exists.  This involves acquiring the  	*/
    /* allocator lock to avoid a race with the collector.		*/
    typedef char * (*GC_fn_type)();
#   ifdef __STDC__
        void * GC_call_with_alloc_lock(GC_fn_type fn, void * client_data);
#   else
        char * GC_call_with_alloc_lock(/* GC_fn_type fn, char * client_data */);
#   endif
# endif

#endif
