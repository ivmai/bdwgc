 # $Id: alpha_mach_dep.s,v 1.2 1993/01/18 22:54:51 dosser Exp $

# define call_push(x)    lda   $16, 0(x);    jsr   $26, GC_push_one

        .text
        .align  4
        .globl  GC_push_regs
        .ent    GC_push_regs 2
GC_push_regs:
        ldgp    $gp, 0($27)
        lda     $sp, -32($sp)
        stq     $26, 8($sp)
        .mask   0x04000000, -8
        .frame  $sp, 16, $26, 0

 #      call_push($0)    # expression eval and int func result

 #      call_push($1)    # temp regs - not preserved cross calls
 #      call_push($2)
 #      call_push($3)
 #      call_push($4)
 #      call_push($5)
 #      call_push($6)
 #      call_push($7)
 #      call_push($8)

        call_push($9)    # Saved regs
        call_push($10)
        call_push($11)
        call_push($12)
        call_push($13)
        call_push($14)

        call_push($15)   # frame ptr or saved reg

 #      call_push($16)   # argument regs - not preserved cross calls
 #      call_push($17)
 #      call_push($18)
 #      call_push($19)
 #      call_push($20)
 #      call_push($21)

 #      call_push($22)   # temp regs - not preserved cross calls
 #      call_push($23)
 #      call_push($24)
 #      call_push($25)

 #      call_push($26)   # return address - expression eval
 #      call_push($27)   # procedure value or temporary reg
 #      call_push($28)   # assembler temp - not presrved
        call_push($29)   # Global Pointer
 #      call_push($30)   # Stack Pointer

        ldgp    $gp, 0($26)
        ldq     $26, 8($sp)
        lda     $sp, 32($sp)
        ret     $31, ($26), 1
        .end    GC_push_regs
