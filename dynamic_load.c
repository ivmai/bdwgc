/*
 * Copyright (c) 1991,1992 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 * Author: Bill Janssen
 * Modified by: Hans Boehm
 */
#include "gc_private.h"
#ifdef DYNAMIC_LOADING
#if !defined(M68K_SUN) && !defined(SPARC)
 --> We only know how to find data segments of dynamic libraries under SunOS 4.X
#endif
#include <sys/types.h>
#include <stdio.h>
#include <dlfcn.h>
#include <link.h>
#include <a.out.h>
#include <stab.h>

extern struct link_dynamic _DYNAMIC;

void GC_setup_dynamic_loading()
{
  struct link_map *lm;
  struct exec *e;

  if (&_DYNAMIC == 0) {
      /* No dynamic libraries.  Furthermore, the rest of this would 	*/
      /* segment fault.							*/
      return;
  }
  for (lm = _DYNAMIC.ld_un.ld_1->ld_loaded;
       lm != (struct link_map *) 0;  lm = lm->lm_next)
    {
      e = (struct exec *) lm->lm_addr;
      GC_add_roots_inner(
      		    ((char *) (N_DATOFF(*e) + lm->lm_addr)),
		    ((char *) (N_BSSADDR(*e) + e->a_bss + lm->lm_addr)));
    }
}

#ifdef DEFINE_DLOPEN
char *GC_dlopen (path, mode) 
     char *path;
     int  mode;
{
  char *etext, *end;
  struct link_map *lm;
  struct exec *e;
  char *handle;

  handle = dlopen(path, mode);
  if (handle == NULL)
    {
      fprintf (stderr,
	       "GC_sun_dlopen:  dlopen(%s, %d) failed:  %s.\n",
	       path, mode, dlerror());
      return (NULL);
    }

  for (lm = _DYNAMIC.ld_un.ld_1->ld_loaded;
       lm != (struct link_map *) 0;  lm = lm->lm_next)
    {
      if (strcmp(path, lm->lm_name) == 0)
	{
	  e = (struct exec *) lm->lm_addr;
	  etext = (void *) (N_DATOFF(*e) + lm->lm_addr);
	  end = (void *) (N_BSSADDR(*e) + e->a_bss + lm->lm_addr);
	  GC_add_roots (etext, end);
	  break;
	}
    }

  if (lm == (struct link_map *) 0)
    {
      fprintf (stderr,
	      "GC_sun_dlopen:  couldn't find \"%s\" in _DYNAMIC link list.\n",
	      path);
      dlclose(handle);
      return (NULL);
    }
  else
    return (handle);
}
#endif
#else
int GC_no_dynamic_loading;
#endif
