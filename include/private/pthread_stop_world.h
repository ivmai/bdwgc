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

#ifndef GC_PTHREAD_STOP_WORLD_H
#define GC_PTHREAD_STOP_WORLD_H

struct thread_stop_info {
#   ifndef GC_OPENBSD_THREADS
      word last_stop_count;     /* GC_last_stop_count value when thread */
                                /* last successfully handled a suspend  */
                                /* signal.                              */
#   endif

    ptr_t stack_ptr;            /* Valid only when stopped.             */
};

#endif
