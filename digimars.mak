# Makefile to build Hans Boehm garbage collector using the Digital Mars
# compiler from www.digitalmars.com
# Written by Walter Bright

DEFINES=-D_WINDOWS -DGC_DLL -DGC_THREADS -DGC_DISCOVER_TASK_THREADS -DALL_INTERIOR_POINTERS -DENABLE_DISCLAIM -DGC_ATOMIC_UNCOLLECTABLE -DGC_GCJ_SUPPORT -DJAVA_FINALIZATION -DNO_EXECUTE_PERMISSION -DUSE_MUNMAP
CFLAGS=-Iinclude -Ilibatomic_ops\src $(DEFINES) -wx -g
LFLAGS=/ma/implib/co
CC=sc

.c.obj:
	$(CC) -c $(CFLAGS) $*

.cpp.obj:
	$(CC) -c $(CFLAGS) -Aa $*

OBJS=	\
	allchblk.obj\
	alloc.obj\
	blacklst.obj\
	checksums.obj\
	dbg_mlc.obj\
	fnlz_mlc.obj\
	dyn_load.obj\
	finalize.obj\
	gc_cpp.obj\
	gcj_mlc.obj\
	headers.obj\
	mach_dep.obj\
	malloc.obj\
	mallocx.obj\
	mark.obj\
	mark_rts.obj\
	misc.obj\
	new_hblk.obj\
	obj_map.obj\
	os_dep.obj\
	ptr_chck.obj\
	reclaim.obj\
	typd_mlc.obj\
	win32_threads.obj

targets: gc.dll gc.lib gctest.exe test_cpp.exe

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

allchblk.obj: allchblk.c
alloc.obj: alloc.c
blacklst.obj: blacklst.c
checksums.obj: checksums.c
dbg_mlc.obj: dbg_mlc.c
dyn_load.obj: dyn_load.c
finalize.obj: finalize.c
fnlz_mlc.obj: fnlz_mlc.c
gc_cpp.obj: gc_cpp.cc gc_cpp.cpp
headers.obj: headers.c
mach_dep.obj: mach_dep.c
malloc.obj: malloc.c
mallocx.obj: mallocx.c
mark.obj: mark.c
mark_rts.obj: mark_rts.c
misc.obj: misc.c
new_hblk.obj: new_hblk.c
obj_map.obj: obj_map.c
os_dep.obj: os_dep.c
ptr_chck.obj: ptr_chck.c
reclaim.obj: reclaim.c
typd_mlc.obj: typd_mlc.c
win32_threads.obj: win32_threads.c
