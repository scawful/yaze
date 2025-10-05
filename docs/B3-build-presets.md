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

### macOS Presets

| Preset | Description | Arch | Warnings | Features |
|--------|-------------|------|----------|----------|
| `mac-dbg` | Debug build | ARM64 | Off | Basic |
| `mac-dbg-v` | Debug verbose | ARM64 | On | Basic |
| `mac-rel` | Release | ARM64 | Off | Basic |
| `mac-x64` | Debug x86_64 | x86_64 | Off | Basic |
| `mac-uni` | Universal binary | Both | Off | Basic |
| `mac-dev` | Development | ARM64 | Off | ROM tests |
| `mac-ai` | AI development | ARM64 | Off | z3ed, JSON, gRPC, ROM tests |
| `mac-z3ed` | z3ed CLI | ARM64 | Off | AI agent support |
| `mac-rooms` | Dungeon editor | ARM64 | Off | Minimal build for room editing |

### Windows Presets

| Preset | Description | Arch | Warnings | Features |
|--------|-------------|------|----------|----------|
| `win-dbg` | Debug build | x64 | Off | Basic |
| `win-dbg-v` | Debug verbose | x64 | On | Basic |
| `win-rel` | Release | x64 | Off | Basic |
| `win-arm` | Debug ARM64 | ARM64 | Off | Basic |
| `win-arm-rel` | Release ARM64 | ARM64 | Off | Basic |
| `win-dev` | Development | x64 | Off | ROM tests |
| `win-ai` | AI development | x64 | Off | z3ed, JSON, gRPC, ROM tests |
| `win-z3ed` | z3ed CLI | x64 | Off | AI agent support |

### Linux Presets

| Preset | Description | Compiler | Warnings |
|--------|-------------|----------|----------|
| `lin-dbg` | Debug | GCC | Off |
| `lin-clang` | Debug | Clang | Off |

### Special Purpose

| Preset | Description |
|--------|-------------|
| `ci` | Continuous integration (no ROM tests) |
| `asan` | AddressSanitizer build |
| `coverage` | Code coverage build |

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
- **ARM64 (Apple Silicon)**: `mac-dbg`, `mac-rel`, `mac-ai`, etc.
- **x86_64 (Intel)**: `mac-x64`
- **Universal Binary**: `mac-uni` (both ARM64 + x86_64)

### Windows
- **x64**: `win-dbg`, `win-rel`, `win-ai`, etc.
- **ARM64**: `win-arm`, `win-arm-rel`

## Build Directories

Most presets use `build/` directory. Exceptions:
- `mac-rooms`: Uses `build_rooms/` to avoid conflicts

## Feature Flags

Common CMake options you can override:

```bash
# Enable/disable components
-DYAZE_BUILD_TESTS=ON/OFF
-DYAZE_BUILD_Z3ED=ON/OFF
-DYAZE_BUILD_EMU=ON/OFF
-DYAZE_BUILD_APP=ON/OFF

# AI features
-DZ3ED_AI=ON/OFF
-DYAZE_WITH_JSON=ON/OFF
-DYAZE_WITH_GRPC=ON/OFF

# Testing
-DYAZE_ENABLE_ROM_TESTS=ON/OFF
-DYAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Build optimization
-DYAZE_MINIMAL_BUILD=ON/OFF
-DYAZE_USE_MODULAR_BUILD=ON/OFF
```

## Examples

### Development with AI features and verbose warnings
```bash
cmake --preset mac-dbg-v -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
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
cmake --preset mac-dbg -DYAZE_MINIMAL_BUILD=ON
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
| `macos-dungeon-dev` | `mac-rooms` |
| `windows-debug` | `win-dbg` |
| `windows-release` | `win-rel` |
| `windows-arm64-debug` | `win-arm` |
| `linux-debug` | `lin-dbg` |
| `linux-clang` | `lin-clang` |

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
- Ensure submodules are initialized: `git submodule update --init --recursive`
- For AI features, make sure you have OpenSSL: `brew install openssl` (macOS)
