#
# Copyright (c) 1994 by Xerox Corporation.  All rights reserved.
# Copyright (c) 1996 by Silicon Graphics.  All rights reserved.
# Copyright (c) 1998 by Fergus Henderson.  All rights reserved.
# Copyright (c) 2000-2010 by Hewlett-Packard Company.  All rights reserved.
# Copyright (c) 2010-2022 Ivan Maidanski
##
# THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
# OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
##
# Permission is hereby granted to use or copy this program
# for any purpose, provided the above notices are retained on all copies.
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

set(PACKAGE_VERSION 8.3.0)
# Version must match that in AC_INIT of configure.ac and that in README.
# Version must conform to: [0-9]+[.][0-9]+[.][0-9]+

# Info (current:revision:age) for the Libtool versioning system.
# These values should match those in cord/cord.am and Makefile.am.
set(LIBCORD_VER_INFO    6:0:5)
set(LIBGC_VER_INFO      6:1:5)
set(LIBGCCPP_VER_INFO   6:0:5)

option(enable_cplusplus "C++ support" OFF)
if (enable_cplusplus)
  project(gc)
else()
  project(gc C)
endif()

if (POLICY CMP0057)
  # Required for CheckLinkerFlag, at least.
  cmake_policy(SET CMP0057 NEW)
endif()

include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CMakePackageConfigHelpers)
include(CTest)
include(GNUInstallDirs)

if (NOT (${CMAKE_VERSION} VERSION_LESS "3.18.0"))
  include(CheckLinkerFlag)
endif()

set(default_enable_threads ON)
find_package(Threads QUIET)
if (NOT (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT) OR EMSCRIPTEN OR WASI)
  set(default_enable_threads OFF)
endif()

# Customize the build by passing "-D<option_name>=ON|OFF" in the command line.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(build_cord "Build cord library" ON)
option(build_tests "Build tests" OFF)
option(enable_docs "Build and install documentation" ON)
option(enable_threads "Support threads" ${default_enable_threads})
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
option(disable_single_obj_compilation "Compile each libgc source file independently" OFF)
option(enable_handle_fork "Attempt to ensure a usable collector after fork()" ON)
option(disable_handle_fork "Prohibit installation of pthread_atfork() handlers" OFF)
option(enable_emscripten_asyncify "Use Emscripten asyncify feature" OFF)
option(install_headers "Install header and pkg-config metadata files" ON)
option(with_libatomic_ops "Use an external libatomic_ops" OFF)
option(without_libatomic_ops "Use atomic_ops.h in libatomic_ops/src" OFF)

# Override the default build type to RelWithDebInfo (this instructs cmake to
# pass -O2 -g -DNDEBUG options to the compiler by default).
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
               STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Convert VER_INFO values to [SO]VERSION ones.
if (BUILD_SHARED_LIBS)
  # cord:
  string(REGEX REPLACE "(.+):.+:.+"  "\\1" cord_cur ${LIBCORD_VER_INFO})
  string(REGEX REPLACE ".+:(.+):.+"  "\\1" cord_rev ${LIBCORD_VER_INFO})
  string(REGEX REPLACE ".+:.+:(.+)$" "\\1" cord_age ${LIBCORD_VER_INFO})
  math(EXPR CORD_SOVERSION "${cord_cur} - ${cord_age}")
  set(CORD_VERSION_PROP "${CORD_SOVERSION}.${cord_age}.${cord_rev}")
  message(STATUS "CORD_VERSION_PROP = ${CORD_VERSION_PROP}")
  # gc:
  string(REGEX REPLACE "(.+):.+:.+"  "\\1" gc_cur ${LIBGC_VER_INFO})
  string(REGEX REPLACE ".+:(.+):.+"  "\\1" gc_rev ${LIBGC_VER_INFO})
  string(REGEX REPLACE ".+:.+:(.+)$" "\\1" gc_age ${LIBGC_VER_INFO})
  math(EXPR GC_SOVERSION "${gc_cur} - ${gc_age}")
  set(GC_VERSION_PROP "${GC_SOVERSION}.${gc_age}.${gc_rev}")
  message(STATUS "GC_VERSION_PROP = ${GC_VERSION_PROP}")
  # gccpp and gctba:
  string(REGEX REPLACE "(.+):.+:.+"  "\\1" gccpp_cur ${LIBGCCPP_VER_INFO})
  string(REGEX REPLACE ".+:(.+):.+"  "\\1" gccpp_rev ${LIBGCCPP_VER_INFO})
  string(REGEX REPLACE ".+:.+:(.+)$" "\\1" gccpp_age ${LIBGCCPP_VER_INFO})
  math(EXPR GCCPP_SOVERSION "${gccpp_cur} - ${gccpp_age}")
  set(GCCPP_VERSION_PROP "${GCCPP_SOVERSION}.${gccpp_age}.${gccpp_rev}")
  message(STATUS "GCCPP_VERSION_PROP = ${GCCPP_VERSION_PROP}")
