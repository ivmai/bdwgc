OBJS= alloc.o reclaim.o allochblk.o misc.o mach_dep.o os_dep.o mark_roots.o headers.o mark.o obj_map.o black_list.o finalize.o new_hblk.o real_malloc.o dynamic_load.o debug_malloc.o

CSRCS= reclaim.c allochblk.c misc.c alloc.c mach_dep.c os_dep.c mark_roots.c headers.c mark.c obj_map.c pcr_interface.c black_list.c finalize.c new_hblk.c real_malloc.c dynamic_load.c debug_malloc.c

SRCS= $(CSRCS) mips_mach_dep.s rs6000_mach_dep.s interface.c gc.h gc_headers.h gc_private.h gc_inline.h gc.man

CC= cc
CFLAGS= -O
# Setjmp_test may yield overly optimistic results when compiled
# without optimization.

SPECIALCFLAGS = 
# Alternative flags to the C compiler for mach_dep.c.
# Mach_dep.c often doesn't like optimization, and it's
# not time-critical anyway.
# Set SPECIALCFLAGS to -q nodirect_code on Encore.

all: gc.a gctest

pcr: PCR-Makefile gc_private.h gc_headers.h gc.h $(SRCS)
	make -f PCR-Makefile

$(OBJS) test.o: gc_private.h gc_headers.h gc.h Makefile

#  On some machines, the ranlib command may have to be removed.
#  On an SGI for example, ranlib doesn't exist, and is not needed.
#  Ditto for Solaris 2.X.
gc.a: $(OBJS)
	ar ru gc.a $(OBJS)
	ranlib gc.a

# On a MIPS-based machine, replace the rule for mach_dep.o by the
# following:
# mach_dep.o: mips_mach_dep.s
#	as -o mach_dep.o mips_mach_dep.s
# On an IBM RS6000, use the following two lines:
# mach_dep.o: rs6000_mach_dep.s
#	as -o mach_dep.o rs6000_mach_dep.s
mach_dep.o: mach_dep.c
	$(CC) -c $(SPECIALCFLAGS) mach_dep.c

clean: 
	rm -f gc.a test.o gctest output-local output-diff $(OBJS) \
	      setjmp_test  mon.out gmon.out a.out core
	-rm -f *~


# On a MIPS system, the BSD version of libc.a should be used to get
# sigsetmask.  I found it necessary to link against the system V
# library first, to get a working version of fprintf.  But this may have
# been due to my failure to find the right version of stdio.h or some
# such thing.
# On a Solaris 2.X system, also make sure you're using BSD libraries.
gctest: test.o gc.a
	$(CC) $(CFLAGS) -o gctest test.o gc.a

# If an optimized setjmp_test generates a segmentation fault,
# odds are your compiler is broken.  Gctest may still work.
# Try compiling setjmp_test unoptimized.
setjmp_test: setjmp_test.c gc.h
	$(CC) $(CFLAGS) -o setjmp_test setjmp_test.c

test: setjmp_test gctest
	./setjmp_test
	./gctest

tar:
	tar cvf gc.tar $(SRCS) Makefile PCR-Makefile OS2_MAKEFILE README test.c setjmp_test.c
	compress gc.tar

lint: $(CSRCS) test.c
	lint $(CSRCS) test.c | egrep -v "possible pointer alignment problem|abort|exit"
