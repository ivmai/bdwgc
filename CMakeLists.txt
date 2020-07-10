#
# Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
# Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
# Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
# Copyright (c) 2000-2010 by Hewlett-Packard Company.  All rights reserved.
##
# THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
# OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
##
# Permission is hereby granted to use or copy this program
# for any purpose,  provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is granted,
# provided the above notices are retained, and a notice that the code was
# modified is included with the above copyright notice.
##

#
#  get cmake and run:
#    cmake -G "Visual Studio 8 2005"
#  in the same dir as this file
#  this will generate gc.sln
#

cmake_minimum_required(VERSION 3.1)

option(enable_cplusplus "C++ support" OFF)
if (enable_cplusplus)
  project(gc)
else()
  project(gc C)
endif()

include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CTest)

# Customize the build by passing "-D<option_name>=ON|OFF" in the command line.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(build_cord "Build cord library" ON)
option(build_tests "Build tests" OFF)
option(enable_threads "Support threads" ON)
option(enable_parallel_mark "Parallelize marking and free list construction" ON)
option(enable_thread_local_alloc "Turn on thread-local allocation optimization" ON)
option(enable_threads_discovery "Enable threads discovery in GC" ON)
option(enable_throw_bad_alloc_library "Turn on C++ gctba library build" ON)
option(enable_gcj_support "Support for gcj" ON)
option(enable_sigrt_signals "Use SIGRTMIN-based signals for thread suspend/resume" OFF)
option(enable_gc_debug "Support for pointer back-tracing" OFF)
option(disable_gc_debug "Disable debugging like GC_dump and its callees" OFF)
option(enable_java_finalization "Support for java finalization" ON)
option(enable_atomic_uncollectable "Support for atomic uncollectible allocation" ON)
option(enable_redirect_malloc "Redirect malloc and friends to GC routines" OFF)
option(enable_disclaim "Support alternative finalization interface" ON)
option(enable_large_config "Optimize for large heap or root set" OFF)
option(enable_gc_assertions "Enable collector-internal assertion checking" OFF)
option(enable_mmap "Use mmap instead of sbrk to expand the heap" OFF)
option(enable_munmap "Return page to the OS if empty for N collections" ON)
option(enable_dynamic_loading "Enable tracing of dynamic library data roots" ON)
option(enable_register_main_static_data "Perform the initial guess of data root sets" ON)
option(enable_checksums "Report erroneously cleared dirty bits" OFF)
option(enable_werror "Pass -Werror to the C compiler (treat warnings as errors)" OFF)
option(enable_single_obj_compilation "Compile all libgc source files into single .o" OFF)
option(enable_handle_fork "Attempt to ensure a usable collector after fork()" ON)
option(disable_handle_fork "Prohibit installation of pthread_atfork() handlers" OFF)
option(install_headers "Install header files" ON)

add_definitions("-DALL_INTERIOR_POINTERS -DNO_EXECUTE_PERMISSION")

# Set struct packing alignment to word (instead of 1-byte).
if (BORLAND)
  add_compile_options(/a4)
elseif (WATCOM)
  add_compile_options(/zp4)
endif()

# Output all warnings.
if (BORLAND)
  # All warnings except for particular ones.
  add_compile_options(/w /w-pro /w-aus /w-par /w-ccc /w-inl /w-rch)
elseif (MSVC)
  # All warnings but ignoring "unreferenced formal parameter" and
  # "conditional expression is constant" ones.
  add_compile_options(/W4 /wd4100 /wd4127)
  # Disable crt security warnings, since unfortunately they warn about all
  # sorts of safe uses of strncpy.
  add_definitions("-D_CRT_SECURE_NO_DEPRECATE")
elseif (WATCOM)
  add_compile_options(/wx)
else()
  # TODO add -[W]pedantic -Wno-long-long
  add_compile_options(-Wall -Wextra)
endif()

include_directories(include)

set(SRC alloc.c reclaim.c allchblk.c misc.c mach_dep.c os_dep.c
        mark_rts.c headers.c mark.c obj_map.c blacklst.c finalize.c
        new_hblk.c dbg_mlc.c malloc.c dyn_load.c typd_mlc.c ptr_chck.c
        mallocx.c)
set(THREADDLLIBS)

set(_HOST ${CMAKE_SYSTEM_PROCESSOR}-unknown-${CMAKE_SYSTEM})
string(TOLOWER ${_HOST} HOST)
message(STATUS "TARGET = ${HOST}")