endif(BUILD_SHARED_LIBS)

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
  add_compile_options(/w /w-aus /w-ccc /w-inl /w-pro /w-rch)
elseif (MSVC)
  # All warnings but ignoring "conditional expression is constant" one.
  add_compile_options(/W4 /wd4127)
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
set(NODIST_SRC)
set(ATOMIC_OPS_LIBS)
set(ATOMIC_OPS_LIBS_CMAKE)
set(THREADDLLIBS_LIST)
set(NEED_LIB_RT)

set(_HOST ${CMAKE_SYSTEM_PROCESSOR}-unknown-${CMAKE_SYSTEM})
string(TOLOWER ${_HOST} HOST)
message(STATUS "TARGET = ${HOST}")

if (enable_threads)
  find_package(Threads REQUIRED)
  message(STATUS "Thread library: ${CMAKE_THREAD_LIBS_INIT}")
  include_directories(${Threads_INCLUDE_DIR})
  if (with_libatomic_ops)
    if (without_libatomic_ops)
      message(FATAL_ERROR
        "with_libatomic_ops and without_libatomic_ops are mutually exclusive")
    endif()
    set(ATOMIC_OPS_LIBS "-latomic_ops")
    find_package(Atomic_ops CONFIG)
    if (Atomic_ops_FOUND)
      get_target_property(AO_INCLUDE_DIRS Atomic_ops::atomic_ops
                          INTERFACE_INCLUDE_DIRECTORIES)
      include_directories(${AO_INCLUDE_DIRS})
      message(STATUS "AO_INCLUDE_DIRS = ${AO_INCLUDE_DIRS}")
      set(ATOMIC_OPS_LIBS_CMAKE Atomic_ops::atomic_ops)
    else()
      set(ATOMIC_OPS_LIBS_CMAKE ${ATOMIC_OPS_LIBS})
    endif()
  elseif (without_libatomic_ops)
    include_directories(libatomic_ops/src)
    # In the tests we use the source files directly from libatomic_ops subtree.
    set(NODIST_SRC libatomic_ops/src/atomic_ops.c)
    if (CMAKE_C_COMPILER_ID STREQUAL "SunPro")
      # SunCC compiler on SunOS (Solaris).
      set(NODIST_SRC ${NODIST_SRC} libatomic_ops/src/atomic_ops_sysdeps.S)
    endif()
  elseif (BORLAND OR MSVC OR WATCOM)
    include_directories(libatomic_ops/src)
    # Note: alternatively, use CFLAGS_EXTRA to pass -I<...>/libatomic_ops/src.
  else()
    # Assume the compiler supports GCC atomic intrinsics.
    add_definitions("-DGC_BUILTIN_ATOMIC")
  endif()
  set(THREADDLLIBS_LIST ${CMAKE_THREAD_LIBS_INIT})
  if (${CMAKE_DL_LIBS} MATCHES ^[^-].*)
    # Some cmake versions have a broken non-empty CMAKE_DL_LIBS omitting "-l".
    # Assume CMAKE_DL_LIBS contains just one library.
    set(THREADDLLIBS_LIST ${THREADDLLIBS_LIST} -l${CMAKE_DL_LIBS})
  else()
    set(THREADDLLIBS_LIST ${THREADDLLIBS_LIST} ${CMAKE_DL_LIBS})
  endif()
  # Thread support detection.
  if (CMAKE_USE_PTHREADS_INIT)
    set(SRC ${SRC} gc_dlopen.c pthread_start.c pthread_support.c)
    if (CYGWIN OR MSYS)
      set(SRC ${SRC} win32_threads.c)
    else()
      if (APPLE)
        set(SRC ${SRC} darwin_stop_world.c)
      else()
        set(SRC ${SRC} pthread_stop_world.c)
      endif()
    endif()
    if (HOST MATCHES .*-.*-hpux10.*)
      message(FATAL_ERROR "HP/UX 10 POSIX threads are not supported.")
    endif()
    # Common defines for POSIX platforms.
    add_definitions("-DGC_THREADS -D_REENTRANT")
    if (enable_parallel_mark)
      add_definitions("-DPARALLEL_MARK")
    endif()
    if (enable_thread_local_alloc)
      add_definitions("-DTHREAD_LOCAL_ALLOC")
      set(SRC ${SRC} specific.c thread_local_alloc.c)
    endif()
    message("Explicit GC_INIT() calls may be required.")
    if (HOST MATCHES .*-.*-hpux11.*)
      message("Only HP/UX 11 POSIX threads are supported.")
      add_definitions("-D_POSIX_C_SOURCE=199506L")
      set(NEED_LIB_RT ON)
    elseif (HOST MATCHES .*-.*-netbsd.*)
      message("Only on NetBSD 2.0 or later.")
      add_definitions("-D_PTHREADS")
      set(NEED_LIB_RT ON)
    elseif (CMAKE_C_COMPILER_ID STREQUAL "SunPro")
      set(NEED_LIB_RT ON)
    endif()
    if (MSYS)
      # Does not provide process fork functionality.
    elseif (APPLE)
      # The incremental mode conflicts with fork handling (for now).
      # Thus, HANDLE_FORK is not defined.
    elseif (enable_handle_fork AND NOT disable_handle_fork)
      add_definitions("-DHANDLE_FORK")
    endif()
    if (enable_sigrt_signals)
      add_definitions("-DGC_USESIGRT_SIGNALS")
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
    set(SRC ${SRC} pthread_start.c) # in case client defines GC_WIN32_PTHREADS
    set(SRC ${SRC} pthread_support.c win32_threads.c)
  elseif (CMAKE_HP_PTHREADS_INIT OR CMAKE_USE_SPROC_INIT)
    message(FATAL_ERROR "Unsupported thread package")
  endif()
