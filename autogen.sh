#!/bin/sh
set -e

# This script creates (or regenerates) configure (as well as aclocal.m4,
# config.h.in, Makefile.in, etc.) missing in the source repository.
#
# If you compile from a distribution tarball, you can skip this.  Otherwise,
# make sure that you have Autoconf, Automake and Libtool installed
# on your system, and that the corresponding *.m4 files are visible
# to the aclocal.  The latter can be achieved by using packages shipped by
# your OS, or by installing custom versions of all four packages to the same
# prefix.  Otherwise, you may need to invoke autoreconf with the appropriate
# -I options to locate the required *.m4 files.

# Install libtool.m4 and ltmain.sh in the build tree.  This command is needed
# if autoreconf and libtoolize are available from the different directories.
# Note: libtoolize might be missing on some platforms.
if (type libtoolize) > /dev/null 2>&1; then
  libtoolize -i -c
else
  echo "libtoolize is not found, ignoring!"
fi

autoreconf -i

echo
echo "Ready to run './configure'."