if (enable_threads)
  find_package(Threads REQUIRED)
  message(STATUS "Thread library: ${CMAKE_THREAD_LIBS_INIT}")
  include_directories(libatomic_ops/src)
  include_directories(${Threads_INCLUDE_DIR})
  set(THREADDLLIBS ${CMAKE_THREAD_LIBS_INIT})
  if (NOT (APPLE OR CYGWIN OR MSYS OR WIN32 OR HOST MATCHES mips-.*-irix6.*))
    set(THREADDLLIBS ${THREADDLLIBS} -ldl)
                                # The predefined CMAKE_DL_LIBS may be broken.
  endif()
endif(enable_threads)

# Thread support detection.
if (CMAKE_USE_PTHREADS_INIT)
  set(SRC ${SRC} pthread_start.c pthread_support.c pthread_stop_world.c)
  if (HOST MATCHES .*-.*-hpux10.*)
    message(FATAL_ERROR "HP/UX 10 POSIX threads are not supported.")
  endif()
  # Assume the compiler supports C11 (GCC) atomic intrinsics.
  add_definitions("-DGC_BUILTIN_ATOMIC")
  # Common defines for POSIX platforms.
  add_definitions("-DGC_THREADS -D_REENTRANT")
  if (enable_parallel_mark)
    add_definitions("-DPARALLEL_MARK")
  endif()
  if (enable_thread_local_alloc)
    add_definitions("-DTHREAD_LOCAL_ALLOC")
    set(SRC ${SRC} thread_local_alloc.c)
  endif()
  message("Explicit GC_INIT() calls may be required.")
  if (HOST MATCHES .*-.*-hpux11.*)
    message("Only HP/UX 11 POSIX threads are supported.")
    add_definitions("-D_POSIX_C_SOURCE=199506L")
  elseif (HOST MATCHES .*-.*-netbsd.*)
    message("Only on NetBSD 2.0 or later.")
    add_definitions("-D_PTHREADS")
  endif()
  if (ANDROID OR MSYS) # ANDROID variable is defined by CMake v3.7.0+.
    # Android NDK does not provide pthread_atfork.
  elseif (APPLE)
    if (enable_handle_fork AND NOT disable_handle_fork)
      # The incremental mode conflicts with fork handling.
      if (enable_parallel_mark)
        add_definitions("-DHANDLE_FORK")
      endif(enable_parallel_mark)
    endif()
    set(SRC ${SRC} darwin_stop_world.c)
  elseif (enable_handle_fork AND NOT disable_handle_fork)
    add_definitions("-DHANDLE_FORK")
  endif()
  if (enable_sigrt_signals)
    add_definitions("-DGC_USESIGRT_SIGNALS")
  endif()
  if (CYGWIN OR MSYS)
    set(SRC ${SRC} win32_threads.c)
  endif()
elseif (CMAKE_USE_WIN32_THREADS_INIT)
  add_definitions("-DGC_THREADS")
  if (enable_parallel_mark)
    add_definitions("-DPARALLEL_MARK")
  endif()
  if (enable_thread_local_alloc AND (enable_parallel_mark OR NOT BUILD_SHARED_LIBS))
    # Imply THREAD_LOCAL_ALLOC unless GC_DLL.
    add_definitions("-DTHREAD_LOCAL_ALLOC")
    set(SRC ${SRC} thread_local_alloc.c)
  endif()
  add_definitions("-DEMPTY_GETENV_RESULTS")
  set(SRC ${SRC} win32_threads.c)
elseif (CMAKE_HP_PTHREADS_INIT OR CMAKE_USE_SPROC_INIT)
  message(FATAL_ERROR "Unsupported thread package")
endif()

if (disable_handle_fork)
  add_definitions("-DNO_HANDLE_FORK")
endif()

if (enable_gcj_support)
  add_definitions("-DGC_GCJ_SUPPORT")
  if (enable_threads AND NOT (enable_thread_local_alloc AND HOST MATCHES .*-.*-kfreebsd.*-gnu))
    # FIXME: For a reason, gctest hangs up on kFreeBSD if both of
    # THREAD_LOCAL_ALLOC and GC_ENABLE_SUSPEND_THREAD are defined.
    add_definitions("-DGC_ENABLE_SUSPEND_THREAD")
  endif()
  set(SRC ${SRC} gcj_mlc.c)
endif(enable_gcj_support)

