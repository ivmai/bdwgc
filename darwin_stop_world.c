/*
 * Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
 * Copyright (c) 2000-2010 by Hewlett-Packard Development Company.
 * All rights reserved.
 * Copyright (c) 2008-2022 Ivan Maidanski
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/pthread_support.h"

/* This probably needs more porting work to ppc64. */

#if defined(DARWIN) && defined(THREADS)

#  include <mach/machine.h>
#  include <sys/sysctl.h>

#  if defined(ARM32) && defined(ARM_THREAD_STATE32)
#    include <CoreFoundation/CoreFoundation.h>
#  endif

/*
 * From "Inside Mac OS X - Mach-O Runtime Architecture" published by Apple:
 *   - Page 49: The space beneath the stack pointer, where a new stack frame
 *     would normally be allocated, is called the red zone.  This area as
 *     shown in Figure 3-2 may be used for any purpose as long as a new stack
 *     frame does not need to be added to the stack.
 *   - Page 50: If a leaf procedure's red zone usage would exceed 224 bytes,
 *     then it must set up a stack frame just like routines that call other
 *     routines.
 */
#  ifdef POWERPC
#    if CPP_WORDSZ == 32
#      define PPC_RED_ZONE_SIZE 224
#    elif CPP_WORDSZ == 64
#      define PPC_RED_ZONE_SIZE 320
#    endif
#  endif

#  ifndef DARWIN_DONT_PARSE_STACK

typedef struct StackFrame {
  unsigned long savedSP;
  unsigned long savedCR;
  unsigned long savedLR;
  /* `unsigned long reserved[2];` */
  /* `unsigned long savedRTOC;` */
} StackFrame;

GC_INNER ptr_t
GC_FindTopOfStack(unsigned long stack_start)
{
  StackFrame *frame = (StackFrame *)MAKE_CPTR(stack_start);

  if (NULL == frame) {
#    ifdef POWERPC
#      if CPP_WORDSZ == 32
    __asm__ __volatile__("lwz %0,0(r1)" : "=r"(frame));
#      else
    __asm__ __volatile__("ld %0,0(r1)" : "=r"(frame));
#      endif
#    elif defined(ARM32)
    volatile ptr_t sp_reg;

    __asm__ __volatile__("mov %0, r7\n" : "=r"(sp_reg));
    frame = (/* no volatile */ StackFrame *)sp_reg;
#    elif defined(AARCH64)
    volatile ptr_t sp_reg;

    __asm__ __volatile__("mov %0, x29\n" : "=r"(sp_reg));
    frame = (/* no volatile */ StackFrame *)sp_reg;
#    else
#      if defined(CPPCHECK)
    GC_noop1_ptr(&frame);
#      endif
    ABORT("GC_FindTopOfStack(0) is not implemented");
#    endif
  }

#    ifdef DEBUG_THREADS_EXTRA
  GC_log_printf("FindTopOfStack start at sp= %p\n", (void *)frame);
#    endif
  while (frame->savedSP != 0) { /*< stop if no more stack frames */
    unsigned long maskedLR;

#    ifdef CPPCHECK
    GC_noop1(frame->savedCR);
#    endif
    frame = (StackFrame *)MAKE_CPTR(frame->savedSP);

    /*
     * We do these next two checks after going to the next frame because
     * the `savedLR` for the first stack frame in the loop is not set up
     * on purpose, so we should not check it.
     */
    maskedLR = frame->savedLR & ~0x3UL;
    if (0 == maskedLR || ~0x3UL == maskedLR) {
      /* The next `savedLR` is bogus, stop. */
      break;
    }
  }
#    ifdef DEBUG_THREADS_EXTRA
  GC_log_printf("FindTopOfStack finish at sp= %p\n", (void *)frame);
#    endif
  return (ptr_t)frame;
}

#  endif /* !DARWIN_DONT_PARSE_STACK */

/*
 * `GC_query_task_threads` controls whether to obtain the list of the
 * threads from the kernel or to use `GC_threads` table.
 */
#  ifdef GC_NO_THREADS_DISCOVERY
#    define GC_query_task_threads FALSE
#  elif defined(GC_DISCOVER_TASK_THREADS)
#    define GC_query_task_threads TRUE
#  else
STATIC GC_bool GC_query_task_threads = FALSE;
#  endif /* !GC_NO_THREADS_DISCOVERY */

