# zapota

A demonstration project showcasing 26 pure C library integrations, cross-compiled from one machine with Zig (`build.zig` and `zig cc`). No CMake, Make, or Autotools.

**Targets:** Windows, Linux, macOS, Raspberry Pi, WASM (WASI), Android. iOS needs an Xcode SDK.

---

## Library Summary

| Domain | Library | Vendor Path | Capability |
| :--- | :--- | :--- | :--- |
| **Scripting** | Lua 5.5 | `vendor/lua` | Embedded scripting language interpreter |
| | Duktape | `vendor/duktape-amal` | Embeddable ES5/ES6 JavaScript engine |
| **Formats & Parsers** | cmark | `vendor/cmark` | CommonMark Markdown parser / HTML renderer |
| | libyaml | `vendor/libyaml` | Streaming YAML event parser & emitter |
| | json-c | `vendor/json-c` | RFC 8259 JSON parse, DOM, serialization |
| **Data & Storage** | SQLite | `vendor/sqlite` | Zero-configuration embedded SQL engine |
| **Networking** | H2O (picohttpparser) | `vendor/h2o` | High-throughput HTTP parser & server |
| | hiredis | `vendor/hiredis` | Minimal Redis client (RESP) |
| | protobuf-c | `vendor/protobuf-c` | Protocol Buffers C runtime |
| | wslay | `vendor/wslay` | RFC 6455 WebSocket framing (no TLS) |
| **GUI & Graphics** | Raylib | `vendor/raylib` | Hardware-accelerated 2D/3D graphics + input |
| | Sokol | `vendor/sokol` | Header-only app/gfx/audio/time (no GPU needed for time/log) |
| | Nuklear | `vendor/nuklear` | Immediate-mode GUI (zero dependencies) |
| | PlutoVG | `vendor/plutovg` | Software 2D vector graphics rasterizer |
| | NanoVG | `vendor/nanovg` | 2D vector drawing (OpenGL / headless null) |
| **Terminal & TUI** | PDCurses | `vendor/pdcurses` | ncurses-compatible TUI, windows, keyboard |
| | libsixel | `vendor/libsixel` | Image quantization to terminal Sixel streams |
| **Physics & ECS** | Box2D | `vendor/box2d` | 2D rigid body physics simulation |
| | Flecs | `vendor/flecs` | High-performance entity-component system |
| **Machine Learning** | GGML | `vendor/ggml` | Tensor computation graph engine |
| **Audio & Video** | x264 | `vendor/x264` | H.264 / AVC video encoder |
| | Opus | `vendor/opus` | Low-latency speech & audio codec (CELT + SILK) |
| | LAME | `vendor/libmp3lame` | MPEG Audio Layer III (MP3) encoder |
| **Audio Plugins** | CLAP | `vendor/clap` | C-native audio plugin standard (DSP + GUI extensions) |
| **Memory** | Boehm GC | `vendor/bdwgc` | Conservative garbage collector linked into every demo |

---

## Cross-platform

One host (this repo was driven from Windows + Zig 0.16) can emit binaries for:

| Platform | How | What you get |
| :--- | :--- | :--- |
| **Windows** | `zig build` / `zig build all` | Full suite, including PDCurses and Raylib (`x86_64` and `aarch64`) |
| **Linux** | `zig build all` | Static musl binaries (`x86_64-linux-musl`, `aarch64-linux-musl`) |
| **macOS** | `zig build all` | Mach-O for `x86_64` and `aarch64`. A Darwin SDK makes the sysroot clean; Zig still emits the binaries without one |
| **Raspberry Pi** | `zig build all` (64-bit) or `-Dtarget` / `-Dcpu` below | ARM Linux. Pi 3/4/5 64-bit OS uses the same `aarch64-linux-musl` artifacts as Linux |
| **WASM** | `zig build wasm` | `wasm32-wasi`. Skips APIs WASI does not have (setjmp, mmap, sockets) |
| **Android** | `zig build android` | `aarch64-linux-android.24` via NDK (`ANDROID_NDK_HOME` or `ANDROID_HOME/ndk`, or `-Dandroid-ndk=`) |
| **iOS** | not in `zig build all` | `aarch64-ios` exists in Zig but there is no bundled iOS libc. Needs Xcode `iPhoneOS.sdk` |

```powershell
zig build all          # Windows + Linux + macOS + WASI + Android (if NDK found)
zig build wasm         # wasm32-wasi only
zig build android      # aarch64 Android only
```

