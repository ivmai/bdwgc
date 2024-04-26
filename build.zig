// THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
// OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
//
// Permission is hereby granted to use or copy this program
// for any purpose, provided the above notices are retained on all copies.
// Permission to modify the code and to distribute modified code is granted,
// provided the above notices are retained, and a notice that the code was
// modified is included with the above copyright notice.

// A script to build and test the collector using Zig build system.
// The script matches CMakeLists.txt as much as possible.

const builtin = @import("builtin");
const std = @import("std");

const zig_min_required_version = "0.12.0";

// TODO: specify PACKAGE_VERSION and LIB*_VER_INFO.

// Compared to the CMake script, a lot more definitions and compiler options
// are hard-coded here, which is natural because build.zig is only built with
// the Zig build system and Zig ships with an embedded clang (as of zig 0.12).
// As a consequence, we do not have to support lots of different compilers.
// And, on the contrary, we know exactly what we get and thus we can align on
// clang's capabilities rather than having to discover compiler capabilities.
// Similarly, since Zig ships libc headers for many platforms, we can, with
// the knowledge of the platform, determine what capabilities should be
// enabled or not.

comptime {
    const required_ver = std.SemanticVersion.parse(zig_min_required_version)
                            catch unreachable;
    if (builtin.zig_version.order(required_ver) == .lt) {
        @compileError(std.fmt.comptimePrint(
            "Zig version {} does not meet the build requirement of {}",
            .{ builtin.zig_version, required_ver },
        ));
    }
}

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{});
    const t = target.result;

    const default_enable_threads = !t.isWasm(); // both emscripten and wasi

    // Customize build by passing "-D<option_name>[=false]" in command line.
    // TODO: support enable_cplusplus
    const build_shared_libs = b.option(bool, "BUILD_SHARED_LIBS",
                "Build shared libraries (otherwise static ones)") orelse true;
    // TODO: support build_cord
    const cflags_extra = b.option([]const u8, "CFLAGS_EXTRA",
                                  "Extra user-defined cflags") orelse "";
    // TODO: support enable_docs
    const enable_threads = b.option(bool, "enable_threads",
                "Support threads") orelse default_enable_threads;
    const enable_parallel_mark = b.option(bool, "enable_parallel_mark",
        "Parallelize marking and free list construction") orelse true;
    const enable_thread_local_alloc = b.option(bool,
        "enable_thread_local_alloc",
        "Turn on thread-local allocation optimization") orelse true;
    const enable_threads_discovery = b.option(bool,
        "enable_threads_discovery",
        "Enable threads discovery in GC") orelse true;
    const enable_rwlock = b.option(bool, "enable_rwlock",
        "Enable reader mode of the allocator lock") orelse false;
    // TODO: support enable_throw_bad_alloc_library
    const enable_gcj_support = b.option(bool, "enable_gcj_support",
                                        "Support for gcj") orelse true;
    const enable_sigrt_signals = b.option(bool, "enable_sigrt_signals",
        "Use SIGRTMIN-based signals for thread suspend/resume") orelse false;
    const enable_valgrind_tracking = b.option(bool,
        "enable_valgrind_tracking",
        "Support tracking GC_malloc and friends for heap profiling tools")
        orelse false;
    const enable_gc_debug = b.option(bool, "enable_gc_debug",
        "Support for pointer back-tracing") orelse false;
    const disable_gc_debug = b.option(bool, "disable_gc_debug",
        "Disable debugging like GC_dump and its callees") orelse false;
    const enable_java_finalization = b.option(bool,
        "enable_java_finalization",
        "Support for java finalization") orelse true;
    const enable_atomic_uncollectable = b.option(bool,
        "enable_atomic_uncollectable",
        "Support for atomic uncollectible allocation") orelse true;
    const enable_redirect_malloc = b.option(bool, "enable_redirect_malloc",
        "Redirect malloc and friend to GC routines") orelse false;
    const enable_disclaim = b.option(bool, "enable_disclaim",
        "Support alternative finalization interface") orelse true;
    const enable_dynamic_pointer_mask = b.option(bool,
        "enable_dynamic_pointer_mask",
        "Support pointer mask/shift set at runtime") orelse false;
    const enable_large_config = b.option(bool, "enable_large_config",
        "Optimize for large heap or root set") orelse false;
    const enable_gc_assertions = b.option(bool, "enable_gc_assertions",
        "Enable collector-internal assertion checking") orelse false;
    const enable_mmap = b.option(bool, "enable_mmap",
        "Use mmap instead of sbrk to expand the heap") orelse false;
    const enable_munmap = b.option(bool, "enable_munmap",
        "Return page to the OS if empty for N collections") orelse true;
    const enable_dynamic_loading = b.option(bool, "enable_dynamic_loading",
        "Enable tracing of dynamic library data roots") orelse true;
    const enable_register_main_static_data = b.option(bool,
        "enable_register_main_static_data",
        "Perform the initial guess of data root sets") orelse true;
    const enable_checksums = b.option(bool, "enable_checksums",
        "Report erroneously cleared dirty bits") orelse false;
    const enable_werror = b.option(bool, "enable_werror",
        "Pass -Werror to the C compiler (treat warnings as errors)")
        orelse false;
    const enable_single_obj_compilation = b.option(bool,
        "enable_single_obj_compilation",
        "Compile all libgc source files into single .o") orelse false;
    const disable_single_obj_compilation = b.option(bool,
        "disable_single_obj_compilation",
        "Compile each libgc source file independently") orelse false;
    const enable_handle_fork = b.option(bool, "enable_handle_fork",
        "Attempt to ensure a usable collector after fork()") orelse true;
    const disable_handle_fork = b.option(bool, "disable_handle_fork",
        "Prohibit installation of pthread_atfork() handlers") orelse false;
    // TODO: support enable_emscripten_asyncify
    const install_headers = b.option(bool, "install_headers",
        "Install header and pkg-config metadata files") orelse true;
    // TODO: support with_libatomic_ops, without_libatomic_ops

    var lib = b.addStaticLibrary(.{
        .name = "gc",
        .target = target,
        .optimize = optimize,
    });
    if (build_shared_libs) {
        // TODO: convert VER_INFO values to [SO]VERSION ones
        lib = b.addSharedLibrary(.{
            .name = "gc",
            .target = target,
            .optimize = optimize,
        });
    }

    var source_files = std.ArrayList([]const u8).init(b.allocator);
    defer source_files.deinit();
    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();

    // Always enabled.
    flags.append("-D ALL_INTERIOR_POINTERS") catch unreachable;
    flags.append("-D NO_EXECUTE_PERMISSION") catch unreachable;

    // Output all warnings.
    flags.appendSlice(&.{
        "-Wall",
        "-Wextra",
        "-Wpedantic",
    }) catch unreachable;

    source_files.appendSlice(&.{
        "allchblk.c",
        "alloc.c",
        "blacklst.c",
        "dbg_mlc.c",
        "dyn_load.c",
        "finalize.c",
        "headers.c",
        "mach_dep.c",
        "malloc.c",
        "mallocx.c",
        "mark.c",
        "mark_rts.c",
        "misc.c",
        "new_hblk.c",
        "obj_map.c",
        "os_dep.c",
        "ptr_chck.c",
        "reclaim.c",
        "typd_mlc.c",
    }) catch unreachable;

    if (enable_threads) { // assume pthreads
        // Zig comes with clang which supports GCC atomic intrinsics.
        flags.append("-D GC_BUILTIN_ATOMIC") catch unreachable;
        // TODO: define and use THREADDLLIBS_LIST

        source_files.appendSlice(&.{
            "gc_dlopen.c",
            "pthread_start.c",
            "pthread_support.c",
        }) catch unreachable;
        if (t.os.tag == .windows) {
            source_files.appendSlice(&.{
                "win32_threads.c",
            }) catch unreachable;
        } else if (t.isDarwin()) {
            source_files.appendSlice(&.{
                "darwin_stop_world.c",
            }) catch unreachable;
        } else {
            source_files.appendSlice(&.{
                "pthread_stop_world.c",
            }) catch unreachable;
        }
        // Common defines for POSIX platforms.
        flags.append("-D GC_THREADS") catch unreachable;
        flags.append("-D _REENTRANT") catch unreachable;
        // TODO: some targets might need _PTHREADS defined too.
        if (enable_parallel_mark) {
            flags.append("-D PARALLEL_MARK") catch unreachable;
        }
        if (enable_thread_local_alloc) {
            flags.append("-D THREAD_LOCAL_ALLOC") catch unreachable;
            source_files.appendSlice(&.{
                "specific.c",
                "thread_local_alloc.c",
            }) catch unreachable;
        }
        // Message for clients: Explicit GC_INIT() calls may be required.
        if (t.os.tag == .windows) {
            // Does not provide process fork functionality.
        } else if (enable_handle_fork and !disable_handle_fork) {
            flags.append("-D HANDLE_FORK") catch unreachable;
        }
        if (enable_sigrt_signals) {
            flags.append("-D GC_USESIGRT_SIGNALS") catch unreachable;
        }
    }

    // TODO: define/use NEED_LIB_RT

    if (disable_handle_fork) {
        flags.append("-D NO_HANDLE_FORK") catch unreachable;
    }

    if (enable_gcj_support) {
        flags.append("-D GC_GCJ_SUPPORT") catch unreachable;
        // TODO: do not define GC_ENABLE_SUSPEND_THREAD on kFreeBSD
        // if enable_thread_local_alloc (a workaround for some bug).
        flags.append("-D GC_ENABLE_SUSPEND_THREAD") catch unreachable;
        source_files.appendSlice(&.{
            "gcj_mlc.c",
        }) catch unreachable;
    }

    if (enable_disclaim) {
        flags.append("-D ENABLE_DISCLAIM") catch unreachable;
        source_files.appendSlice(&.{
            "fnlz_mlc.c",
        }) catch unreachable;
    }

    if (enable_dynamic_pointer_mask) {
        flags.append("-D DYNAMIC_POINTER_MASK") catch unreachable;
    }

    if (enable_java_finalization) {
        flags.append("-D JAVA_FINALIZATION") catch unreachable;
    }

    if (enable_atomic_uncollectable) {
        flags.append("-D GC_ATOMIC_UNCOLLECTABLE") catch unreachable;
    }

    if (enable_valgrind_tracking) {
        flags.append("-D VALGRIND_TRACKING") catch unreachable;
    }

    if (enable_gc_debug) {
        flags.append("-D DBG_HDRS_ALL") catch unreachable;
        flags.append("-D KEEP_BACK_PTRS") catch unreachable;
        if (t.os.tag == .linux) {
            flags.append("-D MAKE_BACK_GRAPH") catch unreachable;
            // TODO: do not define SAVE_CALL_COUNT for e2k
            flags.append("-D SAVE_CALL_COUNT=8") catch unreachable;

            source_files.appendSlice(&.{
                "backgraph.c",
            }) catch unreachable;
        }
    }

    if (disable_gc_debug) {
        flags.append("-D NO_DEBUGGING") catch unreachable;
    }
    if (optimize != .Debug) {
        flags.append("-D NDEBUG") catch unreachable;
    }

    if (enable_redirect_malloc) {
        if (enable_gc_debug) {
            flags.append("-D REDIRECT_MALLOC=GC_debug_malloc_replacement")
                catch unreachable;
            flags.append("-D REDIRECT_REALLOC=GC_debug_realloc_replacement")
                catch unreachable;
            flags.append("-D REDIRECT_FREE=GC_debug_free") catch unreachable;
        } else {
            flags.append("-D REDIRECT_MALLOC=GC_malloc") catch unreachable;
        }
        if (t.os.tag == .windows) {
            flags.append("-D REDIRECT_MALLOC_IN_HEADER") catch unreachable;
        } else {
            flags.append("-D GC_USE_DLOPEN_WRAP") catch unreachable;
        }
    }

    if (enable_mmap or enable_munmap) {
        flags.append("-D USE_MMAP") catch unreachable;
    }

    if (enable_munmap) {
        flags.append("-D USE_MUNMAP") catch unreachable;
    }

    if (!enable_dynamic_loading) {
        flags.append("-D IGNORE_DDYNAMIC_LOADING") catch unreachable;
    }

    if (!enable_register_main_static_data) {
        flags.append("-D GC_DONT_REGISTER_MAIN_STATIC_DATA") catch unreachable;
    }

    if (enable_large_config) {
        flags.append("-D LARGE_CONFIG") catch unreachable;
    }

    if (enable_gc_assertions) {
        flags.append("-D GC_ASSERTIONS") catch unreachable;
    }

    if (!enable_threads_discovery) {
        flags.append("-D GC_NO_THREADS_DISCOVERY") catch unreachable;
    }

    if (enable_rwlock) {
        flags.append("-D USE_RWLOCK") catch unreachable;
    }

    if (enable_checksums) {
        if (enable_munmap or enable_threads) {
            @panic("CHECKSUMS not compatible with USE_MUNMAP or threads");
        }
        flags.append("-D CHECKSUMS") catch unreachable;
        source_files.appendSlice(&.{
            "checksums.c",
        }) catch unreachable;
    }

    if (enable_werror) {
        flags.appendSlice(&.{
            "-Werror",
        }) catch unreachable;
    }

    if (enable_single_obj_compilation
            or (build_shared_libs and !disable_single_obj_compilation)) {
        source_files.clearAndFree();
        source_files.appendSlice(&.{
            "extra/gc.c",
        }) catch unreachable;
        if (enable_threads and !t.isDarwin() and t.os.tag != .windows) {
            flags.append("-D GC_PTHREAD_START_STANDALONE") catch unreachable;
            source_files.appendSlice(&.{
                "pthread_start.c",
            }) catch unreachable;
        }
    }

    // TODO: declare that the libraries do not refer to external symbols
    // of build_shared_libs.

    // zig/clang supports these flags.
    flags.appendSlice(&.{
        // TODO: -Wno-unused-command-line-argument
        // Prevent "__builtin_return_address with nonzero argument is unsafe".
        "-Wno-frame-address",
        "-fno-strict-aliasing",
    }) catch unreachable;

    if (build_shared_libs) {
        flags.append("-D GC_DLL") catch unreachable;
        flags.append("-D GC_VISIBILITY_HIDDEN_SET") catch unreachable;
        flags.appendSlice(&.{
            "-fvisibility=hidden",
        }) catch unreachable;
    } else {
        flags.append("-D GC_NOT_DLL") catch unreachable;
        if (t.os.tag == .windows) {
            // Do not require the clients to link with "user32" system library.
            flags.append("-D DONT_USE_USER32_DLL") catch unreachable;
        }
    }

    // Note: Zig uses clang which ships with these so, unless another
    // sysroot/libc, etc. headers location is pointed out, it is fine to
    // hard-code enable this.
    // -U GC_MISSING_EXECINFO_H
    // -U GC_NO_SIGSETJMP
    flags.append("-D HAVE_SYS_TYPES_H") catch unreachable;
    flags.append("-D HAVE_UNISTD_H") catch unreachable;

    const have_getcontext = !t.abi.isMusl();
    if (!have_getcontext) {
        flags.append("-D NO_GETCONTEXT") catch unreachable;
    }

    if (!t.isDarwin() and t.os.tag != .windows) {
        // dl_iterate_phdr exists (as a strong symbol).
        flags.append("-D HAVE_DL_ITERATE_PHDR") catch unreachable;
        if (enable_threads) {
            // pthread_sigmask() and sigset_t are available and needed.
            flags.append("-D HAVE_PTHREAD_SIGMASK") catch unreachable;
        }
    }

    // Build with GC_wcsdup() support (wcslen is available).
    flags.append("-D GC_REQUIRE_WCSDUP") catch unreachable;

    // pthread_setname_np, if available, may have 1, 2 or 3 arguments.
    if (t.isDarwin()) {
        flags.append("-D HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID")
                catch unreachable;
    } else if (t.os.tag == .linux) {
        flags.append("-D HAVE_PTHREAD_SETNAME_NP_WITH_TID") catch unreachable;
    } else {
        // TODO: support HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG
        // and HAVE_PTHREAD_SET_NAME_NP targets.
    }

    // Define to use 'dladdr' function (used for debugging).
    flags.append("-D HAVE_DLADDR") catch unreachable;

    // Extra user-defined flags (if any) to pass to the compiler.
    if (cflags_extra.len > 0) {
        // Split it up on a space and append each part to flags separately.
        var tokenizer = std.mem.tokenizeScalar(u8, cflags_extra, ' ');
        while (tokenizer.next()) |token| {
            flags.append(token) catch unreachable;
        }
    }

    lib.addCSourceFiles(.{
        .files = source_files.items,
        .flags = flags.items,
    });
    lib.addIncludePath(b.path("include"));
    lib.linkLibC();
    if (install_headers) {
        installHeader(b, lib, "gc.h");
        installHeader(b, lib, "gc/gc.h");
        installHeader(b, lib, "gc/gc_backptr.h");
        installHeader(b, lib, "gc/gc_config_macros.h");
        installHeader(b, lib, "gc/gc_inline.h");
        installHeader(b, lib, "gc/gc_mark.h");
        installHeader(b, lib, "gc/gc_tiny_fl.h");
        installHeader(b, lib, "gc/gc_typed.h");
        installHeader(b, lib, "gc/gc_version.h");
        installHeader(b, lib, "gc/javaxfc.h");
        installHeader(b, lib, "gc/leak_detector.h");
        if (enable_disclaim) {
            installHeader(b, lib, "gc/gc_disclaim.h");
        }
        if (enable_gcj_support) {
            installHeader(b, lib, "gc/gc_gcj.h");
        }
        if (enable_threads) {
            installHeader(b, lib, "gc/gc_pthread_redirects.h");
        }
        // TODO: compose and install bdw-gc.pc and pkgconfig.
    }

    b.installArtifact(lib);

    // Note: there is no "build_tests" option, as the tests are built
    // only if "test" step is requested.
    const test_step = b.step("test", "Run tests");
    addTest(b, lib, test_step, flags, "gctest", "tests/gctest.c");
    addTest(b, lib, test_step, flags, "hugetest", "tests/huge.c");
    addTest(b, lib, test_step, flags, "leaktest", "tests/leak.c");
    addTest(b, lib, test_step, flags, "middletest", "tests/middle.c");
    addTest(b, lib, test_step, flags, "realloctest", "tests/realloc.c");
    addTest(b, lib, test_step, flags, "smashtest", "tests/smash.c");
    // TODO: add staticroots test
    if (enable_gc_debug) {
        addTest(b, lib, test_step, flags, "tracetest", "tests/trace.c");
    }
    if (enable_threads) {
        addTest(b, lib, test_step, flags,
                "atomicopstest", "tests/atomicops.c");
        addTest(b, lib, test_step, flags,
                "initfromthreadtest", "tests/initfromthread.c");
        addTest(b, lib, test_step, flags,
                "subthreadcreatetest", "tests/subthreadcreate.c");
        addTest(b, lib, test_step, flags,
                "threadleaktest", "tests/threadleak.c");
        addTest(b, lib, test_step, flags,
                "threadkeytest", "tests/threadkey.c");
    }
    if (enable_disclaim) {
        addTest(b, lib, test_step, flags,
                "disclaim_bench", "tests/disclaim_bench.c");
        addTest(b, lib, test_step, flags,
                "disclaimtest", "tests/disclaim.c");
        addTest(b, lib, test_step, flags,
                "weakmaptest", "tests/weakmap.c");
    }
}

fn addTest(b: *std.Build, lib: *std.Build.Step.Compile,
           test_step: *std.Build.Step, flags: std.ArrayList([]const u8),
           testname: []const u8, filename: []const u8) void {
    const test_exe = b.addExecutable(.{
        .name = testname,
        .optimize = lib.root_module.optimize.?,
        .target = lib.root_module.resolved_target.?
    });
    test_exe.addCSourceFile(.{
        .file = b.path(filename),
        .flags = flags.items
    });
    test_exe.addIncludePath(b.path("include"));
    test_exe.linkLibrary(lib);
    test_exe.linkLibC();
    const run_test_exe = b.addRunArtifact(test_exe);
    test_step.dependOn(&run_test_exe.step);
}

fn installHeader(b: *std.Build, lib: *std.Build.Step.Compile,
                 hfile: []const u8) void {
   const src_path = b.pathJoin(&.{ "include", hfile });
   lib.installHeader(b.path(src_path), hfile);
}
