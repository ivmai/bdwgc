/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this garbage collector for any purpose,
 * provided the above notices are retained on all copies.
 */
/* Boehm, February 18, 1994 2:23 pm PST */


# ifdef PCR
/*
 * This definition should go in its own file that includes no other
 * header files.  Otherwise, we risk not getting the underlying system
 * malloc.
 */
# define PCR_NO_RENAME
# include <stdlib.h>

# ifdef __STDC__
    char * real_malloc(size_t size)
# else 
    char * real_malloc()
    int size;
# endif
{
    return((char *)malloc(size));
}
#endif /* PCR */

