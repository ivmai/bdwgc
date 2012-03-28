#!/bin/sh
set -e

# This script creates (or regenerates) configure (as well as aclocal.m4,
# config.h.in, Makefile.in, etc.) missing in the source repository.

autoreconf -i

echo
echo "Ready to run './configure'."