GC_API void GC_CALL
GC_use_threads_discovery(void)
{
#  ifdef GC_NO_THREADS_DISCOVERY
  ABORT("Darwin task-threads-based stop and push unsupported");
#  else
#    ifndef GC_ALWAYS_MULTITHREADED
  GC_ASSERT(!GC_need_to_lock);
#    endif
#    ifndef GC_DISCOVER_TASK_THREADS
  GC_query_task_threads = TRUE;
#    endif
  GC_init();
#  endif
}

#  ifndef kCFCoreFoundationVersionNumber_iOS_8_0
#    define kCFCoreFoundationVersionNumber_iOS_8_0 1140.1
#  endif

#  define THREAD_ACT_TO_VPTR(t) THREAD_ID_TO_VPTR(t)
#  define MACH_PORT_TO_VPTR(t) THREAD_ID_TO_VPTR(t)

/*
 * Evaluates the stack range for a given thread.  Returns the lower bound
 * and sets `*phi` to the upper one.  Sets `*pfound_me` to `TRUE` if this
 * is the current thread, otherwise the value is not changed.
 */
STATIC ptr_t
GC_stack_range_for(ptr_t *phi, thread_act_t thread, GC_thread p,
                   mach_port_t my_thread, ptr_t *paltstack_lo,
                   ptr_t *paltstack_hi, GC_bool *pfound_me)
{
#  ifdef DARWIN_DONT_PARSE_STACK
  GC_stack_context_t crtn;
#  endif
  ptr_t lo;

  GC_ASSERT(I_HOLD_LOCK());
  if (thread == my_thread) {
    GC_ASSERT(NULL == p || (p->flags & DO_BLOCKING) == 0);
    lo = GC_approx_sp();
#  ifndef DARWIN_DONT_PARSE_STACK
    *phi = GC_FindTopOfStack(0);
#  endif
    *pfound_me = TRUE;
  } else if (p != NULL && (p->flags & DO_BLOCKING) != 0) {
    lo = p->crtn->stack_ptr;
#  ifndef DARWIN_DONT_PARSE_STACK
    *phi = p->crtn->topOfStack;
#  endif

  } else {
    /*
     * `MACHINE_THREAD_STATE_COUNT` does not seem to be defined everywhere.
     * Hence we use our own variant.  Alternatively, we could use
     * `THREAD_STATE_MAX` (but seems to be not optimal).
     */
    kern_return_t kern_result;
    GC_THREAD_STATE_T state;

#  if defined(ARM32) && defined(ARM_THREAD_STATE32)
    /*
     * Use `ARM_UNIFIED_THREAD_STATE` on iOS8+ 32-bit targets and on
     * 64-bit H/W (iOS7+ 32-bit mode).
     */
    size_t size;
    static cpu_type_t cputype = 0;

    if (cputype == 0) {
      sysctlbyname("hw.cputype", &cputype, &size, NULL, 0);
    }
    if (cputype == CPU_TYPE_ARM64
        || kCFCoreFoundationVersionNumber
               >= kCFCoreFoundationVersionNumber_iOS_8_0) {
      arm_unified_thread_state_t unified_state;
      mach_msg_type_number_t unified_thread_state_count
          = ARM_UNIFIED_THREAD_STATE_COUNT;
#    if defined(CPPCHECK)
#      define GC_ARM_UNIFIED_THREAD_STATE 1
#    else
#      define GC_ARM_UNIFIED_THREAD_STATE ARM_UNIFIED_THREAD_STATE
#    endif
      kern_result = thread_get_state(thread, GC_ARM_UNIFIED_THREAD_STATE,
                                     (natural_t *)&unified_state,
                                     &unified_thread_state_count);
#    if !defined(CPPCHECK)
      if (unified_state.ash.flavor != ARM_THREAD_STATE32) {
        ABORT("unified_state flavor should be ARM_THREAD_STATE32");
      }
#    endif
      state = unified_state;
    } else
#  endif
    /* else */ {
      mach_msg_type_number_t thread_state_count = GC_MACH_THREAD_STATE_COUNT;

      /* Get the thread state (registers, etc.). */
      do {
        kern_result
            = thread_get_state(thread, GC_MACH_THREAD_STATE,
                               (natural_t *)&state, &thread_state_count);
      } while (kern_result == KERN_ABORTED);
    }
#  ifdef DEBUG_THREADS
    GC_log_printf("thread_get_state returns %d\n", kern_result);
#  endif
    if (kern_result != KERN_SUCCESS)
      ABORT("thread_get_state failed");

#  if defined(I386)
    lo = (ptr_t)state.THREAD_FLD(esp);
#    ifndef DARWIN_DONT_PARSE_STACK
    *phi = GC_FindTopOfStack(state.THREAD_FLD(esp));
#    endif
    GC_push_one(state.THREAD_FLD(eax));
    GC_push_one(state.THREAD_FLD(ebx));
    GC_push_one(state.THREAD_FLD(ecx));
    GC_push_one(state.THREAD_FLD(edx));
    GC_push_one(state.THREAD_FLD(edi));
    GC_push_one(state.THREAD_FLD(esi));
    GC_push_one(state.THREAD_FLD(ebp));

#  elif defined(X86_64)
    lo = (ptr_t)state.THREAD_FLD(rsp);
#    ifndef DARWIN_DONT_PARSE_STACK
    *phi = GC_FindTopOfStack(state.THREAD_FLD(rsp));
#    endif
    GC_push_one(state.THREAD_FLD(rax));
    GC_push_one(state.THREAD_FLD(rbx));
    GC_push_one(state.THREAD_FLD(rcx));
    GC_push_one(state.THREAD_FLD(rdx));
    GC_push_one(state.THREAD_FLD(rdi));
    GC_push_one(state.THREAD_FLD(rsi));
    GC_push_one(state.THREAD_FLD(rbp));
    /* `rsp` is skipped. */
    GC_push_one(state.THREAD_FLD(r8));
    GC_push_one(state.THREAD_FLD(r9));
    GC_push_one(state.THREAD_FLD(r10));
    GC_push_one(state.THREAD_FLD(r11));
    GC_push_one(state.THREAD_FLD(r12));
    GC_push_one(state.THREAD_FLD(r13));
    GC_push_one(state.THREAD_FLD(r14));
    GC_push_one(state.THREAD_FLD(r15));

#  elif defined(POWERPC)
    lo = (ptr_t)(state.THREAD_FLD(r1) - PPC_RED_ZONE_SIZE);
#    ifndef DARWIN_DONT_PARSE_STACK
    *phi = GC_FindTopOfStack(state.THREAD_FLD(r1));
#    endif
    GC_push_one(state.THREAD_FLD(r0));
    /* `r1` is skipped. */
    GC_push_one(state.THREAD_FLD(r2));
    GC_push_one(state.THREAD_FLD(r3));
    GC_push_one(state.THREAD_FLD(r4));
    GC_push_one(state.THREAD_FLD(r5));
    GC_push_one(state.THREAD_FLD(r6));
    GC_push_one(state.THREAD_FLD(r7));
    GC_push_one(state.THREAD_FLD(r8));
    GC_push_one(state.THREAD_FLD(r9));
    GC_push_one(state.THREAD_FLD(r10));
    GC_push_one(state.THREAD_FLD(r11));
    GC_push_one(state.THREAD_FLD(r12));
    GC_push_one(state.THREAD_FLD(r13));
    GC_push_one(state.THREAD_FLD(r14));
    GC_push_one(state.THREAD_FLD(r15));
    GC_push_one(state.THREAD_FLD(r16));
    GC_push_one(state.THREAD_FLD(r17));
    GC_push_one(state.THREAD_FLD(r18));
    GC_push_one(state.THREAD_FLD(r19));
    GC_push_one(state.THREAD_FLD(r20));
    GC_push_one(state.THREAD_FLD(r21));
    GC_push_one(state.THREAD_FLD(r22));
    GC_push_one(state.THREAD_FLD(r23));
    GC_push_one(state.THREAD_FLD(r24));
    GC_push_one(state.THREAD_FLD(r25));
    GC_push_one(state.THREAD_FLD(r26));
    GC_push_one(state.THREAD_FLD(r27));
    GC_push_one(state.THREAD_FLD(r28));
    GC_push_one(state.THREAD_FLD(r29));
    GC_push_one(state.THREAD_FLD(r30));
    GC_push_one(state.THREAD_FLD(r31));

#  elif defined(ARM32)
    lo = (ptr_t)state.THREAD_FLD(sp);
#    ifndef DARWIN_DONT_PARSE_STACK
    *phi = GC_FindTopOfStack(state.THREAD_FLD(r[7])); /*< `fp` */
#    endif
    {
      int j;
      for (j = 0; j < 7; j++)
        GC_push_one(state.THREAD_FLD(r[j]));
      /* Skip `r7` (because iOS uses it as a frame pointer). */
      j++;
      for (; j <= 12; j++)
        GC_push_one(state.THREAD_FLD(r[j]));
    }
    /* `cpsr`, `pc` and `sp` are skipped. */
    GC_push_one(state.THREAD_FLD(lr));

#  elif defined(AARCH64)
    lo = (ptr_t)state.THREAD_FLD(sp);
#    ifndef DARWIN_DONT_PARSE_STACK
    *phi = GC_FindTopOfStack(state.THREAD_FLD(fp));
#    endif
    {
      int j;
      for (j = 0; j <= 28; j++) {
        GC_push_one(state.THREAD_FLD(x[j]));
      }
    }
    /* `cpsr`, `fp`, `pc` and `sp` are skipped. */
    GC_push_one(state.THREAD_FLD(lr));

#  elif defined(CPPCHECK)
    lo = NULL;
#  else
#    error FIXME for non-arm/ppc/x86 architectures
#  endif
  }

#  ifndef DARWIN_DONT_PARSE_STACK
  /* TODO: Determine `p` and handle `altstack` if `!DARWIN_DONT_PARSE_STACK` */
  UNUSED_ARG(paltstack_hi);
#  else
  /*
   * `p` is guaranteed to be non-`NULL` regardless of
   * `GC_query_task_threads`.
   */
#    ifdef CPPCHECK
  if (NULL == p)
    ABORT("Bad GC_stack_range_for call");
#    endif
  crtn = p->crtn;
  *phi = crtn->stack_end;
  if (crtn->altstack != NULL && ADDR_GE(lo, crtn->altstack)
      && ADDR_GE(crtn->altstack + crtn->altstack_size, lo)) {
    *paltstack_lo = lo;
    *paltstack_hi = crtn->altstack + crtn->altstack_size;
    lo = crtn->normstack;
    *phi = lo + crtn->normstack_size;
  } else
#  endif
  /* else */ {
    *paltstack_lo = NULL;
  }
#  if defined(STACKPTR_CORRECTOR_AVAILABLE) && defined(DARWIN_DONT_PARSE_STACK)
  if (GC_sp_corrector != 0)
    GC_sp_corrector((void **)&lo, THREAD_ID_TO_VPTR(p->id));
#  endif
#  ifdef DEBUG_THREADS
  GC_log_printf("Darwin: Stack for thread %p is [%p,%p)\n",
                THREAD_ACT_TO_VPTR(thread), (void *)lo, (void *)*phi);
#  endif
  return lo;
}

