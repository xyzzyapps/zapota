const std = @import("std");
const builtin = @import("builtin");

fn addCTree(exe: *std.Build.Step.Compile, b: *std.Build, rel_dir: []const u8, flags: []const []const u8) void {
    const io = b.graph.io;
    var dir = std.Io.Dir.cwd().openDir(io, rel_dir, .{ .iterate = true }) catch @panic("open C tree");
    defer dir.close(io);
    var walker = dir.walk(b.allocator) catch @panic("walk C tree");
    defer walker.deinit();
    while (walker.next(io) catch |err| std.debug.panic("walk: {s}", .{@errorName(err)})) |entry| {
        if (entry.kind != .file) continue;
        if (!std.mem.endsWith(u8, entry.basename, ".c")) continue;
        const full = b.fmt("{s}/{s}", .{ rel_dir, entry.path });
        for (full) |*c| {
            if (c.* == '\\') c.* = '/';
        }
        exe.root_module.addCSourceFile(.{
            .file = b.path(full),
            .flags = flags,
        });
    }
}

fn attachGc(exe: *std.Build.Step.Compile, b: *std.Build, enabled: bool, is_windows: bool) void {
    if (!enabled) return;
    exe.root_module.addIncludePath(b.path("vendor/bdwgc/include"));
    const gc_flags_posix: []const []const u8 = &.{
        "-O2",
        "-DGC_SINGLE_OBJ_BUILD",
        "-DALL_INTERIOR_POINTERS",
        "-DNO_EXECUTE_PERMISSION",
        "-DGC_BUILTIN_ATOMIC",
    };
    const gc_flags_win: []const []const u8 = &.{
        "-O2",
        "-DGC_SINGLE_OBJ_BUILD",
        "-DALL_INTERIOR_POINTERS",
        "-DNO_EXECUTE_PERMISSION",
        "-DGC_BUILTIN_ATOMIC",
        "-DEMPTY_GETENV_RESULTS",
    };
    exe.root_module.addCSourceFile(.{
        .file = b.path("vendor/bdwgc/extra/gc.c"),
        .flags = if (is_windows) gc_flags_win else gc_flags_posix,
    });
    exe.root_module.addCSourceFile(.{
        .file = b.path("src/gc_init.c"),
        .flags = &.{ "-Wall" },
    });
}

fn shipHost(b: *std.Build, exe: *std.Build.Step.Compile, gc: bool, is_windows: bool) void {
    attachGc(exe, b, gc, is_windows);
    b.installArtifact(exe);
}

fn installClapPlugin(
    b: *std.Build,
    lib: *std.Build.Step.Compile,
    tag: []const u8,
    is_macos: bool,
    clap_step: *std.Build.Step,
    all_step: ?*std.Build.Step,
) void {
    const dest_sub: []const u8 = if (is_macos)
        b.fmt("clap/{s}/ZapotaFilter.clap/Contents/MacOS/ZapotaFilter", .{tag})
    else
        b.fmt("clap/{s}/ZapotaFilter.clap", .{tag});
    const inst = b.addInstallArtifact(lib, .{
        .dest_dir = .{ .override = .lib },
        .dest_sub_path = dest_sub,
        .dylib_symlinks = false,
    });
    clap_step.dependOn(&inst.step);
    if (all_step) |s| s.dependOn(&inst.step);
    if (is_macos) {
        const plist = b.addInstallFile(
            b.path("src/clap/Info.plist"),
            b.fmt("lib/clap/{s}/ZapotaFilter.clap/Contents/Info.plist", .{tag}),
        );
        clap_step.dependOn(&plist.step);
        if (all_step) |s| s.dependOn(&plist.step);
    }
}

const CrossKind = enum { windows, posix, wasm, android };

const CrossTarget = struct {
    tag: []const u8,
    triple: []const u8,
    kind: CrossKind,

    fn isWindows(self: CrossTarget) bool {
        return self.kind == .windows;
    }
};

fn applyWasi(exe: *std.Build.Step.Compile) void {
    exe.root_module.addCMacro("_WASI_EMULATED_SIGNAL", "1");
    exe.root_module.addCMacro("_WASI_EMULATED_PROCESS_CLOCKS", "1");
    exe.root_module.linkSystemLibrary("wasi-emulated-signal", .{});
}

fn applyAndroid(exe: *std.Build.Step.Compile, b: *std.Build, ndk: []const u8) void {
    const host_prebuilt = if (builtin.os.tag == .windows) "windows-x86_64" else if (builtin.os.tag == .macos) "darwin-x86_64" else "linux-x86_64";
    const sysroot = b.fmt("{s}/toolchains/llvm/prebuilt/{s}/sysroot", .{ ndk, host_prebuilt });
    const include_dir = b.fmt("{s}/usr/include", .{sysroot});
    const sys_include_dir = b.fmt("{s}/usr/include/aarch64-linux-android", .{sysroot});
    const crt_dir = b.fmt("{s}/usr/lib/aarch64-linux-android/24", .{sysroot});
    const libc_txt = b.fmt(
        \\include_dir={s}
        \\sys_include_dir={s}
        \\crt_dir={s}
        \\msvc_lib_dir=
        \\kernel32_lib_dir=
        \\gcc_dir=
        \\
    , .{ include_dir, sys_include_dir, crt_dir });
    const wf = b.addWriteFiles();
    const libc_file = wf.add("android-libc.txt", libc_txt);
    exe.setLibCFile(libc_file);
    exe.root_module.addSystemIncludePath(.{ .cwd_relative = include_dir });
    exe.root_module.addSystemIncludePath(.{ .cwd_relative = sys_include_dir });
    exe.root_module.addLibraryPath(.{ .cwd_relative = crt_dir });
    exe.root_module.linkSystemLibrary("log", .{});
    exe.root_module.linkSystemLibrary("unwind", .{});
}

fn findAndroidNdk(b: *std.Build) ?[]const u8 {
    if (b.option([]const u8, "android-ndk", "Path to the Android NDK (for aarch64-linux-android)")) |p| {
        if (p.len > 0) return p;
    }
    if (b.graph.environ_map.get("ANDROID_NDK_HOME")) |p| {
        if (p.len > 0) return p;
    }
    const sdk = b.graph.environ_map.get("ANDROID_HOME") orelse return null;
    const ndk_root = b.fmt("{s}/ndk", .{sdk});
    const io = b.graph.io;
    var dir = std.Io.Dir.openDirAbsolute(io, ndk_root, .{ .iterate = true }) catch return null;
    defer dir.close(io);
    var best: ?[]const u8 = null;
    var it = dir.iterate();
    while (it.next(io) catch return best) |entry| {
        if (entry.kind != .directory) continue;
        if (entry.name.len == 0 or entry.name[0] == '.') continue;
        best = b.fmt("{s}/{s}", .{ ndk_root, entry.name });
    }
    return best;
}

