# Primary targets:
# gc.a - builds basic library
# c++ - adds C++ interface to library and include directory
# cords - adds cords (heavyweight strings) to library and include directory
# test - prints porting information, then builds basic version of gc.a, and runs
#        some tests of collector and cords.  Does not add cords or c++ interface to gc.a
# cord/de - builds dumb editor based on cords.
CC= cc
CXX=g++
# Needed only for "make c++", which adds the c++ interface

CFLAGS= -O -DALL_INTERIOR_POINTERS -DSILENT
# Setjmp_test may yield overly optimistic results when compiled
# without optimization.
# -DSILENT disables statistics printing, and improves performance.
# -DCHECKSUMS reports on erroneously clear dirty bits, and unexpectedly
#   altered stubborn objects, at substantial performance cost.
# -DFIND_LEAK causes the collector to assume that all inaccessible
#   objects should have been explicitly deallocated, and reports exceptions
# -DSOLARIS_THREADS enables support for Solaris (thr_) threads.
#   (Clients should also define SOLARIS_THREADS and then include
#   gc.h before performing thr_ or GC_ operations.)
# -DALL_INTERIOR_POINTERS allows all pointers to the interior
#   of objects to be recognized.  (See gc_private.h for consequences.)
# -DSMALL_CONFIG tries to tune the collector for small heap sizes,
#   usually causing it to use less space in such situations.
#   Incremental collection no longer works in this case.
# -DDONT_ADD_BYTE_AT_END is meaningful only with
#   -DALL_INTERIOR_POINTERS.  Normally -DALL_INTERIOR_POINTERS
#   causes all objects to be padded so that pointers just past the end of
#   an object can be recognized.  This can be expensive.  (The padding
#   is normally more than one byte due to alignment constraints.)
#   -DDONT_ADD_BYTE_AT_END disables the padding.

AR= ar
RANLIB= ranlib


# Redefining srcdir allows object code for the nonPCR version of the collector
# to be generated in different directories
srcdir = .
VPATH = $(srcdir)

OBJS= alloc.o reclaim.o allchblk.o misc.o mach_dep.o os_dep.o mark_rts.o headers.o mark.o obj_map.o blacklst.o finalize.o new_hblk.o dyn_load.o dbg_mlc.o malloc.o stubborn.o checksums.o solaris_threads.o typd_mlc.o

CSRCS= reclaim.c allchblk.c misc.c alloc.c mach_dep.c os_dep.c mark_rts.c headers.c mark.c obj_map.c pcr_interface.c blacklst.c finalize.c new_hblk.c real_malloc.c dyn_load.c dbg_mlc.c malloc.c stubborn.c checksums.c solaris_threads.c typd_mlc.c

CORD_SRCS=  cord/cordbscs.c cord/cordxtra.c cord/cordprnt.c cord/de.c cord/cordtest.c cord/cord.h cord/ec.h cord/cord_pos.h cord/de_win.c cord/de_win.h cord/de_cmds.h cord/de_win.ICO cord/de_win.RC

CORD_OBJS=  cord/cordbscs.o cord/cordxtra.o cord/cordprnt.o

SRCS= $(CSRCS) mips_mach_dep.s rs6000_mach_dep.s alpha_mach_dep.s sparc_mach_dep.s gc.h gc_typed.h gc_hdrs.h gc_priv.h gc_private.h config.h gc_mark.h gc_inl.h gc_inline.h gc.man if_mach.c if_not_there.c gc_c++.cc gc_c++.h $(CORD_SRCS)

OTHER_FILES= Makefile PCR-Makefile OS2_MAKEFILE NT_MAKEFILE \
           README test.c setjmp_t.c SMakefile.amiga SCoptions.amiga \
           README.amiga README.win32 cord/README include/gc.h \
           include/gc_typed.h README.QUICK callprocs pc_excludes \
           barrett_diagram README.OS2

CORD_INCLUDE_FILES= $(srcdir)/gc.h $(srcdir)/cord/cord.h $(srcdir)/cord/ec.h \
           $(srcdir)/cord/cord_pos.h

# Libraries needed for curses applications.  Only needed for de.
CURSES= -lcurses -ltermlib

