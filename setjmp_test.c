/* Check whether setjmp actually saves registers in jmp_buf. */
/* If it doesn't, the generic mark_regs code won't work.     */
/* Compilers vary as to whether they will put x in a 	     */
/* (callee-save) register without -O.  The code is	     */
/* contrived such that any decent compiler should put x in   */
/* a callee-save register with -O.  Thus it is is 	     */
/* recommended that this be run optimized.  (If the machine  */
/* has no callee-save registers, then the generic code is    */
/* safe, but this will not be noticed by this piece of       */
/* code.)						     */
#include <stdio.h>
#include <setjmp.h>
#include "gc_private.h"

#ifdef __hpux
/* X/OPEN PG3 defines "void* sbrk();" and this clashes with the definition */
/* in gc_private.h, so we set the clock backwards with _CLASSIC_XOPEN_TYPES. */
/* This is for HP-UX 8.0.
/* sbrk() is not used in this file, of course.  W. Underwood, 15 Jun 1992 */
#define _CLASSIC_XOPEN_TYPES
#include <unistd.h>
int
getpagesize()
{
    return sysconf(_SC_PAGE_SIZE);
}
#endif

#ifdef _AUX_SOURCE
#include <sys/mmu.h>
int
getpagesize()
{
   return PAGESIZE;
}
#endif

#ifdef __OS2__
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSERRORS
#include <os2.h>

int
getpagesize()
{
    ULONG result[1];
    
    if (DosQuerySysInfo(QSV_PAGE_SIZE, QSV_PAGE_SIZE,
    		        (void *)result, sizeof(ULONG)) != NO_ERROR) {
    	fprintf(stderr, "DosQuerySysInfo failed\n");
    	result[0] = 4096;
    }
    return((int)(result[0]));
}
#endif

struct {char a_a; char * a_b;} a;

int * nested_sp()
{
    int dummy;
    
    return(&dummy);
}

main()
{
	int dummy;
	long ps = getpagesize();
	jmp_buf b;
	register int x = strlen("a");  /* 1, slightly disguised */
	static int y = 0;

	if (nested_sp() < &dummy) {
	  printf("Stack appears to grow down, which is the default.\n");
	  printf("A good guess for STACKBOTTOM on this machine is 0x%X.\n",
	         ((long)(&dummy) + ps) & ~(ps-1));
	} else {
	  printf("Stack appears to grow up.\n");
	  printf("Define STACK_GROWS_UP in gc_private.h\n");
	  printf("A good guess for STACKBOTTOM on this machine is 0x%X.\n",
	         ((long)(&dummy) + ps) & ~(ps-1));
	}
	printf("Note that this may vary between machines of ostensibly\n");
	printf("the same architecture (e.g. Sun 3/50s and 3/80s).\n");
	printf("A good guess for ALIGNMENT on this machine is %d.\n",
	       (unsigned long)(&(a.a_b))-(unsigned long)(&a));
	
	/* Encourage the compiler to keep x in a callee-save register */
	x = 2*x-1;
	printf("");
	x = 2*x-1;
	setjmp(b);
	if (y == 1) {
	    if (x == 2) {
		printf("Generic mark_regs code probably wont work\n");
#		if defined(SPARC) || defined(IBMRS6000)
		    printf("Assembly code supplied\n");
#		else
		    printf("Need assembly code\n");
#		endif
	    } else if (x == 1) {
		printf("Generic mark_regs code may work\n");
	    } else {
		printf("Very strange setjmp implementation\n");
	    }
	}
	y++;
	x = 2;
	if (y == 1) longjmp(b,1);
	return(0);
}

int g(x)
int x;
{
	return(x);
}
