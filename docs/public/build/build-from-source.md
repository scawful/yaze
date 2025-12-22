# Build from Source

YAZE uses a modern CMake build system with presets for easy configuration. This guide covers environment setup, dependencies, and platform-specific considerations.

> **Quick Start:** For concise build commands, see the [Build and Test Quick Reference](quick-reference.md).

---

## 1. Environment Verification

Before your first build, run the verification script to ensure your environment is configured correctly.

### Windows (PowerShell)

```powershell
.\scripts\verify-build-environment.ps1

# With automatic fixes
.\scripts\verify-build-environment.ps1 -FixIssues
```

> **Tip:** After verification, run `.\scripts\setup-vcpkg-windows.ps1` to bootstrap vcpkg, install clang-cl/Ninja, and cache the x64-windows triplet.

### macOS and Linux

```bash
./scripts/verify-build-environment.sh

# With automatic fixes
./scripts/verify-build-environment.sh --fix
```

The script checks for CMake, a C++23 compiler, and platform-specific dependencies.

---

## 2. Using Presets

Select a preset that matches your platform and workflow:

| Workflow | Presets |
|----------|---------|
| Debug builds | `mac-dbg`, `lin-dbg`, `win-dbg` |
| AI-enabled builds | `mac-ai`, `lin-ai`, `win-ai` |
| AI + system gRPC (macOS) | `mac-ai-fast` |
| Release builds | `mac-rel`, `lin-rel`, `win-rel` |
| Development (ROM tests) | `mac-dev`, `lin-dev`, `win-dev` |
| Fast test builds | `mac-test`, `lin-test`, `win-test` |

**Build Commands:**
```bash
cmake --preset <name>                        # Configure
cmake --build --preset <name> --target yaze  # Build
```

Add `-v` suffix (e.g., `mac-dbg-v`) to enable verbose compiler warnings.

See the [CMake Presets Guide](presets.md) for the complete preset reference.

---

## Build Directory Policy

By default, native builds live in `build/` and WASM builds live in `build-wasm/`. Keep additional build directories outside the repo to avoid size creep:

```bash
cp CMakeUserPresets.json.example CMakeUserPresets.json
export YAZE_BUILD_ROOT="$HOME/.cache/yaze"
cmake --preset dev-local
```

For scripts and agent-driven builds, set `YAZE_BUILD_DIR` to an external path (for example, `$HOME/.cache/yaze/build`).

---

## Feature Toggles

### Windows Presets

| Preset | Purpose |
|--------|---------|
| `win-dbg`, `win-rel` | Core builds without agent UI or AI. Fastest option. |
| `win-ai`, `win-vs-ai` | Full agent stack (UI + automation + AI runtime) |
| `ci-windows-ai` | CI preset for the complete automation stack |

### CMake Feature Flags

| Option | Default | Description |
|--------|---------|-------------|
| `YAZE_BUILD_AGENT_UI` | ON with GUI | ImGui chat/agent panels |
| `YAZE_ENABLE_REMOTE_AUTOMATION` | ON for `*-ai` | gRPC services and automation |
| `YAZE_ENABLE_AI_RUNTIME` | ON for `*-ai` | Gemini/Ollama AI providers |
| `YAZE_ENABLE_AGENT_CLI` | ON with CLI | z3ed agent commands |

Keep features `OFF` for lightweight GUI development, or enable them for automation workflows.

---

## 3. Dependencies

### Required

- **CMake** 3.16 or later
- **C++23 Compiler**: GCC 13+, Clang 16+, or MSVC 2022 17.4+
- **Git**

### Bundled (No Installation Required)

All core dependencies are included in the `ext/` directory or fetched automatically:
- SDL2, ImGui, Asar, nlohmann/json, cpp-httplib, GoogleTest

### Optional

- **gRPC**: Required for GUI automation and AI features. Enable with `-DYAZE_ENABLE_GRPC=ON`.
- **vcpkg (Windows)**: Speeds up gRPC builds on Windows.

---

## 4. Platform Setup

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install build tools via Homebrew
brew install cmake pkg-config

# Optional: For sandboxed/offline builds
brew install yaml-cpp googletest
```

> **Note:** In sandboxed environments, install yaml-cpp and googletest via Homebrew to avoid network fetch failures. The build system auto-detects Homebrew installations at `/opt/homebrew/opt/` (Apple Silicon) or `/usr/local/opt/` (Intel).

#### Toolchain Options (macOS)

AppleClang (`/usr/bin/clang`) is the default and most reliable. If you want Homebrew LLVM, use the provided toolchain file so libc++ headers and rpaths are consistent:

```bash
# AppleClang (recommended)
cmake --preset mac-dbg --fresh -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++