if (enable_disclaim)
  add_definitions("-DENABLE_DISCLAIM")
  set(SRC ${SRC} fnlz_mlc.c)
endif()

if (enable_java_finalization)
  add_definitions("-DJAVA_FINALIZATION")
endif()

if (enable_atomic_uncollectable)
  add_definitions("-DGC_ATOMIC_UNCOLLECTABLE")
endif()

if (enable_gc_debug)
  add_definitions("-DDBG_HDRS_ALL -DKEEP_BACK_PTRS")
  if (HOST MATCHES i.86-.*-dgux.*|ia64-.*-linux.*|i586-.*-linux.*|i686-.*-linux.*|x86-.*-linux.*|x86_64-.*-linux.*)
    add_definitions("-DMAKE_BACK_GRAPH")
    if (HOST MATCHES .*-.*-.*linux.*)
      add_definitions("-DSAVE_CALL_COUNT=8")
    endif()
    set(SRC ${SRC} backgraph.c)
  endif()
endif(enable_gc_debug)

if (disable_gc_debug)
  add_definitions("-DNO_DEBUGGING")
elseif (WINCE)
  # Read environment variables from "<program>.gc.env" file.
  add_definitions("-DGC_READ_ENV_FILE")
endif()

if (enable_redirect_malloc)
  if (enable_gc_debug)
    add_definitions("-DREDIRECT_MALLOC=GC_debug_malloc_replacement")
    add_definitions("-DREDIRECT_REALLOC=GC_debug_realloc_replacement")
    add_definitions("-DREDIRECT_FREE=GC_debug_free")
  else()
    add_definitions("-DREDIRECT_MALLOC=GC_malloc")
  endif()
  add_definitions("-DGC_USE_DLOPEN_WRAP")
endif(enable_redirect_malloc)

if (enable_munmap)
  add_definitions("-DUSE_MMAP -DUSE_MUNMAP")
elseif (enable_mmap)
  add_definitions("-DUSE_MMAP")
endif()

if (NOT enable_dynamic_loading)
  add_definitions("-DIGNORE_DYNAMIC_LOADING")
endif()

if (NOT enable_register_main_static_data)
  add_definitions("-DGC_DONT_REGISTER_MAIN_STATIC_DATA")
endif()

if (enable_large_config)
  add_definitions("-DLARGE_CONFIG")
endif()

if (enable_gc_assertions)
  add_definitions("-DGC_ASSERTIONS")
endif()

if (NOT enable_threads_discovery)
  add_definitions("-DGC_NO_THREADS_DISCOVERY")
endif()

if (enable_checksums)
  if (enable_munmap OR enable_threads)
    message(FATAL_ERROR "CHECKSUMS not compatible with USE_MUNMAP or threads")
  endif()
  add_definitions("-DCHECKSUMS")
  set(SRC ${SRC} checksums.c)
endif(enable_checksums)

if (enable_werror)
  if (BORLAND)
    add_compile_options(/w!)
  elseif (MSVC)
    add_compile_options(/WX)
    # Workaround "typedef ignored on left of ..." warning reported in
    # imagehlp.h of e.g. Windows Kit 8.1.
    add_compile_options(/wd4091)
  elseif (WATCOM)
    add_compile_options(/we)
  else()
    add_compile_options(-Werror)
    if (APPLE)
      # _dyld_bind_fully_image_containing_address is deprecated in OS X 10.5+
      add_compile_options(-Wno-deprecated-declarations)
    endif()
  endif()
endif(enable_werror)

if (enable_single_obj_compilation OR BUILD_SHARED_LIBS)
  set(SRC extra/gc.c) # override SRC
  if (CMAKE_USE_PTHREADS_INIT)
    add_definitions("-DGC_PTHREAD_START_STANDALONE")
    set(SRC ${SRC} pthread_start.c)
  endif(CMAKE_USE_PTHREADS_INIT)
elseif (BORLAND)
  # Suppress "GC_push_contents_hdr() is declared but never used" warning.
  add_compile_options(/w-use)
endif()

# Add implementation of backtrace() and backtrace_symbols().
if (MSVC)
  set(SRC ${SRC} extra/msvc_dbg.c)
endif()

# Instruct check_c_source_compiles and similar CMake checks not to ignore
# compiler warnings (like "implicit declaration of function").
if (NOT BORLAND AND NOT MSVC AND NOT WATCOM)
  check_c_compiler_flag(-Werror HAVE_FLAG_WERROR)
  if (HAVE_FLAG_WERROR)
    SET(CMAKE_REQUIRED_FLAGS "-Werror")
  endif(HAVE_FLAG_WERROR)
