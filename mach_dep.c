# include "gc.h"
# include <setjmp.h>


/* If no assembly calls are anticipated, it is only necessary to port     */
/* the mark_regs routine near the end of the file to your machine.        */
/* The allocobj and allocaobj routines are designed only as an assembly   */
/* language interface.  The definitions of objfreelist and aobjfreelist   */
/* are useful only if in-line allocation code is generated.               */

/* Definitions similar to the following make it easier to access the free */
/* lists from an assembly lnguage, or in-line C interface.                */
/* They should be added for other architectures.                          */


struct __gc_arrays _gc_arrays = { 0 }; 
	/* The purpose of the initialization is to force _gc_arrays */
	/* into the data segment.  The Fortran-based object file    */
	/* format used by many versions of UNIX otherwise makes the */
	/* following impossible.  (Note that some assemblers and    */
	/* linkers, notably those for Sun-3s, don't realize that    */
	/* this is impossible, and simply generate garbage.)        */

# ifdef M68K_SUN
    asm(".globl _aobjfreelist");
    asm(".globl _objfreelist");
    asm("_aobjfreelist = __gc_arrays");
    asm("_objfreelist = __gc_arrays+0x804");
# endif
# ifdef SPARC
    asm(".global _aobjfreelist");
    asm(".global _objfreelist");
    asm("_aobjfreelist = __gc_arrays");
    asm("_objfreelist = __gc_arrays+0x804");
# endif
# ifdef VAX
    asm(".globl _aobjfreelist");
    asm(".globl _objfreelist");
    asm(".set _aobjfreelist,__gc_arrays");
    asm(".set _objfreelist,__gc_arrays+0x804");
# endif
# ifdef RT
    asm(".globl _aobjfreelist");
    asm(".globl _objfreelist");
    asm(".set _aobjfreelist,__gc_arrays");
    asm(".set _objfreelist,__gc_arrays+0x804");
# endif

/* Call allocobj or allocaobj after first saving at least those registers */
/* not preserved by the C compiler. The register used for return values   */
/* is not saved, since it will be clobbered anyway.                       */
# ifdef RT
    /* This is done in rt_allocobj.s */
# else
#   ifdef M68K_HP
    /* Optimizer is not safe, we want these suckers stored. */
/* #   pragma OPTIMIZE OFF - we claim this is unnecessary if -O flag */
/*                           is not used.  It breaks the collector   */
/*                           on other machines.                      */
    asm("    text");		/* HP/Motorola assembler syntax */
    asm("    global  __allocobj");
    asm("    global  __allocaobj");
    asm("    global  _allocobj");
    asm("    global  _allocaobj");
#   else
    asm("    .text");		/* Default (PDP-11 Unix syntax) */
    asm("	.globl  __allocobj");
    asm("	.globl  __allocaobj");
    asm("	.globl  _allocobj");
    asm("	.globl  _allocaobj");
#   endif

# ifdef M68K_SUN
    asm("_allocobj:");
    asm("   link    a6,#0");
    asm("	movl    d1,sp@-");
    asm("	movl    a0,sp@-");
    asm("	movl    a1,sp@-");
    asm("	movl    sp@(20),sp@-");
    asm("	jbsr    __allocobj");
    asm("	addl    #4,sp");
    asm("	movl    sp@+,a1");
    asm("	movl    sp@+,a0");
    asm("	movl    sp@+,d1");
    asm("	unlk    a6");
    asm("	rts");
    
    asm("_allocaobj:");
    asm("	link    a6,#0");
    asm("	movl    d1,sp@-");
    asm("	movl    a0,sp@-");
    asm("	movl    a1,sp@-");
    asm("	movl    sp@(20),sp@-");
    asm("	jbsr    __allocaobj");
    asm("	addl    #4,sp");
    asm("	movl    sp@+,a1");
    asm("	movl    sp@+,a0");
    asm("	movl    sp@+,d1");
    asm("	unlk    a6");
    asm("	rts");
# endif

# ifdef M68K_HP
    asm("_allocobj:");
    asm("	link     %a6,&0");
    asm("	mov.l    %d1,-(%sp)");
    asm("	mov.l    %a0,-(%sp)");
    asm("	mov.l    %a1,-(%sp)");
    asm("	mov.l    20(%sp),-(%sp)");
    asm("	jsr      __allocobj");
    asm("	add.l    &4,%sp");
    asm("	mov.l    (%sp)+,%a1");
    asm("	mov.l    (%sp)+,%a0");
    asm("	mov.l    (%sp)+,%d1");
    asm("	unlk     %a6");
    asm("	rts");
    
    asm("_allocaobj:");
    asm("	link     %a6,&0");
    asm("	mov.l    %d1,-(%sp)");
    asm("	mov.l    %a0,-(%sp)");
    asm("	mov.l    %a1,-(%sp)");
    asm("	mov.l    20(%sp),-(%sp)");
    asm("	jsr      __allocaobj");
    asm("	add.l    &4,%sp");
    asm("	mov.l    (%sp)+,%a1");
    asm("	mov.l    (%sp)+,%a0");
    asm("	mov.l    (%sp)+,%d1");
    asm("	unlk     %a6");
    asm("	rts");
