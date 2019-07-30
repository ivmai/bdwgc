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

option(enable_cplusplus "C++ support" OFF)
if (enable_cplusplus)
  project(gc)
else()
  project(gc C)
endif()

include(CTest)

cmake_minimum_required(VERSION 3.1)

# Customize the build by passing "-D<option_name>=ON|OFF" in the command line.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(build_cord "Build cord library" ON)
option(build_tests "Build tests" OFF)
option(enable_threads "TODO" OFF) #TODO Support it
option(enable_parallel_mark "Parallelize marking and free list construction" ON)
option(enable_thread_local_alloc "Turn on thread-local allocation optimization" ON)
option(enable_threads_discovery "Enable threads discovery in GC" ON)
option(enable_gcj_support "Support for gcj" ON)
option(enable_sigrt_signals "Use SIGRTMIN-based signals for thread suspend/resume" OFF)
option(enable_gc_debug "Support for pointer back-tracing" OFF)
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
option(install_headers "Install header files" ON)

add_definitions("-DALL_INTERIOR_POINTERS -DNO_EXECUTE_PERMISSION")

if (APPLE AND ("${CMAKE_OSX_ARCHITECTURES}" STREQUAL ""))
    set(CMAKE_OSX_ARCHITECTURES "x86_64;i386"
        CACHE STRING "Build architectures for Mac OS X" FORCE)
endif()

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
include_directories(libatomic_ops/src)

set(SRC alloc.c reclaim.c allchblk.c misc.c mach_dep.c os_dep.c
        mark_rts.c headers.c mark.c obj_map.c blacklst.c finalize.c
        new_hblk.c dbg_mlc.c malloc.c dyn_load.c typd_mlc.c ptr_chck.c
        mallocx.c)
set(LIBS)

if (enable_threads)
  find_package(Threads REQUIRED)
  message("Thread Model: ${CMAKE_THREAD_LIBS_INIT}" )
  include_directories(${Threads_INCLUDE_DIR})
  set(LIBS ${LIBS} ${Threads_LIBRARIES})
endif()

#IF(Threads_FOUND)
#       ADD_DEFINITIONS("")
#ELSE
#       MESSAGE("Parallel mark requires enable_threads ON" )
#ENDIF(Threads_FOUND)

set(_HOST ${CMAKE_HOST_SYSTEM_PROCESSOR}--${CMAKE_SYSTEM})
                                #FIXME missing the vendor field.
string(TOLOWER ${_HOST} HOST)
message("HOST = ${HOST}")

# Thread Detection.  Relying on cmake for lib and includes.
#TODO check cmake detection
if (CMAKE_USE_PTHREADS_INIT)
  set(SRC ${SRC} pthread_start.c pthread_support.c pthread_stop_world.c)
  # Common defines for most POSIX platforms.
  if (HOST MATCHES .*-.*-aix.*|.*-.*-android.*|.*-.*-cygwin.*|.*-.*-darwin.*|.*-.*-.*freebsd.*|.*-.*-haiku.*|.*-.*-gnu.*|.*-.*-hpux11.*|.*-.*-irix.*|.*-.*-.*linux.*|.*-.*-msys.*|.*-.*-nacl.*|.*-.*-netbsd.*|.*-.*-openbsd.*|.*-.*-osf.*|.*-.*-solaris.*)
    add_definitions("-DGC_THREADS -D_REENTRANT")
    if (enable_parallel_mark)
      add_definitions("-DPARALLEL_MARK")
    endif()
    if (enable_thread_local_alloc)
      add_definitions("-DTHREAD_LOCAL_ALLOC")
      set(SRC ${SRC} thread_local_alloc.c)
    endif()
    message("Explicit GC_INIT() calls may be required.")
  endif()
  if (HOST MATCHES .*-.*-hpux11.*)
    message("Only HP/UX 11 POSIX threads are supported.")
    add_definitions("-D_POSIX_C_SOURCE=199506L")
        #TODO test -DVAR=value. Alternative is COMPILE_DEFINITIONS property
  endif()
  if (HOST MATCHES .*-.*-hpux10.*)
    message("HP/UX 10 POSIX threads are not supported.")
  endif()
  if (HOST MATCHES .*-.*-netbsd.*)
    message("Only on NetBSD 2.0 or later.")
    add_definitions("-D_PTHREADS")
  endif()
  if (HOST MATCHES .*-.*-android.*)
    # Android NDK does not provide pthread_atfork.
  elseif (HOST MATCHES .*-.*-aix.*|.*-.*-cygwin.*|.*-.*-freebsd.*|.*-.*-haiku.*|.*-.*-hpux11.*|.*-.*-irix.*|.*-.*-kfreebsd.*-gnu|.*-.*-.*linux.*|.*-.*-netbsd.*|.*-.*-openbsd.*|.*-.*-osf.*|.*-.*-solaris.*)
    if (enable_handle_fork)
      add_definitions("-DHANDLE_FORK")
    endif(enable_handle_fork)
  endif()
  if (HOST MATCHES .*-.*-cygwin.*|.*-.*-msys.*)
    set(SRC ${SRC} win32_threads.c)
  endif()
  if (HOST MATCHES .*-.*-darwin.*)
    if (enable_handle_fork)
      # The incremental mode conflicts with fork handling.
      if (enable_parallel_mark)
        add_definitions("-DHANDLE_FORK")
      endif()
    endif(enable_handle_fork)
    set(SRC ${SRC} darwin_stop_world.c)
    #TODO darwin_threads=true
  endif()
  if (enable_sigrt_signals)
    add_definitions("-DGC_USESIGRT_SIGNALS")
  endif()