Artifacts land in `zig-out/bin/` with a `-{target}` suffix (for example `hello-aarch64-linux`, `demo_json_c-wasm32-wasi`).

### Raspberry Pi

Zig has no `-Drpi` flag. A Pi is ARM Linux: pick the triple, optionally pin the CPU.

| Board | Zig target |
| :--- | :--- |
| **Pi 3 / 4 / 5, 64-bit Raspberry Pi OS** | `aarch64-linux-musl` (already in `zig build all`) |
| Pi 3 | `zig build -Dtarget=aarch64-linux-musl -Dcpu=cortex_a53` |
| Pi 4 | `zig build -Dtarget=aarch64-linux-musl -Dcpu=cortex_a72` |
| Pi 5 | `zig build -Dtarget=aarch64-linux-musl -Dcpu=cortex_a76` |
| **Pi 2, 32-bit** | `zig cc -target arm-linux-gnueabihf -mcpu=cortex_a7` |
| **Pi 1 / Zero** | `zig cc -target arm-linux-musleabihf -mcpu=arm1176jzf_s` |

Use `aarch64-linux-gnu` instead of musl if the board is stock Raspberry Pi OS (glibc). Musl static binaries are easier to copy onto the Pi with no extra `.so` files.

GPIO, camera, and VideoCore are not vendored here. These demos are userspace C.

### WASM skips

Lua, Duktape, SQLite, HTTP, hiredis, PDCurses, Raylib, PlutoVG, Flecs, Sokol, and Boehm GC are omitted from `zig build wasm` (setjmp, mmap, or sockets).

### iOS

Zig will accept `-Dtarget=aarch64-ios`, but this Windows host cannot link it. On a Mac:

```bash
zig cc -target aarch64-ios --sysroot $(xcrun --sdk iphoneos --show-sdk-path) ...
```

---

## What Can You Build?

### Cross-Platform Desktop Apps
Yes. Combine **Raylib** (hardware-accelerated OpenGL/Metal/DirectX windowing), **Nuklear** (immediate-mode UI), **PlutoVG** / **NanoVG** (vector graphics), and **SQLite** (persistence) into native apps targeting Windows, Linux, macOS, and 64-bit Raspberry Pi — compiled from one machine via `zig build all`.

### Cross-Platform Audio Plugins
Yes. The suite includes **CLAP** (`vendor/clap`), the open C-native plugin standard supported by Bitwig, Reaper, FL Studio, and others. Combine with **Opus** / **LAME** for audio codecs and **NanoVG** / **Nuklear** for plugin GUIs. Zig can emit `.clap` / `.dll` / `.so` / `.dylib` shared libraries via `b.addSharedLibrary` with zero runtime dependencies.

### Cross-Platform Terminal Apps
Yes. **PDCurses** gives ncurses-compatible window management and keyboard input. **libsixel** renders images directly in the terminal for Sixel-capable terminals (Windows Terminal, iTerm2, WezTerm, Alacritty, Foot). Add **Lua** or **Duktape** for scripting layers and **json-c** / **libyaml** for CLI configuration tools.

### Cross-Platform Server Apps
Yes. **H2O's `picohttpparser`** plus **wslay** (WebSocket frames). Combine with **SQLite**, **Lua** or **Duktape**, **json-c**, and **hiredis**. Linux and 64-bit Pi targets compile against musl and can be static.

### Android & iOS
`zig build android` was verified with NDK r30 for `aarch64-linux-android.24` (full portable suite except PDCurses/Raylib windowing). iOS still needs Xcode.

| Category | Android | iOS | Notes |
| :--- | :--- | :--- | :--- |
| Pure C parsers & computation (Lua, Duktape, cmark, libyaml, json-c, SQLite, Flecs, Box2D, GGML) | Full | Full* | \*iOS: same sources; link with an iPhoneOS sysroot |
| Audio codecs (Opus, LAME, x264) | Full | Full* | Pure C math/DSP |
| Vector & immediate GUI (PlutoVG, NanoVG, Nuklear) | Full | Full* | Software-rendered or GLES/Metal backends |
| Audio plugins (CLAP) | Full | Full* | CLAP has mobile surface extensions |
| Windowing & graphics (Raylib) | Needs NDK glue | Needs UIView | Not in the default Android/iOS `all` matrix |
| Terminal TUI (PDCurses, libsixel) | Limited | Limited | Termux / iSH, not typical store apps |

---

## Build & Run Instructions

