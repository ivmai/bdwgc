# Redefining srcdir allows object code for the nonPCR version of the collector
# to be generated in different directories
srcdir = .
VPATH = $(srcdir)

OBJS= alloc.o reclaim.o allochblk.o misc.o mach_dep.o os_dep.o mark_roots.o headers.o mark.o obj_map.o black_list.o finalize.o new_hblk.o real_malloc.o dynamic_load.o debug_malloc.o malloc.o stubborn.o checksums.o

CSRCS= reclaim.c allochblk.c misc.c alloc.c mach_dep.c os_dep.c mark_roots.c headers.c mark.c obj_map.c pcr_interface.c black_list.c finalize.c new_hblk.c real_malloc.c dynamic_load.c debug_malloc.c malloc.c stubborn.c checksums.c

CORD_SRCS=  cord/cord_basics.c cord/cord_extras.c cord/de.c cord/cord_test.c cord/cord.h cord/ec.h cord/cord_position.h

CORD_OBJS=  cord/cord_basics.o cord/cord_extras.o

SRCS= $(CSRCS) mips_mach_dep.s rs6000_mach_dep.s alpha_mach_dep.s sparc_mach_dep.s gc.h gc_headers.h gc_private.h config.h gc_inline.h gc.man if_mach.c if_not_there.c $(CORD_SRCS)

INCLUDE_FILES= gc.h cord/cord.h cord/ec.h cord/cord_position.h

# Libraries needed for curses applications.  Only needed for de.
CURSES= -lcurses -ltermlib

# The following is irrelevant on most systems.  But a few
# versions of make otherwise fork the shell specified in
# the SHELL environment variable.
SHELL= /bin/sh

AR= ar
RANLIB= ranlib
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

ALPHACFLAGS = -non_shared
# Extra flags for linking compilation on DEC Alpha

all: gc.a gctest

pcr: PCR-Makefile gc_private.h gc_headers.h gc.h config.h mach_dep.o $(SRCS)
	make -f PCR-Makefile depend
	make -f PCR-Makefile

$(OBJS) test.o: $(srcdir)/gc_private.h $(srcdir)/gc_headers.h $(srcdir)/gc.h $(srcdir)/config.h

gc.a: $(OBJS)
	$(AR) ru gc.a $(OBJS)
	$(RANLIB) gc.a || cat /dev/null
#	ignore ranlib failure; that usually means it doesn't exist, and isn't needed

cords: $(CORD_OBJS) cord/cord_test
	$(AR) ru gc.a $(CORD_OBJS)
	$(RANLIB) gc.a || cat /dev/null
	ln cord/cord.h include/cord.h
	ln cord/ec.h include/ec.h
	ln cord/cord_position.h include/cord_position.h

mach_dep.o: $(srcdir)/mach_dep.c $(srcdir)/mips_mach_dep.s $(srcdir)/rs6000_mach_dep.s if_mach if_not_there
	rm -f mach_dep.o
	./if_mach MIPS "" as -o mach_dep.o $(srcdir)/mips_mach_dep.s
	./if_mach RS6000 "" as -o mach_dep.o $(srcdir)/rs6000_mach_dep.s
	./if_mach ALPHA "" as -o mach_dep.o $(srcdir)/alpha_mach_dep.s
	./if_mach SPARC SUNOS5 as -o mach_dep.o $(srcdir)/sparc_mach_dep.s
	./if_not_there mach_dep.o $(CC) -c $(SPECIALCFLAGS) $(srcdir)/mach_dep.c

mark_roots.o: $(srcdir)/mark_roots.c
	rm -f mark_roots.o
	./if_mach ALPHA "" $(CC) -c $(CFLAGS) -Wo,-notail $(srcdir)/mark_roots.c
	./if_not_there mark_roots.o $(CC) -c $(CFLAGS) $(srcdir)/mark_roots.c
#	work-around for DEC optimizer tail recursion elimination bug

cord/cord_basics.o: $(srcdir)/cord/cord_basics.c $(INCLUDE_FILES)
	$(CC) $(CFLAGS) -c -o cord/cord_basics.o $(srcdir)/cord/cord_basics.c

cord/cord_extras.o: $(srcdir)/cord/cord_extras.c $(INCLUDE_FILES)
	$(CC) $(CFLAGS) -c -o cord/cord_extras.o $(srcdir)/cord/cord_extras.c

cord/cord_test: $(srcdir)/cord/cord_test.c $(CORD_OBJS) gc.a
	$(CC) $(CFLAGS) -o cord/cord_test $(srcdir)/cord/cord_test.c $(CORD_OBJS) gc.a

cord/de: $(srcdir)/cord/de.c $(CORD_OBJS) gc.a
	$(CC) $(CFLAGS) -o cord/de $(srcdir)/cord/de.c $(CORD_OBJS) gc.a $(CURSES)

if_mach: $(srcdir)/if_mach.c $(srcdir)/config.h
	$(CC) $(CFLAGS) -o if_mach $(srcdir)/if_mach.c

if_not_there: $(srcdir)/if_not_there.c
	$(CC) $(CFLAGS) -o if_not_there $(srcdir)/if_not_there.c

clean: 
	rm -f gc.a test.o gctest output-local output-diff $(OBJS) \
	      setjmp_test  mon.out gmon.out a.out core if_not_there if_mach \
	      $(CORD_OBJS) cord/cord_test cord/de
	-rm -f *~

gctest: test.o gc.a if_mach if_not_there
	rm -f gctest
	./if_mach ALPHA "" $(CC) $(CFLAGS) -o gctest $(ALPHACFLAGS) test.o gc.a
	./if_not_there gctest $(CC) $(CFLAGS) -o gctest test.o gc.a

# If an optimized setjmp_test generates a segmentation fault,
# odds are your compiler is broken.  Gctest may still work.
# Try compiling setjmp_test unoptimized.
setjmp_test: $(srcdir)/setjmp_test.c $(srcdir)/gc.h if_mach if_not_there
	rm -f setjmp_test
	./if_mach ALPHA "" $(CC) $(CFLAGS) -o setjmp_test $(ALPHACFLAGS) $(srcdir)/setjmp_test.c
	./if_not_there setjmp_test $(CC) $(CFLAGS) -o setjmp_test $(srcdir)/setjmp_test.c

test: setjmp_test gctest
	./setjmp_test
	./gctest
	make cord/cord_test
	cord/cord_test

tar:
	tar cvf gc.tar $(SRCS) Makefile PCR-Makefile OS2_MAKEFILE README test.c setjmp_test.c \
		SMakefile.amiga SCoptions.amiga README.amiga cord/README include/gc.h
	compress gc.tar

lint: $(CSRCS) test.c
	lint -DLINT $(CSRCS) test.c | egrep -v "possible pointer alignment problem|abort|exit|sbrk|mprotect|syscall"
