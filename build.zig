const std = @import("std");

pub fn build(b: *std.build.Builder) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard release options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("zig-perftest", "src/main.zig");

    exe.addPackagePath("getopt", "libs/getopt/getopt.zig");

    exe.setTarget(target);
    exe.setBuildMode(mode);

    // C compliation options
    const cflags = [_][]const u8{
      "-D_GNU_SOURCE",
      "-g",
      "-O2",
      "-Wall",
//    "-Werror",
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

    // C linker paths
    exe.addLibraryPath("/usr/lib/x86_64-linux-gnu");

    // C source code
    exe.addCSourceFiles(&sourceCodeFiles, &cflags);

    // Link in glibc
    exe.linkLibC();

    exe.install();

    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const exe_tests = b.addTest("src/main.zig");
    exe_tests.setTarget(target);
    exe_tests.setBuildMode(mode);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&exe_tests.step);
}
