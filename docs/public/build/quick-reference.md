# Build and Test Quick Reference

This document is the single source of truth for configuring, building, and testing YAZE on all platforms. Other documentation files link here rather than duplicating these instructions.

---

## 1. Environment Setup

### Clone the Repository

```bash
git clone --recursive https://github.com/scawful/yaze.git
cd yaze
```

### Verify Your Environment

Run the verification script once per machine to check dependencies and fix common issues:

**macOS / Linux:**
```bash
./scripts/verify-build-environment.sh --fix
```

**Windows (PowerShell):**
```powershell
.\scripts\verify-build-environment.ps1 -FixIssues
```

### macOS Toolchain Selection

AppleClang (`/usr/bin/clang`) is the default and most reliable choice. If Homebrew LLVM is on your PATH and you hit SDK header errors, pick the toolchain explicitly:

```bash
# AppleClang (recommended)
cmake --preset mac-dbg --fresh -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++

# Homebrew LLVM (optional)
cmake --preset mac-dbg --fresh -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-brew.toolchain.cmake
```

---

## 2. Build Presets

YAZE uses CMake presets for consistent builds. Configure with `cmake --preset <name>`, then build with `cmake --build --preset <name>`.

### Available Presets (High Level)

- **macOS**: `mac-dbg`, `mac-dbg-v`, `mac-rel`, `mac-dev`, `mac-ai`, `mac-ai-fast`, `mac-uni`, `mac-sdl3`, `mac-test`
- **Windows**: `win-dbg`, `win-dbg-v`, `win-rel`, `win-dev`, `win-ai`, `win-z3ed`, `win-arm`, `win-arm-rel`, `win-vs-dbg`, `win-vs-rel`, `win-vs-ai`, `win-sdl3`, `win-test`
- **Linux**: `lin-dbg`, `lin-dbg-v`, `lin-rel`, `lin-dev`, `lin-ai`, `lin-sdl3`, `lin-test`
- **iOS**: `ios-debug`, `ios-release` (device builds for the thin Xcode shell)
- **CI**: `ci-linux`, `ci-macos`, `ci-windows`, `ci-windows-ai`
- **WASM**: `wasm-debug`, `wasm-release`, `wasm-crash-repro`, `wasm-ai`

`mac-ai-fast` prefers the Homebrew gRPC/protobuf stack for faster configure times (`brew install grpc protobuf abseil`).

**Tip:** Add `-v` suffix (e.g., `mac-dbg-v`) to enable verbose compiler warnings.

---

### iOS (Device) Build Flow

Use the thin iOS shell backed by CMake-built static libs:

```bash
scripts/build-ios.sh           # builds ios-debug + generates Xcode project
scripts/build-ios.sh ios-release
```

This generates `src/ios/yaze_ios.xcodeproj` and a bundled static library at
`build-ios/ios/libyaze_ios_bundle.a`. Open the Xcode project and run on device.
Requires `xcodegen` (`brew install xcodegen`) and the iOS SDK from Xcode.

For CLI-driven device deploys (build + install + optional launch):

```bash
scripts/xcodebuild-ios.sh ios-debug deploy
scripts/xcodebuild-ios.sh ios-debug deploy "Baby Pad"
```

`deploy` resolves the app bundle from DerivedData and installs via
`xcrun devicectl`. Device selection order is `DEVICE` arg,
`$YAZE_IOS_DEVICE`, then `"Baby Pad"`.

---

## 3. Build Directory Policy

| Build Type | Default Directory |
|------------|-------------------|
| Native (desktop/CLI) | `build/` |
| Native (AI presets) | `build_ai/` |
| WASM | `build-wasm/` |

macOS and Windows presets use multi-config generators, so binaries live under
`build_ai/bin/Debug` or `build_ai/bin/Release` (AI presets) and `build/bin/...`
(non-AI presets). Linux uses single-config builds in `build/bin` or
`build_ai/bin` depending on preset.