# endif /* M68K_HP */

# ifdef I386
    asm(".data");
    asm("gc_ret_value: .word 0");
    asm(".word 0");
    asm(".text");

    asm("_allocaobj:");
    asm("pushl %ebp");
    asm("movl %esp,%ebp");
    asm("pushal");
    asm("pushl 8(%ebp)");          /* Push orignal argument */
    asm("call __allocaobj");
    asm("popl %ecx");
    asm("movl %eax,gc_ret_value");  /* Save return value */
    asm("popal");
    asm("movl gc_ret_value,%eax");
    asm("leave");
    asm("ret");

    asm("_allocobj:");
    asm("pushl %ebp");
    asm("movl %esp,%ebp");
    asm("pushal");
    asm("pushl 8(%ebp)");          /* Push orignal argument */
    asm("call __allocobj");
    asm("popl %ecx");
    asm("movl %eax,gc_ret_value");  /* Save return value */
    asm("popal");
    asm("movl gc_ret_value,%eax");
    asm("leave");
    asm("ret");
# endif

# ifdef SPARC
    asm("_allocaobj:");
    asm("	ba	__allocaobj");
    asm("	nop");
    asm("_allocobj:");
    asm("	ba	__allocobj");
    asm("	nop");
    
#   include <sun4/trap.h>
    asm("	.globl	_save_regs_in_stack");
    asm("_save_regs_in_stack:");
    asm("	t	0x3   ! ST_FLUSH_WINDOWS");
    asm("	mov	%sp,%o0");
    asm("	retl");
    asm("	nop");
# endif

# ifdef VAX
    asm("_allocobj:");
    asm(".word    0x3e");
    asm("pushl   4(ap)");
    asm("calls   $1,__allocobj");
    asm("ret");
    asm("_allocaobj:");
    asm(".word   0x3e");
    asm("pushl   4(ap)");
    asm("calls   $1,__allocaobj");
    asm("ret");
# endif

# ifdef NS32K
    asm("_allocobj:");
    asm("enter [],$0");
    asm("movd r1,tos");
    asm("movd r2,tos");
    asm("movd 8(fp),tos");
    asm("bsr ?__allocobj");
    asm("adjspb $-4");
    asm("movd tos,r2");
    asm("movd tos,r1");
    asm("exit []");
    asm("ret $0");
    asm("_allocaobj:");
    asm("enter [],$0");
    asm("movd r1,tos");
    asm("movd r2,tos");
    asm("movd 8(fp),tos");
    asm("bsr ?__allocaobj");
    asm("adjspb $-4");
    asm("movd tos,r2");
    asm("movd tos,r1");
    asm("exit []");
    asm("ret $0");
# endif


# if !defined(VAX) && !defined(M68K_SUN) && !defined(M68K_HP)&& !defined(SPARC) && !defined(I386) && !defined(NS32K)
    /* Assembly language interface routines undefined */
# endif

# endif