endif(enable_threads)

# Check whether -lrt linker option is needed to use clock_gettime.
if (NOT NEED_LIB_RT)
  check_function_exists(clock_gettime HAVE_CLOCK_GETTIME_DIRECTLY)
  if (NOT HAVE_CLOCK_GETTIME_DIRECTLY)
    # Use of clock_gettime() probably requires linking with "rt" library.
    set(NEED_LIB_RT ON)
  endif()
endif()

# Locate and use "rt" library if needed (and the library is available).
if (NEED_LIB_RT)
  find_library(LIBRT rt)
  if (LIBRT)
    set(THREADDLLIBS_LIST ${THREADDLLIBS_LIST} ${LIBRT})
  endif()
endif(NEED_LIB_RT)

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

if (enable_single_obj_compilation OR (BUILD_SHARED_LIBS AND NOT disable_single_obj_compilation))
  set(SRC extra/gc.c) # override SRC
  if (enable_threads AND CMAKE_USE_PTHREADS_INIT AND NOT (APPLE OR CYGWIN OR MSYS))
    add_definitions("-DGC_PTHREAD_START_STANDALONE")
    set(SRC ${SRC} pthread_start.c)
  endif()
elseif (BORLAND)
  # Suppress "GC_push_contents_hdr() is declared but never used" warning.
  add_compile_options(/w-use)
endif()

# Add implementation of backtrace() and backtrace_symbols().
if (MSVC)
  set(SRC ${SRC} extra/msvc_dbg.c)
endif()

# Instruct check_c_source_compiles to skip linking.
# Alternatively, we could set CMAKE_REQUIRED_LIBRARIES properly.
SET(CMAKE_REQUIRED_FLAGS "-c")