If you need per-user or per-agent isolation, create a local
`CMakeUserPresets.json` that points `binaryDir` to a custom path outside the
repo. Avoid creating additional `build_*` folders beyond `build/` and
`build_ai/` to keep the checkout small.

Example:
```bash
cp CMakeUserPresets.json.example CMakeUserPresets.json
export YAZE_BUILD_ROOT="$HOME/.cache/yaze"
cmake --preset dev-local
cmake --build --preset dev-local --target yaze
```

You can also set `YAZE_BUILD_DIR` for scripts and the agent build tool to direct builds to an external path:
```bash
export YAZE_BUILD_DIR="$HOME/.cache/yaze/build"
```

For AI-enabled builds, use the `*-ai` presets and specify only the targets you need:
```bash
cmake --build --preset mac-ai --target yaze z3ed
```

### Local Nightly Install (Isolated)

Use `scripts/install-nightly.sh` to keep a separate clone and install prefix for
nightly builds so your dev `build/` stays untouched.

```bash
scripts/install-nightly.sh
```

Wrappers are created under `~/.local/bin`:
- `yaze-nightly` (GUI)
- `yaze-nightly-grpc` (GUI + gRPC for MCP)
- `z3ed-nightly` (CLI)
- `yaze-mcp-nightly` (MCP server; expects `~/.yaze/yaze-mcp/venv`)

On macOS, a stable app link is created (default: `~/Applications/Yaze Nightly.app`).
Use that path for menu bar launchers (Sketchybar, Raycast, Alfred, etc.).
Example Sketchybar click script: `open -a "$HOME/Applications/Yaze Nightly.app"`.

How it works:
- Clones `origin` into `$YAZE_NIGHTLY_REPO` (default `~/.yaze/nightly/repo`) and keeps it clean.
- Builds into `$YAZE_NIGHTLY_BUILD_DIR` (default `~/.yaze/nightly/repo/build-nightly`).
- Installs into `$YAZE_NIGHTLY_PREFIX/releases/<timestamp>` and updates `.../current` symlink.
- Writes wrapper scripts to `$YAZE_NIGHTLY_BIN_DIR` (default `~/.local/bin`).

Updating:
```bash
scripts/install-nightly.sh  # re-pulls and installs a new release
```

Removing:
```bash
rm -rf ~/.local/yaze/nightly ~/.local/bin/yaze-nightly* ~/.local/bin/z3ed-nightly ~/.local/bin/yaze-mcp-nightly
```

Key overrides:
```bash
export YAZE_NIGHTLY_REPO="$HOME/.yaze/nightly/repo"
export YAZE_NIGHTLY_PREFIX="$HOME/.local/yaze/nightly"
export YAZE_NIGHTLY_BUILD_TYPE=RelWithDebInfo
export YAZE_GRPC_PORT=50051
export YAZE_MCP_REPO="$HOME/.yaze/yaze-mcp"
export YAZE_NIGHTLY_APP_DIR="$HOME/Applications"
```

### Shared Dependency Caches (Recommended)

Set shared caches once per machine to avoid re-downloading dependencies after
cleaning build directories.

**macOS / Linux:**
```bash
export CPM_SOURCE_CACHE="$HOME/.cpm-cache"
export VCPKG_DOWNLOADS="$HOME/.cache/vcpkg/downloads"
export VCPKG_BINARY_SOURCES="clear;files,$HOME/.cache/vcpkg/bincache,readwrite"
```

**Windows (PowerShell):**
```powershell
$env:CPM_SOURCE_CACHE = "$env:USERPROFILE\.cpm-cache"
$env:VCPKG_DOWNLOADS = "$env:LOCALAPPDATA\vcpkg\downloads"
$env:VCPKG_BINARY_SOURCES = "clear;files,$env:LOCALAPPDATA\vcpkg\bincache,readwrite"
```

You can also set these in `CMakeUserPresets.json` (see `CMakeUserPresets.json.example`).

