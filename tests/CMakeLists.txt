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
set_source_files_properties(
    leak_test.c
    test.c
    PROPERTIES LANGUAGE CXX)

add_executable(gctest WIN32 test.c)
target_link_libraries(gctest gc-lib)
add_test(NAME gctest COMMAND gctest)

add_executable(hugetest huge_test.c)
target_link_libraries(hugetest gc-lib)
add_test(NAME hugetest COMMAND hugetest)

add_executable(leaktest leak_test.c)
target_link_libraries(leaktest gc-lib)
add_test(NAME leaktest COMMAND leaktest)

add_executable(middletest middle.c)
target_link_libraries(middletest gc-lib)
add_test(NAME middletest COMMAND middletest)

add_executable(realloc_test realloc_test.c)
target_link_libraries(realloc_test gc-lib)
add_test(NAME realloc_test COMMAND realloc_test)

add_executable(smashtest smash_test.c)
target_link_libraries(smashtest gc-lib)
add_test(NAME smashtest COMMAND smashtest)
