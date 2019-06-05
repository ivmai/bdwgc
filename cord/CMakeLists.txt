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

add_executable(cord cordbscs.c cordxtra.c tests/de.c tests/de_win.c)
set_target_properties(cord PROPERTIES COMPILE_DEFINITIONS GC_NOT_DLL)
target_link_libraries(cord PRIVATE gc)

if (WIN32)
  set_target_properties(cord PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_libraries(cord PRIVATE gdi32)
endif()
