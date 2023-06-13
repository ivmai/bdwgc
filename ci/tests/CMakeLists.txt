cmake_minimum_required(VERSION 3.15.0) 

project(BoehmGC
        VERSION 0.1
        DESCRIPTION "Boehm-GC examples"
        LANGUAGES C)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include) 

if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(richards_dynlib_shim SHARED richards.c)
    add_executable(richards_dynlib.elf dynlib_shim.c)
    target_link_libraries(richards_dynlib.elf richards_dynlib_shim ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
else()
    add_executable(random_mixed_alloc.elf random_mixed_alloc.c)
    add_executable(small_fixed_alloc.elf small_fixed_alloc.c)
    add_executable(binary_tree.elf binary_tree.c)
    add_executable(smash_test.elf smash.c)
    add_executable(huge.elf huge.c)
    add_executable(leak.elf leak.c)
    add_executable(richards.elf richards.c)

    target_link_libraries(random_mixed_alloc.elf  ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
    target_link_libraries(small_fixed_alloc.elf ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
    target_link_libraries(binary_tree.elf m ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
    target_link_libraries(huge.elf ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
    target_link_libraries(smash_test.elf ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
    target_link_libraries(leak.elf ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
    target_link_libraries(richards.elf ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
endif()


if (DEFINED BENCHLIB)
    install(TARGETS
                richards_dynlib.elf richards_dynlib_shim
                RUNTIME DESTINATION bin
                LIBRARY DESTINATION lib)
else()
    install(TARGETS random_mixed_alloc.elf
                small_fixed_alloc.elf
                binary_tree.elf
                huge.elf
                smash_test.elf
                leak.elf
                richards.elf
                RUNTIME DESTINATION bin)
endif()