endif()

if (BUILD_SHARED_LIBS)
  add_definitions("-DGC_DLL")
  # Pass -fvisibility=hidden option if supported.
  check_c_compiler_flag(-fvisibility=hidden HAVE_FLAG_F_VISIBILITY_HIDDEN)
  if (HAVE_FLAG_F_VISIBILITY_HIDDEN)
    add_definitions("-DGC_VISIBILITY_HIDDEN_SET")
    add_compile_options(-fvisibility=hidden)
  else()
    add_definitions("-DGC_NO_VISIBILITY")
  endif()
  check_c_compiler_flag(-Wl,--no-undefined HAVE_FLAG_WL_NO_UNDEFINED)
else()
  add_definitions("-DGC_NOT_DLL")
  if (WIN32)
    # Do not require the clients to link with "user32" system library.
    add_definitions("-DDONT_USE_USER32_DLL")
  endif(WIN32)
endif()

# Disable strict aliasing optimizations.
# It could re-enabled back by a flag passed in CFLAGS_EXTRA.
check_c_compiler_flag(-fno-strict-aliasing HAVE_FLAG_F_NO_STRICT_ALIASING)
if (HAVE_FLAG_F_NO_STRICT_ALIASING)
  add_compile_options(-fno-strict-aliasing)
endif()

# Extra user-defined flags to pass both to C and C++ compilers.
if (DEFINED CFLAGS_EXTRA)
  add_compile_options(${CFLAGS_EXTRA})
endif()

# Check whether execinfo.h header file is present.
check_include_file(execinfo.h HAVE_EXECINFO_H)
if (NOT HAVE_EXECINFO_H)
  add_definitions("-DGC_MISSING_EXECINFO_H")
endif()

# Check for getcontext (uClibc can be configured without it, for example).
check_function_exists(getcontext HAVE_GETCONTEXT)
if (NOT HAVE_GETCONTEXT)
  add_definitions("-DNO_GETCONTEXT")
endif()

# Check whether dl_iterate_phdr exists (as a strong symbol).
check_function_exists(dl_iterate_phdr HAVE_DL_ITERATE_PHDR)
if (HAVE_DL_ITERATE_PHDR)
  add_definitions("-DHAVE_DL_ITERATE_PHDR")
endif()

check_symbol_exists(sigsetjmp setjmp.h HAVE_SIGSETJMP)
if (NOT HAVE_SIGSETJMP)
  add_definitions("-DGC_NO_SIGSETJMP")
endif()

