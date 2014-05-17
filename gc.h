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
/* Boehm, May 19, 1994 2:13 pm PDT */
 
#ifndef _GC_H

# define _GC_H

# include <stddef.h>

/* Define word and signed_word to be unsigned and signed types of the 	*/
/* size as char * or void *.  There seems to be no way to do this	*/
/* even semi-portably.  The following is probably no better/worse 	*/
/* than almost anything else.						*/
/* The ANSI standard suggests that size_t and ptr_diff_t might be 	*/
/* better choices.  But those appear to have incorrect definitions	*/
/* on may systems.  Notably "typedef int size_t" seems to be both	*/
/* frequent and WRONG.							*/
typedef unsigned long GC_word;
typedef long GC_signed_word;

/* Public read-only variables */

extern GC_word GC_gc_no;/* Counter incremented per collection.  	*/
			/* Includes empty GCs at startup.		*/
			

/* Public R/W variables */

extern int GC_quiet;	/* Disable statistics output.  Only matters if	*/
			/* collector has been compiled with statistics	*/
			/* enabled.  This involves a performance cost,	*/
			/* and is thus not the default.			*/

extern int GC_dont_gc;	/* Dont collect unless explicitly requested, e.g. */
			/* beacuse it's not safe.			  */

extern int GC_dont_expand;
			/* Dont expand heap unless explicitly requested */
			/* or forced to.				*/

extern int GC_full_freq;    /* Number of partial collections between	*/
			    /* full collections.  Matters only if	*/
			    /* GC_incremental is set.			*/
			
extern GC_word GC_non_gc_bytes;
			/* Bytes not considered candidates for collection. */
			/* Used only to control scheduling of collections. */

extern GC_word GC_free_space_divisor;
			/* We try to make sure that we allocate at 	*/
			/* least N/GC_free_space_divisor bytes between	*/
			/* collections, where N is the heap size plus	*/
			/* a rough estimate of the root set size.	*/
			/* Initially, GC_free_space_divisor = 4.	*/
			/* Increasing its value will use less space	*/
			/* but more collection time.  Decreasing it	*/
			/* will appreciably decrease collection time	*/
			/* at the expense of space.			*/
			/* GC_free_space_divisor = 1 will effectively	*/
			/* disable collections.				*/

			
/* Public procedures */
/*
 * general purpose allocation routines, with roughly malloc calling conv.
 * The atomic versions promise that no relevant pointers are contained
 * in the object.  The nonatomic versions guarantee that the new object
 * is cleared.  GC_malloc_stubborn promises that no changes to the object
 * will occur after GC_end_stubborn_change has been called on the
 * result of GC_malloc_stubborn. GC_malloc_uncollectable allocates an object
 * that is scanned for pointers to collectable objects, but is not itself
 * collectable.  GC_malloc_uncollectable and GC_free called on the resulting
 * object implicitly update GC_non_gc_bytes appropriately.
 */
#if defined(__STDC__) || defined(__cplusplus)
  extern void * GC_malloc(size_t size_in_bytes);
  extern void * GC_malloc_atomic(size_t size_in_bytes);
  extern void * GC_malloc_uncollectable(size_t size_in_bytes);
  extern void * GC_malloc_stubborn(size_t size_in_bytes);
# else
  extern char * GC_malloc(/* size_in_bytes */);
  extern char * GC_malloc_atomic(/* size_in_bytes */);
  extern char * GC_malloc_uncollectable(/* size_in_bytes */);
  extern char * GC_malloc_stubborn(/* size_in_bytes */);
# endif

/* Explicitly deallocate an object.  Dangerous if used incorrectly.     */
/* Requires a pointer to the base of an object.				*/
/* If the argument is stubborn, it should not be changeable when freed. */
/* An object should not be enable for finalization when it is 		*/
/* explicitly deallocated.						*/
/* GC_free(0) is a no-op, as required by ANSI C for free.		*/
#if defined(__STDC__) || defined(__cplusplus)
  extern void GC_free(void * object_addr);
# else
  extern void GC_free(/* object_addr */);
# endif