endif(CMAKE_USE_PTHREADS_INIT)

if (CMAKE_USE_WIN32_THREADS_INIT)
  add_definitions("-DGC_THREADS")
  if (enable_parallel_mark)
    add_definitions("-DPARALLEL_MARK")
    if (enable_thread_local_alloc)
      add_definitions("-DTHREAD_LOCAL_ALLOC")
      set(SRC ${SRC} thread_local_alloc.c)
    endif()
  endif(enable_parallel_mark)
  add_definitions("-DEMPTY_GETENV_RESULTS")
  set(SRC ${SRC} win32_threads.c)
endif(CMAKE_USE_WIN32_THREADS_INIT)

if (enable_gcj_support)
  add_definitions("-DGC_GCJ_SUPPORT")
  if (enable_threads)
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
  if (HOST MATCHES ia64-.*-linux.*|i586-.*-linux.*|i686-.*-linux.*|x86-.*-linux.*|x86_64-.*-linux.*)
    add_definitions("-DMAKE_BACK_GRAPH")
    add_definitions("-DSAVE_CALL_COUNT=8")
    set(SRC ${SRC} backgraph.c)
  endif()
  if (HOST MATCHES i.86-.*-dgux.*)
    add_definitions("-DMAKE_BACK_GRAPH")
    set(SRC ${SRC} backgraph.c)
  endif()
endif(enable_gc_debug)

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
    message("CHECKSUMS not compatible with USE_MUNMAP or threads")
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

if (BUILD_SHARED_LIBS)
  add_definitions("-DGC_DLL")
else()
  add_definitions("-DGC_NOT_DLL")
  if (WIN32)
    # Do not require the clients to link with "user32" system library.
    add_definitions("-DDONT_USE_USER32_DLL")
  endif(WIN32)
endif()

# Extra user-defined flags to pass both to C and C++ compilers.
if (DEFINED CFLAGS_EXTRA)
  add_compile_options(${CFLAGS_EXTRA})
endif()

add_library(gc ${SRC})

if (enable_cplusplus)
  add_library(gccpp gc_cpp.cc)
  target_link_libraries(gccpp PRIVATE gc)
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

    if (WIN32)
      add_executable(de cord/tests/de.c cord/tests/de_win.c cord/tests/de_win.rc)
      set_target_properties(de PROPERTIES WIN32_EXECUTABLE TRUE)
      target_link_libraries(de PRIVATE cord gc gdi32)
    endif()
  endif(build_cord)

  # Compile some tests as C++ to test extern "C" in header files.
  if (enable_cplusplus)
    set_source_files_properties(tests/leak_test.c tests/test.c
                                PROPERTIES LANGUAGE CXX)
  endif()

  add_executable(gctest WIN32 tests/test.c)
  target_link_libraries(gctest PRIVATE gc)
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

  if (enable_gc_debug)
    add_executable(tracetest tests/trace_test.c)
    target_link_libraries(tracetest PRIVATE gc)
    add_test(NAME tracetest COMMAND tracetest)
  endif()

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
    target_link_libraries(disclaim_test PRIVATE gc)
    add_test(NAME disclaim_test COMMAND disclaim_test)

    add_executable(disclaim_weakmap_test tests/disclaim_weakmap_test.c)
    target_link_libraries(disclaim_weakmap_test PRIVATE gc)
    add_test(NAME disclaim_weakmap_test COMMAND disclaim_weakmap_test)
  endif()
endif(build_tests)