Ensure [Zig](https://ziglang.org/download/) 0.16.0+ is installed.

### Run Demos on Host

```powershell
zig build run             # Hello World
zig build run-lua         # Lua 5.5
zig build run-cmark       # CommonMark
zig build run-yaml        # LibYAML
zig build run-flecs       # Flecs ECS
zig build run-box2d       # Box2D Physics
zig build run-plutovg     # PlutoVG Vector Graphics
zig build run-nuklear     # Nuklear GUI
zig build run-http        # H2O HTTP Server (port 8080)
zig build run-sqlite      # SQLite Database
zig build run-curses      # PDCurses TUI
zig build run-raylib      # Raylib Graphics
zig build run-x264        # x264 Video Encoder
zig build run-opus        # Opus Audio Codec
zig build run-lame        # LAME MP3 Encoder
zig build run-duktape     # Duktape JS Engine
zig build run-ggml        # GGML Tensors
zig build run-json-c      # JSON-C Parser
zig build run-sixel       # Libsixel Terminal Graphics
zig build run-nanovg      # NanoVG Headless Graphics
zig build run-clap        # CLAP Filter Plugin + NanoVG UI
zig build run-sokol       # Sokol time/log
zig build run-hiredis     # hiredis Redis client
zig build run-protobuf-c  # protobuf-c runtime
zig build run-wslay       # wslay WebSocket frames
zig build run-gc          # Boehm garbage collector
```

Boehm GC is linked into **every** demo by default (`GC_INIT` runs before `main`). Pass `-Dgc=false` to build without it. Demos that allocate with `GC_MALLOC` (see `src/demo_gc.c`) are collected; other demos keep using libc `malloc` unless they include `gc.h`.

### Cross-compile

See [Cross-platform](#cross-platform) above. Short form:

```powershell
zig build all
zig build wasm
zig build android
zig build -Dtarget=aarch64-linux-musl -Dcpu=cortex_a72   # Pi 4
```

### Update vendor packages

Requires `git` on `PATH`. Clones or fast-forwards every tree listed in `VENDOR.md`:

```powershell
zig build vendor-update
```

### Suggested libraries (not vendored yet)

Useful, C-first, and realistic to cross-compile with `zig cc` for the four product lines this repo targets:

| Use | Library | Why |
| :--- | :--- | :--- |
| **Desktop** | [miniaudio](https://github.com/mackron/miniaudio) | Single-header playback/capture; pairs with Raylib/Nuklear apps. |
| | [tinyfiledialogs](https://github.com/native-toolkit/tinyfiledialogs) | Native open/save/alert dialogs without a toolkit. |
| | [PhysFS](https://github.com/icculus/physfs) | Zip-backed virtual filesystem for app data. |
| **Audio plugins** | [miniaudio](https://github.com/mackron/miniaudio) / [dr_libs](https://github.com/mackron/dr_libs) | Host-side WAV/FLAC/MP3 decode next to CLAP. |
| | [pffft](https://bitbucket.org/jpommier/pffft) or [KISS FFT](https://github.com/mborgerding/kissfft) | Small FFT for EQ, pitch, analyzers. |
| | [LV2](https://lv2plug.in/) | C plugin API if you also want Ardour/Carla hosts. |
| **Terminal** | [termbox2](https://github.com/termbox/termbox2) | Smaller TUI than PDCurses; good for TUIs that are not ncurses clones. |
| | [notcurses](https://github.com/dankamongmen/notcurses) | Modern TUI (images, RGB); heavier, still C. |
| | [linenoise](https://github.com/antirez/linenoise) | Line editing/history for REPL CLIs (Lua/Duktape shells). |
| **Server** | [Mongoose](https://github.com/cesanta/mongoose) | Embedded HTTP, WebSocket, MQTT in one C file. |
| | [libuv](https://github.com/libuv/libuv) | Cross-platform event loop (used by Node); good with picohttpparser. |
| | [mbedTLS](https://github.com/Mbed-TLS/mbedtls) | TLS without OpenSSL. |
| | [libsodium](https://github.com/jedisct1/libsodium) | Crypto (NaCl boxes, hashes, sign) without OpenSSL. |

Skip C++-first stacks (Dear ImGui, JUCE, iPlug2, Boost.Asio) unless you are ready to add a C++ compile unit. Prefer MIT/BSD/zlib over GPL when the binary is a shipped app or plugin; x264 (GPL-2.0) and LAME (LGPL-2.0) already constrain those two demos.