**Windows Helper Scripts:**
- Quick builds: `scripts/agents/windows-smoke-build.ps1`
- Test runs: `scripts/agents/run-tests.sh` (or PowerShell equivalent)

---

## 4. Common Build Commands

**Tip:** Prefer `./scripts/yaze` and `./scripts/z3ed` to run the newest local
binary (the wrappers prefer `build_ai/` outputs when available).

### Standard Debug Build

**macOS:**
```bash
cmake --preset mac-dbg
cmake --build --preset mac-dbg --target yaze
```

**Linux:**
```bash
cmake --preset lin-dbg
cmake --build --preset lin-dbg --target yaze
```

**Windows:**
```bash
cmake --preset win-dbg
cmake --build --preset win-dbg --target yaze
```

### AI-Enabled Build (with gRPC and z3ed CLI)

**macOS:**
```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze z3ed
```

**Linux:**
```bash
cmake --preset lin-ai
cmake --build --preset lin-ai --target yaze z3ed
```

**Windows:**
```bash
cmake --preset win-ai
cmake --build --preset win-ai --target yaze z3ed
```

---

## 5. Testing

YAZE uses CTest with GoogleTest. Tests are organized by category using labels.

### Quick Start

```bash
# Run stable tests (fast, no ROM required)
ctest --test-dir build -L stable -j4

# Run all enabled tests
ctest --test-dir build --output-on-failure

# Run tests matching a pattern
ctest --test-dir build -R "Dungeon"
```

### Direct Test Binaries

```bash
# macOS/Windows (multi-config)
./build/bin/Debug/yaze_emu_test --emu_test_rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_unit --rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_integration --rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_gui --rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_benchmark --rom=roms/alttp_vanilla.sfc

# Linux (single-config)
./build/bin/yaze_emu_test --emu_test_rom=roms/alttp_vanilla.sfc
```

### Test Categories

| Category | Command | Description |
|----------|---------|-------------|
| Stable | `ctest --test-dir build -L stable` | Core unit tests, always available |
| GUI | `ctest --test-dir build -L gui` | GUI smoke tests |
| ROM-dependent | `ctest --test-dir build -L rom_dependent` | Requires a Zelda 3 ROM |
| Experimental | `ctest --test-dir build -L experimental` | AI/experimental features |

### Enabling ROM-Dependent Tests

```bash
# Configure with ROM path
cmake --preset mac-dev -DYAZE_TEST_ROM_VANILLA_PATH="$PWD/roms/alttp_vanilla.sfc"

# Build and run
cmake --build --preset mac-dev --target yaze_test
ctest --test-dir build -L rom_dependent
```

### Test Coverage by Preset

| Preset | Stable | GUI | ROM-Dep | Experimental |
|--------|:------:|:---:|:-------:|:------------:|
| `*-dbg` | Yes | Yes | No | No |
| `*-ai` | Yes | Yes | No | Yes |
| `*-dev` | Yes | Yes | Yes | No |
| `*-rel` | No | No | No | No |

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `YAZE_TEST_ROM_VANILLA` | Path to vanilla ROM for ROM-dependent tests |
| `YAZE_TEST_ROM_EXPANDED` | Path to expanded (ZSCustom/OOS) ROM |
| `YAZE_TEST_ROM_PATH` | Legacy ROM path (vanilla fallback) |
| `YAZE_SKIP_ROM_TESTS` | Skip ROM tests (useful for CI) |
| `YAZE_ENABLE_UI_TESTS` | Enable GUI tests (auto-detected if display available) |

---

## 6. Further Reading

- **[Build Troubleshooting](troubleshooting.md)** - Solutions for common build issues
- **[Platform Compatibility](platform-compatibility.md)** - Platform-specific notes and CI/CD details
- **[CMake Presets Guide](presets.md)** - Complete preset reference
- **[Testing Guide](../developer/testing-guide.md)** - Comprehensive testing documentation
