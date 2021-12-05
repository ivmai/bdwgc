# Makefile to build Hans Boehm garbage collector using the Digital Mars
# compiler from www.digitalmars.com
# Written by Walter Bright

CFLAGS_EXTRA=
DEFINES=-D_WINDOWS -DGC_DLL -DGC_THREADS -DGC_DISCOVER_TASK_THREADS \
    -DALL_INTERIOR_POINTERS -DENABLE_DISCLAIM -DGC_ATOMIC_UNCOLLECTABLE \
    -DGC_GCJ_SUPPORT -DJAVA_FINALIZATION -DNO_EXECUTE_PERMISSION -DUSE_MUNMAP
CFLAGS=-Iinclude -Ilibatomic_ops\src $(DEFINES) -g $(CFLAGS_EXTRA)
LFLAGS=/ma/implib/co
CC=sc

# Must precede other goals.
all: gc.dll gc.lib

gc.obj: extra\gc.c
	$(CC) -c $(CFLAGS) extra\gc.c -ogc.obj

.cpp.obj:
	$(CC) -c $(CFLAGS) -Aa $*

OBJS= gc.obj gc_badalc.obj gc_cpp.obj

check: gctest.exe test_cpp.exe
	gctest.exe
	test_cpp.exe

gc.lib: gc.dll

gc.dll: $(OBJS) gc.def digimars.mak
	$(CC) -ogc.dll $(OBJS) -L$(LFLAGS) gc.def kernel32.lib user32.lib

gc.def: digimars.mak
	echo LIBRARY GC >gc.def
	echo DESCRIPTION "Boehm-Demers-Weiser Garbage Collector" >>gc.def
	echo EXETYPE NT	>>gc.def
	echo EXPORTS >>gc.def
	echo GC_is_visible_print_proc >>gc.def
	echo GC_is_valid_displacement_print_proc >>gc.def

clean:
	del *.log gc.def gc.dll gc.lib gc.map gctest.map test_cpp.map
	del tests\test.obj gctest.exe tests\test_cpp.obj test_cpp.exe
	del $(OBJS)

gctest.exe: gc.lib tests\test.obj
	$(CC) -ogctest.exe tests\test.obj gc.lib

tests\test.obj: tests\test.c
	$(CC) -c $(CFLAGS) tests\test.c -otests\test.obj

test_cpp.exe: gc.lib tests\test_cpp.obj
	$(CC) -otest_cpp.exe tests\test_cpp.obj gc.lib

tests\test_cpp.obj: tests\test_cpp.cc
	$(CC) -c $(CFLAGS) -cpp tests\test_cpp.cc -otests\test_cpp.obj

gc_badalc.obj: gc_badalc.cc gc_badalc.cpp
gc_cpp.obj: gc_cpp.cc gc_cpp.cpp
