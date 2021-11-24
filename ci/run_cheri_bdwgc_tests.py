#!/usr/bin/env python3
#
# Copyright (c) 2021 The CapableVMs "CHERI Examples" Contributors.
# SPDX-License-Identifier: MIT OR Apache-2.0
#
# This is expected to be run only from `.buildbot.sh`.
#
# This starts a `qemu-system` to run the tests and then safely shutdown the VM.
# This relies on infrastructure provided by cheribuild:
#   https://github.com/CTSRD-CHERI/cheribuild/tree/master/test-scripts

import argparse
import os
import sys
from pathlib import Path
import importlib.util

# The cheribuild infrastructure expects the first entry in sys.path to be
# $CHERIBUILD/test-scripts, as if this script exists in that directory. This
# script should work in such an environment, but we typically fake it by setting
# PYTHONPATH (in `.buildbot.sh`), then dropping this script's actual directory
# from sys.path.
#
# This avoids the need to write outside the buildbot test directory.

test_scripts_dir = str(Path(importlib.util.find_spec("run_tests_common").origin).parent.absolute())
sys.path = sys.path[sys.path.index(test_scripts_dir):]

from run_tests_common import boot_cheribsd, run_tests_main


def run_cheri_bdwgc_tests(qemu: boot_cheribsd.QemuCheriBSDInstance, args: argparse.Namespace) -> bool:
    if args.sysroot_dir is not None:
        boot_cheribsd.set_ld_library_path_with_sysroot(qemu)
    boot_cheribsd.info("Running tests for cheri-bdwgc")

    return os.system("./ci/run_tests.sh") == 0


if __name__ == '__main__':
    run_tests_main(test_function=run_cheri_bdwgc_tests, need_ssh=True, should_mount_builddir=False)