# The following is irrelevant on most systems.  But a few
# versions of make otherwise fork the shell specified in
# the SHELL environment variable.
SHELL= /bin/sh

SPECIALCFLAGS = 
# Alternative flags to the C compiler for mach_dep.c.
# Mach_dep.c often doesn't like optimization, and it's
# not time-critical anyway.
# Set SPECIALCFLAGS to -q nodirect_code on Encore.

ALPHACFLAGS = -non_shared
# Extra flags for linking compilation on DEC Alpha

all: gc.a gctest

pcr: PCR-Makefile gc_private.h gc_hdrs.h gc.h config.h mach_dep.o $(SRCS)
	make -f PCR-Makefile depend
	make -f PCR-Makefile

$(OBJS) test.o: $(srcdir)/gc_priv.h $(srcdir)/gc_hdrs.h $(srcdir)/gc.h \
    $(srcdir)/config.h $(srcdir)/gc_typed.h Makefile
# The dependency on Makefile is needed.  Changing
# options such as -DSILENT affects the size of GC_arrays,
# invalidating all .o files that rely on gc_priv.h

mark.o typd_mlc.o finalize.o: $(srcdir)/gc_mark.h

gc.a: $(OBJS)
	$(AR) ru gc.a $(OBJS)
	$(RANLIB) gc.a || cat /dev/null
#	ignore ranlib failure; that usually means it doesn't exist, and isn't needed

cords: $(CORD_OBJS) cord/cordtest
	$(AR) ru gc.a $(CORD_OBJS)
	$(RANLIB) gc.a || cat /dev/null
	cp $(srcdir)/cord/cord.h include/cord.h
	cp $(srcdir)/cord/ec.h include/ec.h
	cp $(srcdir)/cord/cord_pos.h include/cord_pos.h

gc_c++.o: $(srcdir)/gc_c++.cc $(srcdir)/gc_c++.h
	$(CXX) -c -O $(srcdir)/gc_c++.cc
	
c++: gc_c++.o $(srcdir)/gc_c++.h
	$(AR) ru gc.a gc_c++.o
	$(RANLIB) gc.a || cat /dev/null
	cp $(srcdir)/gc_c++.h include/gc_c++.h 

mach_dep.o: $(srcdir)/mach_dep.c $(srcdir)/mips_mach_dep.s $(srcdir)/rs6000_mach_dep.s if_mach if_not_there
	rm -f mach_dep.o
	./if_mach MIPS "" as -o mach_dep.o $(srcdir)/mips_mach_dep.s
	./if_mach RS6000 "" as -o mach_dep.o $(srcdir)/rs6000_mach_dep.s
	./if_mach ALPHA "" as -o mach_dep.o $(srcdir)/alpha_mach_dep.s
	./if_mach SPARC SUNOS5 as -o mach_dep.o $(srcdir)/sparc_mach_dep.s
	./if_not_there mach_dep.o $(CC) -c $(SPECIALCFLAGS) $(srcdir)/mach_dep.c

mark_rts.o: $(srcdir)/mark_rts.c if_mach if_not_there
	rm -f mark_rts.o
	./if_mach ALPHA "" $(CC) -c $(CFLAGS) -Wo,-notail $(srcdir)/mark_rts.c
	./if_not_there mark_rts.o $(CC) -c $(CFLAGS) $(srcdir)/mark_rts.c
#	work-around for DEC optimizer tail recursion elimination bug

cord/cordbscs.o: $(srcdir)/cord/cordbscs.c $(CORD_INCLUDE_FILES)
	$(CC) $(CFLAGS) -c $(srcdir)/cord/cordbscs.c
	mv cordbscs.o cord/cordbscs.o
#  not all compilers understand -o filename

cord/cordxtra.o: $(srcdir)/cord/cordxtra.c $(CORD_INCLUDE_FILES)
	$(CC) $(CFLAGS) -c $(srcdir)/cord/cordxtra.c
	mv cordxtra.o cord/cordxtra.o

cord/cordprnt.o: $(srcdir)/cord/cordprnt.c $(CORD_INCLUDE_FILES)
	$(CC) $(CFLAGS) -c $(srcdir)/cord/cordprnt.c
	mv cordprnt.o cord/cordprnt.o