/* Routine to mark from registers that are preserved by the C compiler. */
/* This must be ported to every new architecture.  There is a generic   */
/* version at the end, that is likely, but not guaranteed to work       */
/* on your architecture.  Run the test_setjmp program to see whether    */
/* there is any chance it will work.                                    */
mark_regs()
{
#       ifdef RT
	  register long TMP_SP; /* must be bound to r11 */
#       endif
#       ifdef VAX
	/* VAX - generic code below does not work under 4.2 */
	  /* r1 through r5 are caller save, and therefore     */
	  /* on the stack or dead.                            */
	  asm("pushl r11");     asm("calls $1,_tl_mark");
	  asm("pushl r10"); 	asm("calls $1,_tl_mark");
	  asm("pushl r9");	asm("calls $1,_tl_mark");
	  asm("pushl r8");	asm("calls $1,_tl_mark");
	  asm("pushl r7");	asm("calls $1,_tl_mark");
	  asm("pushl r6");	asm("calls $1,_tl_mark");
#       endif
#       ifdef M68K_SUN
	/*  M68K_SUN - could be replaced by generic code */
	  /* a0, a1 and d1 are caller save          */
	  /*  and therefore are on stack or dead.   */
	
	  asm("subqw #0x4,sp");		/* allocate word on top of stack */

	  asm("movl a2,sp@");	asm("jbsr _tl_mark");
	  asm("movl a3,sp@");	asm("jbsr _tl_mark");
	  asm("movl a4,sp@");	asm("jbsr _tl_mark");
	  asm("movl a5,sp@");	asm("jbsr _tl_mark");
	  /* Skip frame pointer and stack pointer */
	  asm("movl d1,sp@");	asm("jbsr _tl_mark");
	  asm("movl d2,sp@");	asm("jbsr _tl_mark");
	  asm("movl d3,sp@");	asm("jbsr _tl_mark");
	  asm("movl d4,sp@");	asm("jbsr _tl_mark");
	  asm("movl d5,sp@");	asm("jbsr _tl_mark");
	  asm("movl d6,sp@");	asm("jbsr _tl_mark");
	  asm("movl d7,sp@");	asm("jbsr _tl_mark");

	  asm("addqw #0x4,sp");		/* put stack back where it was	*/
#       endif

#       ifdef M68K_HP
	/*  M68K_HP - could be replaced by generic code */
	  /* a0, a1 and d1 are caller save.  */
	
	  asm("subq.w &0x4,%sp");	/* allocate word on top of stack */

	  asm("mov.l %a2,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %a3,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %a4,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %a5,(%sp)"); asm("jsr _tl_mark");
	  /* Skip frame pointer and stack pointer */
	  asm("mov.l %d1,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %d2,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %d3,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %d4,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %d5,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %d6,(%sp)"); asm("jsr _tl_mark");
	  asm("mov.l %d7,(%sp)"); asm("jsr _tl_mark");

	  asm("addq.w &0x4,%sp");	/* put stack back where it was	*/
#       endif /* M68K_HP */

#       ifdef I386
	/* I386 code, generic code does not appear to work */
	  asm("pushl %eax");  asm("call _tl_mark"); asm("addl $4,%esp");
	  asm("pushl %ecx");  asm("call _tl_mark"); asm("addl $4,%esp");
	  asm("pushl %edx");  asm("call _tl_mark"); asm("addl $4,%esp");
	  asm("pushl %esi");  asm("call _tl_mark"); asm("addl $4,%esp");
	  asm("pushl %edi");  asm("call _tl_mark"); asm("addl $4,%esp");
	  asm("pushl %ebx");  asm("call _tl_mark"); asm("addl $4,%esp");
#       endif

#       ifdef NS32K
	  asm ("movd r3, tos"); asm ("bsr ?_tl_mark"); asm ("adjspb $-4");
	  asm ("movd r4, tos"); asm ("bsr ?_tl_mark"); asm ("adjspb $-4");
	  asm ("movd r5, tos"); asm ("bsr ?_tl_mark"); asm ("adjspb $-4");
	  asm ("movd r6, tos"); asm ("bsr ?_tl_mark"); asm ("adjspb $-4");
	  asm ("movd r7, tos"); asm ("bsr ?_tl_mark"); asm ("adjspb $-4");
#       endif

#       ifdef SPARC
	  /* generic code will not work */
	  save_regs_in_stack();
#       endif

#	ifdef RT
	    tl_mark(TMP_SP);    /* tl_mark from r11 */

	    asm("cas r11, r6, r0"); tl_mark(TMP_SP);	/* r6 */
	    asm("cas r11, r7, r0"); tl_mark(TMP_SP);	/* through */
	    asm("cas r11, r8, r0"); tl_mark(TMP_SP);	/* r10 */
	    asm("cas r11, r9, r0"); tl_mark(TMP_SP);
	    asm("cas r11, r10, r0"); tl_mark(TMP_SP);

	    asm("cas r11, r12, r0"); tl_mark(TMP_SP); /* r12 */
	    asm("cas r11, r13, r0"); tl_mark(TMP_SP); /* through */
	    asm("cas r11, r14, r0"); tl_mark(TMP_SP); /* r15 */
	    asm("cas r11, r15, r0"); tl_mark(TMP_SP);
#       endif

#     if 0
	/* Generic code                          */
	/* The idea is due to Parag Patel at HP. */
	/* We're not sure whether he would like  */
	/* to be he acknowledged for it or not.  */
	{
	    jmp_buf regs;
	    register word * i = (word *) regs;
	    register word * lim = (word *) (((char *)(regs)) + (sizeof regs));

	    /* Setjmp on Sun 3s doesn't clear all of the buffer.  */
	    /* That tends to preserve garbage.  Clear it.         */
		for (; i < lim; i++) {
		    *i = 0;
		}
	    (void) _setjmp(regs);
	    tl_mark_all(regs, lim);
	}
#     endif

      /* other machines... */
#       if !(defined M68K_SUN) && !defined(M68K_HP) && !(defined VAX) && !(defined RT) && !(defined SPARC) && !(defined I386) &&!(defined NS32K)
	    --> bad news <--
#       endif
}
