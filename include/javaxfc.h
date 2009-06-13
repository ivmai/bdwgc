# ifndef _GC_H
#   include "gc.h"
# endif

# ifdef __cplusplus
    extern "C" {
# endif

/*
 * Invoke all remaining finalizers that haven't yet been run, or
 * repeatedly notify that there are finalizers to be run.
 * This is needed for strict compliance with the Java standard, 
 * which can make the runtime guarantee that all finalizers are run.
 * This is problematic for several reasons:
 * 1) It means that finalizers, and all methods called by them,
 *    must be prepared to deal with objects that have been finalized in
 *    spite of the fact that they are still referenced by statically
 *    allocated pointer variables.
 * 1) It may mean that we get stuck in an infinite loop running
 *    finalizers which create new finalizable objects, though that's
 *    probably unlikely.
 * Thus this is not recommended for general use.
 */
GC_API void GC_CALL GC_finalize_all(void);

# ifdef __cplusplus
    }  /* end of extern "C" */
# endif
