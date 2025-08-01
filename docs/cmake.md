# Building bdwgc with CMake

UNIX and Win32 binaries (both 32- and 64-bit) can be built using CMake.
CMake is an open-source tool like [`automake`](autoconf.md) - it generates
makefiles.

CMake (as of v3.14.5) is able to generate:

  * Borland Makefiles
  * MSYS Makefiles
  * MinGW Makefiles
  * NMake Makefiles
  * Unix Makefiles
  * Visual Studio 16 2019
  * Visual Studio 15 2017
  * Visual Studio 14 2015
  * Visual Studio 12 2013
  * Visual Studio 11 2012
  * Visual Studio 10 2010
  * Visual Studio 9 2008
  * Watcom WMake


## Build process

The steps are:

  1. Install cmake (e.g. from [cmake.org](https://cmake.org/));

  2. Add directory containing `cmake` executable to `PATH` environment
     variable;

  3. Run cmake from the bdwgc root directory, passing the target with `-G`
     option - e.g. type `cmake -G "Visual Studio 9 2008"` and use the `gc.sln`
     file generated by cmake to build `gc` library.

Notes:

  * Specify `-Denable_cplusplus=ON` option to build `gccpp` and `gctba`
    libraries (i.e. the ones that provide bdwgc C++ support).

  * Specify `-Dbuild_tests=ON` option to the tests (and run them by
    `ctest -V`).

You can also run cmake from a build directory to build outside of the root of
the source tree - just specify the path to the latter, e.g.:

```sh
mkdir out
cd out
cmake -G "Visual Studio 9 2008" -Dbuild_tests=ON ..
cmake --build . --config Release
ctest --build-config Release -V
```

Here is a sample for Linux (build, test and install, w/o C++ support):

```sh
mkdir out
cd out
cmake -Dbuild_tests=ON ..
cmake --build .
ctest
make install
```


# Input

The main input to cmake is `CMakeLists.txt` file (script) in the bdwgc root
directory.  For help, go to [cmake.org](https://cmake.org/).


# How to import bdwgc

Another project could add bdwgc as one of its dependencies with something like
this in their `CMakeLists.txt`:

```cmake
find_package(BDWgc 8.2.0 REQUIRED)
add_executable(Foo foo.c)
target_link_libraries(Foo BDWgc::gc)
```

Other exported libraries are: `cord`, `gccpp`, `gctba`.