# Homebrew LLVM
brew install llvm
cmake --preset mac-dbg --fresh -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-brew.toolchain.cmake
```

`mac-ai-fast` expects Homebrew gRPC/protobuf/abseil (`brew install grpc protobuf abseil`).

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libgtk-3-dev libdbus-1-dev
```

### Windows

1. **Install Visual Studio 2022** with "Desktop development with C++" workload
2. **Install Ninja** (recommended): `choco install ninja` or via Visual Studio Installer
3. **Run the verifier:**
   ```powershell
   .\scripts\verify-build-environment.ps1 -FixIssues
   ```
4. **Bootstrap vcpkg:**
   ```powershell
   .\scripts\setup-vcpkg-windows.ps1
   ```
5. **Build:**
   - Use `win-*` presets with Ninja generator
   - Use `win-vs-*` presets for Visual Studio IDE
   - For AI features, use `win-ai` or `win-vs-ai`

## 5. Testing

The project uses CTest and GoogleTest. Tests are organized into categories using labels. See the [Testing Guide](../developer/testing-guide.md) for details.

### Running Tests with Presets

The easiest way to run tests is with `ctest` presets.

```bash
# Configure a development build (enables ROM-dependent tests)
cmake --preset mac-dev -DYAZE_TEST_ROM_VANILLA_PATH="$PWD/roms/alttp_vanilla.sfc"

# Build the tests
cmake --build --preset mac-dev --target yaze_test

# Run stable tests (fast, run in CI)
ctest --preset stable

# Run all tests, including ROM-dependent and experimental
ctest --preset all
```

### Running Tests Manually

You can also run tests by invoking the test executable directly or using CTest with labels.

```bash
# Run tests via the executables (multi-config paths on macOS/Windows)
./build/bin/Debug/yaze_emu_test --emu_test_rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_stable --rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_gui --rom=roms/alttp_vanilla.sfc
./build/bin/Debug/yaze_test_benchmark --rom=roms/alttp_vanilla.sfc

# Run only stable tests using CTest labels
ctest --test-dir build --label-regex "STABLE"

# Run tests matching a name
ctest --test-dir build -R "AsarWrapperTest"

# Exclude ROM-dependent tests
ctest --test-dir build --label-exclude "ROM_DEPENDENT"
```

## 6. IDE Integration

### VS Code (Recommended)
1.  Install the **CMake Tools** extension.
2.  Open the project folder.
3.  Select a preset from the status bar (e.g., `mac-ai`). On Windows, choose the desired kit (e.g., “Visual Studio Build Tools 2022”) so the generator matches your preset (`win-*` uses Ninja, `win-vs-*` uses Visual Studio).
4.  Press F5 to build and debug.
5.  After changing presets, run `cp build/compile_commands.json .` to update IntelliSense.

### Visual Studio (Windows)
1.  Select **File → Open → Folder** and choose the `yaze` directory.
2.  Visual Studio will automatically detect `CMakePresets.json`.
3.  Select the desired preset (e.g., `win-dbg` or `win-ai`) from the configuration dropdown.
4.  Press F5 to build and run.

### Xcode (macOS)
```bash
# Generate an Xcode project from a preset
cmake --preset mac-dbg -G Xcode

# Open the project
open build/yaze.xcodeproj
```

## 7. Windows Build Optimization

### GitHub Actions / CI Builds

**Current Configuration (Optimized):**
- **Compilers**: Both clang-cl and MSVC supported (matrix)
- **vcpkg**: Only fast packages (SDL2, yaml-cpp) - 2 minutes
- **gRPC**: Built via FetchContent (v1.75.1) - cached after first build
- **Caching**: Aggressive multi-tier caching (vcpkg + FetchContent + sccache)
- **Agent matrix**: A dedicated `ci-windows-ai` job runs outside pull requests to exercise the full gRPC + AI runtime stack.
- **Expected time**: 
  - First build: ~10-15 minutes
  - Cached build: ~3-5 minutes

**Why FetchContent for gRPC in CI?**
- vcpkg's latest gRPC (v1.71.0) has no pre-built binaries
- Building from source via vcpkg: 45-90 minutes
- FetchContent with caching: 10-15 minutes first time, <1 min cached
- Better control over gRPC version (v1.75.1 - latest stable)
- BoringSSL ASM disabled on Windows for clang-cl compatibility
- zlib conflict: gRPC's FetchContent builds its own zlib, conflicts with vcpkg's

### Desktop Development: Faster builds with vcpkg (optional)

