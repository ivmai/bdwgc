#!/bin/bash
set -e

set ${CHERIBUILD:="/home/buildbot/build"}
set ${CHERI_DIR:="/home/buildbot/cheri/output"}
set ${SSHPORT:=10021}
set ${SSHHOST:=localhost}
set ${PYTHONPATH:=${CHERIBUILD}/test-scripts}


export SSHPORT
export SSHHOST
export PYTHONPATH

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

add_bdwgc_test_suite()
{
    SRC_DIR=ci/tests
    BDWGC_TEST_FILES="smash.c \
                      leak.c  \
                      huge.c"
    for src_file in ${BDWGC_TEST_FILES}; do
        ln -fs ../../tests/${src_file} ${SRC_DIR}/${src_file}
    done
}

build_bdwgc_clients()
{
    SRC_DIR=ci/tests
    BUILD_DIR=bdwgc_client_build
    INSTALL_DIR=bdwgc_install
    BUILD_OPTS="-DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"
    # Link tests in the BDWGC test suite
    add_bdwgc_test_suite

    echo "Building test clients for CI/CD"
    mkdir -p ${BUILD_DIR}
    cmake -B ${BUILD_DIR} -S ${SRC_DIR} ${BUILD_OPTS} -DCMAKE_TOOLCHAIN_FILE=../../${1}.cmake
    cmake --build ${BUILD_DIR}
    cmake --install ${BUILD_DIR}
}


clean()
{
    rm -rf ${@}

    SRC_DIR=ci/tests
    BDWGC_TEST_FILES="smash.c \
                      leak.c  \
                      huge.c"

    for src_file in ${BDWGC_TEST_FILES}; do
        rm ${SRC_DIR}/${src_file}
    done
}

# Run test suite for riscv64-purecap
echo "Checking bdwgc library builds correctly"
build_bdwgc 'riscv64-purecap'

echo "Checking cheri bdwgc clients build correctly"
build_bdwgc_clients 'riscv64-purecap'

echo "Running tests for riscv64-purecap"
args=(
      --architecture riscv64-purecap
      # Qemu System to use
      --qemu-cmd ${CHERI_DIR}/sdk/bin/qemu-system-riscv64cheri
      # Kernel (to avoid the default one)
      --kernel ${CHERI_DIR}/rootfs-riscv64-purecap/boot/kernel/kernel
      # Bios (to avoid the default one)
      --bios bbl-riscv64cheri-virt-fw_jump.bin
      --disk-image ${CHERI_DIR}/cheribsd-riscv64-purecap.img
      # Required build-dir in CheriBSD
      --build-dir .
      --ssh-port $SSHPORT
      --ssh-key $HOME/.ssh/id_ed25519.pub
    )
BUILDBOT_PLATFORM=riscv64-purecap python3 ci/run_cheri_bdwgc_tests.py "${args[@]}"

echo "removing riscv64-purecap unit-test files"
clean bdwgc_build bdwgc_client_build bdwgc_install


# Run test suite for morello-purecap
echo "Checking bdwgc library builds correctly"
build_bdwgc 'morello-purecap'

echo "Checking cheri bdwgc clients build correctly"
build_bdwgc_clients 'morello-purecap'

echo "Running tests for morello-purecap"
args=(
      --architecture morello-purecap
      # Qemu System to use
      --qemu-cmd ${CHERI_DIR}/sdk/bin/qemu-system-morello
      # Kernel (to avoid the default one)
      --kernel ${CHERI_DIR}/rootfs-morello-purecap/boot/kernel/kernel
      # Bios (to avoid the default one)
      --bios edk2-aarch64-code.fd
      --disk-image ${CHERI_DIR}/cheribsd-morello-purecap.img
      # Required build-dir in CheriBSD
      --build-dir .
      --ssh-port $SSHPORT
      --ssh-key $HOME/.ssh/id_ed25519.pub
      )
BUILDBOT_PLATFORM=morello-purecap python3 ci/run_cheri_bdwgc_tests.py "${args[@]}"

echo "removing morello-purecap unit-test files"
clean bdwgc_build bdwgc_client_build bdwgc_install
