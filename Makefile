OBJS= alloc.o reclaim.o allochblk.o misc.o mach_dep.o mark_roots.o
# add rt_allocobj.o for RT version

SRCS= reclaim.c allochblk.c misc.c alloc.c mach_dep.c rt_allocobj.s mips_mach_dep.s mark_roots.c

CFLAGS= -O

# Set SPECIALCFLAGS to -q nodirect_code on Encore.
# On Sun systems under 4.0, it's probably safer to link with -Bstatic.
# I'm not sure that all static data will otherwise be found.
# It also makes sense to replace -O with -O4, though it doesn't appear
# to make much difference.

SPECIALCFLAGS = 

all: gc.a gctest

$(OBJS): gc.h

gc.a: $(OBJS)
	ar ru gc.a $(OBJS)
	ranlib gc.a

# mach_dep.c doesn't like optimization
# On a MIPS machine, move mips_mach_dep.s to mach_dep.s and remove
# mach_dep.c as well as the following two lines from this Makefile
# On an IBM RS6000, do the same thing with rs6000_mach_dep.s.  Notice
# that the assembly language interface to the allocator is not completely
# implemented on an RS6000.
mach_dep.o: mach_dep.c
	cc -c ${SPECIALCFLAGS} mach_dep.c

clean: 
	rm -f gc.a test.o cons.o gctest output-local output-diff $(OBJS)

test.o: cons.h test.c

cons.o: cons.h cons.c

# On a MIPS system, the BSD version of libc.a should be used to get
# sigsetmask.  I found it necessary to link against the system V
# library first, to get a working version of fprintf.  But this may have
# been due to my failure to find the right version of stdio.h or some
# such thing.
gctest: test.o cons.o gc.a
	cc $(CFLAGS) -o gctest test.o cons.o gc.a

setjmp_test: setjmp_test.c gc.h
	cc -o setjmp_test -O setjmp_test.c

test: setjmp_test gctest
	./setjmp_test
	@echo "WARNING: for GC test to work, all debugging output must be turned off"
	rm -f output-local
	./gctest > output-local
	-diff correct-output output-local > output-diff
	-@test -s output-diff && echo 'Output of program "gctest" is not correct.  GC does not work.' || echo 'Output of program "gctest" is correct.  GC probably works.' 
	
shar:
	makescript -o gc.shar README Makefile gc.h ${SRCS} test.c cons.c cons.h
