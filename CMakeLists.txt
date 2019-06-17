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
option(enable_single_obj_compilation "Compile all libgc source files into single .o" OFF)
option(enable_handle_fork "Attempt to ensure a usable collector after fork()" ON)
option(install_headers "Install header files" ON)

add_definitions("-DALL_INTERIOR_POINTERS -DNO_EXECUTE_PERMISSION")

if (APPLE)
  if ("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;i386"
        CACHE STRING "Build architectures for Mac OS X" FORCE)
  endif()
elseif (MSVC)
  add_definitions("-D_CRT_SECURE_NO_DEPRECATE")
endif()

#LIBATOMIC #TODO
#ADD_LIBRARY(atomic_ops STATIC )
#SET_TARGET_PROPERTIES(atomic_ops PROPERTIES COMPILE_FLAGS -DNO_DEBUGGING)


#LIBGC

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

if (enable_single_obj_compilation)
  set(SRC extra/gc.c) # override SRC
  if (CMAKE_USE_PTHREADS_INIT)
    add_definitions("-DGC_PTHREAD_START_STANDALONE")
    set(SRC ${SRC} pthread_start.c)
  endif(CMAKE_USE_PTHREADS_INIT)
endif()

if (enable_cplusplus)
  set(SRC ${SRC} gc_cpp.cc)
endif()

add_library(gc STATIC ${SRC})
set_target_properties(gc PROPERTIES
                      COMPILE_DEFINITIONS GC_NOT_DLL)
#TODO TARGET_LINK_LIBRARIES(...  ...  ${LIBS})

# TODO if build_cord then add_library(cord ...)

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
  endif(enable_cplusplus)
  if (enable_disclaim)
    install(FILES include/gc_disclaim.h DESTINATION include/gc)
  endif(enable_disclaim)
  # TODO if build_cord then install cord.h, cord_pos.h, ec.h
endif()

if (build_tests)
  add_definitions("-DGC_NOT_DLL")

  if (build_cord)
    add_executable(cordtest cord/cordbscs.c cord/cordprnt.c cord/cordxtra.c
                   cord/tests/cordtest.c)
    target_link_libraries(cordtest PRIVATE gc)
    add_test(NAME cordtest COMMAND cordtest)

    if (WIN32)
      add_executable(de cord/cordbscs.c cord/cordxtra.c
                     cord/tests/de.c cord/tests/de_win.c)
      set_target_properties(de PROPERTIES WIN32_EXECUTABLE TRUE)
      target_link_libraries(de PRIVATE gc gdi32)
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
    add_executable(test_cpp tests/test_cpp.cc)
    target_link_libraries(test_cpp PRIVATE gc)
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
