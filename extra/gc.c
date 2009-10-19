/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2009 by Hewlett-Packard Development Company.
 * All rights reserved.
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

/* This file could be used for the following purposes:          */
/* - get the complete GC as a single link object file (module); */
/* - enable more compiler optimizations.                        */

/* Tip: to get the highest level of compiler optimizations, the typical */
/* compiler options (GCC) to use are:                                   */
/* -O3 -fno-strict-aliasing -march=native -Wall -fprofile-generate/use  */

/* This file is not well tested (for now). */


#define GC_INNER STATIC
#define GC_EXTERN GC_INNER
                /* STATIC is defined in gcconfig.h. */

/* Small files go first... */
#include "../backgraph.c"
#include "../blacklst.c"
#include "../checksums.c"
#include "../gcj_mlc.c"
#include "../headers.c"
#include "../malloc.c"
#include "../new_hblk.c"
#include "../obj_map.c"
#include "../ptr_chck.c"
#include "../stubborn.c"

#include "../allchblk.c"
#include "../alloc.c"
#include "../dbg_mlc.c"
#include "../finalize.c"
#include "../mallocx.c"
#include "../mark.c"
#include "../mark_rts.c"
#include "../reclaim.c"
#include "../typd_mlc.c"

#include "../misc.c"
#include "../os_dep.c"
#include "../thread_local_alloc.c"

/* Most platform-specific files go here... */
#include "../darwin_stop_world.c"
#include "../dyn_load.c"
#include "../gc_dlopen.c"
#include "../mach_dep.c"
#include "../pcr_interface.c"
#include "../pthread_stop_world.c"
#include "../pthread_support.c"
#include "../specific.c"
#include "../win32_threads.c"

/* real_malloc.c, extra/MacOS.c, extra/msvc_dbg.c are not included. */