GC_INNER void
GC_push_all_stacks(void)
{
  ptr_t hi, altstack_lo, altstack_hi;
  task_t my_task = current_task();
  mach_port_t my_thread = mach_thread_self();
  GC_bool found_me = FALSE;
  int nthreads = 0;
  word total_size = 0;

  GC_ASSERT(I_HOLD_LOCK());
  GC_ASSERT(GC_thr_initialized);

#  ifndef DARWIN_DONT_PARSE_STACK
  if (GC_query_task_threads) {
    int i;
    kern_return_t kern_result;
    thread_act_array_t act_list;
    mach_msg_type_number_t listcount;

    /* Obtain the list of the threads from the kernel. */
    kern_result = task_threads(my_task, &act_list, &listcount);
    if (kern_result != KERN_SUCCESS)
      ABORT("task_threads failed");

    for (i = 0; i < (int)listcount; i++) {
      thread_act_t thread = act_list[i];
      ptr_t lo = GC_stack_range_for(&hi, thread, NULL, my_thread, &altstack_lo,
                                    &altstack_hi, &found_me);

      if (lo) {
        GC_ASSERT(ADDR_GE(hi, lo));
        total_size += hi - lo;
        GC_push_all_stack(lo, hi);
      }
      /* TODO: Handle `altstack`. */
      nthreads++;
      mach_port_deallocate(my_task, thread);
    }

    vm_deallocate(my_task, (vm_address_t)act_list,
                  sizeof(thread_t) * listcount);
  } else
#  endif /* !DARWIN_DONT_PARSE_STACK */
  /* else */ {
    int i;

    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      GC_thread p;

      for (p = GC_threads[i]; p != NULL; p = p->tm.next) {
        GC_ASSERT(THREAD_TABLE_INDEX(p->id) == i);
        if (!KNOWN_FINISHED(p)) {
          thread_act_t thread = (thread_act_t)(p->mach_thread);
          ptr_t lo = GC_stack_range_for(&hi, thread, p, my_thread,
                                        &altstack_lo, &altstack_hi, &found_me);

          if (lo) {
            GC_ASSERT(ADDR_GE(hi, lo));
            total_size += hi - lo;
            GC_push_all_stack_sections(lo, hi, p->crtn->traced_stack_sect);
          }
          if (altstack_lo) {
            total_size += altstack_hi - altstack_lo;
            GC_push_all_stack(altstack_lo, altstack_hi);
          }
          nthreads++;
        }
      }
    }
  }

  mach_port_deallocate(my_task, my_thread);
  GC_VERBOSE_LOG_PRINTF("Pushed %d thread stacks\n", nthreads);
  if (!found_me && !GC_in_thread_creation)
    ABORT("Collecting from unknown thread");
  GC_total_stacksize = total_size;
}

