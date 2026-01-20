# Build Presets Guide

This document explains the reorganized CMake preset system for Yaze.

## Design Principles

1. **Short, memorable names** - No more `macos-dev-z3ed-ai`, just `mac-ai`
2. **Warnings off by default** - Add `-v` suffix for verbose (e.g., `mac-dbg-v`)
3. **Clear architecture support** - Explicit ARM64 and x86_64 presets
4. **Platform prefixes** - `mac-`, `win-`, `lin-` for easy identification

## Quick Start

### macOS Development
```bash
# Most common: AI-enabled development
cmake --preset mac-ai
cmake --build --preset mac-ai

# Basic debug build (no AI)
cmake --preset mac-dbg
cmake --build --preset mac-dbg

# Verbose warnings for debugging
cmake --preset mac-dbg-v
cmake --build --preset mac-dbg-v

# Release build
cmake --preset mac-rel
cmake --build --preset mac-rel
```

### Windows Development
```bash
# Debug build (x64)
cmake --preset win-dbg
cmake --build --preset win-dbg

# AI-enabled development
cmake --preset win-ai
cmake --build --preset win-ai

# ARM64 support
cmake --preset win-arm
cmake --build --preset win-arm
```

## All Presets

### Cross-platform Presets

| Preset | Description | Notes |
|--------|-------------|-------|
| `dev` | Full development build | AI + gRPC + tests |
| `dev-system-grpc` | Dev build using system gRPC/protobuf | Requires gRPC/protobuf/abseil installed |
| `release` | Optimized release build | LTO enabled |
| `minimal` | Minimal build | No gRPC/AI |

### macOS Presets

| Preset | Description | Arch | Warnings | Notes |
|--------|-------------|------|----------|-------|
| `mac-dbg` | Debug build | Host | Off | Tests enabled |
| `mac-dbg-v` | Debug build (verbose) | Host | On | Extra warnings |
| `mac-rel` | Release build | Host | Off | LTO enabled |
| `mac-dev` | Development build | Host | Off | ROM tests enabled |
| `mac-ai` | AI + gRPC dev | Host | Off | Agent UI + automation |
| `mac-ai-fast` | AI dev (system gRPC) | Host | Off | `brew install grpc protobuf abseil` |
| `mac-uni` | Universal binary | arm64 + x86_64 | Off | Release packaging |
| `mac-sdl3` | SDL3 build | Host | Off | Experimental |
| `mac-test` | Fast test build | Host | Off | RelWithDebInfo |

### Windows Presets

| Preset | Description | Arch | Generator | Notes |
|--------|-------------|------|-----------|-------|
| `win-dbg` | Debug build | x64 | Ninja | Tests enabled |
| `win-dbg-v` | Debug build (verbose) | x64 | Ninja | Extra warnings |
| `win-rel` | Release build | x64 | Ninja | LTO enabled |
| `win-dev` | Development build | x64 | Ninja | ROM tests enabled |
| `win-ai` | AI + gRPC dev | x64 | Ninja | Agent UI + automation |
| `win-z3ed` | z3ed CLI | x64 | Ninja | CLI stack only |
| `win-arm` | Debug ARM64 | ARM64 | Ninja | |
| `win-arm-rel` | Release ARM64 | ARM64 | Ninja | |
| `win-vs-dbg` | Debug build | x64 | Visual Studio | |
| `win-vs-rel` | Release build | x64 | Visual Studio | |
| `win-vs-ai` | AI + gRPC dev | x64 | Visual Studio | |
| `win-sdl3` | SDL3 build | x64 | Ninja | Experimental |
| `win-test` | Fast test build | x64 | Ninja | RelWithDebInfo |

### Linux Presets

| Preset | Description | Compiler | Notes |
|--------|-------------|----------|-------|
| `lin-dbg` | Debug build | GCC/Clang | Tests enabled |
| `lin-dbg-v` | Debug build (verbose) | GCC/Clang | Extra warnings |
| `lin-rel` | Release build | GCC/Clang | LTO enabled |
| `lin-dev` | Development build | GCC/Clang | ROM tests enabled |
| `lin-ai` | AI + gRPC dev | GCC/Clang | Agent UI + automation |
| `lin-sdl3` | SDL3 build | GCC/Clang | Experimental |
| `lin-test` | Fast test build | GCC/Clang | RelWithDebInfo |

### CI Presets

| Preset | Description |
|--------|-------------|
| `ci-linux` | Linux CI build |
| `ci-macos` | macOS CI build |
| `ci-windows` | Windows CI build |
| `ci-windows-ai` | Windows AI/automation CI build |

### WASM Presets

| Preset | Description |
|--------|-------------|
| `wasm-debug` | WebAssembly debug build |
| `wasm-release` | WebAssembly release build |
| `wasm-crash-repro` | Minimal repro build |
| `wasm-ai` | WebAssembly AI build |

### Test Presets

Run with `ctest --preset <name>`.

| Preset | Description |
|--------|-------------|
| `all` | All tests (including ROM-dependent) |
| `stable` | Stable tests only |
| `unit` | Unit tests only |
| `integration` | Integration tests only |
| `fast` | macOS fast test preset |
| `fast-win` | Windows fast test preset |
| `fast-lin` | Linux fast test preset |
| `stable-ai` | Stable tests against `ci-windows-ai` |
| `unit-ai` | Unit tests against `ci-windows-ai` |
| `integration-ai` | Integration tests against `ci-windows-ai` |