# Instruct check_c_source_compiles and similar CMake checks not to ignore
# compiler warnings (like "implicit declaration of function").
if (NOT BORLAND AND NOT MSVC AND NOT WATCOM)
  check_c_compiler_flag(-Werror HAVE_FLAG_WERROR)
  if (HAVE_FLAG_WERROR)
    SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror")
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
  if (${CMAKE_VERSION} VERSION_LESS "3.18.0")
    set(WL_NO_UNDEFINED_OPT "-Wl,--no-undefined")
    check_c_compiler_flag(${WL_NO_UNDEFINED_OPT} HAVE_FLAG_WL_NO_UNDEFINED)
  else()
    set(WL_NO_UNDEFINED_OPT "LINKER:--no-undefined")
    check_linker_flag(C "${WL_NO_UNDEFINED_OPT}" HAVE_FLAG_WL_NO_UNDEFINED)
  endif()
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
  separate_arguments(CFLAGS_EXTRA_LIST UNIX_COMMAND "${CFLAGS_EXTRA}")
  add_compile_options(${CFLAGS_EXTRA_LIST})
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

# Build with GC_wcsdup() support if possible.
check_symbol_exists(wcslen wchar.h HAVE_WCSLEN)
if (HAVE_WCSLEN)
  add_definitions("-DGC_REQUIRE_WCSDUP")
endif()

