# Compiling on any platform for any platform with Zig

## Requirements

- [Zig for the host platform](https://ziglang.org/download/)

## Compile Luanti `zig build`

1. Ensure `zig` is findable in your path (open a terminal and type `zig version`)
2. Navigate that terminal to this repository root dir
3. Determine the options you would like to compile with
4. Run `zig build`
5. The resulting binary is in `bin/`

## Remaining things to get working:

- SDL2 - doable, cmake-based, check out vcpkg
- CURL - follow vcpkg
- FREETYPE, perhaps done: https://github.com/mitchellh/zig-build-freetype
- GETTEXT, follow vcpkg
- GMP, follow vcpkg
- ICONV, follow vcpkg
- JPEG, follow vcpkg
- JSON, follow vcpkg
- LEVELDB, follow vcpkg
- PostgreSQL, follow vcpkg
- REDIS, follow vcpkg
- SPATIAL, follow vcpkg
- LUA, follow vcpkg
- OGG, follow vcpkg
- OPENAL, follow vcpkg
- PNG perhaps done: https://github.com/mitchellh/zig-build-libpng
- PROMETHEUS_PULL, follow vcpkg
- PROMETHEUS_CORE, follow vcpkg
- SQLITE3, follow vcpkg
- VORBISFILE, follow vcpkg
- ZLIB - Doable, perhaps done: https://github.com/ExeVirus/zig-build-zlib
- ZSTD, follow vcpkg