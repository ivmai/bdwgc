# Makefile to build Hans Boehm garbage collector using the Digital Mars
# compiler from www.digitalmars.com
# Written by Walter Bright

CFLAGS_EXTRA=
DEFINES=-D_WINDOWS -DGC_DLL -DGC_THREADS -DGC_DISCOVER_TASK_THREADS \
    -DALL_INTERIOR_POINTERS -DENABLE_DISCLAIM -DGC_ATOMIC_UNCOLLECTABLE \
    -DGC_GCJ_SUPPORT -DJAVA_FINALIZATION -DNO_EXECUTE_PERMISSION \
    -DGC_REQUIRE_WCSDUP -DUSE_MUNMAP
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

check: gctest.exe cpptest.exe
	gctest.exe
	cpptest.exe

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
	del *.log gc.def gc.dll gc.lib gc.map gctest.map cpptest.map
	del tests\gctest.obj gctest.exe tests\cpptest.obj cpptest.exe
	del $(OBJS)

gctest.exe: gc.lib tests\gctest.obj
	$(CC) -ogctest.exe tests\gctest.obj gc.lib

tests\gctest.obj: tests\gctest.c
	$(CC) -c $(CFLAGS) tests\gctest.c -otests\gctest.obj

cpptest.exe: gc.lib tests\cpptest.obj
	$(CC) -ocpptest.exe tests\cpptest.obj gc.lib

tests\cpptest.obj: tests\cpp.cc
	$(CC) -c $(CFLAGS) -cpp tests\cpp.cc -otests\cpptest.obj

gc_badalc.obj: gc_badalc.cc gc_badalc.cpp
gc_cpp.obj: gc_cpp.cc gc_cpp.cpp
