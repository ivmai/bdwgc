# define call_mark(x)     move    $4,x;    jal     tl_mark

 # Set up _gc_arrays with labels in the middle
    .data
    .globl  _gc_arrays
    .globl  aobjfreelist
    .globl  objfreelist
    .align  2
_gc_arrays:
aobjfreelist:
    .word   0 : 513
objfreelist:
    .word   0 : 513
 # either hblkmap or hblklist.  Reserve space for HBLK_MAP, which is bigger.
    .word   0 : 8192

    .text
 # Mark from machine registers that are saved by C compiler
    .globl  mark_regs
    .ent    mark_regs
mark_regs:
    subu    $sp,4       ## Need to save only return address
    sw      $31,4($sp)
    .mask   0x80000000,0
    .frame  $sp,4,$31
    call_mark($2)
    call_mark($3)
    call_mark($16)
    call_mark($17)
    call_mark($18)
    call_mark($19)
    call_mark($20)
    call_mark($21)
    call_mark($22)
    call_mark($23)
    call_mark($30)
    lw      $31,4($sp)
    addu    $sp,4
    j       $31
    .end    mark_regs

    .globl  allocobj
    .ent    allocobj
allocobj:
    subu    $sp,68
    sw      $31,68($sp)
    sw      $25,64($sp)
    sw      $24,60($sp)
    sw      $15,56($sp)
    sw      $14,52($sp)
    sw      $13,48($sp)
    sw      $12,44($sp)
    sw      $11,40($sp)
    sw      $10,36($sp)
    sw      $9,32($sp)
    sw      $8,28($sp)
    sw      $7,24($sp)
    sw      $6,20($sp)
    sw      $5,16($sp)
    sw      $4,12($sp)
    sw      $3,8($sp)
    .set    noat
    sw      $at,4($sp)
    .set    at
    .mask   0x8300fffa,0
    .frame  $sp,68,$31
    jal     _allocobj
    lw      $31,68($sp)
    lw      $25,64($sp)
    lw      $24,60($sp)
    lw      $15,56($sp)
    lw      $14,52($sp)
    lw      $13,48($sp)
    lw      $12,44($sp)
    lw      $11,40($sp)
    lw      $10,36($sp)
    lw      $9,32($sp)
    lw      $8,28($sp)
    lw      $7,24($sp)
    lw      $6,20($sp)
    lw      $5,16($sp)
    lw      $4,12($sp)
    lw      $3,8($sp)
 #  don't restore $2, since it's the return value
    .set    noat
    lw      $at,4($sp)
    .set    at
    addu    $sp,68
    j       $31
    .end    allocobj

    .globl  allocaobj
    .ent    allocaobj
allocaobj:
    subu    $sp,68
    sw      $31,68($sp)
    sw      $25,64($sp)
    sw      $24,60($sp)
    sw      $15,56($sp)
    sw      $14,52($sp)
    sw      $13,48($sp)
    sw      $12,44($sp)
    sw      $11,40($sp)
    sw      $10,36($sp)
    sw      $9,32($sp)
    sw      $8,28($sp)
    sw      $7,24($sp)
    sw      $6,20($sp)
    sw      $5,16($sp)
    sw      $4,12($sp)
    sw      $3,8($sp)
    .set    noat
    sw      $at,4($sp)
    .set    at
    .mask   0x8300fffa,0
    .frame  $sp,68,$31
    jal     _allocaobj
    lw      $31,68($sp)
    lw      $25,64($sp)
    lw      $24,60($sp)
    lw      $15,56($sp)
    lw      $14,52($sp)
    lw      $13,48($sp)
    lw      $12,44($sp)
    lw      $11,40($sp)
    lw      $10,36($sp)
    lw      $9,32($sp)
    lw      $8,28($sp)
    lw      $7,24($sp)
    lw      $6,20($sp)
    lw      $5,16($sp)
    lw      $4,12($sp)
    lw      $3,8($sp)
 #  don't restore $2, since it's the return value
    .set    noat
    lw      $at,4($sp)
    .set    at
    addu    $sp,68
    j       $31
    .end    allocaobj
