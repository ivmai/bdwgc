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
#include "gc.h"
main()
{
	jmp_buf b;
	register int x = strlen("a");  /* 1, slightly disguised */
	static int y = 0;

	/* Encourage the compiler to keep x in a callee-save register */
	printf("");
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