#  ifndef GC_NO_THREADS_DISCOVERY

#    ifdef MPROTECT_VDB
STATIC mach_port_t GC_mach_handler_thread = 0;
STATIC GC_bool GC_use_mach_handler_thread = FALSE;

GC_INNER void
GC_darwin_register_self_mach_handler(void)
{
  GC_mach_handler_thread = mach_thread_self();
  GC_use_mach_handler_thread = TRUE;
}
#    endif /* MPROTECT_VDB */

#    ifndef GC_MAX_MACH_THREADS
#      define GC_MAX_MACH_THREADS THREAD_TABLE_SZ
#    endif

struct GC_mach_thread {
  thread_act_t thread;
  GC_bool suspended;
};

struct GC_mach_thread GC_mach_threads[GC_MAX_MACH_THREADS];
STATIC int GC_mach_threads_count = 0;
/* FIXME: It is better to implement `GC_mach_threads` as a hash set. */

/*
 * Return `TRUE` if there is a thread in `act_list` that was not
 * in `old_list`.
 */
STATIC GC_bool
GC_suspend_thread_list(thread_act_array_t act_list, int count,
                       thread_act_array_t old_list, int old_count,
                       task_t my_task, mach_port_t my_thread)
{
  int i;
  int j = -1;
  GC_bool changed = FALSE;

  GC_ASSERT(I_HOLD_LOCK());
  for (i = 0; i < count; i++) {
    thread_act_t thread = act_list[i];
    GC_bool found;
    kern_return_t kern_result;

    if (thread == my_thread
#    ifdef MPROTECT_VDB
        || (GC_mach_handler_thread == thread && GC_use_mach_handler_thread)
#    endif
#    ifdef PARALLEL_MARK
        || GC_is_mach_marker(thread) /*< ignore the parallel markers */
#    endif
    ) {
      /*
       * Do not add our one, parallel marker and the handler threads;
       * consider it as found (e.g., it was processed earlier).
       */
      mach_port_deallocate(my_task, thread);
      continue;
    }

    /* Find the current thread in the old list. */
    found = FALSE;
    {
      /* The previous found thread index. */
      int last_found = j;

      /* Search for the thread starting from the last found one first. */
      while (++j < old_count)
        if (old_list[j] == thread) {
          found = TRUE;
          break;
        }
      if (!found) {
        /* If not found, search in the rest (beginning) of the list. */
        for (j = 0; j < last_found; j++)
          if (old_list[j] == thread) {
            found = TRUE;
            break;
          }
      }
    }

    if (found) {
      /* It is already in the list, skip processing, release `mach` port. */
      mach_port_deallocate(my_task, thread);
      continue;
    }

    /* Add it to the `GC_mach_threads` list. */
    if (GC_mach_threads_count == GC_MAX_MACH_THREADS)
      ABORT("Too many threads");
    GC_mach_threads[GC_mach_threads_count].thread = thread;
    /* The default is "not suspended". */
    GC_mach_threads[GC_mach_threads_count].suspended = FALSE;
    changed = TRUE;

#    ifdef DEBUG_THREADS
    GC_log_printf("Suspending %p\n", THREAD_ACT_TO_VPTR(thread));
#    endif
    /*
     * Unconditionally suspend the thread.  It will do no harm if it is
     * already suspended by the client logic.
     */
    GC_acquire_dirty_lock();
    do {
      kern_result = thread_suspend(thread);
    } while (kern_result == KERN_ABORTED);
    GC_release_dirty_lock();
    if (kern_result != KERN_SUCCESS) {
      /*
       * The thread may have quit since the `thread_threads()` call we
       * mark as already suspended so it is not dealt with anymore later.
       */
      GC_mach_threads[GC_mach_threads_count].suspended = FALSE;
    } else {
      /* Mark the thread as suspended and require resume. */
      GC_mach_threads[GC_mach_threads_count].suspended = TRUE;
      if (GC_on_thread_event)
        GC_on_thread_event(GC_EVENT_THREAD_SUSPENDED,
                           THREAD_ACT_TO_VPTR(thread));
    }
    GC_mach_threads_count++;
  }
  return changed;
}