/*
 * Stubborn objects may be changed only if the collector is explicitly informed.
 * The collector is implicitly informed of coming change when such
 * an object is first allocated.  The following routines inform the
 * collector that an object will no longer be changed, or that it will
 * once again be changed.  Only nonNIL pointer stores into the object
 * are considered to be changes.  The argument to GC_end_stubborn_change
 * must be exacly the value returned by GC_malloc_stubborn or passed to
 * GC_change_stubborn.  (In the second case it may be an interior pointer
 * within 512 bytes of the beginning of the objects.)
 * There is a performance penalty for allowing more than
 * one stubborn object to be changed at once, but it is acceptable to
 * do so.  The same applies to dropping stubborn objects that are still
 * changeable.
 */
void GC_change_stubborn(/* p */);
void GC_end_stubborn_change(/* p */);

/* Return a pointer to the base (lowest address) of an object given	*/
/* a pointer to a location within the object.				*/
/* Return 0 if displaced_pointer doesn't point to within a valid	*/
/* object.								*/
# if defined(__STDC__) || defined(__cplusplus)
  void * GC_base(void * displaced_pointer);
# else
  char * GC_base(/* char * displaced_pointer */);
# endif

/* Given a pointer to the base of an object, return its size in bytes.	*/
/* The returned size may be slightly larger than what was originally	*/
/* requested.								*/
# if defined(__STDC__) || defined(__cplusplus)
  size_t GC_size(void * object_addr);
# else
  size_t GC_size(/* char * object_addr */);
# endif

/* For compatibility with C library.  This is occasionally faster than	*/
/* a malloc followed by a bcopy.  But if you rely on that, either here	*/
/* or with the standard C library, your code is broken.  In my		*/
/* opinion, it shouldn't have been invented, but now we're stuck. -HB	*/
/* The resulting object has the same kind as the original.		*/
/* If the argument is stubborn, the result will have changes enabled.	*/
/* It is an error to have changes enabled for the original object.	*/
/* Follows ANSI comventions for NULL old_object.			*/
# if defined(__STDC__) || defined(__cplusplus)
    extern void * GC_realloc(void * old_object, size_t new_size_in_bytes);
# else
    extern char * GC_realloc(/* old_object, new_size_in_bytes */);
# endif


/* Explicitly increase the heap size.	*/
/* Returns 0 on failure, 1 on success.  */
extern int GC_expand_hp(/* number_of_bytes */);

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
/* This is a no-op if the collector was compiled with recognition of	*/
/* arbitrary interior pointers enabled, which is now the default.	*/
void GC_register_displacement(/* n */);

/* Explicitly trigger a collection. 	*/
void GC_gcollect();

/* Return the number of bytes in the heap.  Excludes collector private	*/
/* data structures.  Includes empty blocks and fragmentation loss.	*/
/* Includes some pages that were allocated but never written.		*/
size_t GC_get_heap_size();

/* Enable incremental/generational collection.	*/
/* Not advisable unless dirty bits are 		*/
/* available or most heap objects are		*/
/* pointerfree(atomic) or immutable.		*/
/* Don't use in leak finding mode.		*/
/* Ignored if GC_dont_gc is true.		*/
void GC_enable_incremental();

/* Allocate an object of size lb bytes.  The client guarantees that	*/
/* as long as the object is live, it will be referenced by a pointer	*/
/* that points to somewhere within the first 256 bytes of the object.	*/
/* (This should normally be declared volatile to prevent the compiler	*/
/* from invalidating this assertion.)  This routine is only useful	*/
/* if a large array is being allocated.  It reduces the chance of 	*/
/* accidentally retaining such an array as a result of scanning an	*/
/* integer that happens to be an address inside the array.  (Actually,	*/
/* it reduces the chance of the allocator not finding space for such	*/
/* an array, since it will try hard to avoid introducing such a false	*/
/* reference.)  On a SunOS 4.X or MS Windows system this is recommended */
/* for arrays likely to be larger than 100K or so.  For other systems,	*/
/* or if the collector is not configured to recognize all interior	*/
/* pointers, the threshold is normally much higher.			*/
# if defined(__STDC__) || defined(__cplusplus)
  void * GC_malloc_ignore_off_page(size_t lb);
# else
  char * GC_malloc_ignore_off_page(/* size_t lb */);
# endif

