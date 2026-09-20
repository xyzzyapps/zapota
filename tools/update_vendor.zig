//! Fetch or fast-forward every vendored package from its upstream git remote.
//! Invoked by `zig build vendor-update`. Requires `git` on PATH.

const std = @import("std");

const Kind = enum {
    git,
    amalgamation,
};

const Package = struct {
    name: []const u8,
    path: []const u8,
    url: []const u8,
    kind: Kind = .git,
    files: []const []const u8 = &.{},
};

const packages = [_]Package{
    .{ .name = "lua", .path = "vendor/lua", .url = "https://github.com/lua/lua.git" },
    .{ .name = "cmark", .path = "vendor/cmark", .url = "https://github.com/commonmark/cmark.git" },
    .{ .name = "libyaml", .path = "vendor/libyaml", .url = "https://github.com/yaml/libyaml.git" },
    .{ .name = "flecs", .path = "vendor/flecs", .url = "https://github.com/SanderMertens/flecs.git" },
    .{ .name = "box2d", .path = "vendor/box2d", .url = "https://github.com/erincatto/box2d.git" },
    .{ .name = "plutovg", .path = "vendor/plutovg", .url = "https://github.com/sammycage/plutovg.git" },
    .{ .name = "nuklear", .path = "vendor/nuklear", .url = "https://github.com/Immediate-Mode-UI/Nuklear.git" },
    .{ .name = "h2o", .path = "vendor/h2o", .url = "https://github.com/h2o/h2o.git" },
    .{ .name = "sqlite", .path = "vendor/sqlite", .url = "https://github.com/azadkuh/sqlite-amalgamation.git" },
    .{ .name = "pdcurses", .path = "vendor/pdcurses", .url = "https://github.com/wmcbrine/PDCurses.git" },
    .{ .name = "raylib", .path = "vendor/raylib", .url = "https://github.com/raysan5/raylib.git" },
    .{ .name = "x264", .path = "vendor/x264", .url = "https://code.videolan.org/videolan/x264.git" },
    .{ .name = "opus", .path = "vendor/opus", .url = "https://gitlab.xiph.org/xiph/opus.git" },
    .{ .name = "libmp3lame", .path = "vendor/libmp3lame", .url = "https://github.com/Distrotech/lame.git" },
    .{
        .name = "duktape-amal",
        .path = "vendor/duktape-amal",
        .url = "https://github.com/svaarala/duktape.git",
        .kind = .amalgamation,
        .files = &.{ "src/duktape.c", "src/duktape.h", "src/duk_config.h" },
    },
    .{ .name = "ggml", .path = "vendor/ggml", .url = "https://github.com/ggerganov/ggml.git" },
    .{ .name = "json-c", .path = "vendor/json-c", .url = "https://github.com/json-c/json-c.git" },
    .{ .name = "libsixel", .path = "vendor/libsixel", .url = "https://github.com/libsixel/libsixel.git" },
    .{ .name = "nanovg", .path = "vendor/nanovg", .url = "https://github.com/memononen/nanovg.git" },
    .{ .name = "clap", .path = "vendor/clap", .url = "https://github.com/free-audio/clap.git" },
    .{ .name = "sokol", .path = "vendor/sokol", .url = "https://github.com/floooh/sokol.git" },
    .{ .name = "hiredis", .path = "vendor/hiredis", .url = "https://github.com/redis/hiredis.git" },
    .{ .name = "protobuf-c", .path = "vendor/protobuf-c", .url = "https://github.com/protobuf-c/protobuf-c.git" },
    .{ .name = "linenoise", .path = "vendor/linenoise", .url = "https://github.com/antirez/linenoise.git" },
    .{ .name = "libuv", .path = "vendor/libuv", .url = "https://github.com/libuv/libuv.git" },
    .{ .name = "libsodium", .path = "vendor/libsodium", .url = "https://github.com/jedisct1/libsodium.git" },
    .{ .name = "wslay", .path = "vendor/wslay", .url = "https://github.com/tatsuhiro-t/wslay.git" },
    .{ .name = "bdwgc", .path = "vendor/bdwgc", .url = "https://github.com/ivmai/bdwgc.git" },
    .{ .name = "notcurses", .path = "vendor/notcurses", .url = "https://github.com/dankamongmen/notcurses.git" },
    .{ .name = "miniaudio", .path = "vendor/miniaudio", .url = "https://github.com/mackron/miniaudio.git" },
    .{ .name = "dr_libs", .path = "vendor/dr_libs", .url = "https://github.com/mackron/dr_libs.git" },
};

