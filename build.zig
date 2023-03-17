const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // declare getopt module
    const getoptModule = std.Build.CreateModuleOptions {
      .source_file = .{ .path = "libs/getopt/getopt.zig" },
    };
    const getoptModuleObj = b.addModule("getopt", getoptModule);

    const exe = b.addExecutable(.{
        .name = "zig-perftest",
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    exe.verbose_link = true;

    // Add getopt module to executable
    exe.addModule("getopt", getoptModuleObj);

    // C compliation options
    const cflags = [_][]const u8{
      "-D_GNU_SOURCE",
      "-g",
      "-O2",
      "-Wall",
      "-march=native",
      "-std=c11",
    };

    // C source code
    const sourceCodeFiles = [_][]const u8{
      "./src/ib/ib_common.c",
      "./src/ib/ib_device.c",
    };

    // C include paths
    exe.addIncludePath("./src/ib");
    exe.addIncludePath("/usr/include");
    exe.addIncludePath("/usr/include/infiniband");
    exe.addIncludePath("/usr/include/x86_64-linux-gnu");

    // C dynamic linker paths
    exe.addLibraryPath("/usr/lib/x86_64-linux-gnu");

    // C source code
    exe.addCSourceFiles(&sourceCodeFiles, &cflags);

    // Link in glibc
    exe.linkLibC();

    // And link in dependent IB libraries
    exe.linkSystemLibrary("m");
    exe.linkSystemLibrary("mlx5");
    exe.linkSystemLibrary("efa");
    exe.linkSystemLibrary("rdmacm");
    exe.linkSystemLibrary("ibverbs");
    exe.linkSystemLibrary("pci");
    exe.linkSystemLibrary("pthread");
    exe.linkSystemLibrary("nl-route-3");
    exe.linkSystemLibrary("nl-3");

    // exe.pie = true;
    // exe.force_pic = true;
    // exe.rdynamic = true;
    // exe.linkage = std.build.LibExeObjStep.Linkage.dynamic;

    // This declares intent for the executable to be installed into the
    // standard location when the user invokes the "install" step (the default
    // step when running `zig build`).
    exe.install();

    // This *creates* a RunStep in the build graph, to be executed when another
    // step is evaluated that depends on it. The next line below will establish
    // such a dependency.
    const run_cmd = exe.run();

    // By making the run step depend on the install step, it will be run from the
    // installation directory rather than directly from within the cache directory.
    // This is not necessary, however, if the application depends on other installed
    // files, this ensures they will be present and in the expected location.
    run_cmd.step.dependOn(b.getInstallStep());

    // This allows the user to pass arguments to the application in the build
    // command itself, like this: `zig build run -- arg1 arg2 etc`
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // This creates a build step. It will be visible in the `zig build --help` menu,
    // and can be selected like this: `zig build run`
    // This will evaluate the `run` step rather than the default, which is "install".
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    // Creates a step for unit testing.
    const exe_tests = b.addTest(.{
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    // Similar to creating the run step earlier, this exposes a `test` step to
    // the `zig build --help` menu, providing a way for the user to request
    // running the unit tests.
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&exe_tests.step);
}
