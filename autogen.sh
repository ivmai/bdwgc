#!/bin/sh
set -e

# This script creates (or regenerates) configure (as well as aclocal.m4,
# config.h.in, Makefile.in, etc.) missing in the source repository.

# Note: in case of problems (e.g., in cygwin/mingw or OS X), please make
# sure 'pkg-config' is installed on your host.

autoreconf -i

echo
echo "Ready to run './configure'."