#  endif /* !GC_NO_THREADS_DISCOVERY */

GC_INNER void
GC_stop_world(void)
{
  task_t my_task = current_task();
  mach_port_t my_thread = mach_thread_self();
  kern_return_t kern_result;

  GC_ASSERT(I_HOLD_LOCK());
  GC_ASSERT(GC_thr_initialized);
#  ifdef DEBUG_THREADS
  GC_log_printf("Stopping the world from thread %p\n",
                MACH_PORT_TO_VPTR(my_thread));
#  endif
#  ifdef PARALLEL_MARK
  if (GC_parallel) {
    GC_acquire_mark_lock();
    /* We should have previously waited for it to become zero. */
    GC_ASSERT(GC_fl_builder_count == 0);
  }
#  endif /* PARALLEL_MARK */

  if (GC_query_task_threads) {
#  ifndef GC_NO_THREADS_DISCOVERY
    GC_bool changed;
    thread_act_array_t act_list, prev_list;
    mach_msg_type_number_t listcount, prevcount;

    /*
     * Clear out the `mach` threads list table.  We do not need to really
     * clear `GC_mach_threads[]` as it is used only in the range from 0 to
     * `GC_mach_threads_count - 1`, inclusive.
     */
    GC_mach_threads_count = 0;

    /*
     * Loop stopping threads until you have gone over the whole list twice
     * without a new one appearing.  `thread_create()` will not return (and
     * thus the thread stop) until the new thread exists, so there is
     * no window whereby you could stop a thread, recognize it is stopped,
     * but then have a new thread it created before stopping shows up later.
     */
    changed = TRUE;
    prev_list = NULL;
    prevcount = 0;
    do {
      kern_result = task_threads(my_task, &act_list, &listcount);

      if (kern_result == KERN_SUCCESS) {
        changed = GC_suspend_thread_list(act_list, listcount, prev_list,
                                         prevcount, my_task, my_thread);

        if (prev_list != NULL) {
          /*
           * Thread ports are not deallocated by list, unused ports
           * deallocated in `GC_suspend_thread_list`, used ones - kept
           * in `GC_mach_threads` till `GC_start_world` as otherwise thread
           * object change can occur and `GC_start_world` will not find the
           * thread to resume which will cause application to hang.
           */
          vm_deallocate(my_task, (vm_address_t)prev_list,
                        sizeof(thread_t) * prevcount);
        }

        /* Repeat while having changes. */
        prev_list = act_list;
        prevcount = listcount;
      }
    } while (changed);

    GC_ASSERT(prev_list != 0);
    /* The thread ports are not deallocated by list, see above. */
    vm_deallocate(my_task, (vm_address_t)act_list,
                  sizeof(thread_t) * listcount);
#  endif /* !GC_NO_THREADS_DISCOVERY */

  } else {
    unsigned i;

    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      GC_thread p;

      for (p = GC_threads[i]; p != NULL; p = p->tm.next) {
        if ((p->flags & (FINISHED | DO_BLOCKING)) == 0
            && p->mach_thread != my_thread) {
          GC_acquire_dirty_lock();
          do {
            kern_result = thread_suspend(p->mach_thread);
          } while (kern_result == KERN_ABORTED);
          GC_release_dirty_lock();
          if (kern_result != KERN_SUCCESS)
            ABORT("thread_suspend failed");
          if (GC_on_thread_event)
            GC_on_thread_event(GC_EVENT_THREAD_SUSPENDED,
                               MACH_PORT_TO_VPTR(p->mach_thread));
        }
      }
    }
  }

