/* Silly implementation of Lisp cons. Intentionally wastes lots of space */
/* to test collector.                                                    */
# include <stdio.h>
# include "cons.h"

int extra_count = 0;        /* Amount of space wasted in cons node */

sexpr cons (x, y)
sexpr x;
sexpr y;
{
    register sexpr r;
    register int i;
    register int *p;
    
    extra_count++;
    extra_count %= 3000;
    r = (sexpr) gc_malloc(8 + extra_count);
    for (p = (int *)r; ((char *)p) < ((char *)r) + extra_count + 8; p++) {
	if (*p) {
	    fprintf(stderr, "Found nonzero at %X\n", p);
	    abort(p);
        }
        *p = 13;
    }
    r -> sexpr_car = x;
    r -> sexpr_cdr = y;
    return(r);
}
