OBJS= alloc.o reclaim.o allochblk.o misc.o mach_dep.o os_dep.o mark_roots.o headers.o mark.o obj_map.o black_list.o finalize.o new_hblk.o real_malloc.o dynamic_load.o debug_malloc.o

CSRCS= reclaim.c allochblk.c misc.c alloc.c mach_dep.c os_dep.c mark_roots.c headers.c mark.c obj_map.c pcr_interface.c black_list.c finalize.c new_hblk.c real_malloc.c dynamic_load.c debug_malloc.c

SRCS= $(CSRCS) mips_mach_dep.s rs6000_mach_dep.s interface.c gc.h gc_headers.h gc_private.h config.h gc_inline.h gc.man if_mach.c if_not_there.c

# The following is irrelevant on most systems.  But a few
# versions of make otherwise fork the shell specified in
# the SHELL environment variable.
SHELL= /bin/sh

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

pcr: PCR-Makefile gc_private.h gc_headers.h gc.h config.h mach_dep.o $(SRCS)
	make -f PCR-Makefile depend
	make -f PCR-Makefile

$(OBJS) test.o: gc_private.h gc_headers.h gc.h config.h Makefile

gc.a: $(OBJS)
	ar ru gc.a $(OBJS)
	ranlib gc.a || cat /dev/null
#	ignore ranlib failure; that usually means it doesn't exist, and isn't needed

mach_dep.o: mach_dep.c mips_mach_dep.s rs6000_mach_dep.s if_mach if_not_there
	rm -f mach_dep.o
	./if_mach MIPS "" as -o mach_dep.o mips_mach_dep.s
	./if_mach RS6000 "" as -o mach_dep.o rs6000_mach_dep.s
	./if_not_there mach_dep.o $(CC) -c $(SPECIALCFLAGS) mach_dep.c

if_mach: if_mach.c config.h
	$(CC) $(CFLAGS) -o if_mach if_mach.c
	
if_not_there: if_not_there.c
	$(CC) $(CFLAGS) -o if_not_there if_not_there.c

clean: 
	rm -f gc.a test.o gctest output-local output-diff $(OBJS) \
	      setjmp_test  mon.out gmon.out a.out core
	-rm -f *~

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