/* Debugging (annotated) allocation.  GC_gcollect will check 		*/
/* objects allocated in this way for overwrites, etc.			*/
# if defined(__STDC__) || defined(__cplusplus)
  extern void * GC_debug_malloc(size_t size_in_bytes,
  				char * descr_string, int descr_int);
  extern void * GC_debug_malloc_atomic(size_t size_in_bytes,
  				       char * descr_string, int descr_int);
  extern void * GC_debug_malloc_uncollectable(size_t size_in_bytes,
  				           char * descr_string, int descr_int);
  extern void * GC_debug_malloc_stubborn(size_t size_in_bytes,
  				         char * descr_string, int descr_int);
  extern void GC_debug_free(void * object_addr);
  extern void * GC_debug_realloc(void * old_object,
  			 	 size_t new_size_in_bytes,
  			 	 char * descr_string, int descr_int);
# else
  extern char * GC_debug_malloc(/* size_in_bytes, descr_string, descr_int */);
  extern char * GC_debug_malloc_atomic(/* size_in_bytes, descr_string,
  					  descr_int */);
  extern char * GC_debug_malloc_uncollectable(/* size_in_bytes, descr_string,
  					  descr_int */);
  extern char * GC_debug_malloc_stubborn(/* size_in_bytes, descr_string,
  					  descr_int */);
  extern void GC_debug_free(/* object_addr */);
  extern char * GC_debug_realloc(/* old_object, new_size_in_bytes,
  			            descr_string, descr_int */);
# endif
void GC_debug_change_stubborn(/* p */);
void GC_debug_end_stubborn_change(/* p */);
# ifdef GC_DEBUG
#   define GC_MALLOC(sz) GC_debug_malloc(sz, __FILE__, __LINE__)
#   define GC_MALLOC_ATOMIC(sz) GC_debug_malloc_atomic(sz, __FILE__, __LINE__)
#   define GC_MALLOC_UNCOLLECTABLE(sz) GC_debug_malloc_uncollectable(sz, \
							__FILE__, __LINE__)
#   define GC_REALLOC(old, sz) GC_debug_realloc(old, sz, __FILE__, \
							       __LINE__)
#   define GC_FREE(p) GC_debug_free(p)
#   define GC_REGISTER_FINALIZER(p, f, d, of, od) \
	GC_register_finalizer(GC_base(p), GC_debug_invoke_finalizer, \
			      GC_make_closure(f,d), of, od)
#   define GC_MALLOC_STUBBORN(sz) GC_debug_malloc_stubborn(sz, __FILE__, \
							       __LINE__)
#   define GC_CHANGE_STUBBORN(p) GC_debug_change_stubborn(p)
#   define GC_END_STUBBORN_CHANGE(p) GC_debug_end_stubborn_change(p)
# else
#   define GC_MALLOC(sz) GC_malloc(sz)
#   define GC_MALLOC_ATOMIC(sz) GC_malloc_atomic(sz)
#   define GC_MALLOC_UNCOLLECTABLE(sz) GC_malloc_uncollectable(sz)
#   define GC_REALLOC(old, sz) GC_realloc(old, sz)
#   define GC_FREE(p) GC_free(p)
#   define GC_REGISTER_FINALIZER(p, f, d, of, od) \
	GC_register_finalizer(p, f, d, of, od)
#   define GC_MALLOC_STUBBORN(sz) GC_malloc_stubborn(sz)
#   define GC_CHANGE_STUBBORN(p) GC_change_stubborn(p)
#   define GC_END_STUBBORN_CHANGE(p) GC_end_stubborn_change(p)
# endif
/* The following are included because they are often convenient, and	*/
/* reduce the chance for a misspecifed size argument.  But calls may	*/
/* expand to something syntactically incorrect if t is a complicated	*/
/* type expression.  							*/
# define GC_NEW(t) (t *)GC_MALLOC(sizeof (t))
# define GC_NEW_ATOMIC(t) (t *)GC_MALLOC_ATOMIC(sizeof (t))
# define GC_NEW_STUBBORN(t) (t *)GC_MALLOC_STUBBORN(sizeof (t))
# define GC_NEW_UNCOLLECTABLE(t) (t *)GC_MALLOC_UNCOLLECTABLE(sizeof (t))