fn shipCross(b: *std.Build, all_step: *std.Build.Step, extra_step: ?*std.Build.Step, exe: *std.Build.Step.Compile, gc: bool, t: CrossTarget, ndk: ?[]const u8) void {
    switch (t.kind) {
        .wasm => applyWasi(exe),
        .android => if (ndk) |n| applyAndroid(exe, b, n),
        else => {},
    }
    // Boehm GC has no WASI port in this tree; keep it on native OS targets.
    const use_gc = gc and t.kind != .wasm;
    attachGc(exe, b, use_gc, t.isWindows());
    const inst = b.addInstallArtifact(exe, .{});
    all_step.dependOn(&inst.step);
    if (extra_step) |s| s.dependOn(&inst.step);
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const enable_gc = b.option(bool, "gc", "Link Boehm GC into every demo") orelse true;
    const clap_step = b.step("clap", "Build ZapotaFilter.clap shared libraries (Win/Linux/macOS)");

    const is_windows_target = target.result.os.tag == .windows;

    // -------------------------------------------------------------
    // Helper: Lua
    // -------------------------------------------------------------
    const configureLua = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/lua"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_lua.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            const lua_srcs = [_][]const u8{
                "vendor/lua/lapi.c",
                "vendor/lua/lauxlib.c",
                "vendor/lua/lbaselib.c",
                "vendor/lua/lcode.c",
                "vendor/lua/lcorolib.c",
                "vendor/lua/lctype.c",
                "vendor/lua/ldblib.c",
                "vendor/lua/ldebug.c",
                "vendor/lua/ldo.c",
                "vendor/lua/ldump.c",
                "vendor/lua/lfunc.c",
                "vendor/lua/lgc.c",
                "vendor/lua/linit.c",
                "vendor/lua/liolib.c",
                "vendor/lua/llex.c",
                "vendor/lua/lmathlib.c",
                "vendor/lua/lmem.c",
                "vendor/lua/loadlib.c",
                "vendor/lua/lobject.c",
                "vendor/lua/lopcodes.c",
                "vendor/lua/loslib.c",
                "vendor/lua/lparser.c",
                "vendor/lua/lstate.c",
                "vendor/lua/lstring.c",
                "vendor/lua/lstrlib.c",
                "vendor/lua/ltable.c",
                "vendor/lua/ltablib.c",
                "vendor/lua/ltm.c",
                "vendor/lua/lundump.c",
                "vendor/lua/lutf8lib.c",
                "vendor/lua/lvm.c",
                "vendor/lua/lzio.c",
            };
            for (lua_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-O2" },
                });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: Flecs
    // -------------------------------------------------------------
    const configureFlecs = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/flecs/distr"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_flecs.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/flecs/distr/flecs.c"),
                .flags = &.{ "-O2" },
            });
            if (is_win) {
                exe.root_module.linkSystemLibrary("ws2_32", .{});
                exe.root_module.linkSystemLibrary("dbghelp", .{});
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: PDCurses
    // -------------------------------------------------------------
    const configurePDCurses = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/pdcurses"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/curses_demo.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });

            const pdcurses_core = [_][]const u8{
                "vendor/pdcurses/pdcurses/addch.c",
                "vendor/pdcurses/pdcurses/addchstr.c",
                "vendor/pdcurses/pdcurses/addstr.c",
                "vendor/pdcurses/pdcurses/attr.c",
                "vendor/pdcurses/pdcurses/beep.c",
                "vendor/pdcurses/pdcurses/bkgd.c",
                "vendor/pdcurses/pdcurses/border.c",
                "vendor/pdcurses/pdcurses/clear.c",
                "vendor/pdcurses/pdcurses/color.c",
                "vendor/pdcurses/pdcurses/delch.c",
                "vendor/pdcurses/pdcurses/deleteln.c",
                "vendor/pdcurses/pdcurses/getch.c",
                "vendor/pdcurses/pdcurses/getstr.c",
                "vendor/pdcurses/pdcurses/getyx.c",
                "vendor/pdcurses/pdcurses/inch.c",
                "vendor/pdcurses/pdcurses/inchstr.c",
                "vendor/pdcurses/pdcurses/initscr.c",
                "vendor/pdcurses/pdcurses/inopts.c",
                "vendor/pdcurses/pdcurses/insch.c",
                "vendor/pdcurses/pdcurses/insstr.c",
                "vendor/pdcurses/pdcurses/instr.c",
                "vendor/pdcurses/pdcurses/kernel.c",
                "vendor/pdcurses/pdcurses/keyname.c",
                "vendor/pdcurses/pdcurses/mouse.c",
                "vendor/pdcurses/pdcurses/move.c",
                "vendor/pdcurses/pdcurses/outopts.c",
                "vendor/pdcurses/pdcurses/overlay.c",
                "vendor/pdcurses/pdcurses/pad.c",
                "vendor/pdcurses/pdcurses/panel.c",
                "vendor/pdcurses/pdcurses/printw.c",
                "vendor/pdcurses/pdcurses/refresh.c",
                "vendor/pdcurses/pdcurses/scanw.c",
                "vendor/pdcurses/pdcurses/scr_dump.c",
                "vendor/pdcurses/pdcurses/scroll.c",
                "vendor/pdcurses/pdcurses/slk.c",
                "vendor/pdcurses/pdcurses/termattr.c",
                "vendor/pdcurses/pdcurses/touch.c",
                "vendor/pdcurses/pdcurses/util.c",
                "vendor/pdcurses/pdcurses/window.c",
                "vendor/pdcurses/pdcurses/debug.c",
            };

            for (pdcurses_core) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-DPDC_WIDE=0", "-DPDC_FORCE_UTF8=0", "-DPDCDEBUG=0" },
                });
            }

            if (is_win) {
                exe.root_module.addIncludePath(builder.path("vendor/pdcurses/wincon"));
                const pdcurses_wincon = [_][]const u8{
                    "vendor/pdcurses/wincon/pdcclip.c",
                    "vendor/pdcurses/wincon/pdcdisp.c",
                    "vendor/pdcurses/wincon/pdcgetsc.c",
                    "vendor/pdcurses/wincon/pdckbd.c",
                    "vendor/pdcurses/wincon/pdcscrn.c",
                    "vendor/pdcurses/wincon/pdcsetsc.c",
                    "vendor/pdcurses/wincon/pdcutil.c",
                };
                for (pdcurses_wincon) |src| {
                    exe.root_module.addCSourceFile(.{
                        .file = builder.path(src),
                        .flags = &.{ "-DPDC_WIDE=0", "-DPDC_FORCE_UTF8=0", "-DPDCDEBUG=0" },
                    });
                }
                exe.root_module.linkSystemLibrary("user32", .{});
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: Raylib
    // -------------------------------------------------------------
    const configureRaylib = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/raylib/src"));
            exe.root_module.addIncludePath(builder.path("vendor/raylib/src/external/glfw/include"));

            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/raylib_demo.c"),
                .flags = &.{ "-Wall", "-Wextra", "-DPLATFORM_DESKTOP" },
            });

            const raylib_srcs = [_][]const u8{
                "vendor/raylib/src/rcore.c",
                "vendor/raylib/src/rshapes.c",
                "vendor/raylib/src/rtextures.c",
                "vendor/raylib/src/rtext.c",
                "vendor/raylib/src/rmodels.c",
                "vendor/raylib/src/raudio.c",
                "vendor/raylib/src/rglfw.c",
            };

            for (raylib_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-DPLATFORM_DESKTOP", "-DGRAPHICS_API_OPENGL_33" },
                });
            }

            if (is_win) {
                exe.root_module.linkSystemLibrary("opengl32", .{});
                exe.root_module.linkSystemLibrary("gdi32", .{});
                exe.root_module.linkSystemLibrary("winmm", .{});
                exe.root_module.linkSystemLibrary("shell32", .{});
                exe.root_module.linkSystemLibrary("user32", .{});
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: cmark
    // -------------------------------------------------------------
    const configureCmark = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/cmark/src"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_cmark.c"),
                .flags = &.{ "-Wall", "-Wextra", "-DCMARK_STATIC_DEFINE" },
            });
            const cmark_srcs = [_][]const u8{
                "vendor/cmark/src/blocks.c",
                "vendor/cmark/src/buffer.c",
                "vendor/cmark/src/cmark_ctype.c",
                "vendor/cmark/src/cmark.c",
                "vendor/cmark/src/commonmark.c",
                "vendor/cmark/src/houdini_href_e.c",
                "vendor/cmark/src/houdini_html_e.c",
                "vendor/cmark/src/houdini_html_u.c",
                "vendor/cmark/src/html.c",
                "vendor/cmark/src/inlines.c",
                "vendor/cmark/src/iterator.c",
                "vendor/cmark/src/latex.c",
                "vendor/cmark/src/man.c",
                "vendor/cmark/src/node.c",
                "vendor/cmark/src/references.c",
                "vendor/cmark/src/render.c",
                "vendor/cmark/src/scanners.c",
                "vendor/cmark/src/utf8.c",
                "vendor/cmark/src/xml.c",
            };
            for (cmark_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-Wall", "-Wextra", "-DCMARK_STATIC_DEFINE" },
                });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: libyaml
    // -------------------------------------------------------------
    const configureLibYaml = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/libyaml/include"));
            exe.root_module.addIncludePath(builder.path("vendor/libyaml/src"));
            const yaml_flags = &.{
                "-Wall",
                "-Wextra",
                "-DYAML_DECLARE_STATIC",
                "-DYAML_VERSION_MAJOR=0",
                "-DYAML_VERSION_MINOR=2",
                "-DYAML_VERSION_PATCH=5",
                "-DYAML_VERSION_STRING=\"0.2.5\"",
            };
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_yaml.c"),
                .flags = yaml_flags,
            });
            const yaml_srcs = [_][]const u8{
                "vendor/libyaml/src/api.c",
                "vendor/libyaml/src/dumper.c",
                "vendor/libyaml/src/emitter.c",
                "vendor/libyaml/src/loader.c",
                "vendor/libyaml/src/parser.c",
                "vendor/libyaml/src/reader.c",
                "vendor/libyaml/src/scanner.c",
                "vendor/libyaml/src/writer.c",
            };
            for (yaml_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = yaml_flags,
                });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: PlutoVG
    // -------------------------------------------------------------
    const configurePlutoVG = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/plutovg/include"));
            exe.root_module.addIncludePath(builder.path("vendor/plutovg/source"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_plutovg.c"),
                .flags = &.{ "-Wall", "-Wextra", "-DPLUTOVG_BUILD_STATIC" },
            });
            const plutovg_srcs = [_][]const u8{
                "vendor/plutovg/source/plutovg-blend.c",
                "vendor/plutovg/source/plutovg-canvas.c",
                "vendor/plutovg/source/plutovg-font.c",
                "vendor/plutovg/source/plutovg-ft-math.c",
                "vendor/plutovg/source/plutovg-ft-raster.c",
                "vendor/plutovg/source/plutovg-ft-stroker.c",
                "vendor/plutovg/source/plutovg-matrix.c",
                "vendor/plutovg/source/plutovg-paint.c",
                "vendor/plutovg/source/plutovg-path.c",
                "vendor/plutovg/source/plutovg-rasterize.c",
                "vendor/plutovg/source/plutovg-surface.c",
            };
            for (plutovg_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-DPLUTOVG_BUILD_STATIC" },
                });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: Box2D
    // -------------------------------------------------------------
    const configureBox2D = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/box2d/include"));
            exe.root_module.addIncludePath(builder.path("vendor/box2d/src"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_box2d.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            const box2d_srcs = [_][]const u8{
                "vendor/box2d/src/aabb.c",
                "vendor/box2d/src/arena_allocator.c",
                "vendor/box2d/src/bitset.c",
                "vendor/box2d/src/body.c",
                "vendor/box2d/src/broad_phase.c",
                "vendor/box2d/src/constraint_graph.c",
                "vendor/box2d/src/contact_solver.c",
                "vendor/box2d/src/contact.c",
                "vendor/box2d/src/core.c",
                "vendor/box2d/src/distance_joint.c",
                "vendor/box2d/src/distance.c",
                "vendor/box2d/src/dynamic_tree.c",
                "vendor/box2d/src/geometry.c",
                "vendor/box2d/src/hull.c",
                "vendor/box2d/src/id_pool.c",
                "vendor/box2d/src/island.c",
                "vendor/box2d/src/joint.c",
                "vendor/box2d/src/manifold.c",
                "vendor/box2d/src/math_functions.c",
                "vendor/box2d/src/motor_joint.c",
                "vendor/box2d/src/mover_joint.c",
                "vendor/box2d/src/mover.c",
                "vendor/box2d/src/parallel_for.c",
                "vendor/box2d/src/physics_world.c",
                "vendor/box2d/src/pogo_joint.c",
                "vendor/box2d/src/prismatic_joint.c",
                "vendor/box2d/src/recording_replay.c",
                "vendor/box2d/src/recording.c",
                "vendor/box2d/src/revolute_joint.c",
                "vendor/box2d/src/scheduler.c",
                "vendor/box2d/src/sensor.c",
                "vendor/box2d/src/shape.c",
                "vendor/box2d/src/solver_set.c",
                "vendor/box2d/src/solver.c",
                "vendor/box2d/src/table.c",
                "vendor/box2d/src/timer.c",
                "vendor/box2d/src/types.c",
                "vendor/box2d/src/weld_joint.c",
                "vendor/box2d/src/wheel_joint.c",
                "vendor/box2d/src/world_snapshot.c",
            };
            for (box2d_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-O2" },
                });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: x264
    // -------------------------------------------------------------
    const configureX264 = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/x264"));
            const x264_flags = &.{
                "-O2",
                "-DHAVE_CONFIG_H",
            };
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_x264.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });

            const common_srcs = [_][]const u8{
                "vendor/x264/common/base.c",
                "vendor/x264/common/bitstream.c",
                "vendor/x264/common/cabac.c",
                "vendor/x264/common/common.c",
                "vendor/x264/common/cpu.c",
                "vendor/x264/common/dct.c",
                "vendor/x264/common/deblock.c",
                "vendor/x264/common/frame.c",
                "vendor/x264/common/macroblock.c",
                "vendor/x264/common/mc.c",
                "vendor/x264/common/mvpred.c",
                "vendor/x264/common/osdep.c",
                "vendor/x264/common/pixel.c",
                "vendor/x264/common/predict.c",
                "vendor/x264/common/quant.c",
                "vendor/x264/common/rectangle.c",
                "vendor/x264/common/set.c",
                "vendor/x264/common/tables.c",
                "vendor/x264/common/threadpool.c",
                "vendor/x264/common/vlc.c",
            };
            for (common_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = x264_flags });
            }

            if (is_win) {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path("vendor/x264/common/win32thread.c"),
                    .flags = x264_flags,
                });
            }

            const encoder_srcs = [_][]const u8{
                "vendor/x264/encoder/analyse.c",
                "vendor/x264/encoder/api.c",
                "vendor/x264/encoder/cabac.c",
                "vendor/x264/encoder/cavlc.c",
                "vendor/x264/encoder/encoder.c",
                "vendor/x264/encoder/lookahead.c",
                "vendor/x264/encoder/macroblock.c",
                "vendor/x264/encoder/me.c",
                "vendor/x264/encoder/ratecontrol.c",
                "vendor/x264/encoder/set.c",
            };
            for (encoder_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = x264_flags });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: Opus
    // -------------------------------------------------------------
    const configureOpus = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/opus"));
            exe.root_module.addIncludePath(builder.path("vendor/opus/include"));
            exe.root_module.addIncludePath(builder.path("vendor/opus/celt"));
            exe.root_module.addIncludePath(builder.path("vendor/opus/silk"));
            exe.root_module.addIncludePath(builder.path("vendor/opus/silk/float"));

            const opus_flags = &.{
                "-O2",
                "-DHAVE_CONFIG_H",
                "-DOPUS_BUILD",
            };

            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_opus.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });

            const opus_srcs = [_][]const u8{
                "vendor/opus/src/analysis.c",
                "vendor/opus/src/extensions.c",
                "vendor/opus/src/mapping_matrix.c",
                "vendor/opus/src/mlp.c",
                "vendor/opus/src/mlp_data.c",
                "vendor/opus/src/opus.c",
                "vendor/opus/src/opus_decoder.c",
                "vendor/opus/src/opus_encoder.c",
                "vendor/opus/src/opus_multistream.c",
                "vendor/opus/src/opus_multistream_decoder.c",
                "vendor/opus/src/opus_multistream_encoder.c",
                "vendor/opus/src/opus_projection_decoder.c",
                "vendor/opus/src/opus_projection_encoder.c",
                "vendor/opus/src/repacketizer.c",
            };
            for (opus_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = opus_flags });
            }

            const celt_srcs = [_][]const u8{
                "vendor/opus/celt/bands.c",
                "vendor/opus/celt/celt.c",
                "vendor/opus/celt/celt_decoder.c",
                "vendor/opus/celt/celt_encoder.c",
                "vendor/opus/celt/celt_lpc.c",
                "vendor/opus/celt/cwrs.c",
                "vendor/opus/celt/entcode.c",
                "vendor/opus/celt/entdec.c",
                "vendor/opus/celt/entenc.c",
                "vendor/opus/celt/kiss_fft.c",
                "vendor/opus/celt/laplace.c",
                "vendor/opus/celt/mathops.c",
                "vendor/opus/celt/mdct.c",
                "vendor/opus/celt/modes.c",
                "vendor/opus/celt/pitch.c",
                "vendor/opus/celt/quant_bands.c",
                "vendor/opus/celt/rate.c",
                "vendor/opus/celt/vq.c",
            };
            for (celt_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = opus_flags });
            }

            const silk_srcs = [_][]const u8{
                "vendor/opus/silk/A2NLSF.c",
                "vendor/opus/silk/CNG.c",
                "vendor/opus/silk/HP_variable_cutoff.c",
                "vendor/opus/silk/LPC_analysis_filter.c",
                "vendor/opus/silk/LPC_fit.c",
                "vendor/opus/silk/LPC_inv_pred_gain.c",
                "vendor/opus/silk/LP_variable_cutoff.c",
                "vendor/opus/silk/NLSF2A.c",
                "vendor/opus/silk/NLSF_VQ.c",
                "vendor/opus/silk/NLSF_VQ_weights_laroia.c",
                "vendor/opus/silk/NLSF_decode.c",
                "vendor/opus/silk/NLSF_del_dec_quant.c",
                "vendor/opus/silk/NLSF_encode.c",
                "vendor/opus/silk/NLSF_stabilize.c",
                "vendor/opus/silk/NLSF_unpack.c",
                "vendor/opus/silk/NSQ.c",
                "vendor/opus/silk/NSQ_del_dec.c",
                "vendor/opus/silk/PLC.c",
                "vendor/opus/silk/VAD.c",
                "vendor/opus/silk/VQ_WMat_EC.c",
                "vendor/opus/silk/ana_filt_bank_1.c",
                "vendor/opus/silk/biquad_alt.c",
                "vendor/opus/silk/bwexpander.c",
                "vendor/opus/silk/bwexpander_32.c",
                "vendor/opus/silk/check_control_input.c",
                "vendor/opus/silk/code_signs.c",
                "vendor/opus/silk/control_SNR.c",
                "vendor/opus/silk/control_audio_bandwidth.c",
                "vendor/opus/silk/control_codec.c",
                "vendor/opus/silk/debug.c",
                "vendor/opus/silk/dec_API.c",
                "vendor/opus/silk/decode_core.c",
                "vendor/opus/silk/decode_frame.c",
                "vendor/opus/silk/decode_indices.c",
                "vendor/opus/silk/decode_parameters.c",
                "vendor/opus/silk/decode_pitch.c",
                "vendor/opus/silk/decode_pulses.c",
                "vendor/opus/silk/decoder_set_fs.c",
                "vendor/opus/silk/enc_API.c",
                "vendor/opus/silk/encode_indices.c",
                "vendor/opus/silk/encode_pulses.c",
                "vendor/opus/silk/gain_quant.c",
                "vendor/opus/silk/init_decoder.c",
                "vendor/opus/silk/init_encoder.c",
                "vendor/opus/silk/inner_prod_aligned.c",
                "vendor/opus/silk/interpolate.c",
                "vendor/opus/silk/lin2log.c",
                "vendor/opus/silk/log2lin.c",
                "vendor/opus/silk/pitch_est_tables.c",
                "vendor/opus/silk/process_NLSFs.c",
                "vendor/opus/silk/quant_LTP_gains.c",
                "vendor/opus/silk/resampler.c",
                "vendor/opus/silk/resampler_down2.c",
                "vendor/opus/silk/resampler_down2_3.c",
                "vendor/opus/silk/resampler_private_AR2.c",
                "vendor/opus/silk/resampler_private_IIR_FIR.c",
                "vendor/opus/silk/resampler_private_down_FIR.c",
                "vendor/opus/silk/resampler_private_up2_HQ.c",
                "vendor/opus/silk/resampler_rom.c",
                "vendor/opus/silk/shell_coder.c",
                "vendor/opus/silk/sigm_Q15.c",
                "vendor/opus/silk/sort.c",
                "vendor/opus/silk/stereo_LR_to_MS.c",
                "vendor/opus/silk/stereo_MS_to_LR.c",
                "vendor/opus/silk/stereo_decode_pred.c",
                "vendor/opus/silk/stereo_encode_pred.c",
                "vendor/opus/silk/stereo_find_predictor.c",
                "vendor/opus/silk/stereo_quant_pred.c",
                "vendor/opus/silk/sum_sqr_shift.c",
                "vendor/opus/silk/table_LSF_cos.c",
                "vendor/opus/silk/tables_LTP.c",
                "vendor/opus/silk/tables_NLSF_CB_NB_MB.c",
                "vendor/opus/silk/tables_NLSF_CB_WB.c",
                "vendor/opus/silk/tables_gain.c",
                "vendor/opus/silk/tables_other.c",
                "vendor/opus/silk/tables_pitch_lag.c",
                "vendor/opus/silk/tables_pulses_per_block.c",
            };
            for (silk_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = opus_flags });
            }

            const silk_float_srcs = [_][]const u8{
                "vendor/opus/silk/float/LPC_analysis_filter_FLP.c",
                "vendor/opus/silk/float/LPC_inv_pred_gain_FLP.c",
                "vendor/opus/silk/float/LTP_analysis_filter_FLP.c",
                "vendor/opus/silk/float/LTP_scale_ctrl_FLP.c",
                "vendor/opus/silk/float/apply_sine_window_FLP.c",
                "vendor/opus/silk/float/autocorrelation_FLP.c",
                "vendor/opus/silk/float/burg_modified_FLP.c",
                "vendor/opus/silk/float/bwexpander_FLP.c",
                "vendor/opus/silk/float/corrMatrix_FLP.c",
                "vendor/opus/silk/float/encode_frame_FLP.c",
                "vendor/opus/silk/float/energy_FLP.c",
                "vendor/opus/silk/float/find_LPC_FLP.c",
                "vendor/opus/silk/float/find_LTP_FLP.c",
                "vendor/opus/silk/float/find_pitch_lags_FLP.c",
                "vendor/opus/silk/float/find_pred_coefs_FLP.c",
                "vendor/opus/silk/float/inner_product_FLP.c",
                "vendor/opus/silk/float/k2a_FLP.c",
                "vendor/opus/silk/float/noise_shape_analysis_FLP.c",
                "vendor/opus/silk/float/pitch_analysis_core_FLP.c",
                "vendor/opus/silk/float/process_gains_FLP.c",
                "vendor/opus/silk/float/regularize_correlations_FLP.c",
                "vendor/opus/silk/float/residual_energy_FLP.c",
                "vendor/opus/silk/float/scale_copy_vector_FLP.c",
                "vendor/opus/silk/float/scale_vector_FLP.c",
                "vendor/opus/silk/float/schur_FLP.c",
                "vendor/opus/silk/float/sort_FLP.c",
                "vendor/opus/silk/float/warped_autocorrelation_FLP.c",
                "vendor/opus/silk/float/wrappers_FLP.c",
            };
            for (silk_float_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = opus_flags });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: LAME
    // -------------------------------------------------------------
    const configureLame = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/libmp3lame"));
            exe.root_module.addIncludePath(builder.path("vendor/libmp3lame/include"));
            exe.root_module.addIncludePath(builder.path("vendor/libmp3lame/libmp3lame"));

            const lame_flags = &.{
                "-O2",
                "-DHAVE_CONFIG_H",
                "-fno-sanitize=undefined",
            };

            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_lame.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });

            const lame_srcs = [_][]const u8{
                "vendor/libmp3lame/libmp3lame/VbrTag.c",
                "vendor/libmp3lame/libmp3lame/bitstream.c",
                "vendor/libmp3lame/libmp3lame/encoder.c",
                "vendor/libmp3lame/libmp3lame/fft.c",
                "vendor/libmp3lame/libmp3lame/gain_analysis.c",
                "vendor/libmp3lame/libmp3lame/id3tag.c",
                "vendor/libmp3lame/libmp3lame/lame.c",
                "vendor/libmp3lame/libmp3lame/newmdct.c",
                "vendor/libmp3lame/libmp3lame/presets.c",
                "vendor/libmp3lame/libmp3lame/psymodel.c",
                "vendor/libmp3lame/libmp3lame/quantize.c",
                "vendor/libmp3lame/libmp3lame/quantize_pvt.c",
                "vendor/libmp3lame/libmp3lame/reservoir.c",
                "vendor/libmp3lame/libmp3lame/set_get.c",
                "vendor/libmp3lame/libmp3lame/tables.c",
                "vendor/libmp3lame/libmp3lame/takehiro.c",
                "vendor/libmp3lame/libmp3lame/util.c",
                "vendor/libmp3lame/libmp3lame/vbrquantize.c",
                "vendor/libmp3lame/libmp3lame/version.c",
            };
            for (lame_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = lame_flags });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: Duktape (amalgamation build)
    // -------------------------------------------------------------
    const configureDuktape = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/duktape-amal"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_duktape.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/duktape-amal/duktape.c"),
                .flags = &.{ "-O2", "-fno-sanitize=undefined" },
            });
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: GGML (minimal tensor core)
    // -------------------------------------------------------------
    const configureGGML = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/ggml/include"));
            exe.root_module.addIncludePath(builder.path("vendor/ggml/src"));
            exe.root_module.addIncludePath(builder.path("vendor/ggml/src/ggml-cpu"));

            const ggml_flags = &.{
                "-O2",
                "-fno-sanitize=undefined",
                "-DGGML_USE_CPU",
            };
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_ggml.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/ggml/src/ggml.c"),
                .flags = ggml_flags,
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/ggml/src/ggml-quants.c"),
                .flags = ggml_flags,
            });
            // Stubs for backend/threading/cpu symbols not needed in minimal demo
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/ggml_backend_stubs.c"),
                .flags = ggml_flags,
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/ggml_threading_stubs.c"),
                .flags = ggml_flags,
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/ggml_cpu_stubs.c"),
                .flags = ggml_flags,
            });
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: JSON-C
    // -------------------------------------------------------------
    const configureJsonC = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/json-c"));
            const jsonc_flags = &.{ "-O2", "-DHAVE_STDINT_H=1" };

            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_json_c.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });

            const jsonc_srcs = [_][]const u8{
                "vendor/json-c/arraylist.c",
                "vendor/json-c/debug.c",
                "vendor/json-c/json_c_version.c",
                "vendor/json-c/json_object.c",
                "vendor/json-c/json_object_iterator.c",
                "vendor/json-c/json_tokener.c",
                "vendor/json-c/json_util.c",
                "vendor/json-c/json_visit.c",
                "vendor/json-c/json_pointer.c",
                "vendor/json-c/json_patch.c",
                "vendor/json-c/linkhash.c",
                "vendor/json-c/printbuf.c",
                "vendor/json-c/random_seed.c",
                "vendor/json-c/strerror_override.c",
            };
            for (jsonc_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = jsonc_flags });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: Libsixel
    // -------------------------------------------------------------
    const configureLibsixel = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/libsixel/include"));
            exe.root_module.addIncludePath(builder.path("vendor/libsixel/src"));

            const sixel_flags = &.{
                "-O2",
                "-DSIXEL_STATIC",
                "-DHAVE_ERRNO_H",
                "-DHAVE_STDLIB_H",
                "-DHAVE_STRING_H",
                "-DHAVE_STDIO_H",
                "-DHAVE_MATH_H",
                "-DHAVE_INTTYPES_H",
                "-DHAVE_STDINT_H",
            };

            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_sixel.c"),
                .flags = &.{ "-Wall", "-Wextra", "-DSIXEL_STATIC" },
            });

            const sixel_srcs = [_][]const u8{
                "vendor/libsixel/src/output.c",
                "vendor/libsixel/src/fromsixel.c",
                "vendor/libsixel/src/tosixel.c",
                "vendor/libsixel/src/encoder.c",
                "vendor/libsixel/src/decoder.c",
                "vendor/libsixel/src/dither.c",
                "vendor/libsixel/src/frame.c",
                "vendor/libsixel/src/chunk.c",
                "vendor/libsixel/src/loader.c",
                "vendor/libsixel/src/quant.c",
                "vendor/libsixel/src/pixelformat.c",
                "vendor/libsixel/src/status.c",
                "vendor/libsixel/src/writer.c",
                "vendor/libsixel/src/allocator.c",
                "vendor/libsixel/src/tty.c",
                "vendor/libsixel/src/scale.c",
                "vendor/libsixel/src/frompnm.c",
                "vendor/libsixel/src/fromgif.c",
            };
            for (sixel_srcs) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = sixel_flags });
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: NanoVG (null/headless backend)
    // -------------------------------------------------------------
    const configureNanoVG = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/nanovg/src"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_nanovg.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/nanovg/src/nanovg.c"),
                .flags = &.{ "-O2" },
            });
        }
    }.apply;

    // -------------------------------------------------------------
    // Helper: CLAP plugin (header-only CLAP + NanoVG)
    // -------------------------------------------------------------
    const configureCLAP = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, as_plugin: bool, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/clap/include"));
            exe.root_module.addIncludePath(builder.path("vendor/nanovg/src"));
            const flags: []const []const u8 = if (as_plugin)
                &.{ "-Wall", "-Wextra", "-DZAPOTA_CLAP_LIB", "-fvisibility=hidden" }
            else
                &.{ "-Wall", "-Wextra" };
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_clap.c"),
                .flags = flags,
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/zapota_ui.c"),
                .flags = flags,
            });
            if (is_win) {
                exe.root_module.linkSystemLibrary("user32", .{});
                exe.root_module.linkSystemLibrary("gdi32", .{});
            }
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/nanovg/src/nanovg.c"),
                .flags = &.{ "-O2" },
            });
        }
    }.apply;

    const configureSokol = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/sokol"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_sokol.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
        }
    }.apply;

    const configureHiredis = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/hiredis"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_hiredis.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            const hiredis_srcs = [_][]const u8{
                "vendor/hiredis/alloc.c",
                "vendor/hiredis/async.c",
                "vendor/hiredis/dict.c",
                "vendor/hiredis/hiredis.c",
                "vendor/hiredis/net.c",
                "vendor/hiredis/read.c",
                "vendor/hiredis/sds.c",
                "vendor/hiredis/sockcompat.c",
            };
            for (hiredis_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-O2" },
                });
            }
            if (is_win) exe.root_module.linkSystemLibrary("ws2_32", .{});
        }
    }.apply;

    const configureProtobufC = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/protobuf-c"));
            exe.root_module.addIncludePath(builder.path("vendor/protobuf-c/protobuf-c"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_protobuf_c.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/protobuf-c/protobuf-c/protobuf-c.c"),
                .flags = &.{ "-O2" },
            });
        }
    }.apply;

    const configureWslay = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/wslay/lib/includes"));
            exe.root_module.addIncludePath(builder.path("vendor/wslay/lib"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_wslay.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            const net_flag: []const u8 = if (is_win) "-DHAVE_WINSOCK2_H" else "-DHAVE_ARPA_INET_H";
            const wslay_srcs = [_][]const u8{
                "vendor/wslay/lib/wslay_event.c",
                "vendor/wslay/lib/wslay_frame.c",
                "vendor/wslay/lib/wslay_net.c",
                "vendor/wslay/lib/wslay_queue.c",
            };
            for (wslay_srcs) |src| {
                exe.root_module.addCSourceFile(.{
                    .file = builder.path(src),
                    .flags = &.{ "-O2", net_flag },
                });
            }
            if (is_win) exe.root_module.linkSystemLibrary("ws2_32", .{});
        }
    }.apply;

    const configureGcDemo = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/bdwgc/include"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_gc.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
        }
    }.apply;

    const configureLinenoise = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/linenoise"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_linenoise.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("vendor/linenoise/linenoise.c"),
                .flags = &.{ "-O2" },
            });
        }
    }.apply;

    const configureLibuv = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool, is_linux: bool, is_mac: bool, is_android: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/libuv/include"));
            exe.root_module.addIncludePath(builder.path("vendor/libuv/src"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_libuv.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            const common = [_][]const u8{
                "vendor/libuv/src/fs-poll.c",
                "vendor/libuv/src/idna.c",
                "vendor/libuv/src/inet.c",
                "vendor/libuv/src/random.c",
                "vendor/libuv/src/strscpy.c",
                "vendor/libuv/src/strtok.c",
                "vendor/libuv/src/thread-common.c",
                "vendor/libuv/src/threadpool.c",
                "vendor/libuv/src/timer.c",
                "vendor/libuv/src/uv-common.c",
                "vendor/libuv/src/uv-data-getter-setters.c",
                "vendor/libuv/src/version.c",
            };
            const flags_win: []const []const u8 = &.{ "-O2", "-DWIN32_LEAN_AND_MEAN", "-D_WIN32_WINNT=0x0A00" };
            const flags_unix: []const []const u8 = &.{ "-O2", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-D_GNU_SOURCE" };
            const flags = if (is_win) flags_win else flags_unix;
            for (common) |src| {
                exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = flags });
            }
            if (is_win) {
                const win = [_][]const u8{
                    "vendor/libuv/src/win/async.c",
                    "vendor/libuv/src/win/core.c",
                    "vendor/libuv/src/win/detect-wakeup.c",
                    "vendor/libuv/src/win/dl.c",
                    "vendor/libuv/src/win/error.c",
                    "vendor/libuv/src/win/fs.c",
                    "vendor/libuv/src/win/fs-event.c",
                    "vendor/libuv/src/win/getaddrinfo.c",
                    "vendor/libuv/src/win/getnameinfo.c",
                    "vendor/libuv/src/win/handle.c",
                    "vendor/libuv/src/win/loop-watcher.c",
                    "vendor/libuv/src/win/pipe.c",
                    "vendor/libuv/src/win/thread.c",
                    "vendor/libuv/src/win/poll.c",
                    "vendor/libuv/src/win/process.c",
                    "vendor/libuv/src/win/process-stdio.c",
                    "vendor/libuv/src/win/signal.c",
                    "vendor/libuv/src/win/snprintf.c",
                    "vendor/libuv/src/win/stream.c",
                    "vendor/libuv/src/win/tcp.c",
                    "vendor/libuv/src/win/tty.c",
                    "vendor/libuv/src/win/udp.c",
                    "vendor/libuv/src/win/util.c",
                    "vendor/libuv/src/win/winapi.c",
                    "vendor/libuv/src/win/winsock.c",
                };
                for (win) |src| {
                    exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = flags });
                }
                for ([_][]const u8{ "psapi", "user32", "advapi32", "iphlpapi", "userenv", "ws2_32", "dbghelp", "ole32", "shell32", "api-ms-win-core-synch-l1-2-0" }) |lib| {
                    exe.root_module.linkSystemLibrary(lib, .{});
                }
            } else {
                const unix = [_][]const u8{
                    "vendor/libuv/src/unix/async.c",
                    "vendor/libuv/src/unix/core.c",
                    "vendor/libuv/src/unix/dl.c",
                    "vendor/libuv/src/unix/fs.c",
                    "vendor/libuv/src/unix/getaddrinfo.c",
                    "vendor/libuv/src/unix/getnameinfo.c",
                    "vendor/libuv/src/unix/loop-watcher.c",
                    "vendor/libuv/src/unix/loop.c",
                    "vendor/libuv/src/unix/pipe.c",
                    "vendor/libuv/src/unix/poll.c",
                    "vendor/libuv/src/unix/process.c",
                    "vendor/libuv/src/unix/random-devurandom.c",
                    "vendor/libuv/src/unix/signal.c",
                    "vendor/libuv/src/unix/stream.c",
                    "vendor/libuv/src/unix/tcp.c",
                    "vendor/libuv/src/unix/thread.c",
                    "vendor/libuv/src/unix/tty.c",
                    "vendor/libuv/src/unix/udp.c",
                };
                for (unix) |src| {
                    exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = flags });
                }
                if (is_linux or is_android) {
                    const linux = [_][]const u8{
                        "vendor/libuv/src/unix/linux.c",
                        "vendor/libuv/src/unix/procfs-exepath.c",
                        "vendor/libuv/src/unix/random-getrandom.c",
                        "vendor/libuv/src/unix/random-sysctl-linux.c",
                        "vendor/libuv/src/unix/proctitle.c",
                    };
                    for (linux) |src| {
                        exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = flags });
                    }
                    if (is_android) {
                        exe.root_module.addCSourceFile(.{
                            .file = builder.path("vendor/libuv/src/unix/random-getentropy.c"),
                            .flags = flags,
                        });
                    }
                }
                if (is_mac) {
                    const mac = [_][]const u8{
                        "vendor/libuv/src/unix/proctitle.c",
                        "vendor/libuv/src/unix/bsd-ifaddrs.c",
                        "vendor/libuv/src/unix/kqueue.c",
                        "vendor/libuv/src/unix/darwin-proctitle.c",
                        "vendor/libuv/src/unix/darwin.c",
                        "vendor/libuv/src/unix/fsevents.c",
                        "vendor/libuv/src/unix/random-getentropy.c",
                    };
                    for (mac) |src| {
                        exe.root_module.addCSourceFile(.{ .file = builder.path(src), .flags = flags });
                    }
                }
            }
        }
    }.apply;

    const configureLibsodium = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addIncludePath(builder.path("vendor/libsodium/src/libsodium/include"));
            exe.root_module.addIncludePath(builder.path("vendor/libsodium/src/libsodium/include/sodium"));
            exe.root_module.addCMacro("SODIUM_STATIC", "1");
            exe.root_module.addCMacro("CONFIGURED", "1");
            exe.root_module.addCMacro("NATIVE_LITTLE_ENDIAN", "1");
            exe.root_module.addCMacro("HAVE_INTTYPES_H", "1");
            exe.root_module.addCMacro("HAVE_STDINT_H", "1");
            exe.root_module.addCMacro("_GNU_SOURCE", "1");
            if (is_win) {
                exe.root_module.addCMacro("HAVE_RAISE", "1");
                exe.root_module.addCMacro("HAVE_SYS_PARAM_H", "1");
            } else {
                exe.root_module.addCMacro("HAVE_POSIX_MEMALIGN", "1");
                exe.root_module.addCMacro("HAVE_PTHREAD", "1");
                exe.root_module.addCMacro("HAVE_NANOSLEEP", "1");
                exe.root_module.addCMacro("HAVE_MMAP", "1");
                exe.root_module.addCMacro("HAVE_SYS_RANDOM_H", "1");
            }
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_libsodium.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            addCTree(exe, builder, "vendor/libsodium/src/libsodium", &.{ "-O2", "-fno-strict-aliasing", "-fwrapv" });
            if (is_win) exe.root_module.linkSystemLibrary("advapi32", .{});
        }
    }.apply;

    const configureMiniaudio = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/miniaudio"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_miniaudio.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
        }
    }.apply;

    const configureDrWav = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build) void {
            exe.root_module.addIncludePath(builder.path("vendor/dr_libs"));
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_dr_wav.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
        }
    }.apply;

    const configureWindow = struct {
        fn apply(exe: *std.Build.Step.Compile, builder: *std.Build, is_win: bool) void {
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/demo_window.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            exe.root_module.addCSourceFile(.{
                .file = builder.path("src/zapota_ui.c"),
                .flags = &.{ "-Wall", "-Wextra" },
            });
            if (is_win) {
                exe.root_module.linkSystemLibrary("user32", .{});
                exe.root_module.linkSystemLibrary("gdi32", .{});
            }
        }
    }.apply;

    // -------------------------------------------------------------
    // 1. Host Demo Executables & Run Steps
    // -------------------------------------------------------------

    // 1.1 Hello World
    const hello_exe = b.addExecutable(.{
        .name = "hello",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    hello_exe.root_module.addCSourceFile(.{ .file = b.path("src/main.c"), .flags = &.{ "-Wall", "-Wextra" } });
    shipHost(b, hello_exe, enable_gc, is_windows_target);
    const run_hello = b.addRunArtifact(hello_exe);
    run_hello.step.dependOn(b.getInstallStep());
    b.step("run", "Run Hello World demo").dependOn(&run_hello.step);

    // 1.2 Lua 5.4
    const lua_exe = b.addExecutable(.{
        .name = "demo_lua",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureLua(lua_exe, b);
    shipHost(b, lua_exe, enable_gc, is_windows_target);
    const run_lua = b.addRunArtifact(lua_exe);
    run_lua.step.dependOn(b.getInstallStep());
    b.step("run-lua", "Run Lua 5.4 demo").dependOn(&run_lua.step);

    // 1.3 cmark
    const cmark_exe = b.addExecutable(.{
        .name = "demo_cmark",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureCmark(cmark_exe, b);
    shipHost(b, cmark_exe, enable_gc, is_windows_target);
    const run_cmark = b.addRunArtifact(cmark_exe);
    run_cmark.step.dependOn(b.getInstallStep());
    b.step("run-cmark", "Run cmark Markdown demo").dependOn(&run_cmark.step);

    // 1.4 libyaml
    const yaml_exe = b.addExecutable(.{
        .name = "demo_yaml",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureLibYaml(yaml_exe, b);
    shipHost(b, yaml_exe, enable_gc, is_windows_target);
    const run_yaml = b.addRunArtifact(yaml_exe);
    run_yaml.step.dependOn(b.getInstallStep());
    b.step("run-yaml", "Run LibYAML demo").dependOn(&run_yaml.step);

    // 1.5 Flecs ECS
    const flecs_exe = b.addExecutable(.{
        .name = "demo_flecs",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureFlecs(flecs_exe, b, is_windows_target);
    shipHost(b, flecs_exe, enable_gc, is_windows_target);
    const run_flecs = b.addRunArtifact(flecs_exe);
    run_flecs.step.dependOn(b.getInstallStep());
    b.step("run-flecs", "Run Flecs ECS demo").dependOn(&run_flecs.step);

    // 1.6 Box2D Physics
    const box2d_exe = b.addExecutable(.{
        .name = "demo_box2d",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureBox2D(box2d_exe, b);
    shipHost(b, box2d_exe, enable_gc, is_windows_target);
    const run_box2d = b.addRunArtifact(box2d_exe);
    run_box2d.step.dependOn(b.getInstallStep());
    b.step("run-box2d", "Run Box2D physics demo").dependOn(&run_box2d.step);

    // 1.7 PlutoVG
    const plutovg_exe = b.addExecutable(.{
        .name = "demo_plutovg",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configurePlutoVG(plutovg_exe, b);
    shipHost(b, plutovg_exe, enable_gc, is_windows_target);
    const run_plutovg = b.addRunArtifact(plutovg_exe);
    run_plutovg.step.dependOn(b.getInstallStep());
    b.step("run-plutovg", "Run PlutoVG vector graphics demo").dependOn(&run_plutovg.step);

    // 1.8 Nuklear GUI
    const nuklear_exe = b.addExecutable(.{
        .name = "demo_nuklear",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    nuklear_exe.root_module.addIncludePath(b.path("vendor/nuklear"));
    nuklear_exe.root_module.addCSourceFile(.{ .file = b.path("src/demo_nuklear.c"), .flags = &.{ "-Wall", "-Wextra" } });
    shipHost(b, nuklear_exe, enable_gc, is_windows_target);
    const run_nuklear = b.addRunArtifact(nuklear_exe);
    run_nuklear.step.dependOn(b.getInstallStep());
    b.step("run-nuklear", "Run Nuklear GUI demo").dependOn(&run_nuklear.step);

    // 1.9 H2O HTTP Server
    const http_exe = b.addExecutable(.{
        .name = "http_server",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    http_exe.root_module.addIncludePath(b.path("vendor/h2o/include"));
    http_exe.root_module.addIncludePath(b.path("vendor/h2o/deps/picohttpparser"));
    http_exe.root_module.addCSourceFile(.{ .file = b.path("src/http_server.c"), .flags = &.{ "-Wall", "-Wextra" } });
    http_exe.root_module.addCSourceFile(.{ .file = b.path("vendor/h2o/deps/picohttpparser/picohttpparser.c"), .flags = &.{ "-Wall", "-Wextra" } });
    if (is_windows_target) http_exe.root_module.linkSystemLibrary("ws2_32", .{});
    shipHost(b, http_exe, enable_gc, is_windows_target);
    const run_http = b.addRunArtifact(http_exe);
    run_http.step.dependOn(b.getInstallStep());
    b.step("run-http", "Run H2O HTTP server demo").dependOn(&run_http.step);

    // 1.10 SQLite
    const sqlite_exe = b.addExecutable(.{
        .name = "sqlite_demo",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    sqlite_exe.root_module.addIncludePath(b.path("vendor/sqlite"));
    sqlite_exe.root_module.addCSourceFile(.{ .file = b.path("src/sqlite_demo.c"), .flags = &.{ "-Wall", "-Wextra" } });
    sqlite_exe.root_module.addCSourceFile(.{ .file = b.path("vendor/sqlite/sqlite3.c"), .flags = &.{ "-DSQLITE_THREADSAFE=0", "-DSQLITE_OMIT_LOAD_EXTENSION" } });
    shipHost(b, sqlite_exe, enable_gc, is_windows_target);
    const run_sqlite = b.addRunArtifact(sqlite_exe);
    run_sqlite.step.dependOn(b.getInstallStep());
    b.step("run-sqlite", "Run SQLite demo").dependOn(&run_sqlite.step);

    // 1.11 PDCurses
    const curses_exe = b.addExecutable(.{
        .name = "curses_demo",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configurePDCurses(curses_exe, b, is_windows_target);
    shipHost(b, curses_exe, enable_gc, is_windows_target);
    const run_curses = b.addRunArtifact(curses_exe);
    run_curses.step.dependOn(b.getInstallStep());
    b.step("run-curses", "Run PDCurses TUI demo").dependOn(&run_curses.step);

    // 1.12 Raylib
    const raylib_exe = b.addExecutable(.{
        .name = "raylib_demo",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureRaylib(raylib_exe, b, is_windows_target);
    shipHost(b, raylib_exe, enable_gc, is_windows_target);
    const run_raylib = b.addRunArtifact(raylib_exe);
    run_raylib.step.dependOn(b.getInstallStep());
    b.step("run-raylib", "Run Raylib demo").dependOn(&run_raylib.step);

    // 1.13 x264 Video Encoder
    const x264_exe = b.addExecutable(.{
        .name = "demo_x264",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureX264(x264_exe, b, is_windows_target);
    shipHost(b, x264_exe, enable_gc, is_windows_target);
    const run_x264 = b.addRunArtifact(x264_exe);
    b.step("run-x264", "Run x264 video encoder demo").dependOn(&run_x264.step);

    // 1.14 Opus Audio Codec
    const opus_exe = b.addExecutable(.{
        .name = "demo_opus",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureOpus(opus_exe, b);
    shipHost(b, opus_exe, enable_gc, is_windows_target);
    const run_opus = b.addRunArtifact(opus_exe);
    b.step("run-opus", "Run Opus audio codec demo").dependOn(&run_opus.step);

    // 1.15 LAME MP3 Encoder
    const lame_exe = b.addExecutable(.{
        .name = "demo_lame",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureLame(lame_exe, b);
    shipHost(b, lame_exe, enable_gc, is_windows_target);
    const run_lame = b.addRunArtifact(lame_exe);
    b.step("run-lame", "Run LAME MP3 audio encoder demo").dependOn(&run_lame.step);

    // 1.16 Duktape JavaScript Engine
    const duktape_exe = b.addExecutable(.{
        .name = "demo_duktape",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureDuktape(duktape_exe, b);
    shipHost(b, duktape_exe, enable_gc, is_windows_target);
    const run_duktape = b.addRunArtifact(duktape_exe);
    b.step("run-duktape", "Run Duktape JavaScript engine demo").dependOn(&run_duktape.step);

    // 1.17 GGML Tensor Computation
    const ggml_exe = b.addExecutable(.{
        .name = "demo_ggml",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureGGML(ggml_exe, b);
    shipHost(b, ggml_exe, enable_gc, is_windows_target);
    const run_ggml = b.addRunArtifact(ggml_exe);
    b.step("run-ggml", "Run GGML tensor computation demo").dependOn(&run_ggml.step);

    // 1.18 JSON-C
    const jsonc_exe = b.addExecutable(.{
        .name = "demo_json_c",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureJsonC(jsonc_exe, b);
    shipHost(b, jsonc_exe, enable_gc, is_windows_target);
    const run_jsonc = b.addRunArtifact(jsonc_exe);
    b.step("run-json-c", "Run JSON-C parser demo").dependOn(&run_jsonc.step);

    // 1.19 Libsixel
    const sixel_exe = b.addExecutable(.{
        .name = "demo_sixel",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureLibsixel(sixel_exe, b);
    shipHost(b, sixel_exe, enable_gc, is_windows_target);
    const run_sixel = b.addRunArtifact(sixel_exe);
    b.step("run-sixel", "Run Libsixel terminal graphics demo").dependOn(&run_sixel.step);

    // 1.20 NanoVG
    const nanovg_exe = b.addExecutable(.{
        .name = "demo_nanovg",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureNanoVG(nanovg_exe, b);
    shipHost(b, nanovg_exe, enable_gc, is_windows_target);
    const run_nanovg = b.addRunArtifact(nanovg_exe);
    b.step("run-nanovg", "Run NanoVG headless vector graphics demo").dependOn(&run_nanovg.step);

    // 1.21 CLAP Audio Plugin
    const clap_exe = b.addExecutable(.{
        .name = "demo_clap",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureCLAP(clap_exe, b, false, is_windows_target);
    shipHost(b, clap_exe, enable_gc, is_windows_target);
    const run_clap = b.addRunArtifact(clap_exe);
    b.step("run-clap", "Run CLAP audio plugin validator (exe)").dependOn(&run_clap.step);

    const clap_lib = b.addLibrary(.{
        .name = "ZapotaFilter",
        .linkage = .dynamic,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .pic = true,
        }),
    });
    configureCLAP(clap_lib, b, true, is_windows_target);
    const host_clap_tag = b.fmt("{s}-{s}", .{ @tagName(target.result.cpu.arch), @tagName(target.result.os.tag) });
    installClapPlugin(b, clap_lib, host_clap_tag, target.result.os.tag == .macos, clap_step, null);

    // 1.22 Sokol
    const sokol_exe = b.addExecutable(.{
        .name = "demo_sokol",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureSokol(sokol_exe, b);
    shipHost(b, sokol_exe, enable_gc, is_windows_target);
    const run_sokol = b.addRunArtifact(sokol_exe);
    b.step("run-sokol", "Run Sokol time/log demo").dependOn(&run_sokol.step);

    // 1.23 hiredis
    const hiredis_exe = b.addExecutable(.{
        .name = "demo_hiredis",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureHiredis(hiredis_exe, b, is_windows_target);
    shipHost(b, hiredis_exe, enable_gc, is_windows_target);
    const run_hiredis = b.addRunArtifact(hiredis_exe);
    b.step("run-hiredis", "Run hiredis Redis client demo").dependOn(&run_hiredis.step);

    // 1.24 protobuf-c
    const protobufc_exe = b.addExecutable(.{
        .name = "demo_protobuf_c",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureProtobufC(protobufc_exe, b);
    shipHost(b, protobufc_exe, enable_gc, is_windows_target);
    const run_protobufc = b.addRunArtifact(protobufc_exe);
    b.step("run-protobuf-c", "Run protobuf-c runtime demo").dependOn(&run_protobufc.step);

    // 1.25 wslay WebSockets
    const wslay_exe = b.addExecutable(.{
        .name = "demo_wslay",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureWslay(wslay_exe, b, is_windows_target);
    shipHost(b, wslay_exe, enable_gc, is_windows_target);
    const run_wslay = b.addRunArtifact(wslay_exe);
    b.step("run-wslay", "Run wslay WebSocket frame demo").dependOn(&run_wslay.step);

    // 1.26 Boehm GC
    const gc_exe = b.addExecutable(.{
        .name = "demo_gc",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureGcDemo(gc_exe, b);
    shipHost(b, gc_exe, enable_gc, is_windows_target);
    const run_gc = b.addRunArtifact(gc_exe);
    b.step("run-gc", "Run Boehm garbage collector demo").dependOn(&run_gc.step);

    // 1.27 libuv
    const libuv_exe = b.addExecutable(.{
        .name = "demo_libuv",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureLibuv(libuv_exe, b, is_windows_target, target.result.os.tag == .linux, target.result.os.tag == .macos, target.result.abi == .android);
    shipHost(b, libuv_exe, enable_gc, is_windows_target);
    const run_libuv = b.addRunArtifact(libuv_exe);
    b.step("run-libuv", "Run libuv event-loop demo").dependOn(&run_libuv.step);

    // 1.28 libsodium
    const sodium_exe = b.addExecutable(.{
        .name = "demo_libsodium",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureLibsodium(sodium_exe, b, is_windows_target);
    shipHost(b, sodium_exe, enable_gc, is_windows_target);
    const run_sodium = b.addRunArtifact(sodium_exe);
    b.step("run-libsodium", "Run libsodium crypto demo").dependOn(&run_sodium.step);

    // 1.29 linenoise (POSIX terminals; skip Windows)
    if (!is_windows_target) {
        const linenoise_exe = b.addExecutable(.{
            .name = "demo_linenoise",
            .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
        });
        configureLinenoise(linenoise_exe, b);
        shipHost(b, linenoise_exe, enable_gc, false);
        const run_linenoise = b.addRunArtifact(linenoise_exe);
        b.step("run-linenoise", "Run linenoise line-editing demo").dependOn(&run_linenoise.step);
    }

    // 1.30 miniaudio
    const miniaudio_exe = b.addExecutable(.{
        .name = "demo_miniaudio",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureMiniaudio(miniaudio_exe, b);
    shipHost(b, miniaudio_exe, enable_gc, is_windows_target);
    const run_miniaudio = b.addRunArtifact(miniaudio_exe);
    b.step("run-miniaudio", "Run miniaudio waveform demo").dependOn(&run_miniaudio.step);

    // 1.31 dr_wav
    const drwav_exe = b.addExecutable(.{
        .name = "demo_dr_wav",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureDrWav(drwav_exe, b);
    shipHost(b, drwav_exe, enable_gc, is_windows_target);
    const run_drwav = b.addRunArtifact(drwav_exe);
    b.step("run-dr-wav", "Run dr_wav encode/decode demo").dependOn(&run_drwav.step);

    // 1.32 Runtime window (Win32 / dlopen X11 / dlopen AppKit)
    const window_exe = b.addExecutable(.{
        .name = "demo_window",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }),
    });
    configureWindow(window_exe, b, is_windows_target);
    shipHost(b, window_exe, enable_gc, is_windows_target);
    const run_window = b.addRunArtifact(window_exe);
    b.step("run-window", "Open a native window for about two seconds").dependOn(&run_window.step);

    // -------------------------------------------------------------
    // 2. Cross-Compilation Matrix Step: "all"
    // -------------------------------------------------------------
    const all_step = b.step("all", "Cross-compile demos for Windows, Linux, macOS, WASI, and Android");
    const wasm_step = b.step("wasm", "Cross-compile portable demos for wasm32-wasi");
    const android_step = b.step("android", "Cross-compile demos for aarch64-linux-android (needs NDK)");

    const android_ndk = findAndroidNdk(b);

    const target_list = [_]CrossTarget{
        .{ .tag = "x86_64-windows", .triple = "x86_64-windows-gnu", .kind = .windows },
        .{ .tag = "aarch64-windows", .triple = "aarch64-windows-gnu", .kind = .windows },
        .{ .tag = "x86_64-linux", .triple = "x86_64-linux-musl", .kind = .posix },
        .{ .tag = "aarch64-linux", .triple = "aarch64-linux-musl", .kind = .posix },
        .{ .tag = "x86_64-macos", .triple = "x86_64-macos", .kind = .posix },
        .{ .tag = "aarch64-macos", .triple = "aarch64-macos", .kind = .posix },
        .{ .tag = "wasm32-wasi", .triple = "wasm32-wasi", .kind = .wasm },
    };

    var targets: std.ArrayListUnmanaged(CrossTarget) = .empty;
    targets.appendSlice(b.allocator, &target_list) catch @panic("OOM");
    if (android_ndk != null) {
        targets.append(b.allocator, .{
            .tag = "aarch64-android",
            .triple = "aarch64-linux-android.24",
            .kind = .android,
        }) catch @panic("OOM");
    }

    for (targets.items) |t| {
        const extra_step: ?*std.Build.Step = switch (t.kind) {
            .wasm => wasm_step,
            .android => android_step,
            else => null,
        };
        const cross_query = std.Target.Query.parse(.{ .arch_os_abi = t.triple }) catch unreachable;
        const cross_target = b.resolveTargetQuery(cross_query);

        // Hello World
        const c_hello = b.addExecutable(.{
            .name = b.fmt("hello-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        c_hello.root_module.addCSourceFile(.{ .file = b.path("src/main.c"), .flags = &.{ "-Wall", "-Wextra" } });
        shipCross(b, all_step, extra_step, c_hello, enable_gc, t, android_ndk);

        // Lua 5.4 (setjmp is not available on WASI without wasm exceptions)
        if (t.kind != .wasm) {
        const c_lua = b.addExecutable(.{
            .name = b.fmt("demo_lua-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureLua(c_lua, b);
        shipCross(b, all_step, extra_step, c_lua, enable_gc, t, android_ndk);
        }

        // cmark
        const c_cmark = b.addExecutable(.{
            .name = b.fmt("demo_cmark-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureCmark(c_cmark, b);
        shipCross(b, all_step, extra_step, c_cmark, enable_gc, t, android_ndk);

        // libyaml
        const c_yaml = b.addExecutable(.{
            .name = b.fmt("demo_yaml-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureLibYaml(c_yaml, b);
        shipCross(b, all_step, extra_step, c_yaml, enable_gc, t, android_ndk);

        // Flecs (needs POSIX/Win debug APIs; skip WASI)
        if (t.kind != .wasm) {
        const c_flecs = b.addExecutable(.{
            .name = b.fmt("demo_flecs-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureFlecs(c_flecs, b, t.isWindows());
        shipCross(b, all_step, extra_step, c_flecs, enable_gc, t, android_ndk);
        }

        // Box2D
        const c_box2d = b.addExecutable(.{
            .name = b.fmt("demo_box2d-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureBox2D(c_box2d, b);
        shipCross(b, all_step, extra_step, c_box2d, enable_gc, t, android_ndk);

        // PlutoVG (FreeType raster uses setjmp/mmap; skip WASI)
        if (t.kind != .wasm) {
        const c_plutovg = b.addExecutable(.{
            .name = b.fmt("demo_plutovg-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configurePlutoVG(c_plutovg, b);
        shipCross(b, all_step, extra_step, c_plutovg, enable_gc, t, android_ndk);
        }

        // Nuklear
        const c_nuklear = b.addExecutable(.{
            .name = b.fmt("demo_nuklear-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        c_nuklear.root_module.addIncludePath(b.path("vendor/nuklear"));
        c_nuklear.root_module.addCSourceFile(.{ .file = b.path("src/demo_nuklear.c"), .flags = &.{ "-Wall", "-Wextra" } });
        shipCross(b, all_step, extra_step, c_nuklear, enable_gc, t, android_ndk);

        // H2O HTTP Server (needs BSD sockets; skip WASI)
        if (t.kind != .wasm) {
            const c_http = b.addExecutable(.{
                .name = b.fmt("http_server-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            c_http.root_module.addIncludePath(b.path("vendor/h2o/include"));
            c_http.root_module.addIncludePath(b.path("vendor/h2o/deps/picohttpparser"));
            c_http.root_module.addCSourceFile(.{ .file = b.path("src/http_server.c"), .flags = &.{ "-Wall", "-Wextra" } });
            c_http.root_module.addCSourceFile(.{ .file = b.path("vendor/h2o/deps/picohttpparser/picohttpparser.c"), .flags = &.{ "-Wall", "-Wextra" } });
            if (t.isWindows()) c_http.root_module.linkSystemLibrary("ws2_32", .{});
            shipCross(b, all_step, extra_step, c_http, enable_gc, t, android_ndk);
        }

        // SQLite (Unix VFS needs fcntl/mmap; skip WASI)
        if (t.kind != .wasm) {
            const c_sqlite = b.addExecutable(.{
                .name = b.fmt("sqlite_demo-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            c_sqlite.root_module.addIncludePath(b.path("vendor/sqlite"));
            c_sqlite.root_module.addCSourceFile(.{ .file = b.path("src/sqlite_demo.c"), .flags = &.{ "-Wall", "-Wextra" } });
            c_sqlite.root_module.addCSourceFile(.{ .file = b.path("vendor/sqlite/sqlite3.c"), .flags = &.{ "-DSQLITE_THREADSAFE=0", "-DSQLITE_OMIT_LOAD_EXTENSION" } });
            shipCross(b, all_step, extra_step, c_sqlite, enable_gc, t, android_ndk);
        }

        // PDCurses (Windows)
        if (t.isWindows()) {
            const c_curses = b.addExecutable(.{
                .name = b.fmt("curses_demo-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configurePDCurses(c_curses, b, true);
            shipCross(b, all_step, extra_step, c_curses, enable_gc, t, android_ndk);
        }

        // Raylib (Windows)
        if (t.isWindows()) {
            const c_raylib = b.addExecutable(.{
                .name = b.fmt("raylib_demo-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureRaylib(c_raylib, b, true);
            shipCross(b, all_step, extra_step, c_raylib, enable_gc, t, android_ndk);
        }

        // x264 Video Encoder
        const c_x264 = b.addExecutable(.{
            .name = b.fmt("demo_x264-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureX264(c_x264, b, t.isWindows());
        shipCross(b, all_step, extra_step, c_x264, enable_gc, t, android_ndk);

        // Opus Audio Codec
        const c_opus = b.addExecutable(.{
            .name = b.fmt("demo_opus-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureOpus(c_opus, b);
        shipCross(b, all_step, extra_step, c_opus, enable_gc, t, android_ndk);

        // LAME MP3 Encoder
        const c_lame = b.addExecutable(.{
            .name = b.fmt("demo_lame-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureLame(c_lame, b);
        shipCross(b, all_step, extra_step, c_lame, enable_gc, t, android_ndk);

        // Duktape (setjmp; skip WASI)
        if (t.kind != .wasm) {
            const c_duktape = b.addExecutable(.{
                .name = b.fmt("demo_duktape-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureDuktape(c_duktape, b);
            shipCross(b, all_step, extra_step, c_duktape, enable_gc, t, android_ndk);
        }

        // GGML Tensor Computation
        const c_ggml = b.addExecutable(.{
            .name = b.fmt("demo_ggml-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureGGML(c_ggml, b);
        shipCross(b, all_step, extra_step, c_ggml, enable_gc, t, android_ndk);

        // JSON-C
        const c_jsonc = b.addExecutable(.{
            .name = b.fmt("demo_json_c-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureJsonC(c_jsonc, b);
        shipCross(b, all_step, extra_step, c_jsonc, enable_gc, t, android_ndk);

        // Libsixel
        const c_sixel = b.addExecutable(.{
            .name = b.fmt("demo_sixel-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureLibsixel(c_sixel, b);
        shipCross(b, all_step, extra_step, c_sixel, enable_gc, t, android_ndk);

        // NanoVG
        const c_nanovg = b.addExecutable(.{
            .name = b.fmt("demo_nanovg-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureNanoVG(c_nanovg, b);
        shipCross(b, all_step, extra_step, c_nanovg, enable_gc, t, android_ndk);

        // CLAP Audio Plugin
        const c_clap = b.addExecutable(.{
            .name = b.fmt("demo_clap-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureCLAP(c_clap, b, false, t.isWindows());
        shipCross(b, all_step, extra_step, c_clap, enable_gc, t, android_ndk);

        if (t.kind == .windows or t.kind == .posix) {
            const clap_shared = b.addLibrary(.{
                .name = b.fmt("ZapotaFilter-{s}", .{t.tag}),
                .linkage = .dynamic,
                .root_module = b.createModule(.{
                    .target = cross_target,
                    .optimize = optimize,
                    .link_libc = true,
                    .pic = true,
                }),
            });
            configureCLAP(clap_shared, b, true, t.isWindows());
            installClapPlugin(
                b,
                clap_shared,
                t.tag,
                std.mem.indexOf(u8, t.triple, "macos") != null,
                clap_step,
                all_step,
            );
        }

        if (t.kind != .wasm) {
            const c_sokol = b.addExecutable(.{
                .name = b.fmt("demo_sokol-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureSokol(c_sokol, b);
            shipCross(b, all_step, extra_step, c_sokol, enable_gc, t, android_ndk);
        }

        if (t.kind != .wasm) {
            const c_hiredis = b.addExecutable(.{
                .name = b.fmt("demo_hiredis-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureHiredis(c_hiredis, b, t.isWindows());
            shipCross(b, all_step, extra_step, c_hiredis, enable_gc, t, android_ndk);
        }

        const c_protobufc = b.addExecutable(.{
            .name = b.fmt("demo_protobuf_c-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureProtobufC(c_protobufc, b);
        shipCross(b, all_step, extra_step, c_protobufc, enable_gc, t, android_ndk);

        const c_wslay = b.addExecutable(.{
            .name = b.fmt("demo_wslay-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureWslay(c_wslay, b, t.isWindows());
        shipCross(b, all_step, extra_step, c_wslay, enable_gc, t, android_ndk);

        if (t.kind != .wasm) {
            const c_gc = b.addExecutable(.{
                .name = b.fmt("demo_gc-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureGcDemo(c_gc, b);
            shipCross(b, all_step, extra_step, c_gc, true, t, android_ndk);
        }

        if (t.kind != .wasm) {
            const c_uv = b.addExecutable(.{
                .name = b.fmt("demo_libuv-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureLibuv(
                c_uv,
                b,
                t.isWindows(),
                t.kind == .android or std.mem.indexOf(u8, t.triple, "linux") != null,
                std.mem.indexOf(u8, t.triple, "macos") != null,
                t.kind == .android,
            );
            shipCross(b, all_step, extra_step, c_uv, enable_gc, t, android_ndk);
        }

        const c_sodium = b.addExecutable(.{
            .name = b.fmt("demo_libsodium-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureLibsodium(c_sodium, b, t.isWindows());
        shipCross(b, all_step, extra_step, c_sodium, enable_gc, t, android_ndk);

        if (t.kind != .wasm and !t.isWindows()) {
            const c_ln = b.addExecutable(.{
                .name = b.fmt("demo_linenoise-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureLinenoise(c_ln, b);
            shipCross(b, all_step, extra_step, c_ln, enable_gc, t, android_ndk);
        }

        const c_ma = b.addExecutable(.{
            .name = b.fmt("demo_miniaudio-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureMiniaudio(c_ma, b);
        shipCross(b, all_step, extra_step, c_ma, enable_gc, t, android_ndk);

        const c_dr = b.addExecutable(.{
            .name = b.fmt("demo_dr_wav-{s}", .{t.tag}),
            .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
        });
        configureDrWav(c_dr, b);
        shipCross(b, all_step, extra_step, c_dr, enable_gc, t, android_ndk);

        if (t.kind == .windows or t.kind == .posix) {
            const c_win = b.addExecutable(.{
                .name = b.fmt("demo_window-{s}", .{t.tag}),
                .root_module = b.createModule(.{ .target = cross_target, .optimize = optimize, .link_libc = true }),
            });
            configureWindow(c_win, b, t.isWindows());
            shipCross(b, all_step, extra_step, c_win, enable_gc, t, android_ndk);
        }
    }

    // -------------------------------------------------------------
    // 3. Vendor update: clone / fast-forward every package in VENDOR.md
    // -------------------------------------------------------------
    const update_vendor = b.addExecutable(.{
        .name = "update_vendor",
        .root_module = b.createModule(.{
            .root_source_file = b.path("tools/update_vendor.zig"),
            .target = b.resolveTargetQuery(.{}),
            .optimize = .ReleaseSafe,
        }),
    });
    const run_update_vendor = b.addRunArtifact(update_vendor);
    run_update_vendor.setCwd(b.path("."));
    b.step("vendor-update", "Clone or fast-forward all vendor packages from upstream git").dependOn(&run_update_vendor.step);
}
