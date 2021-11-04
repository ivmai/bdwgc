set(CMAKE_SYSTEM_NAME Linux)

# Default SDK path
set(SDK "$ENV{HOME}/cheri/output/morello-sdk" CACHE PATH "path to cheri SDK")

# Set toolchain compilers
set(CMAKE_C_COMPILER ${SDK}/bin/clang)
set(CMAKE_CXX_COMPILER ${SDK}/bin/clang++)

# Don't run the linker on compiler check
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Use only cross compiler tools for compilation and linking
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Set correct machine and abi flags
add_compile_options(--config cheribsd-morello-purecap.cfg)
add_link_options(-fuse-ld=lld --config cheribsd-morello-purecap.cfg)
