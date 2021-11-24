#!/bin/bash
set -e

echo "building for riscv64-purecap"
CHERIBUILD=~/build
PLATFORMS='riscv64-purecap'

export SSHPORT=10021
export SSHHOST=localhost
export PYTHONPATH="$CHERIBUILD"/test-scripts

# arg-1 : platform to build. Toolchain file is expected to have the same name
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


    echo "Building bdwgc library for CI/CD"
    mkdir -p ${BUILD_DIR}
    cmake -B ${BUILD_DIR} -S . ${BUILD_OPTS} -DCMAKE_TOOLCHAIN_FILE=${1}.cmake
    cmake --build ${BUILD_DIR}
    cmake --install ${BUILD_DIR}
}

build_bdwgc_clients()
{
    SRC_DIR=ci/tests
    BUILD_DIR=bdwgc_client_build
    INSTALL_DIR=bdwgc_install
    BUILD_OPTS="-DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"

    echo "Building test clients for CI/CD"
    mkdir -p ${BUILD_DIR}
    cmake -B ${BUILD_DIR} -S ${SRC_DIR} ${BUILD_OPTS} -DCMAKE_TOOLCHAIN_FILE=../../${1}.cmake
    cmake --build ${BUILD_DIR}
    cmake --install ${BUILD_DIR}
}


clean()
{
    rm -rf ${@}
}

echo "Checking bdwgc library builds correctly"
build_bdwgc ${PLATFORMS}
echo "Checking cheri bdwgc clients build correctly"
build_bdwgc_clients ${PLATFORMS}

echo "Running tests for riscv64-purecap"
args=(
    --architecture riscv64-purecap
    # Qemu System to use
    --qemu-cmd $HOME/cheri/output/sdk/bin/qemu-system-riscv64cheri
    # Kernel (to avoid the default one) 
    --kernel $HOME/cheri/output/rootfs-riscv64-purecap/boot/kernel/kernel
    # Bios (to avoid the default one) 
    --bios bbl-riscv64cheri-virt-fw_jump.bin
    --disk-image $HOME/cheri/output/cheribsd-riscv64-purecap.img
    # Required build-dir in CheriBSD
    --build-dir .
    --ssh-port $SSHPORT
    --ssh-key $HOME/.ssh/id_ed25519.pub
    )
BUILDBOT_PLATFORM=riscv64-purecap python3 ci/run_cheri_bdwgc_tests.py "${args[@]}"

echo "removing up unit-test files" 
clean bdwgc_build bdwgc_client_build bdwgc_install
