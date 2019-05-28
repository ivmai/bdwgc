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

set(CMAKE_LEGACY_CYGWIN_WIN32 0) # Remove when CMake >= 2.8.4 is required

project(gc)

include(CTest)

cmake_minimum_required(VERSION 2.6)

option(BDWGC_INSTALL_CORD "Install cord" ON)
option(BDWGC_SKIP_HEADERS "Skip installing header files" OFF)
option(BDWGC_SKIP_TESTS "Skip building tests" OFF)

if (WIN32)
  add_definitions("-D_CRT_SECURE_NO_DEPRECATE")
endif()

add_definitions("-DALL_INTERIOR_POINTERS -DNO_EXECUTE_PERMISSION")

if (APPLE)
  if ("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    set(CMAKE_OSX_ARCHITECTURES "ppc;i386;x86_64"
        CACHE STRING "Build architectures for Mac OS X" FORCE)
  endif()
endif(APPLE)

set(SRC alloc.c reclaim.c allchblk.c misc.c mach_dep.c os_dep.c
        mark_rts.c headers.c mark.c obj_map.c blacklst.c finalize.c
        new_hblk.c dbg_mlc.c malloc.c dyn_load.c typd_mlc.c ptr_chck.c
        mallocx.c)
set(LIBS)

option(enable_threads "TODO" OFF)
if (enable_threads)
  find_package(Threads REQUIRED)
  message("Thread Model: ${CMAKE_THREAD_LIBS_INIT}" )
  include_directories(${Threads_INCLUDE_DIR})
  set(LIBS ${LIBS} ${Threads_LIBRARIES})
endif()

option(enable_handle_fork "Attempt to ensure a usable collector after fork()" ON)
option(enable_thread_local_alloc "Turn on thread-local allocation optimization" ON)
option(enable_parallel_mark "Parallelize marking and free list construction" ON)

#IF(Threads_FOUND)
#       ADD_DEFINITIONS("")
#ELSE
#       MESSAGE("Parallel mark requires enable_threads ON" )
#ENDIF(Threads_FOUND)

#OPTION(enable_cplusplus "install C++ support" ON)
set(SRC ${SRC} gc_cpp.cc)

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

option(enable_gcj_support "Support for gcj" ON)
if (enable_gcj_support)
  add_definitions("-DGC_GCJ_SUPPORT")
  if (enable_threads)
    add_definitions("-DGC_ENABLE_SUSPEND_THREAD")
  endif()
  set(SRC ${SRC} gcj_mlc.c)
endif(enable_gcj_support)

option(enable_disclaim "Support alternative finalization interface" ON)
if (enable_disclaim)
  add_definitions("-DENABLE_DISCLAIM")
  set(SRC ${SRC} fnlz_mlc.c)
endif()

option(enable_java_finalization "Support for java finalization" ON)
if (enable_java_finalization)
  add_definitions("-DJAVA_FINALIZATION")
endif()

option(enable_atomic_uncollectable "Support for atomic uncollectible allocation" ON)
if (enable_atomic_uncollectable)
  add_definitions("-DGC_ATOMIC_UNCOLLECTABLE")
endif()

option(enable_gc_debug "Support for pointer back-tracing" OFF)
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

option(enable_redirect_malloc "Redirect malloc and friends to GC routines" OFF)
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

option(enable_mmap "Use mmap instead of sbrk to expand the heap" OFF)

option(enable_munmap "Return page to the OS if empty for N collections" ON)
if (enable_munmap)
  add_definitions("-DUSE_MMAP -DUSE_MUNMAP")
elseif (enable_mmap)
  add_definitions("-DUSE_MMAP")
endif()

option(enable_dynamic_loading "Enable tracing of dynamic library data roots" ON)
if (NOT enable_dynamic_loading)
  add_definitions("-DIGNORE_DYNAMIC_LOADING")
endif()

option(enable_register_main_static_data "Perform the initial guess of data root sets" ON)
if (NOT enable_register_main_static_data)
  add_definitions("-DGC_DONT_REGISTER_MAIN_STATIC_DATA")
endif()

option(enable_large_config "Optimize for large heap or root set" OFF)
if (enable_large_config)
  add_definitions("-DLARGE_CONFIG")
endif()

option(enable_gc_assertions "Enable collector-internal assertion checking" OFF)
if (enable_gc_assertions)
  add_definitions("-DGC_ASSERTIONS")
endif()

option(enable_threads_discovery "Enable threads discovery in GC" ON)
if (NOT enable_threads_discovery)
  add_definitions("-DGC_NO_THREADS_DISCOVERY")
endif()

option(enable_checksums "Report erroneously cleared dirty bits" OFF)
if (enable_checksums)
  if (enable_munmap OR enable_threads)
    message("CHECKSUMS not compatible with USE_MUNMAP or threads")
  endif()
  add_definitions("-DCHECKSUMS")
  set(SRC ${SRC} checksums.c)
endif(enable_checksums)

# Create target
add_library(gc ${SRC})
target_include_directories(gc  
    PUBLIC     
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions(gc PRIVATE GC_NOT_DLL)
else()
    target_compile_definitions(gc PRIVATE GC_DLL)
endif()

# Install library
install(
    TARGETS gc
    LIBRARY DESTINATION lib 
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin 
    INCLUDES DESTINATION include
)

# Install headers
if (NOT BDWGC_SKIP_HEADERS)
    file(GLOB BDWGC_HEADERS "include/*.h")
    install(FILES ${BDWGC_HEADERS} DESTINATION include/gc)

    file(GLOB BDWGC_EXTRA_HEADERS "include/extra/*.h")
    install(FILES ${BDWGC_EXTRA_HEADERS} DESTINATION include)
endif()

# Install tools
if (WIN32 AND BDWGC_INSTALL_CORD)
    add_executable(cord 
                   cord/cordbscs.c 
                   cord/cordxtra.c
                   cord/tests/de.c
                   cord/tests/de_win.c)
    set_target_properties(cord PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(cord PRIVATE GC_NOT_DLL)

    target_link_libraries(cord PRIVATE gc gdi32)
    target_include_directories(cord
        PRIVATE      
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    install(
        TARGETS cord 
        DESTINATION tools/cord 
    )
endif()

if (NOT BDWGC_SKIP_TESTS)
  add_subdirectory(tests)
endif()
