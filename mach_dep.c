/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/gc_priv.h"

#include <stdio.h>
#include <setjmp.h>

#if defined(OS2) || defined(CX_UX)
# define _setjmp(b) setjmp(b)
# define _longjmp(b,v) longjmp(b,v)
#endif

#ifdef AMIGA
# ifndef __GNUC__
#   include <dos.h>
# else
#   include <machine/reg.h>
# endif
#endif

#if defined(__MWERKS__) && !defined(POWERPC)

asm static void PushMacRegisters()
{
    sub.w   #4,sp                   // reserve space for one parameter.
    move.l  a2,(sp)
    jsr         GC_push_one
    move.l  a3,(sp)
    jsr         GC_push_one
    move.l  a4,(sp)
    jsr         GC_push_one
#   if !__option(a6frames)
        // <pcb> perhaps a6 should be pushed if stack frames are not being used.
        move.l  a6,(sp)
        jsr             GC_push_one
#   endif
        // skip a5 (globals), a6 (frame pointer), and a7 (stack pointer)
    move.l  d2,(sp)
    jsr         GC_push_one
    move.l  d3,(sp)
    jsr         GC_push_one
    move.l  d4,(sp)
    jsr         GC_push_one
    move.l  d5,(sp)
    jsr         GC_push_one
    move.l  d6,(sp)
    jsr         GC_push_one
    move.l  d7,(sp)
    jsr         GC_push_one
    add.w   #4,sp                   // fix stack.
    rts
}

#endif /* __MWERKS__ */

# if defined(SPARC) || defined(IA64)
    /* Value returned from register flushing routine; either sp (SPARC) */
    /* or ar.bsp (IA64).                                                */
    GC_INNER ptr_t GC_save_regs_ret_val = NULL;
# endif

/* Routine to mark from registers that are preserved by the C compiler. */
/* This must be ported to every new architecture.  It is not optional,  */
/* and should not be used on platforms that are either UNIX-like, or    */
/* require thread support.                                              */

#undef HAVE_PUSH_REGS

#if defined(USE_ASM_PUSH_REGS)
# define HAVE_PUSH_REGS
#else  /* No asm implementation */

# if defined(M68K) && defined(AMIGA)
    /* This function is not static because it could also be             */
    /* errorneously defined in .S file, so this error would be caught   */
    /* by the linker.                                                   */
    void GC_push_regs(void)
    {
         /*  AMIGA - could be replaced by generic code                  */
         /* a0, a1, d0 and d1 are caller save */

#       ifdef __GNUC__
          asm("subq.w &0x4,%sp");       /* allocate word on top of stack */

          asm("mov.l %a2,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %a3,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %a4,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %a5,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %a6,(%sp)"); asm("jsr _GC_push_one");
          /* Skip frame pointer and stack pointer */
          asm("mov.l %d2,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %d3,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %d4,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %d5,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %d6,(%sp)"); asm("jsr _GC_push_one");
          asm("mov.l %d7,(%sp)"); asm("jsr _GC_push_one");

          asm("addq.w &0x4,%sp");       /* put stack back where it was  */
#       else /* !__GNUC__ */
          GC_push_one(getreg(REG_A2));
          GC_push_one(getreg(REG_A3));
#         ifndef __SASC
            /* Can probably be changed to #if 0 -Kjetil M. (a4=globals) */
            GC_push_one(getreg(REG_A4));
#         endif
          GC_push_one(getreg(REG_A5));
          GC_push_one(getreg(REG_A6));
          /* Skip stack pointer */
          GC_push_one(getreg(REG_D2));
          GC_push_one(getreg(REG_D3));
          GC_push_one(getreg(REG_D4));
          GC_push_one(getreg(REG_D5));
          GC_push_one(getreg(REG_D6));
          GC_push_one(getreg(REG_D7));
#       endif /* !__GNUC__ */
    }
#   define HAVE_PUSH_REGS

# elif defined(M68K) && defined(MACOS)

#   if defined(THINK_C)
#     define PushMacReg(reg) \
              move.l  reg,(sp) \
              jsr             GC_push_one
      void GC_push_regs(void)
      {
          asm {
              sub.w   #4,sp          ; reserve space for one parameter.
              PushMacReg(a2);
              PushMacReg(a3);
              PushMacReg(a4);
              ; skip a5 (globals), a6 (frame pointer), and a7 (stack pointer)
              PushMacReg(d2);
              PushMacReg(d3);
              PushMacReg(d4);
              PushMacReg(d5);
              PushMacReg(d6);
              PushMacReg(d7);
              add.w   #4,sp          ; fix stack.
          }
      }
