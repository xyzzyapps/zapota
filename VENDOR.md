# Vendor packages

zapota vendors third-party C libraries under `vendor/`. This file is the license
and provenance record. Refresh sources with:

```powershell
zig build vendor-update
```

That step clones or fast-forwards each package from the upstream listed below
(`git` must be on `PATH`). Duktape is an amalgamation (`vendor/duktape-amal`);
if the git tree has no `src/duktape.c`, the existing amalgamation is left in
place.

zapota itself is MIT. Copyleft packages (x264 GPL-2.0, LAME LGPL-2.0) apply to
binaries that link them.

| Package | Path | Upstream | License | Notes |
| :--- | :--- | :--- | :--- | :--- |
| Lua 5.5 | `vendor/lua` | https://github.com/lua/lua.git | MIT | Copyright Lua.org, PUC-Rio |
| Duktape 2.7 | `vendor/duktape-amal` | https://github.com/svaarala/duktape.git | MIT | Amalgamation (`duktape.c` / `duktape.h` / `duk_config.h`) |
| cmark | `vendor/cmark` | https://github.com/commonmark/cmark.git | BSD-2-Clause | Copyright John MacFarlane |
| libyaml | `vendor/libyaml` | https://github.com/yaml/libyaml.git | MIT | Copyright Ingy döt Net, Kirill Simonov |
| json-c | `vendor/json-c` | https://github.com/json-c/json-c.git | MIT | Copyright Eric Haszlakiewicz; Metaparadigm Pte Ltd |
| SQLite | `vendor/sqlite` | https://github.com/azadkuh/sqlite-amalgamation.git | Public Domain + BSD-3-Clause | Engine is public domain (sqlite.org). The amalgamation packaging repo is BSD-3-Clause (amir zamani). |
| H2O (picohttpparser) | `vendor/h2o` | https://github.com/h2o/h2o.git | MIT | Copyright DeNA Co., Ltd. Demo uses `deps/picohttpparser`. |
| Raylib | `vendor/raylib` | https://github.com/raysan5/raylib.git | zlib | Copyright Ramon Santamaria (@raysan5) |
| Nuklear | `vendor/nuklear` | https://github.com/Immediate-Mode-UI/Nuklear.git | MIT or public domain | Dual license; choose either. Copyright Micha Mettke |
| PlutoVG | `vendor/plutovg` | https://github.com/sammycage/plutovg.git | MIT | Bundled FreeType raster bits use FTL (`source/FTL.TXT`) |
| NanoVG | `vendor/nanovg` | https://github.com/memononen/nanovg.git | zlib | Copyright Mikko Mononen |
| PDCurses | `vendor/pdcurses` | https://github.com/wmcbrine/PDCurses.git | Public domain | https://pdcurses.org/ |
| libsixel | `vendor/libsixel` | https://github.com/libsixel/libsixel.git | MIT | Copyright Hayaki Saito and libsixel developers |
| Box2D | `vendor/box2d` | https://github.com/erincatto/box2d.git | MIT | Copyright Erin Catto |
| Flecs | `vendor/flecs` | https://github.com/SanderMertens/flecs.git | MIT | Copyright Sander Mertens; portions Meta Platforms |
| GGML | `vendor/ggml` | https://github.com/ggerganov/ggml.git | MIT | Copyright The ggml authors |
| x264 | `vendor/x264` | https://code.videolan.org/videolan/x264.git | GPL-2.0 | Copyleft. Linking x264 into a binary makes that binary GPL-2.0. |
| Opus | `vendor/opus` | https://gitlab.xiph.org/xiph/opus.git | BSD-3-Clause | Copyright Xiph.Org and contributors |
| LAME | `vendor/libmp3lame` | https://github.com/Distrotech/lame.git | LGPL-2.0 | GNU Library GPL v2. Dynamic linking is the usual compliance path. |
| CLAP | `vendor/clap` | https://github.com/free-audio/clap.git | MIT | Header-only plugin API. Copyright Alexandre BIQUE |
| Sokol | `vendor/sokol` | https://github.com/floooh/sokol.git | zlib | Copyright Andre Weissflog |
| hiredis | `vendor/hiredis` | https://github.com/redis/hiredis.git | BSD-3-Clause | Copyright Salvatore Sanfilippo, Pieter Noordhuis |
| protobuf-c | `vendor/protobuf-c` | https://github.com/protobuf-c/protobuf-c.git | BSD-2-Clause | Copyright Dave Benson and the protobuf-c authors |
| wslay | `vendor/wslay` | https://github.com/tatsuhiro-t/wslay.git | MIT | RFC 6455 WebSocket library. Copyright Tatsuhiro Tsujikawa |
| Boehm GC | `vendor/bdwgc` | https://github.com/ivmai/bdwgc.git | MIT-style | Conservative GC. Copyright Boehm, Demers, Xerox, SGI, HP, Ivan Maidanski |

## License files in tree

| Package | File |
| :--- | :--- |
| Lua | `vendor/lua/lua.h` (copyright notice at end of header) |
| Duktape | `vendor/duktape-amal/duktape.h` |
| cmark | `vendor/cmark/COPYING` |
| libyaml | `vendor/libyaml/License` |
| json-c | `vendor/json-c/COPYING` |
| SQLite | `vendor/sqlite/LICENSE` (packaging); sqlite.org public-domain dedication for `sqlite3.c` |
| H2O | `vendor/h2o/LICENSE` |
| Raylib | `vendor/raylib/LICENSE` |
| Nuklear | `vendor/nuklear/LICENSE` |
| PlutoVG | `vendor/plutovg/LICENSE` |
| NanoVG | `vendor/nanovg/LICENSE.txt` |
| PDCurses | `vendor/pdcurses/README.md` |
| libsixel | `vendor/libsixel/LICENSE` |
| Box2D | `vendor/box2d/LICENSE` |
| Flecs | `vendor/flecs/LICENSE` |
| GGML | `vendor/ggml/LICENSE` |
| x264 | `vendor/x264/COPYING` |
| Opus | `vendor/opus/COPYING` |
| LAME | `vendor/libmp3lame/COPYING` |
| CLAP | `vendor/clap/LICENSE` |
| Sokol | `vendor/sokol/LICENSE` |
| hiredis | `vendor/hiredis/COPYING` |
| protobuf-c | `vendor/protobuf-c/LICENSE` |
| wslay | `vendor/wslay/COPYING` |
| Boehm GC | `vendor/bdwgc/LICENSE` |
