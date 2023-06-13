#!/bin/bash
#
# Copyright (c) 2021 The CapableVMs "CHERI Examples" Contributors.
# SPDX-License-Identifier: MIT OR Apache-2.0
#
# Test that examples execute as expected.
#
# This is designed to be run by .buildbot.sh, but can also be run manually too,
# as long as you have a suitable environment, and an execution platform set up
# and running so that the make-based run-* targets have something to connect to.
# Notable environment variables:
#
# - BUILDBOT_PLATFORM
#   - Used to compile with the correct toolchain. Set to something like
#     "morello-purecap" or "riscv64-purecap".
# - SSHPORT
# - SSHUSER (defaults to 'root')
# - SSHHOST (defaults to 'localhost')
#
# TODO: Extend this to cover interactive examples and other architectures. 
# For now, we just test examples on `morello-hybrid`.

set -e

ERROR=false
if [ -z "$SSHPORT" ]; then
    echo "SSHPORT is empty or not set" >&2
    ERROR=true
fi
if [ -z "$BUILDBOT_PLATFORM" ]; then
    echo "BUILDBOT_PLATFORM is empty or not set" >&2
    ERROR=true
fi
$ERROR && exit 1

# One time setup per architecture
setup() 
{
    HOST_INSTALL_PREFIX=${1:-bdwgc_install}
    # Copy library files to target VM
    scp -o "StrictHostKeyChecking no" -P ${SSHPORT} ${HOST_INSTALL_PREFIX}/lib/libgc.so.1.5.1 ${HOST_INSTALL_PREFIX}/lib/libcord.so.1.5.0 root@${SSHHOST}:/root
    if [ $? -ne 0 ]; then 
        echo "Error copying bdwgc library to ${BUILDBOT_PLATFORM} VM"
        exit 1
    fi 	
    # Ensure linker in the VM can find the copied bdwgc libraries 
    ssh -o "StrictHostKeyChecking no" -p ${SSHPORT} root@${SSHHOST} -t 'ln -fs libgc.so.1 libgc.so \
                                                     && ln -fs libgc.so.1.5.1 libgc.so.1 \
						     && ln -fs libcord.so.1 libcord.so \
						     && ln -fs libcord.so.1.5.0 libcord.so.1'
    if [ $? -ne 0 ]; then 
        echo "Error creating links for bdwgc library within ${BUILDBOT_PLATFORM} VM execution environment"
        exit 1
    fi 	
}

# arg-1 : OK - to obtain exit status from executing ssh cmd
# arg-2 : compiled unit-test executable (with relative path)
# arg-3 : benchmark library for shim executable if compiled with DYNAMIC_LOADING=ON
run_single_bench()
{
    FILES="${2} ${3}"
    echo "Transferring  ${FILES}"
    scp -o "StrictHostKeyChecking no" -P ${SSHPORT} ${FILES}  root@${SSHHOST}:/root
    if [ $? -ne 0 ]; then
        echo "Error copying unit-test executable ${name} to ${BUILDBOT_PLATFORM} VM"
        exit 1
    fi

    # These tests should trigger an "In-address space security exception"
    # So they should fail, i.e. `exit=1`
    if [ "$1" == "to_fail" ]; then
        exit_status=0
        RESULT={{$(ssh -o "StrictHostKeyChecking no" -p ${SSHPORT} root@${SSHHOST} -t \
	               "LD_LIBRARY_PATH=/root:\$\{LD_LIBRARY_PATH\} ./$(basename ${name})")} && exit_status=1} || true
        if [ $exit_status -ne 0 ]; then
            exit 1
        fi
    else
        ssh -o "StrictHostKeyChecking no" -p ${SSHPORT} root@${SSHHOST} -t \
	         "LD_LIBRARY_PATH=/root:\$\{LD_LIBRARY_PATH\} ./$(basename ${name})"
    fi
}

# arg-1 : OK - to obtain exit status from executing ssh cmd
# arg-2 : compiled unit-test executable (with relative path)
run()
{
    DYNLIB=${2:-"no-dynlib"}
    for name in "${@:3}"; do
      if [ ${DYNLIB} == "dynlib" ]; then
          INSTALLDIR=$(dirname ${name} | cut -d "/" -f1)
          LIBROOT=$(basename ${name} .elf)
          LIBNAME="${INSTALLDIR}/lib/lib${LIBROOT}_shim.so"
          run_single_bench ${1} ${name} ${LIBNAME}
      else
          run_single_bench ${1} ${name}
      fi
    done

}

# Copy bdwgc library and set up environment 
setup "bdwgc_install"

# Execute 
run OK "no-dynlib" \
       "bdwgc_install/bin/small_fixed_alloc.elf"  \
       "bdwgc_install/bin/random_mixed_alloc.elf" \
       "bdwgc_install/bin/smash_test.elf"         \
       "bdwgc_install/bin/leak.elf"               \
       "bdwgc_install/bin/binary_tree.elf"        \
       "bdwgc_install/bin/huge.elf"               \
       "bdwgc_install/bin/richards.elf"

setup "bdwgc_install_dynlib"

run OK "dynlib" \
       "bdwgc_install_dynlib/bin/richards_dynlib.elf"