/* Finalization.  Some of these primitives are grossly unsafe.		*/
/* The idea is to make them both cheap, and sufficient to build		*/
/* a safer layer, closer to PCedar finalization.			*/
/* The interface represents my conclusions from a long discussion	*/
/* with Alan Demers, Dan Greene, Carl Hauser, Barry Hayes, 		*/
/* Christian Jacobi, and Russ Atkinson.  It's not perfect, and		*/
/* probably nobody else agrees with it.	    Hans-J. Boehm  3/13/92	*/
# if defined(__STDC__) || defined(__cplusplus)
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
	/* Fn should terminate as quickly as possible, and	*/
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
	/* If GC_register_finalizer is aborted as a result of	*/
	/* a signal, the object may be left with no		*/
	/* finalization, even if neither the old nor new	*/
	/* finalizer were NULL.					*/
	/* Obj should be the nonNULL starting address of an 	*/
	/* object allocated by GC_malloc or friends.		*/	

/* The following routine may be used to break cycles between	*/
/* finalizable objects, thus causing cyclic finalizable		*/
/* objects to be finalized in the correct order.  Standard	*/
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
	/* otherwise.						*/
	/* Only exists for backward compatibility.  See below:	*/
int GC_general_register_disappearing_link(/* void ** link, void * obj */);
	/* A slight generalization of the above. *link is	*/
	/* cleared when obj first becomes inaccessible.  This	*/
	/* can be used to implement weak pointers easily and	*/
	/* safely. Typically link will point to a location	*/
	/* holding a disguised pointer to obj.  In this way	*/
	/* soft pointers are broken before any object		*/
	/* reachable from them are finalized.  Each link	*/
	/* May be registered only once, i.e. with one obj	*/
	/* value.  This was added after a long email discussion */
	/* with John Ellis.					*/
	/* Obj must be a pointer to the first word of an object */
	/* we allocated.  It is unsafe to explicitly deallocate */
	/* the object containing link.  Explicitly deallocating */
	/* obj may or may not cause link to eventually be	*/
	/* cleared.						*/
int GC_unregister_disappearing_link(/* void ** link */);
	/* Returns 0 if link was not actually registered.	*/
	/* Undoes a registration by either of the above two	*/
	/* routines.						*/

/* Auxiliary fns to make finalization work correctly with displaced	*/
/* pointers introduced by the debugging allocators.			*/
# if defined(__STDC__) || defined(__cplusplus)
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
#   if defined(__STDC__) || defined(__cplusplus)
#     define HIDE_POINTER(p) (~(size_t)(p))
#     define REVEAL_POINTER(p) ((void *)(HIDE_POINTER(p)))
#   else
#     define HIDE_POINTER(p) (~(unsigned long)(p))
#     define REVEAL_POINTER(p) ((char *)(HIDE_POINTER(p)))
#   endif
    /* Converting a hidden pointer to a real pointer requires verifying	*/
    /* that the object still exists.  This involves acquiring the  	*/
    /* allocator lock to avoid a race with the collector.		*/

#   if defined(__STDC__) || defined(__cplusplus)
        typedef void * (*GC_fn_type)();
        void * GC_call_with_alloc_lock(GC_fn_type fn, void * client_data);
#   else
        typedef char * (*GC_fn_type)();
        char * GC_call_with_alloc_lock(/* GC_fn_type fn, char * client_data */);
#   endif
# endif

#ifdef SOLARIS_THREADS
/* We need to intercept calls to many of the threads primitives, so 	*/
/* that we can locate thread stacks and stop the world.			*/
/* Note also that the collector cannot see thread specific data.	*/
/* Thread specific data should generally consist of pointers to		*/
/* uncollectable objects, which are deallocated using the destructor	*/
/* facility in thr_keycreate.						*/
# include <thread.h>
  int GC_thr_create(void *stack_base, size_t stack_size,
                    void *(*start_routine)(void *), void *arg, long flags,
                    thread_t *new_thread);
  int GC_thr_join(thread_t wait_for, thread_t *departed, void **status);
  int GC_thr_suspend(thread_t target_thread);
  int GC_thr_continue(thread_t target_thread);
  void * GC_dlopen(const char *path, int mode);

# define thr_create GC_thr_create
# define thr_join GC_thr_join
# define thr_suspend GC_thr_suspend
# define thr_continue GC_thr_continue
# define dlopen GC_dlopen

/* This returns a list of objects, linked through their first		*/
/* word.  Its use can greatly reduce lock contention problems, since	*/
/* the allocation lock can be acquired and released many fewer times.	*/
void * GC_malloc_many(size_t lb);
#define GC_NEXT(p) (*(void **)(p)) 	/* Retrieve the next element	*/
					/* in returned list.		*/

#endif /* SOLARIS_THREADS */

#endif /* _GC_H */
