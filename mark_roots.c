# include <stdio.h>
# include "gc.h"

/* Call the mark routines (tl_mark for a single pointer, mark_all */
/* on groups of pointers) on every top level accessible pointer.  */
/* This is source language specific.  The following works for C.  */

mark_roots()
{
    int * dummy = 0;
    long sp_approx = 0;

    /*
     * mark from registers - i.e., call tl_mark(i) for each
     * register i
     */
	mark_regs(ALIGNMENT); /* usually defined in machine_dep.c */

#       ifdef DEBUG
	    gc_printf("done marking from regs - calling mark_all\n");
#	endif

      /* put stack pointer into sp_approx            */
      /* and mark everything on the stack.           */
	/* A hack */
	sp_approx = ((long)(&dummy));
	mark_all( sp_approx, stacktop, ALIGNMENT );


    /* Mark everything in data and bss segments.                             */
    /* Skip gc data structures. (It's OK to mark these, but it wastes time.) */
	{
	    extern char etext, end;

	    mark_all(DATASTART, begin_gc_arrays, ALIGNMENT);
	    mark_all(end_gc_arrays, &end, ALIGNMENT);
	}
}


/* Top level mark routine. Mark from the object pointed to by p.       */
/* This is defined here, since alignment is not an explicit parameter. */
/* Thus the routine is language specific.                              */
/* Tl_mark is normally called by mark_regs, and thus must be defined.  */
void tl_mark(p)
word * p;
{
    word * q;

    q = p;
    mark_all(&q, (&q)+1, ALIGNMENT);
}

/* Interface to mark_all that does not require alignment parameter.  */
/* Defined here to keep mach_dep.c programming language independent. */
void tl_mark_all(b,t)
word *b, *t;
{
    mark_all(b, t, ALIGNMENT);
}
