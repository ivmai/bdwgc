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

add_definitions("-DGC_NOT_DLL")

# Compile some tests as C++ to test extern "C" in header files.
if (enable_cplusplus)
  set_source_files_properties(leak_test.c test.c
                              PROPERTIES LANGUAGE CXX)
endif()

add_executable(gctest WIN32 test.c)
target_link_libraries(gctest PRIVATE gc)
add_test(NAME gctest COMMAND gctest)

add_executable(hugetest huge_test.c)
target_link_libraries(hugetest PRIVATE gc)
add_test(NAME hugetest COMMAND hugetest)

add_executable(leaktest leak_test.c)
target_link_libraries(leaktest PRIVATE gc)
add_test(NAME leaktest COMMAND leaktest)

add_executable(middletest middle.c)
target_link_libraries(middletest PRIVATE gc)
add_test(NAME middletest COMMAND middletest)

add_executable(realloc_test realloc_test.c)
target_link_libraries(realloc_test PRIVATE gc)
add_test(NAME realloc_test COMMAND realloc_test)

add_executable(smashtest smash_test.c)
target_link_libraries(smashtest PRIVATE gc)
add_test(NAME smashtest COMMAND smashtest)

if (enable_gc_debug)
  add_executable(tracetest trace_test.c)
  target_link_libraries(tracetest PRIVATE gc)
  add_test(NAME tracetest COMMAND tracetest)
endif()

if (enable_cplusplus)
  # TODO add_executable(test_cpp test_cpp.cc)
  # target_link_libraries(test_cpp PRIVATE gc)
  # add_test(NAME test_cpp COMMAND test_cpp)
endif()

if (enable_disclaim)
  add_executable(disclaim_bench disclaim_bench.c)
  target_link_libraries(disclaim_bench PRIVATE gc)
  add_test(NAME disclaim_bench COMMAND disclaim_bench)

  add_executable(disclaim_test disclaim_test.c)
  target_link_libraries(disclaim_test PRIVATE gc)
  add_test(NAME disclaim_test COMMAND disclaim_test)

  add_executable(disclaim_weakmap_test disclaim_weakmap_test.c)
  target_link_libraries(disclaim_weakmap_test PRIVATE gc)
  add_test(NAME disclaim_weakmap_test COMMAND disclaim_weakmap_test)
endif()