## Warning Control

By default, all presets suppress compiler warnings with `-w` for a cleaner build experience.

### To Enable Verbose Warnings:

1. Use a preset with `-v` suffix (e.g., `mac-dbg-v`, `win-dbg-v`)
2. Or set `YAZE_SUPPRESS_WARNINGS=OFF` manually:
   ```bash
   cmake --preset mac-dbg -DYAZE_SUPPRESS_WARNINGS=OFF
   ```

## Architecture Support

### macOS
- **Apple Silicon (arm64)**: `mac-dbg`, `mac-rel`, `mac-ai`, `mac-dev`, etc.
- **Intel (x86_64)**: Use the same `mac-*` presets (they target the host arch).
- **Universal Binary**: `mac-uni` (arm64 + x86_64)

### Windows
- **x64**: `win-dbg`, `win-rel`, `win-ai`, etc.
- **ARM64**: `win-arm`, `win-arm-rel`

## Build Directories

Most presets use `build/`. WASM presets use `build-wasm/`. Keep only these two build directories in the repo to avoid bloat. For isolated agent builds, point `binaryDir` to a path outside the repo via `CMakeUserPresets.json`.

### Shared Dependency Caches

To avoid re-downloading dependencies after cleaning build dirs, set shared caches via `CMakeUserPresets.json` (see `CMakeUserPresets.json.example`) or environment variables:

```bash
export CPM_SOURCE_CACHE="$HOME/.cpm-cache"
export VCPKG_DOWNLOADS="$HOME/.cache/vcpkg/downloads"
export VCPKG_BINARY_SOURCES="clear;files,$HOME/.cache/vcpkg/bincache,readwrite"
```

For scripts and the agent build tool, you can also set `YAZE_BUILD_DIR` to an external path (e.g., `$HOME/.cache/yaze/build`) to keep the working tree clean.

## Feature Flags

Common CMake options you can override:

```bash
# Enable/disable components
-DYAZE_BUILD_TESTS=ON/OFF
-DYAZE_BUILD_AGENT_UI=ON/OFF

# AI features
-DYAZE_ENABLE_AI=ON/OFF
-DYAZE_ENABLE_AI_RUNTIME=ON/OFF
-DYAZE_ENABLE_GRPC=ON/OFF
-DYAZE_PREFER_SYSTEM_GRPC=ON/OFF
-DYAZE_ENABLE_JSON=ON/OFF
-DYAZE_ENABLE_REMOTE_AUTOMATION=ON/OFF

# Testing
-DYAZE_ENABLE_ROM_TESTS=ON/OFF
-DYAZE_TEST_ROM_VANILLA_PATH=/path/to/alttp_vanilla.sfc
-DYAZE_TEST_ROM_EXPANDED_PATH=/path/to/oos168.sfc
-DYAZE_USE_SDL3=ON/OFF
-DYAZE_ENABLE_LTO=ON/OFF
```

## Examples

### Development with AI features and verbose warnings
```bash
cmake --preset mac-dbg-v -DYAZE_ENABLE_AI=ON -DYAZE_ENABLE_GRPC=ON -DYAZE_BUILD_AGENT_UI=ON
cmake --build --preset mac-dbg-v
```

### Release build for distribution (macOS Universal)
```bash
cmake --preset mac-uni
cmake --build --preset mac-uni
cpack --preset mac-uni
```

### Quick minimal build for testing
```bash
cmake --preset mac-dbg -DYAZE_ENABLE_AI=OFF -DYAZE_ENABLE_GRPC=OFF -DYAZE_BUILD_AGENT_UI=OFF
cmake --build --preset mac-dbg -j12
```

## Updating compile_commands.json

After configuring with a new preset, copy the compile commands for IDE support:

```bash
cp build/compile_commands.json .
```

This ensures clangd and other LSP servers can find headers and understand build flags.

## Migration from Old Presets

Old preset names have been simplified:

| Old Name | New Name |
|----------|----------|
| `macos-dev-z3ed-ai` | `mac-ai` |
| `macos-debug` | `mac-dbg` |
| `macos-release` | `mac-rel` |
| `macos-debug-universal` | `mac-uni` |
| `windows-debug` | `win-dbg` |
| `windows-release` | `win-rel` |
| `windows-arm64-debug` | `win-arm` |
| `linux-debug` | `lin-dbg` |

## Troubleshooting

### Warnings are still showing
- Make sure you're using a preset without `-v` suffix
- Check `cmake` output for `✓ Warnings suppressed` message
- Reconfigure and rebuild: `rm -rf build && cmake --preset mac-dbg`

### clangd can't find nlohmann/json
- Run `cmake --preset <your-preset>` to regenerate compile_commands.json
- Copy to root: `cp build/compile_commands.json .`
- Restart your IDE or LSP server

### Build fails with missing dependencies
- Re-run configure to fetch CPM dependencies: `cmake --preset <your-preset>`
- If offline, set `CPM_SOURCE_CACHE` to a populated cache directory
- For AI features, make sure you have OpenSSL: `brew install openssl` (macOS)
