# Build Instructions

yaze uses a modern CMake build system with presets for easy configuration. This guide covers how to build yaze on macOS, Linux, and Windows.

## 1. Environment Verification

**Before your first build**, run the verification script to ensure your environment is configured correctly.

### Windows (PowerShell)
```powershell
.\scripts\verify-build-environment.ps1

# With automatic fixes
.\scripts\verify-build-environment.ps1 -FixIssues
```

### macOS & Linux (Bash)
```bash
./scripts/verify-build-environment.sh

# With automatic fixes
./scripts\verify-build-environment.sh --fix
```

The script checks for required tools like CMake, a C++23 compiler, and platform-specific dependencies.

## 2. Quick Start: Building with Presets

We use CMake Presets for simple, one-command builds. See the [CMake Presets Guide](presets.md) for a full list.

### macOS
```bash
# Configure a debug build (Apple Silicon)
cmake --preset mac-dbg

# Build the project
cmake --build --preset mac-dbg
```

### Linux
```bash
# Configure a debug build
cmake --preset lin-dbg

# Build the project
cmake --build --preset lin-dbg
```

### Windows
```bash
# Configure a debug build for Visual Studio (x64)
cmake --preset win-dbg

# Build the project
cmake --build --preset win-dbg
```

### AI-Enabled Build (All Platforms)
To build with the `z3ed` AI agent features:
```bash
# macOS
cmake --preset mac-ai
cmake --build --preset mac-ai

# Windows
cmake --preset win-ai
cmake --build --preset win-ai
```

## 3. Dependencies

-   **Required**: CMake 3.16+, C++23 Compiler (GCC 13+, Clang 16+, MSVC 2019+), Git.
-   **Bundled**: All other dependencies (SDL2, ImGui, Abseil, Asar, GoogleTest, etc.) are included as Git submodules or managed by CMake's `FetchContent`. No external package manager is required for a basic build.
-   **Optional**: 
    -   **gRPC**: For GUI test automation. Can be enabled with `-DYAZE_WITH_GRPC=ON`.
    -   **vcpkg (Windows)**: Can be used for faster gRPC builds on Windows (optional).

## 4. Platform Setup

### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Recommended: Install build tools via Homebrew
brew install cmake pkg-config
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libgtk-3-dev libdbus-1-dev
```

### Windows
-   **Visual Studio 2022** is required, with the "Desktop development with C++" workload.
-   The `verify-build-environment.ps1` script will help identify any missing components.
-   For building with gRPC, see the "Windows Build Optimization" section below.

## 5. Testing

The project uses CTest and GoogleTest. Tests are organized into categories using labels. See the [Testing Guide](../developer/testing-guide.md) for details.

### Running Tests with Presets

The easiest way to run tests is with `ctest` presets.

```bash
# Configure a development build (enables ROM-dependent tests)
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=/path/to/your/zelda3.sfc

# Build the tests
cmake --build --preset mac-dev --target yaze_test

# Run stable tests (fast, run in CI)
ctest --preset dev

# Run all tests, including ROM-dependent and experimental
ctest --preset all
```

### Running Tests Manually

You can also run tests by invoking the test executable directly or using CTest with labels.

```bash
# Run all tests via the executable
./build/bin/yaze_test

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
3.  Select a preset from the status bar (e.g., `mac-ai`).
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
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && .\bootstrap-vcpkg.bat

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
**Cause**: You are building code that requires AI features without using an AI-enabled preset, or your Git submodules are not initialized.
**Solution**:
1.  Use an AI preset like `win-ai` or `mac-ai`.
2.  Ensure submodules are present by running `git submodule update --init --recursive`.

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
