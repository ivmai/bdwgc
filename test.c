/* Somewhat nonconvincing test for garbage collector.                */
/* Note that this intentionally uses the worlds worst implementation */
/* of cons.  It eats up gobs of memory in an attempt to break the    */
/* collector.  Process size should grow to about 1.5 Meg and stay    */
/* there.                                                            */
/* Should take about 25 seconds (2 minutes) to run on a              */
/* Sun 3/60 (Vax 11/750)                                             */
/* (The Vax does reasonably well here because the compiler assures   */
/* longword pointer alignment.)                                      */

# include <stdio.h>
# include "cons.h"

/* Return reverse(x) concatenated with y */
sexpr reverse1(x, y)
sexpr x, y;
{
    if (null(x)) {
        return(y);
    } else {
        return( reverse1(cdr(x), cons(car(x), y)) );
    }
}

sexpr reverse(x)
sexpr x;
{
    return( reverse1(x, nil) );
}

sexpr ints(low, up)
int low, up;
{
    if (low > up) {
	return(nil);
    } else {
        return(cons(low, ints(low+1, up)));
    }
}

void print_int_list(x)
sexpr x;
{
    if (null(x)) {
        printf("NIL\n");
    } else {
        printf("%d", car(x));
        if (!null(cdr(x))) {
            printf(", ");
            print_int_list(cdr(x));
        } else {
            printf("\n");
        }
    }
}

/* Try to force a to be strangely aligned */
struct {
  char dummy;
  sexpr aa;
} A;
#define a A.aa

main()
{
    int i;
    sexpr b;

    gc_init();
    a = ints(1, 100);
    b = ints(1, 50);
    print_int_list(a);
    print_int_list(b);
    print_int_list(reverse(a));
    print_int_list(reverse(b));
    for (i = 0; i < 100; i++) {
        b = reverse(reverse(b));
    }
    print_int_list(a);
    print_int_list(b);
    print_int_list(reverse(a));
    print_int_list(reverse(b));
    return(0);
}

