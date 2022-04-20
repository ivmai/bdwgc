cmake_minimum_required(VERSION 3.15.0) 

project(BoehmGC
        VERSION 0.1
        DESCRIPTION "Boehm-GC examples"
        LANGUAGES C)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include) 
add_executable(random_mixed_alloc.elf random_mixed_alloc.c)
add_executable(small_fixed_alloc.elf small_fixed_alloc.c)
add_executable(binary_tree.elf binary_tree.c)
add_executable(smash_test.elf smash.c)
target_link_libraries(random_mixed_alloc.elf libpthread.so ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
target_link_libraries(small_fixed_alloc.elf libpthread.so ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
target_link_libraries(binary_tree.elf libm.so libpthread.so ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)
target_link_libraries(smash_test.elf libpthread.so ${CMAKE_INSTALL_PREFIX}/lib/libgc.so)

install(TARGETS random_mixed_alloc.elf
                small_fixed_alloc.elf
                binary_tree.elf
                smash_test.elf
                RUNTIME DESTINATION bin)