cord/cordtest: $(srcdir)/cord/cordtest.c $(CORD_OBJS) gc.a
	rm -f cord/cordtest
	./if_mach SPARC SUNOS5 $(CC) $(CFLAGS) -o cord/cordtest $(srcdir)/cord/cordtest.c $(CORD_OBJS) gc.a -lthread
	./if_not_there cord/cord_test $(CC) $(CFLAGS) -o cord/cordtest $(srcdir)/cord/cordtest.c $(CORD_OBJS) gc.a

cord/de: $(srcdir)/cord/de.c $(srcdir)/cord/cordbscs.o $(srcdir)/cord/cordxtra.o gc.a
	rm -f cord/de
	./if_mach SPARC SUNOS5 $(CC) $(CFLAGS) -o cord/de $(srcdir)/cord/de.c $(srcdir)/cord/cordbscs.o $(srcdir)/cord/cordxtra.o gc.a $(CURSES) -lthread
	./if_mach RS6000 "" $(CC) $(CFLAGS) -o cord/de $(srcdir)/cord/de.c $(srcdir)/cord/cordbscs.o $(srcdir)/cord/cordxtra.o gc.a -lcurses
	./if_not_there cord/de $(CC) $(CFLAGS) -o cord/de $(srcdir)/cord/de.c $(srcdir)/cord/cordbscs.o $(srcdir)/cord/cordxtra.o gc.a $(CURSES)

if_mach: $(srcdir)/if_mach.c $(srcdir)/config.h
	$(CC) $(CFLAGS) -o if_mach $(srcdir)/if_mach.c

if_not_there: $(srcdir)/if_not_there.c
	$(CC) $(CFLAGS) -o if_not_there $(srcdir)/if_not_there.c

clean: 
	rm -f gc.a test.o gctest output-local output-diff $(OBJS) \
	      setjmp_test  mon.out gmon.out a.out core if_not_there if_mach \
	      $(CORD_OBJS) cord/cordtest cord/de
	-rm -f *~

gctest: test.o gc.a if_mach if_not_there
	rm -f gctest
	./if_mach ALPHA "" $(CC) $(CFLAGS) -o gctest $(ALPHACFLAGS) test.o gc.a
	./if_mach SPARC SUNOS5 $(CC) $(CFLAGS) -o gctest $(CFLAGS) test.o gc.a -lthread
	./if_not_there gctest $(CC) $(CFLAGS) -o gctest test.o gc.a

# If an optimized setjmp_test generates a segmentation fault,
# odds are your compiler is broken.  Gctest may still work.
# Try compiling setjmp_t.c unoptimized.
setjmp_test: $(srcdir)/setjmp_t.c $(srcdir)/gc.h if_mach if_not_there
	rm -f setjmp_test
	./if_mach ALPHA "" $(CC) $(CFLAGS) -o setjmp_test $(ALPHACFLAGS) $(srcdir)/setjmp_t.c
	./if_not_there setjmp_test $(CC) $(CFLAGS) -o setjmp_test $(srcdir)/setjmp_t.c

test: setjmp_test gctest
	./setjmp_test
	./gctest
	make cord/cordtest
	cord/cordtest

gc.tar: $(SRCS) $(OTHER_FILES)
	tar cvf gc.tar $(SRCS) $(OTHER_FILES)
	
pc_gc.tar: $(SRCS) $(OTHER_FILES)
	tar cvfX pc_gc.tar pc_excludes $(SRCS) $(OTHER_FILES)

floppy: pc_gc.tar
	-mmd a:/cord
	-mmd a:/include
	mkdir /tmp/pc_gc
	cat pc_gc.tar | (cd /tmp/pc_gc; tar xvf -)
	-mcopy -tmn /tmp/pc_gc/* a:
	-mcopy -tmn /tmp/pc_gc/cord/* a:/cord
	-mcopy -mn /tmp/pc_gc/cord/de_win.ICO a:/cord
	-mcopy -tmn /tmp/pc_gc/include/* a:/cord
	rm -r /tmp/pc_gc

gc.tar.Z: gc.tar
	compress gc.tar

lint: $(CSRCS) test.c
	lint -DLINT $(CSRCS) test.c | egrep -v "possible pointer alignment problem|abort|exit|sbrk|mprotect|syscall"