pub fn main(init: std.process.Init) !void {
    const allocator = init.gpa;
    const io = init.io;

    try requireGit(allocator, io);

    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, "vendor");

    var failed: usize = 0;
    var updated: usize = 0;

    for (packages) |pkg| {
        std.debug.print("==> {s}\n", .{pkg.name});
        updateOne(allocator, io, cwd, pkg) catch |err| {
            std.debug.print("    FAILED: {s}\n", .{@errorName(err)});
            failed += 1;
            continue;
        };
        updated += 1;
    }

    std.debug.print("\nUpdated {d}/{d} packages", .{ updated, packages.len });
    if (failed != 0) {
        std.debug.print(", {d} failed\n", .{failed});
        std.process.exit(1);
    }
    std.debug.print("\n", .{});
}

fn requireGit(allocator: std.mem.Allocator, io: std.Io) !void {
    const result = std.process.run(allocator, io, .{
        .argv = &.{ "git", "--version" },
    }) catch {
        std.debug.print("git was not found on PATH. Install Git and retry.\n", .{});
        return error.GitNotFound;
    };
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);
    switch (result.term) {
        .exited => |code| if (code != 0) return error.GitNotFound,
        else => return error.GitNotFound,
    }
    const ver = std.mem.trimEnd(u8, result.stdout, "\r\n");
    std.debug.print("Using {s}\n\n", .{ver});
}

fn updateOne(allocator: std.mem.Allocator, io: std.Io, cwd: std.Io.Dir, pkg: Package) !void {
    switch (pkg.kind) {
        .git => try updateGitTree(allocator, io, cwd, pkg),
        .amalgamation => try updateAmalgamation(allocator, io, cwd, pkg),
    }
}

fn updateGitTree(allocator: std.mem.Allocator, io: std.Io, cwd: std.Io.Dir, pkg: Package) !void {
    const git_dir = try std.fs.path.join(allocator, &.{ pkg.path, ".git" });
    defer allocator.free(git_dir);

    if (pathExists(cwd, io, git_dir)) {
        try runGit(allocator, io, &.{ "git", "-C", pkg.path, "fetch", "--depth", "1", "origin" });
        try runGit(allocator, io, &.{ "git", "-C", pkg.path, "reset", "--hard", "FETCH_HEAD" });
        try runGit(allocator, io, &.{ "git", "-C", pkg.path, "clean", "-fd" });
        std.debug.print("    fast-forwarded {s}\n", .{pkg.path});
        return;
    }

    const tmp = try std.fmt.allocPrint(allocator, "{s}.new", .{pkg.path});
    defer allocator.free(tmp);
    if (pathExists(cwd, io, tmp)) try cwd.deleteTree(io, tmp);

    try runGit(allocator, io, &.{ "git", "clone", "--depth", "1", pkg.url, tmp });
    if (pathExists(cwd, io, pkg.path)) try cwd.deleteTree(io, pkg.path);
    try cwd.rename(tmp, cwd, pkg.path, io);
    std.debug.print("    cloned {s}\n", .{pkg.path});
}

fn updateAmalgamation(allocator: std.mem.Allocator, io: std.Io, cwd: std.Io.Dir, pkg: Package) !void {
    const tmp = try std.fmt.allocPrint(allocator, "{s}.src", .{pkg.path});
    defer allocator.free(tmp);
    if (pathExists(cwd, io, tmp)) try cwd.deleteTree(io, tmp);

    try runGit(allocator, io, &.{ "git", "clone", "--depth", "1", pkg.url, tmp });
    defer cwd.deleteTree(io, tmp) catch {};

    try cwd.createDirPath(io, pkg.path);

    var copied: usize = 0;
    for (pkg.files) |rel| {
        const src = try std.fs.path.join(allocator, &.{ tmp, rel });
        defer allocator.free(src);
        const dest_name = std.fs.path.basename(rel);
        const dest = try std.fs.path.join(allocator, &.{ pkg.path, dest_name });
        defer allocator.free(dest);

        cwd.copyFile(src, cwd, dest, io, .{}) catch {
            std.debug.print("    missing {s} in upstream clone\n", .{rel});
            continue;
        };
        copied += 1;
        std.debug.print("    copied {s}\n", .{dest_name});
    }

    if (copied == 0) {
        std.debug.print("    amalgamation not in git; left {s} unchanged\n", .{pkg.path});
        std.debug.print("    download a release tarball from https://duktape.org/ if you need a newer amalgamation\n", .{});
    }
}

fn pathExists(dir: std.Io.Dir, io: std.Io, sub: []const u8) bool {
    dir.access(io, sub, .{}) catch return false;
    return true;
}

fn runGit(allocator: std.mem.Allocator, io: std.Io, argv: []const []const u8) !void {
    const result = try std.process.run(allocator, io, .{
        .argv = argv,
        .stdout_limit = .unlimited,
        .stderr_limit = .unlimited,
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    const ok = switch (result.term) {
        .exited => |code| code == 0,
        else => false,
    };
    if (!ok) {
        const err = std.mem.trimEnd(u8, result.stderr, " \r\n");
        if (err.len != 0) std.debug.print("    git: {s}\n", .{err});
        return error.GitFailed;
    }
}
