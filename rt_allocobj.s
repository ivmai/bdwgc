/*
 * This (assembly) file contains the functions:
 *	struct obj * allocobj(sz)
 *	struct obj * allocaobj(sz)
 */


/*
 * allocobj(i) insures that the free list entry for objects of size
 * i is not empty.
 *
 * Call _allocobj after first saving the registers which
 * are not guaranteed to be preserved (r0-r5 and r15).
 *
 * Note: the reason we have to use this interface between the caller
 * and the garbage collector is in order to preserve the caller's registers
 * which the C compiler would normally trash.  We just stick 'em on the stack
 * so that the mark_all procedure (which marks everything on the stack) will
 * see them.
 *
 * this is the RT version.
 */

/* this prolog was copied from a cc-produced .s file */
	.text
	.align 2
	.data
	.align 2
	.ltorg
	.text
	.ascii "<allocobj>"
	.align 2
	.globl _.allocobj
_.allocobj:
	.data
	.globl _allocobj
_allocobj: .long _.allocobj	/* text area contains instr ptr	*/
	.text
    /*
     * save registers which will be trashed on the stack in the place
     * the RT linkage convention uses for saving registers
     */
	.using	_allocobj,r14	/* tell assembler r14 is reliable base */
	stm	r3, -100+(3*4)(r1)	/* we don't save r1 cause it's sp */
	ai	r1,r1,-(36+13*4)
	mr	r14, r0		/* initialize data area pointer */

	balix	r15, _._allocobj	/* call _allocobj()	*/
	get	r0,$.long(__allocobj)	/* get data area pointer */

	lm	r3, -100+(36+13*4)+(3*4)(r1)	/* restore regs */
	brx	r15		/* return to caller (no restore req'd)	*/
	ai	r1, $(36+13*4)	/* restore r1 to where it belongs */

/* trace table for allocobj */
	.align 2
	.byte	0xdf		/* magic1 */
	.byte	0x07		/* code */
	.byte	0xdf		/* magic2 */
	.byte	0x08		/* first_gpr << 4 | opt stuff */
	.byte	0x01		/* no. args and stack reg num	*/
	.byte	0x3c		/* 0011 1100 ==> stack frame sz = 60	*/
	.data
	.ltorg

	.text
	.ascii "<allocaobj>"
	.align 2
	.globl _.allocaobj
_.allocaobj:
	.data
	.globl _allocaobj
_allocaobj: .long _.allocaobj	/* text area contains instr ptr	*/
	.text
    /*
     * save registers which will be trashed on the stack in the place
     * the RT linkage convention uses for saving registers
     */
	.using	_allocaobj,r14	/* tell assembler r14 is reliable base */
	stm	r3, -100+(3*4)(r1)	/* we don't save r1 cause it's sp */
	ai	r1,r1,-(36+13*4)
	mr	r14, r0		/* initialize data area pointer */

	balix	r15, _._allocaobj	/* call _allocaobj()	*/
	get	r0,$.long(__allocaobj)	/* get data area pointer */

	lm	r3, -100+(36+13*4)+(3*4)(r1)	/* restore regs */
	brx	r15		/* return to caller (no restore req'd)	*/
	ai	r1, $(36+13*4)	/* restore r1 to where it belongs */

/* trace table for allocaobj */
	.align 2
	.byte	0xdf		/* magic1 */
	.byte	0x07		/* code */
	.byte	0xdf		/* magic2 */
	.byte	0x08		/* first_gpr << 4 | opt stuff */
	.byte	0x01		/* no. args and stack reg num	*/
	.byte	0x3c		/* 0011 1100 ==> stack frame sz = 60	*/
	.data
	.ltorg


.globl .oVpcc
.globl .oVncs
.set .oVpcc, 0
.set .oVncs, 0