#  ifdef MPROTECT_VDB
  if (GC_auto_incremental) {
    GC_mprotect_stop();
  }
#  endif
#  ifdef PARALLEL_MARK
  if (GC_parallel)
    GC_release_mark_lock();
#  endif

#  ifdef DEBUG_THREADS
  GC_log_printf("World stopped from %p\n", MACH_PORT_TO_VPTR(my_thread));
#  endif
  mach_port_deallocate(my_task, my_thread);
}

GC_INLINE void
GC_thread_resume(thread_act_t thread)
{
  kern_return_t kern_result;
#  if defined(DEBUG_THREADS) || defined(GC_ASSERTIONS)
  struct thread_basic_info info;
  mach_msg_type_number_t outCount = THREAD_BASIC_INFO_COUNT;

#    ifdef CPPCHECK
  info.run_state = 0;
#    endif
  kern_result = thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&info,
                            &outCount);
  if (kern_result != KERN_SUCCESS)
    ABORT("thread_info failed");
#  endif
  GC_ASSERT(I_HOLD_LOCK());
#  ifdef DEBUG_THREADS
  GC_log_printf("Resuming thread %p with state %d\n",
                THREAD_ACT_TO_VPTR(thread), info.run_state);
#  endif
  /* Resume the thread. */
  kern_result = thread_resume(thread);
  if (kern_result != KERN_SUCCESS) {
    WARN("thread_resume(%p) failed: mach port invalid\n", thread);
  } else if (GC_on_thread_event) {
    GC_on_thread_event(GC_EVENT_THREAD_UNSUSPENDED,
                       THREAD_ACT_TO_VPTR(thread));
  }
}

