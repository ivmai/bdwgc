# define call_GC_mark(x)     move    $4,x;    jal     GC_tl_mark

    .text
 # Mark from machine registers that are saved by C compiler
    .globl  GC_mark_regs
    .ent    GC_mark_regs
GC_mark_regs:
    subu    $sp,4       ## Need to save only return address
    sw      $31,4($sp)
    .mask   0x80000000,0
    .frame  $sp,4,$31
    call_GC_mark($2)
    call_GC_mark($3)
    call_GC_mark($16)
    call_GC_mark($17)
    call_GC_mark($18)
    call_GC_mark($19)
    call_GC_mark($20)
    call_GC_mark($21)
    call_GC_mark($22)
    call_GC_mark($23)
    call_GC_mark($30)
    lw      $31,4($sp)
    addu    $sp,4
    j       $31
    .end    GC_mark_regs
