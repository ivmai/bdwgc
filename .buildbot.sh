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

clean_bdwgc()
{
    BUILD_DIR=bdwgc_build
    INSTALL_DIR=bdwgc_install
    rm -rf ${BUILD_DIR}
    rm -rf ${INSTALL_DIR}
}

clean_bdwgc
build_bdwgc riscv64-purecap.cmake
clean_bdwgc
build_bdwgc morello-purecap.cmake
clean_bdwgc
