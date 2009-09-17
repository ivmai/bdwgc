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

/* The version here should match that in configure/configure.ac */
/* Eventually this one may become unnecessary.  For now we need */
/* it to keep the old-style build process working.              */
#define GC_TMP_VERSION_MAJOR 7
#define GC_TMP_VERSION_MINOR 2
#define GC_TMP_ALPHA_VERSION 3

#ifndef GC_NOT_ALPHA
#   define GC_NOT_ALPHA 0xff
#endif

#if defined(GC_VERSION_MAJOR)
# if GC_TMP_VERSION_MAJOR != GC_VERSION_MAJOR || \
     GC_TMP_VERSION_MINOR != GC_VERSION_MINOR || \
     defined(GC_ALPHA_VERSION) != (GC_TMP_ALPHA_VERSION != GC_NOT_ALPHA) || \
     defined(GC_ALPHA_VERSION) && GC_TMP_ALPHA_VERSION != GC_ALPHA_VERSION
#   error Inconsistent version info.  Check doc/README, include/gc_version.h, and configure.ac.
# endif
#else
# define GC_VERSION_MAJOR GC_TMP_VERSION_MAJOR
# define GC_VERSION_MINOR GC_TMP_VERSION_MINOR
# define GC_ALPHA_VERSION GC_TMP_ALPHA_VERSION
#endif