For desktop development, you can use vcpkg for faster gRPC builds:

```powershell
# Bootstrap vcpkg and prefetch packages
.\scripts\setup-vcpkg-windows.ps1

# Configure with vcpkg
cmake -B build -DYAZE_USE_VCPKG_GRPC=ON -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
```

**Benefits:**
- Pre-compiled gRPC packages: ~5 minutes vs ~10-15 minutes
- No need to build gRPC from source
- Faster iteration during development

**Note:** CI/CD workflows use FetchContent by default for reliability.

### Local Development

#### Fast Build (Recommended)

Use FetchContent for all dependencies (matches CI):
```powershell
# Configure (first time: ~15 min, subsequent: ~2 min)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config RelWithDebInfo --parallel
```

#### Using vcpkg (Optional)

If you prefer vcpkg for local development:
```powershell
# Install ONLY the fast packages
vcpkg install sdl2:x64-windows yaml-cpp:x64-windows

# Let CMake use FetchContent for gRPC
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
```

**DO NOT** install grpc or zlib via vcpkg:
- gRPC v1.71.0 has no pre-built binaries (45-90 min build)
- zlib conflicts with gRPC's bundled zlib

### Compiler Support

**clang-cl (Recommended):**
- Used in both CI and release workflows
- Better diagnostics than MSVC
- Fully compatible with MSVC libraries

**MSVC:**
- Also tested in CI matrix
- Fallback option if clang-cl issues occur

**Compiler Flags (Applied Automatically):**
- `/bigobj` - Large object files (required for gRPC)
- `/permissive-` - Standards conformance
- `/wd4267 /wd4244` - Suppress harmless conversion warnings
- `/constexpr:depth2048` - Template instantiation depth

## 8. Troubleshooting

Build issues, especially on Windows, often stem from environment misconfiguration. Before anything else, run the verification script.

```powershell
# Run the verification script in PowerShell
.\scripts\verify-build-environment.ps1
```

This script is your primary diagnostic tool and can detect most common problems.

### Automatic Fixes

If the script finds issues, you can often fix them automatically by running it with the `-FixIssues` flag. This can:
- Synchronize Git submodules.
- Correct Git `core.autocrlf` and `core.longpaths` settings, which are critical for cross-platform compatibility on Windows.
- Prompt to clean stale CMake caches.

```powershell
# Attempt to fix detected issues automatically
.\scripts\verify-build-environment.ps1 -FixIssues
```

### Cleaning Stale Builds

After pulling major changes or switching branches, your build directory can become "stale," leading to strange compiler or linker errors. The verification script will warn you about old build files. You can clean them manually or use the `-CleanCache` flag.

**This will delete all `build*` and `out` directories.**

```powershell
# Clean all build artifacts to start fresh
.\scripts\verify-build-environment.ps1 -CleanCache
```

### Common Issues

#### "nlohmann/json.hpp: No such file or directory"
**Cause**: You are building code that requires AI features without using an AI-enabled preset, or CMake has not fetched CPM dependencies yet.
**Solution**:
1.  Use an AI preset like `win-ai` or `mac-ai`.
2.  Reconfigure to fetch dependencies: `cmake --preset <preset>` (or delete the build folder and re-run).

#### "Cannot open file 'yaze.exe': Permission denied"
**Cause**: A previous instance of `yaze.exe` is still running in the background.
**Solution**: Close it using Task Manager or run:
```cmd
taskkill /F /IM yaze.exe
```

#### "C++ standard 'cxx_std_23' not supported"
**Cause**: Your compiler is too old.
**Solution**: Update your tools. You need Visual Studio 2022 17.4+, GCC 13+, or Clang 16+. The verification script checks this.

#### Visual Studio Can't Find Presets
**Cause**: VS failed to parse `CMakePresets.json` or its cache is corrupt.
**Solution**:
1.  Close and reopen the folder (`File -> Close Folder`).
2.  Check the "CMake" pane in the Output window for specific JSON parsing errors.
3.  Delete the hidden `.vs` directory in the project root to force Visual Studio to re-index the project.

#### Git Line Ending (CRLF) Issues
**Cause**: Git may be automatically converting line endings, which can break shell scripts and other assets.
**Solution**: The verification script checks for this. Use the `-FixIssues` flag or run `git config --global core.autocrlf false` to prevent this behavior.

#### File Path Length Limit on Windows
**Cause**: By default, Windows has a 260-character path limit, which can be exceeded by nested dependencies.
**Solution**: The verification script checks for this. Use the `-FixIssues` flag or run `git config --global core.longpaths true` to enable long path support.