GC_INNER void
GC_start_world(void)
{
  task_t my_task = current_task();

  /* The allocator lock is held continuously since the world stopped. */
  GC_ASSERT(I_HOLD_LOCK());
#  ifdef DEBUG_THREADS
  GC_log_printf("World starting\n");
#  endif
#  ifdef MPROTECT_VDB
  if (GC_auto_incremental) {
    GC_mprotect_resume();
  }
#  endif

  if (GC_query_task_threads) {
#  ifndef GC_NO_THREADS_DISCOVERY
    int i, j;
    kern_return_t kern_result;
    thread_act_array_t act_list;
    mach_msg_type_number_t listcount;

    kern_result = task_threads(my_task, &act_list, &listcount);
    if (kern_result != KERN_SUCCESS)
      ABORT("task_threads failed");

    j = (int)listcount;
    for (i = 0; i < GC_mach_threads_count; i++) {
      thread_act_t thread = GC_mach_threads[i].thread;

      if (GC_mach_threads[i].suspended) {
        /*
         * The thread index found during the previous iteration
         * (reaching `listcount` value means no thread found yet).
         */
        int last_found = j;

        /* Search for the thread starting from the last found one first. */
        while (++j < (int)listcount) {
          if (act_list[j] == thread)
            break;
        }
        if (j >= (int)listcount) {
          /* If not found, search in the rest (beginning) of the list. */
          for (j = 0; j < last_found; j++) {
            if (act_list[j] == thread)
              break;
          }
        }
        if (j != last_found) {
          /* The thread is alive, resume it. */
          GC_thread_resume(thread);
        }
      } else {
        /*
         * This thread has failed to be suspended by `GC_stop_world`,
         * no action is needed.
         */
#    ifdef DEBUG_THREADS
        GC_log_printf("Not resuming thread %p as it is not suspended\n",
                      THREAD_ACT_TO_VPTR(thread));
#    endif
      }
      mach_port_deallocate(my_task, thread);
    }

    for (i = 0; i < (int)listcount; i++)
      mach_port_deallocate(my_task, act_list[i]);
    vm_deallocate(my_task, (vm_address_t)act_list,
                  sizeof(thread_t) * listcount);
#  endif /* !GC_NO_THREADS_DISCOVERY */

  } else {
    int i;
    mach_port_t my_thread = mach_thread_self();

    for (i = 0; i < THREAD_TABLE_SZ; i++) {
      GC_thread p;

      for (p = GC_threads[i]; p != NULL; p = p->tm.next) {
        if ((p->flags & (FINISHED | DO_BLOCKING)) == 0
            && p->mach_thread != my_thread)
          GC_thread_resume(p->mach_thread);
      }
    }

    mach_port_deallocate(my_task, my_thread);
  }

#  ifdef DEBUG_THREADS
  GC_log_printf("World started\n");
#  endif
}

#endif /* DARWIN && THREADS */