# pthread_setname_np, if available, may have 1, 2 or 3 arguments.
if (CMAKE_USE_PTHREADS_INIT)
  check_c_source_compiles("
#include <pthread.h>\n
int main(void) { (void)pthread_setname_np(\"thread-name\"); return 0; }"
    HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID)
  if (HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID)
    # Define to use 'pthread_setname_np(const char*)' function.
    add_definitions("-DHAVE_PTHREAD_SETNAME_NP_WITHOUT_TID")
  else()
    check_c_source_compiles("
#include <pthread.h>\n
int main(void) {\n
  (void)pthread_setname_np(pthread_self(), \"thread-name-%u\", 0); return 0; }"
      HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG)
    if (HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG)
      # Define to use 'pthread_setname_np(pthread_t, const char*, void *)'.
      add_definitions("-DHAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG")
    else()
      check_c_source_compiles("
#if defined(__linux__) || defined(__GLIBC__) || defined(__GNU__) || defined(__CYGWIN__)\n
#define _GNU_SOURCE 1\n
#endif\n
#include <pthread.h>\n
int main(void) {\n
  (void)pthread_setname_np(pthread_self(), \"thread-name\"); return 0; }"
        HAVE_PTHREAD_SETNAME_NP_WITH_TID)
      if (HAVE_PTHREAD_SETNAME_NP_WITH_TID)
        # Define to use 'pthread_setname_np(pthread_t, const char*)' function.
        add_definitions("-DHAVE_PTHREAD_SETNAME_NP_WITH_TID")
      endif()
    endif(HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG)
  endif (HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID)
endif()

# Check for dladdr (used for debugging).
check_c_source_compiles("
#define _GNU_SOURCE 1\n
#include <dlfcn.h>\n
int main(void) { Dl_info info; (void)dladdr(\"\", &info); return 0; }"
  HAVE_DLADDR)
if (HAVE_DLADDR)
  # Define to use 'dladdr' function.
  add_definitions("-DHAVE_DLADDR")
endif()

add_library(gc ${SRC})
if (enable_threads)
  target_link_libraries(gc PRIVATE ${THREADDLLIBS})
endif()

if (enable_cplusplus)
  add_library(gccpp gc_badalc.cc gc_cpp.cc)
  target_link_libraries(gccpp PRIVATE gc)
  if (enable_throw_bad_alloc_library)
    # The same as gccpp but contains only gc_badalc.
    add_library(gctba gc_badalc.cc)
    target_link_libraries(gctba PRIVATE gc)
  endif(enable_throw_bad_alloc_library)
endif()

if (build_cord)
  set(CORD_SRC cord/cordbscs.c cord/cordprnt.c cord/cordxtra.c)
  add_library(cord ${CORD_SRC})
  target_link_libraries(cord PRIVATE gc)
  install(TARGETS cord EXPORT cordExports
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
          RUNTIME DESTINATION bin
          INCLUDES DESTINATION include)
endif()

if (BUILD_SHARED_LIBS AND HAVE_FLAG_WL_NO_UNDEFINED)
  # Declare that the libraries do not refer to external symbols.
  # TODO: use add_link_options() when cmake_minimum_required > 3.13
  target_link_libraries(gc PRIVATE -Wl,--no-undefined)
  if (enable_cplusplus)
    target_link_libraries(gccpp PRIVATE -Wl,--no-undefined)
    if (enable_throw_bad_alloc_library)
      target_link_libraries(gctba PRIVATE -Wl,--no-undefined)
    endif(enable_throw_bad_alloc_library)
  endif(enable_cplusplus)
  if (build_cord)
    target_link_libraries(cord PRIVATE -Wl,--no-undefined)
  endif(build_cord)
endif()

install(TARGETS gc EXPORT gcExports
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

if (enable_cplusplus)
  install(TARGETS gccpp EXPORT gccppExports
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
          RUNTIME DESTINATION bin
          INCLUDES DESTINATION include)
endif()

if (install_headers)
  install(FILES include/gc.h
                include/gc_backptr.h
                include/gc_config_macros.h
                include/gc_gcj.h
                include/gc_inline.h
                include/gc_mark.h
                include/gc_pthread_redirects.h
                include/gc_tiny_fl.h
                include/gc_typed.h
                include/gc_version.h
                include/javaxfc.h
                include/leak_detector.h
          DESTINATION include/gc)
  install(FILES include/extra/gc.h DESTINATION include)
  if (enable_cplusplus)
    install(FILES include/gc_allocator.h
                  include/gc_cpp.h
            DESTINATION include/gc)
    install(FILES include/extra/gc_cpp.h DESTINATION include)
  endif()
  if (enable_disclaim)
    install(FILES include/gc_disclaim.h DESTINATION include/gc)
  endif()
  if (build_cord)
    install(FILES include/cord.h
                  include/cord_pos.h
                  include/ec.h
            DESTINATION include/gc)
  endif()
endif(install_headers)

if (build_tests)
  if (build_cord)
    add_executable(cordtest cord/tests/cordtest.c)
    target_link_libraries(cordtest PRIVATE cord gc)
    add_test(NAME cordtest COMMAND cordtest)

    if (WIN32 AND NOT CYGWIN)
      add_executable(de cord/tests/de.c cord/tests/de_win.c
                     cord/tests/de_win.rc)
      set_target_properties(de PROPERTIES WIN32_EXECUTABLE TRUE)
      target_link_libraries(de PRIVATE cord gc gdi32)
    endif()
  endif(build_cord)

  # Compile some tests as C++ to test extern "C" in header files.
  if (enable_cplusplus)
    set_source_files_properties(tests/leak_test.c tests/test.c
                                PROPERTIES LANGUAGE CXX)
    # To avoid "treating 'c' input as 'c++' when in C++ mode" Clang warning.
    if (NOT (BORLAND OR MSVC OR WATCOM))
      add_compile_options(-x c++)
    endif()
  endif(enable_cplusplus)

  add_executable(gctest WIN32 tests/test.c)
  target_link_libraries(gctest PRIVATE gc ${THREADDLLIBS})
  add_test(NAME gctest COMMAND gctest)
  if (WATCOM)
    # Suppress "conditional expression in if statement is always true/false"
    # and "unreachable code" warnings in GC_MALLOC_[ATOMIC_]WORDS.
    target_compile_options(gctest PRIVATE
                           /wcd=13 /wcd=201 /wcd=367 /wcd=368 /wcd=726)
  endif()

  add_executable(hugetest tests/huge_test.c)
  target_link_libraries(hugetest PRIVATE gc)
  add_test(NAME hugetest COMMAND hugetest)

  add_executable(leaktest tests/leak_test.c)
  target_link_libraries(leaktest PRIVATE gc)
  add_test(NAME leaktest COMMAND leaktest)

  add_executable(middletest tests/middle.c)
  target_link_libraries(middletest PRIVATE gc)
  add_test(NAME middletest COMMAND middletest)

  add_executable(realloc_test tests/realloc_test.c)
  target_link_libraries(realloc_test PRIVATE gc)
  add_test(NAME realloc_test COMMAND realloc_test)

  add_executable(smashtest tests/smash_test.c)
  target_link_libraries(smashtest PRIVATE gc)
  add_test(NAME smashtest COMMAND smashtest)

  if (NOT (BUILD_SHARED_LIBS AND WIN32))
    add_library(staticrootslib_test tests/staticrootslib.c)
    target_link_libraries(staticrootslib_test PRIVATE gc)
    add_library(staticrootslib2_test tests/staticrootslib.c)
    target_compile_options(staticrootslib2_test PRIVATE "-DSTATICROOTSLIB2")
    target_link_libraries(staticrootslib2_test PRIVATE gc)
    add_executable(staticrootstest tests/staticrootstest.c)
    target_compile_options(staticrootstest PRIVATE "-DSTATICROOTSLIB2")
    target_link_libraries(staticrootstest PRIVATE
                          gc staticrootslib_test staticrootslib2_test)
    add_test(NAME staticrootstest COMMAND staticrootstest)
  endif()

  if (enable_gc_debug)
    add_executable(tracetest tests/trace_test.c)
    target_link_libraries(tracetest PRIVATE gc)
    add_test(NAME tracetest COMMAND tracetest)
  endif()

  if (enable_threads)
    add_executable(test_atomic_ops tests/test_atomic_ops.c)
    target_link_libraries(test_atomic_ops PRIVATE gc)
    add_test(NAME test_atomic_ops COMMAND test_atomic_ops)

    add_executable(threadleaktest tests/thread_leak_test.c)
    target_link_libraries(threadleaktest PRIVATE gc ${THREADDLLIBS})
    add_test(NAME threadleaktest COMMAND threadleaktest)

    if (NOT WIN32)
      add_executable(threadkey_test tests/threadkey_test.c)
      target_link_libraries(threadkey_test PRIVATE gc ${THREADDLLIBS})
      add_test(NAME threadkey_test COMMAND threadkey_test)
    endif()

    add_executable(subthreadcreate_test tests/subthread_create.c)
    target_link_libraries(subthreadcreate_test PRIVATE gc ${THREADDLLIBS})
    add_test(NAME subthreadcreate_test COMMAND subthreadcreate_test)

    add_executable(initsecondarythread_test tests/initsecondarythread.c)
    target_link_libraries(initsecondarythread_test PRIVATE gc ${THREADDLLIBS})
    add_test(NAME initsecondarythread_test COMMAND initsecondarythread_test)
  endif(enable_threads)

  if (enable_cplusplus)
    add_executable(test_cpp WIN32 tests/test_cpp.cc)
    target_link_libraries(test_cpp PRIVATE gc gccpp)
    add_test(NAME test_cpp COMMAND test_cpp)
  endif()

  if (enable_disclaim)
    add_executable(disclaim_bench tests/disclaim_bench.c)
    target_link_libraries(disclaim_bench PRIVATE gc)
    add_test(NAME disclaim_bench COMMAND disclaim_bench)

    add_executable(disclaim_test tests/disclaim_test.c)
    target_link_libraries(disclaim_test PRIVATE gc ${THREADDLLIBS})
    add_test(NAME disclaim_test COMMAND disclaim_test)

    add_executable(disclaim_weakmap_test tests/disclaim_weakmap_test.c)
    target_link_libraries(disclaim_weakmap_test PRIVATE gc ${THREADDLLIBS})
    add_test(NAME disclaim_weakmap_test COMMAND disclaim_weakmap_test)
  endif()
endif(build_tests)