# pthread_setname_np, if available, may have 1, 2 or 3 arguments.
if (enable_threads AND CMAKE_USE_PTHREADS_INIT)
  check_c_source_compiles("
#define _GNU_SOURCE 1\n
#include <pthread.h>\n
int main(void) { (void)pthread_setname_np(\"thread-name\"); return 0; }"
    HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID)
  if (HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID)
    # Define to use 'pthread_setname_np(const char*)' function.
    add_definitions("-DHAVE_PTHREAD_SETNAME_NP_WITHOUT_TID")
  else()
    check_c_source_compiles("
#define _GNU_SOURCE 1\n
#include <pthread.h>\n
int main(void) {\n
  (void)pthread_setname_np(pthread_self(), \"thread-name-%u\", 0); return 0; }"
      HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG)
    if (HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG)
      # Define to use 'pthread_setname_np(pthread_t, const char*, void *)'.
      add_definitions("-DHAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG")
    else()
      check_c_source_compiles("
#define _GNU_SOURCE 1\n
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

# Check for emscripten; use asyncify feature if requested.
check_c_source_compiles("
#ifndef __EMSCRIPTEN__\n
# error This is not Emscripten\n
#endif\n
int main(void) { return 0; }"
  EMSCRIPTEN)
if (EMSCRIPTEN AND enable_emscripten_asyncify)
  # Use this option if your program is targeting -sASYNCIFY.  The latter is
  # required to scan the stack, ASYNCIFY_STACK_SIZE is probably needed for
  # gctest only.
  add_definitions("-DEMSCRIPTEN_ASYNCIFY")
  set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -sASYNCIFY -sASYNCIFY_STACK_SIZE=128000")
endif()

add_library(gc ${SRC})
target_link_libraries(gc
                PRIVATE ${ATOMIC_OPS_LIBS_CMAKE} ${THREADDLLIBS_LIST})
target_include_directories(gc INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

if (enable_cplusplus)
  if (BORLAND OR MSVC OR WATCOM)
    add_library(gccpp gc_badalc.cpp gc_cpp.cpp)
  else()
    add_library(gccpp gc_badalc.cc gc_cpp.cc)
  endif()
  target_link_libraries(gccpp PRIVATE gc)
  target_include_directories(gccpp INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
  if (enable_throw_bad_alloc_library)
    # The same as gccpp but contains only gc_badalc.
    if (BORLAND OR MSVC OR WATCOM)
      add_library(gctba gc_badalc.cpp)
    else()
      add_library(gctba gc_badalc.cc)
    endif()
    target_link_libraries(gctba PRIVATE gc)
    target_include_directories(gctba INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
  endif(enable_throw_bad_alloc_library)
endif()

if (build_cord)
  set(CORD_SRC cord/cordbscs.c cord/cordprnt.c cord/cordxtra.c)
  add_library(cord ${CORD_SRC})
  target_link_libraries(cord PRIVATE gc)
  target_include_directories(cord INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
  if (BUILD_SHARED_LIBS)
    set_property(TARGET cord PROPERTY VERSION ${CORD_VERSION_PROP})
    set_property(TARGET cord PROPERTY SOVERSION ${CORD_SOVERSION})
  endif()
  install(TARGETS cord EXPORT BDWgcTargets
          LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
          INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif(build_cord)

if (BUILD_SHARED_LIBS AND HAVE_FLAG_WL_NO_UNDEFINED)
  # Declare that the libraries do not refer to external symbols.
  if (${CMAKE_VERSION} VERSION_LESS "3.13.0")
    target_link_libraries(gc PRIVATE ${WL_NO_UNDEFINED_OPT})
    if (enable_cplusplus)
      target_link_libraries(gccpp PRIVATE ${WL_NO_UNDEFINED_OPT})
      if (enable_throw_bad_alloc_library)
        target_link_libraries(gctba PRIVATE ${WL_NO_UNDEFINED_OPT})
      endif(enable_throw_bad_alloc_library)
    endif(enable_cplusplus)
    if (build_cord)
      target_link_libraries(cord PRIVATE ${WL_NO_UNDEFINED_OPT})
    endif(build_cord)
  else()
    target_link_options(gc PRIVATE ${WL_NO_UNDEFINED_OPT})
    if (enable_cplusplus)
      target_link_options(gccpp PRIVATE ${WL_NO_UNDEFINED_OPT})
      if (enable_throw_bad_alloc_library)
        target_link_options(gctba PRIVATE ${WL_NO_UNDEFINED_OPT})
      endif(enable_throw_bad_alloc_library)
    endif(enable_cplusplus)
    if (build_cord)
      target_link_options(cord PRIVATE ${WL_NO_UNDEFINED_OPT})
    endif(build_cord)
  endif()
endif()

if (BUILD_SHARED_LIBS)
  set_property(TARGET gc PROPERTY VERSION ${GC_VERSION_PROP})
  set_property(TARGET gc PROPERTY SOVERSION ${GC_SOVERSION})
endif()
install(TARGETS gc EXPORT BDWgcTargets
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if (enable_cplusplus)
  if (BUILD_SHARED_LIBS)
    set_property(TARGET gccpp PROPERTY VERSION ${GCCPP_VERSION_PROP})
    set_property(TARGET gccpp PROPERTY SOVERSION ${GCCPP_SOVERSION})
  endif()
  install(TARGETS gccpp EXPORT BDWgcTargets
          LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
          RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
          INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  if (enable_throw_bad_alloc_library)
    if (BUILD_SHARED_LIBS)
      set_property(TARGET gctba PROPERTY VERSION ${GCCPP_VERSION_PROP})
      set_property(TARGET gctba PROPERTY SOVERSION ${GCCPP_SOVERSION})
    endif()
    install(TARGETS gctba EXPORT BDWgcTargets
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  endif(enable_throw_bad_alloc_library)
endif(enable_cplusplus)

if (install_headers)
  install(FILES include/gc/gc.h
                include/gc/gc_backptr.h
                include/gc/gc_config_macros.h
                include/gc/gc_inline.h
                include/gc/gc_mark.h
                include/gc/gc_tiny_fl.h
                include/gc/gc_typed.h
                include/gc/gc_version.h
                include/gc/javaxfc.h
                include/gc/leak_detector.h
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gc")
  install(FILES include/gc.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
  if (enable_cplusplus)
    install(FILES include/gc_cpp.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(FILES include/gc/gc_allocator.h
                  include/gc/gc_cpp.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gc")
  endif()
  if (enable_disclaim)
    install(FILES include/gc/gc_disclaim.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gc")
  endif()
  if (enable_gcj_support)
    install(FILES include/gc/gc_gcj.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gc")
  endif()
  if (enable_threads AND CMAKE_USE_PTHREADS_INIT)
    install(FILES include/gc/gc_pthread_redirects.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gc")
  endif()
  if (build_cord)
    install(FILES include/gc/cord.h
                  include/gc/cord_pos.h
                  include/gc/ec.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/gc")
  endif()

  # Provide pkg-config metadata.
  set(prefix "${CMAKE_INSTALL_PREFIX}")
  set(exec_prefix \${prefix})
  set(includedir "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
  set(libdir "${CMAKE_INSTALL_FULL_LIBDIR}")
  string(REPLACE ";" " " THREADDLLIBS "${THREADDLLIBS_LIST}")
  # ATOMIC_OPS_LIBS, PACKAGE_VERSION are defined above.
  configure_file(bdw-gc.pc.in bdw-gc.pc @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bdw-gc.pc"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif(install_headers)

if (build_tests)
  if (build_cord)
    add_executable(cordtest cord/tests/cordtest.c ${NODIST_SRC})
    target_link_libraries(cordtest PRIVATE cord gc)
    add_test(NAME cordtest COMMAND cordtest)

    if (WIN32 AND NOT CYGWIN)
      add_executable(de cord/tests/de.c cord/tests/de_win.c
                     cord/tests/de_win.rc ${NODIST_SRC})
      set_target_properties(de PROPERTIES WIN32_EXECUTABLE TRUE)
      target_link_libraries(de PRIVATE cord gc gdi32)
    endif()
  endif(build_cord)

  # Compile some tests as C++ to test extern "C" in header files.
  if (enable_cplusplus)
    set_source_files_properties(tests/leak.c PROPERTIES LANGUAGE CXX)
    if (NOT MSVC)
      # WinMain-based test hangs at startup if compiled by VC as C++ code.
      set_source_files_properties(tests/gctest.c PROPERTIES LANGUAGE CXX)
    endif()
    # To avoid "treating 'c' input as 'c++' when in C++ mode" Clang warning.
    if (NOT (BORLAND OR MSVC OR WATCOM))
      add_compile_options(-x c++)
    endif()
  endif(enable_cplusplus)

  add_executable(gctest WIN32 tests/gctest.c ${NODIST_SRC})
  target_link_libraries(gctest
                PRIVATE gc ${ATOMIC_OPS_LIBS_CMAKE} ${THREADDLLIBS_LIST})
  add_test(NAME gctest COMMAND gctest)
  if (WATCOM)
    # Suppress "conditional expression in if statement is always true/false"
    # and "unreachable code" warnings in GC_MALLOC_[ATOMIC_]WORDS.
    target_compile_options(gctest PRIVATE
                           /wcd=13 /wcd=201 /wcd=367 /wcd=368 /wcd=726)
  endif()

  add_executable(hugetest tests/huge.c ${NODIST_SRC})
  target_link_libraries(hugetest PRIVATE gc)
  add_test(NAME hugetest COMMAND hugetest)

  add_executable(leaktest tests/leak.c ${NODIST_SRC})
  target_link_libraries(leaktest PRIVATE gc)
  add_test(NAME leaktest COMMAND leaktest)

  add_executable(middletest tests/middle.c ${NODIST_SRC})
  target_link_libraries(middletest PRIVATE gc)
  add_test(NAME middletest COMMAND middletest)

  add_executable(realloctest tests/realloc.c ${NODIST_SRC})
  target_link_libraries(realloctest PRIVATE gc)
  add_test(NAME realloctest COMMAND realloctest)

  add_executable(smashtest tests/smash.c ${NODIST_SRC})
  target_link_libraries(smashtest PRIVATE gc)
  add_test(NAME smashtest COMMAND smashtest)

  if (NOT (BUILD_SHARED_LIBS AND WIN32))
    add_library(staticroots_lib_test tests/staticroots_lib.c)
    target_link_libraries(staticroots_lib_test PRIVATE gc)
    add_library(staticroots_lib2_test tests/staticroots_lib.c)
    target_compile_options(staticroots_lib2_test PRIVATE "-DSTATICROOTSLIB2")
    target_link_libraries(staticroots_lib2_test PRIVATE gc)
    add_executable(staticrootstest tests/staticroots.c ${NODIST_SRC})
    target_compile_options(staticrootstest PRIVATE "-DSTATICROOTSLIB2")
    target_link_libraries(staticrootstest PRIVATE
                          gc staticroots_lib_test staticroots_lib2_test)
    add_test(NAME staticrootstest COMMAND staticrootstest)
  endif()

  if (enable_gc_debug)
    add_executable(tracetest tests/trace.c ${NODIST_SRC})
    target_link_libraries(tracetest PRIVATE gc)
    add_test(NAME tracetest COMMAND tracetest)
  endif()

  if (enable_threads)
    add_executable(atomicopstest tests/atomicops.c ${NODIST_SRC})
    target_link_libraries(atomicopstest
                PRIVATE ${ATOMIC_OPS_LIBS_CMAKE} ${THREADDLLIBS_LIST})
    add_test(NAME atomicopstest COMMAND atomicopstest)

    add_executable(initfromthreadtest tests/initfromthread.c ${NODIST_SRC})
    target_link_libraries(initfromthreadtest PRIVATE gc ${THREADDLLIBS_LIST})
    add_test(NAME initfromthreadtest COMMAND initfromthreadtest)

    add_executable(subthreadcreatetest tests/subthreadcreate.c ${NODIST_SRC})
    target_link_libraries(subthreadcreatetest
                PRIVATE gc ${ATOMIC_OPS_LIBS_CMAKE} ${THREADDLLIBS_LIST})
    add_test(NAME subthreadcreatetest COMMAND subthreadcreatetest)

    add_executable(threadleaktest tests/threadleak.c ${NODIST_SRC})
    target_link_libraries(threadleaktest PRIVATE gc ${THREADDLLIBS_LIST})
    add_test(NAME threadleaktest COMMAND threadleaktest)

    if (NOT WIN32)
      add_executable(threadkeytest tests/threadkey.c ${NODIST_SRC})
      target_link_libraries(threadkeytest PRIVATE gc ${THREADDLLIBS_LIST})
      add_test(NAME threadkeytest COMMAND threadkeytest)
    endif()
  endif(enable_threads)

  if (enable_cplusplus)
    add_executable(cpptest WIN32 tests/cpp.cc ${NODIST_SRC})
    target_link_libraries(cpptest PRIVATE gc gccpp)
    add_test(NAME cpptest COMMAND cpptest)
  endif()

  if (enable_disclaim)
    add_executable(disclaim_bench tests/disclaim_bench.c ${NODIST_SRC})
    target_link_libraries(disclaim_bench PRIVATE gc)
    add_test(NAME disclaim_bench COMMAND disclaim_bench)

    add_executable(disclaimtest tests/disclaim.c ${NODIST_SRC})
    target_link_libraries(disclaimtest PRIVATE gc ${THREADDLLIBS_LIST})
    add_test(NAME disclaimtest COMMAND disclaimtest)

    add_executable(weakmaptest tests/weakmap.c ${NODIST_SRC})
    target_link_libraries(weakmaptest
                PRIVATE gc ${ATOMIC_OPS_LIBS_CMAKE} ${THREADDLLIBS_LIST})
    add_test(NAME weakmaptest COMMAND weakmaptest)
  endif()
endif(build_tests)

if (enable_docs)
  install(FILES AUTHORS ChangeLog LICENSE README.md
          DESTINATION "${CMAKE_INSTALL_DOCDIR}")
  install(FILES
            docs/README.autoconf
            docs/README.cmake
            docs/README.cords
            docs/README.environment
            docs/README.macros
            docs/debugging.md
            docs/faq.md
            docs/finalization.md
            docs/gcdescr.md
            docs/gcinterface.md
            docs/leak.md
            docs/overview.md
            docs/porting.md
            docs/scale.md
            docs/simple_example.md
            docs/tree.md
          DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs")
  install(FILES
            docs/platforms/README.aix
            docs/platforms/README.amiga
            docs/platforms/README.arm_cross
            docs/platforms/README.darwin
            docs/platforms/README.dgux386
            docs/platforms/README.emscripten
            docs/platforms/README.ews4800
            docs/platforms/README.hp
            docs/platforms/README.linux
            docs/platforms/README.mac
            docs/platforms/README.os2
            docs/platforms/README.sgi
            docs/platforms/README.solaris2
            docs/platforms/README.symbian
            docs/platforms/README.uts
            docs/platforms/README.win32
            docs/platforms/README.win64
          DESTINATION "${CMAKE_INSTALL_DOCDIR}/docs/platforms")

  install(FILES gc.man DESTINATION "${CMAKE_INSTALL_MANDIR}/man3" RENAME gc.3)
endif(enable_docs)

# CMake config/targets files.
install(EXPORT BDWgcTargets FILE BDWgcTargets.cmake
        NAMESPACE BDWgc:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bdwgc")

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/BDWgcConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bdwgc"
        NO_SET_AND_CHECK_MACRO)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/BDWgcConfigVersion.cmake"
        VERSION "${PACKAGE_VERSION}" COMPATIBILITY AnyNewerVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BDWgcConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/BDWgcConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/bdwgc")

export(EXPORT BDWgcTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/BDWgcTargets.cmake")
