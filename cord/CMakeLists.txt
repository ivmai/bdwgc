#
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

# TODO add_library(cord ...)

if (build_tests)
  add_executable(cordtest cordbscs.c cordprnt.c cordxtra.c
                 tests/cordtest.c)
  target_compile_definitions(cordtest PRIVATE GC_NOT_DLL)
  target_link_libraries(cordtest PRIVATE gc)
  add_test(NAME cordtest COMMAND cordtest)

  if (WIN32)
    add_executable(de cordbscs.c cordxtra.c
                   tests/de.c tests/de_win.c)
    set_target_properties(de PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_definitions(de PRIVATE GC_NOT_DLL)
    target_link_libraries(de PRIVATE gc)
    target_link_libraries(de PRIVATE gdi32)
  endif(WIN32)
endif()
