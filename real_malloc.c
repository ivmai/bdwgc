/* We put this here to minimize the risk of inlining. */
/*VARARGS*/
GC_noop() {}

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

# ifdef __OS2__

# include <stddef.h>
# define INCL_DOSMEMMGR
# define INCL_DOSERRORS
# include <os2.h>

void * os2_alloc(size_t bytes)
{
    void * result;

    if (DosAllocMem(&result, bytes, PAG_EXECUTE | PAG_READ |
    				    PAG_WRITE | PAG_COMMIT)
		    != NO_ERROR) {
	return(0);
    }
    if (result == 0) return(os2_alloc(bytes));
    return(result);
}

# endif /* OS2 */
