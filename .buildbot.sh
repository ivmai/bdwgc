#!/bin/sh
set -e

# arg-1 : cmake toolchain file
build_bdwgc()
{
    BUILD_DIR=bdwgc_build
    INSTALL_DIR=bdwgc_install
    BUILD_OPTS="-DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
                -Denable_gcj_support=OFF \
                -Denable_parallel_mark=OFF \
                -Denable_threads=OFF \
                -Denable_dynamic_loading=OFF"


    mkdir -p ${BUILD_DIR}
    cmake -B ${BUILD_DIR} -S . ${BUILD_OPTS} -DCMAKE_TOOLCHAIN_FILE=${1}
    cmake --build ${BUILD_DIR}
}

build_bdwgc riscv64-purecap.cmake
