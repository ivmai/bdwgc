/*
 * Copyright (c) 2007 by Hewlett-Packard Company. All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */

#ifndef _GC_DISCLAIM_H
#define _GC_DISCLAIM_H

#include "gc.h"

/* Register a function notifier_proc which will be called on each	*/
/* object of the given kind before it is reclaimed.  If notifier_proc	*/
/* returns non-zero, the collector will not reclaim the object on this	*/
/* GC cycle.  Objects reachable from proc will be protected from	*/
/* collection if mark_from_all=1, but at the expense that long chains	*/
/* of objects will take many cycles to reclaim.				*/
/* Not available if configured with --disable-disclaim.			*/
int GC_register_disclaim_proc(int kind,
			      int (*proc)(void *obj, void *cd), void *cd,
			      int mark_from_all);

/* The finalizer closure used by GC_finalized_malloc.			*/
struct GC_finalizer_closure {
    void (*proc)(void *obj, void *cd);
    void *cd;
};

/* Allocate size bytes which is finalized with fc.  This uses a 	*/
/* dedicated object kind with the disclaim mechanism for maximum.  It	*/
/* is more efficient than GC_register_finalizer and friends.  You need	*/
/* to call GC_init_finalized_malloc before using this.			*/
GC_API void *GC_finalized_malloc(size_t size, struct GC_finalizer_closure *fc);

/* Prepare the object kind used for GC_finalized_malloc.		*/
GC_API void GC_init_finalized_malloc(void);

#endif
