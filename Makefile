# Redefining srcdir allows object code for the nonPCR version of the collector
# to be generated in different directories
srcdir = .
VPATH = $(srcdir)

OBJS= alloc.o reclaim.o allochblk.o misc.o mach_dep.o os_dep.o mark_roots.o headers.o mark.o obj_map.o black_list.o finalize.o new_hblk.o real_malloc.o dynamic_load.o debug_malloc.o malloc.o stubborn.o checksums.o

CSRCS= reclaim.c allochblk.c misc.c alloc.c mach_dep.c os_dep.c mark_roots.c headers.c mark.c obj_map.c pcr_interface.c black_list.c finalize.c new_hblk.c real_malloc.c dynamic_load.c debug_malloc.c malloc.c stubborn.c checksums.c

SRCS= $(CSRCS) mips_mach_dep.s rs6000_mach_dep.s alpha_mach_dep.s gc.h gc_headers.h gc_private.h config.h gc_inline.h gc.man if_mach.c if_not_there.c

# The following is irrelevant on most systems.  But a few
# versions of make otherwise fork the shell specified in
# the SHELL environment variable.
SHELL= /bin/sh

CC= cc
CFLAGS= -O -DSILENT
# Setjmp_test may yield overly optimistic results when compiled
# without optimization.
# -DSILENT disables statistics printing, and improves performance.
# -DCHECKSUMS reports on erroneously clear dirty bits, and unexpectedly
# altered stubborn objects, at substantial performance cost.
# -DFIND_LEAK causes the collector to assume that all inaccessible
# objects should have been explicitly deallocated, and reports exceptions

SPECIALCFLAGS = 
# Alternative flags to the C compiler for mach_dep.c.
# Mach_dep.c often doesn't like optimization, and it's
# not time-critical anyway.
# Set SPECIALCFLAGS to -q nodirect_code on Encore.

all: gc.a gctest

pcr: PCR-Makefile gc_private.h gc_headers.h gc.h config.h mach_dep.o $(SRCS)
	make -f PCR-Makefile depend
	make -f PCR-Makefile

$(OBJS) test.o: $(srcdir)/gc_private.h $(srcdir)/gc_headers.h $(srcdir)/gc.h $(srcdir)/config.h

gc.a: $(OBJS)
	ar ru gc.a $(OBJS)
	ranlib gc.a || cat /dev/null
#	ignore ranlib failure; that usually means it doesn't exist, and isn't needed

mach_dep.o: $(srcdir)/mach_dep.c $(srcdir)/mips_mach_dep.s $(srcdir)/rs6000_mach_dep.s if_mach if_not_there
	rm -f mach_dep.o
	./if_mach MIPS "" as -o mach_dep.o $(srcdir)/mips_mach_dep.s
	./if_mach RS6000 "" as -o mach_dep.o $(srcdir)/rs6000_mach_dep.s
	./if_mach ALPHA "" as -o mach_dep.o $(srcdir)/alpha_mach_dep.s
	./if_not_there mach_dep.o $(CC) -c $(SPECIALCFLAGS) $(srcdir)/mach_dep.c

if_mach: $(srcdir)/if_mach.c $(srcdir)/config.h
	$(CC) $(CFLAGS) -o if_mach $(srcdir)/if_mach.c

if_not_there: $(srcdir)/if_not_there.c
	$(CC) $(CFLAGS) -o if_not_there $(srcdir)/if_not_there.c

clean: 
	rm -f gc.a test.o gctest output-local output-diff $(OBJS) \
	      setjmp_test  mon.out gmon.out a.out core if_not_there if_mach
	-rm -f *~

gctest: test.o gc.a if_mach if_not_there
	rm -f gctest
	./if_mach ALPHA "" $(CC) $(CFLAGS) -o gctest -non_shared test.o gc.a
	./if_not_there gctest $(CC) $(CFLAGS) -o gctest test.o gc.a

# If an optimized setjmp_test generates a segmentation fault,
# odds are your compiler is broken.  Gctest may still work.
# Try compiling setjmp_test unoptimized.
setjmp_test: $(srcdir)/setjmp_test.c $(srcdir)/gc.h if_mach if_not_there
	rm -f setjmp_test
	./if_mach ALPHA "" $(CC) $(CFLAGS) -o setjmp_test -non_shared $(srcdir)/setjmp_test.c
	./if_not_there setjmp_test $(CC) $(CFLAGS) -o setjmp_test $(srcdir)/setjmp_test.c

test: setjmp_test gctest
	./setjmp_test
	./gctest

tar:
	tar cvf gc.tar $(SRCS) Makefile PCR-Makefile OS2_MAKEFILE README test.c setjmp_test.c \
		SMakefile.amiga SCoptions.amiga README.amiga
	compress gc.tar

lint: $(CSRCS) test.c
	lint -DLINT $(CSRCS) test.c | egrep -v "possible pointer alignment problem|abort|exit|sbrk|mprotect|syscall"