#     define HAVE_PUSH_REGS
#     undef PushMacReg
#   elif defined(__MWERKS__)
      void GC_push_regs(void)
      {
          PushMacRegisters();
      }
#     define HAVE_PUSH_REGS
#   endif /* __MWERKS__ */
# endif /* MACOS */

#endif /* !USE_ASM_PUSH_REGS */

#if defined(HAVE_PUSH_REGS) && defined(THREADS)
# error GC_push_regs cannot be used with threads
 /* Would fail for GC_do_blocking.  There are probably other safety     */
 /* issues.                                                             */
# undef HAVE_PUSH_REGS
#endif

#if defined(UNIX_LIKE) && !defined(NO_GETCONTEXT) && \
        (defined(DARWIN) || defined(HURD) || defined(OPENBSD) \
         || defined(ARM32) || defined(MIPS))
# define NO_GETCONTEXT
#endif

#if defined(LINUX) && defined(SPARC) && !defined(NO_GETCONTEXT)
# define NO_GETCONTEXT
#endif

#if !defined(HAVE_PUSH_REGS) && defined(UNIX_LIKE)
# include <signal.h>
# ifndef NO_GETCONTEXT
#   include <ucontext.h>
# endif
#endif

/* Ensure that either registers are pushed, or callee-save registers    */
/* are somewhere on the stack, and then call fn(arg, ctxt).             */
/* ctxt is either a pointer to a ucontext_t we generated, or NULL.      */
GC_INNER void GC_with_callee_saves_pushed(void (*fn)(ptr_t, void *),
                                          ptr_t arg)
{
    word dummy;
    void * context = 0;

#   if defined(HAVE_PUSH_REGS)
      GC_push_regs();
#   elif defined(UNIX_LIKE) && !defined(NO_GETCONTEXT)
      /* Older versions of Darwin seem to lack getcontext(). */
      /* ARM and MIPS Linux often doesn't support a real     */
      /* getcontext().                                       */
      ucontext_t ctxt;
      if (getcontext(&ctxt) < 0)
        ABORT ("Getcontext failed: Use another register retrieval method?");
      context = &ctxt;
#     if defined(SPARC) || defined(IA64)
        /* On a register window machine, we need to save register       */
        /* contents on the stack for this to work.  This may already be */
        /* subsumed by the getcontext() call.                           */
        GC_save_regs_ret_val = GC_save_regs_in_stack();
#     endif /* register windows. */
#   elif defined(HAVE_BUILTIN_UNWIND_INIT) && \
         !(defined(POWERPC) && defined(DARWIN))
      /* This was suggested by Richard Henderson as the way to  */
      /* force callee-save registers and register windows onto  */
      /* the stack.                                             */
      /* Mark Sibly points out that this doesn't seem to work   */
      /* on MacOS 10.3.9/PowerPC.                               */
      __builtin_unwind_init();
#   else /* !HAVE_BUILTIN_UNWIND_INIT && !UNIX_LIKE  */
         /* && !HAVE_PUSH_REGS                       */
        /* Generic code                          */
        /* The idea is due to Parag Patel at HP. */
        /* We're not sure whether he would like  */
        /* to be he acknowledged for it or not.  */
        jmp_buf regs;
        register word * i = (word *) regs;
        register ptr_t lim = (ptr_t)(regs) + (sizeof regs);

        /* Setjmp doesn't always clear all of the buffer.               */
        /* That tends to preserve garbage.  Clear it.                   */
        for (; (char *)i < lim; i++) {
            *i = 0;
        }
#       if defined(MSWIN32) || defined(MSWINCE) \
                  || defined(UTS4) || defined(LINUX) || defined(EWS4800)
          (void) setjmp(regs);
#       else
          (void) _setjmp(regs);
          /* We don't want to mess with signals. According to   */
          /* SUSV3, setjmp() may or may not save signal mask.   */
          /* _setjmp won't, but is less portable.               */
#       endif
#   endif /* !HAVE_PUSH_REGS ... */
    /* FIXME: context here is sometimes just zero.  At the moment the   */
    /* callees don't really need it.                                    */
    fn(arg, context);
    /* Strongly discourage the compiler from treating the above */
    /* as a tail-call, since that would pop the register        */
    /* contents before we get a chance to look at them.         */
    GC_noop1((word)(&dummy));
}

#if defined(ASM_CLEAR_CODE)
# ifdef LINT
    /*ARGSUSED*/
    ptr_t GC_clear_stack_inner(ptr_t arg, word limit)
    {
      return(arg);
    }
    /* The real version is in a .S file */
# endif
#endif /* ASM_CLEAR_CODE */
