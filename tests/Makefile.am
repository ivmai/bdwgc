AM_CPPFLAGS=-I$(top_builddir)/include -I$(top_builddir)/include/private

if CPLUSPLUS
extra_checks = test_cpp
else
extra_checks = 
endif

## FIXME: trace_test don't works on macosx 10.3 
## gcc -g -O2 -o .libs/tracetest trace_test.o  ../.libs/libgc.dylib -lpthread
## ld: Undefined symbols:
## _GC_generate_random_backtrace

check_PROGRAMS = gctest leaktest middletest threadleaktest $(extra_checks)

gctest_SOURCES = test.c
gctest_LDADD = $(top_builddir)/libgc.la $(THREADLIBS) $(UNWINDLIBS) $(EXTRA_TEST_LIBS)
gctest_DEPENDENCIES = $(top_builddir)/libgc.la

leaktest_SOURCES = leak_test.c
leaktest_LDADD = $(top_builddir)/libgc.la $(THREADLIBS) $(UNWINDLIBS) $(EXTRA_TEST_LIBS)

middletest_SOURCES = middle.c
middletest_LDADD = $(top_builddir)/libgc.la $(THREADLIBS) $(UNWINDLIBS) $(EXTRA_TEST_LIBS)

## tracetest_SOURCES = trace_test.c
## tracetest_LDADD = $(top_builddir)/libgc.la $(THREADLIBS) $(UNWINDLIBS) $(EXTRA_TEST_LIBS)

threadleaktest_SOURCES = thread_leak_test.c
threadleaktest_LDADD = $(top_builddir)/libgc.la $(THREADLIBS) $(UNWINDLIBS) $(EXTRA_TEST_LIBS)

if CPLUSPLUS
test_cpp_SOURCES = test_cpp.cc
test_cpp_LDADD = $(top_builddir)/libgc.la $(top_builddir)/libgccpp.la $(THREADLIBS) $(UNWINDLIBS) $(EXTRA_TEST_LIBS)
endif

TESTS = gctest leaktest middletest threadleaktest $(extra_checks)
