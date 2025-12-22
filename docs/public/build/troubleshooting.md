# YAZE Build Troubleshooting Guide

**Last Updated**: December 2025
**Related Docs**: quick-reference.md, build-from-source.md, presets.md

## Table of Contents
- [gRPC ARM64 Issues](#grpc-arm64-issues)
- [Windows Build Issues](#windows-build-issues)
- [macOS Issues](#macos-issues)
- [Disk Usage and Build Directory Bloat](#disk-usage-and-build-directory-bloat)
- [Linux Issues](#linux-issues)
- [Common Build Errors](#common-build-errors)

---

## gRPC ARM64 Issues

### Preferred Path: System gRPC on macOS

On Apple Silicon, the fastest and most reliable setup is Homebrew's gRPC stack:

```bash
brew install grpc protobuf abseil
cmake --preset mac-ai-fast
```

You can also force it in any preset with `-DYAZE_PREFER_SYSTEM_GRPC=ON`.

### If You Must Build gRPC From Source

When building gRPC/Abseil from source on ARM64, x86-only SSE flags can still surface. If you see errors like:

```
clang++: error: unsupported option '-msse4.1' for target 'arm64-apple-darwin...'
```

switch to system gRPC (`YAZE_PREFER_SYSTEM_GRPC=ON`) or disable gRPC for that build:

```bash
cmake --preset mac-dbg -DYAZE_ENABLE_GRPC=OFF
```

### Toolchain Consistency (AppleClang vs Homebrew LLVM)

If Homebrew LLVM is on your PATH, SDK headers can be picked up in the wrong order and cause missing type errors. Either:

```bash
# Force AppleClang
cmake --preset mac-dbg --fresh -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++

# Or use the Homebrew LLVM toolchain file
cmake --preset mac-dbg --fresh -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-brew.toolchain.cmake
```

---

## Windows Build Issues

### MSVC Compatibility with gRPC

**Problem**: gRPC v1.75.1 has MSVC compilation errors in UPB (micro protobuf) code

**Error**:
```
error C2099: initializer is not a constant
```

**Solution**: Use gRPC v1.67.1 (MSVC-compatible, tested) or use vcpkg pre-built packages

### vcpkg Integration (Recommended)

#### Setup vcpkg

```powershell
# Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install packages
.\vcpkg install grpc:x64-windows protobuf:x64-windows sdl2:x64-windows yaml-cpp:x64-windows
```

#### Configure CMake to Use vcpkg

**Option 1**: Set environment variable
```powershell
$env:CMAKE_TOOLCHAIN_FILE = "C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --preset win-dbg
```

**Option 2**: Use CMake preset with toolchain
```json
// CMakePresets.json (already configured)
{
  "name": "win-dbg",
  "cacheVariables": {
    "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  }
}
```

#### Expected Build Times

| Method | Time | Version | Status |
|--------|------|---------|--------|
| **vcpkg** (recommended) | ~5-10 min | gRPC 1.71.0 | ✅ Pre-compiled binaries |
| **FetchContent** (fallback) | ~30-40 min | gRPC 1.67.1 | ✅ MSVC-compatible |
| **FetchContent** (old) | ~45+ min | gRPC 1.75.1 | ❌ UPB compilation errors |

### Long Path Issues

Windows has a default path length limit of 260 characters, which can cause issues with deep dependency trees.

**Solution**:
```powershell
# Enable long paths for Git
git config --global core.longpaths true

# Enable long paths system-wide (requires admin)
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
  -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

### Missing Visual Studio Components

**Error**: "Could not find Visual C++ compiler"

**Solution**: Install "Desktop development with C++" workload via Visual Studio Installer

```powershell
# Verify Visual Studio installation
.\scripts\verify-build-environment.ps1
```

### Package Detection Issues

**Problem**: `find_package(gRPC CONFIG)` not finding vcpkg-installed packages

**Causes**:
1. Case sensitivity: vcpkg uses lowercase `grpc` config
2. Namespace mismatch: vcpkg provides `gRPC::grpc++` target
3. Missing packages in `vcpkg.json`

**Solution**: Enhanced detection in `cmake/grpc_windows.cmake`:

```cmake
# Try both case variations
find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
  find_package(grpc CONFIG QUIET)  # Try lowercase
  if(grpc_FOUND)
    set(gRPC_FOUND TRUE)
  endif()
endif()

# Create aliases for non-namespaced targets
if(TARGET gRPC::grpc++)
  add_library(grpc++ ALIAS gRPC::grpc++)
  add_library(grpc++_reflection ALIAS gRPC::grpc++_reflection)
endif()
```

---

## macOS Issues

### Homebrew SDL2 Not Found

**Problem**: SDL.h headers not found even with Homebrew SDL2 installed

**Solution**: Add Homebrew include path explicitly

```cmake
# In cmake/dependencies/sdl2.cmake
if(APPLE)
  include_directories(/opt/homebrew/opt/sdl2/include/SDL2)  # Apple Silicon
  include_directories(/usr/local/opt/sdl2/include/SDL2)    # Intel
endif()
```

### Code Signing Issues

**Problem**: "yaze.app is damaged and can't be opened"

**Solution**: Sign the application bundle
```bash
codesign --force --deep --sign - build/bin/yaze.app
```

### Multiple Xcode Versions

**Problem**: CMake using wrong SDK or compiler version

**Solution**: Select Xcode version explicitly
```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
```

---

## Disk Usage and Build Directory Bloat

If the repository grows unexpectedly large, check for extra build directories in the repo root. The supported in-repo directories are `build/` and `build-wasm/`. Move any additional builds to an external path via `CMakeUserPresets.json` or `YAZE_BUILD_DIR`, then delete the old folders:

```bash
rm -rf build_agent build_agent_llvm build_agent_ninja build_gemini
```

Keeping CPM/vcpkg caches in `~/.cache` or `~/.cpm-cache` prevents re-downloading after cleanups.

---

## Linux Issues

### Missing Dependencies

**Error**: Headers not found for various libraries

**Solution**: Install development packages

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build pkg-config \
  libglew-dev libxext-dev libwavpack-dev libboost-all-dev \
  libpng-dev python3-dev \
  libasound2-dev libpulse-dev \
  libx11-dev libxrandr-dev libxcursor-dev libxinerama-dev libxi-dev \
  libxss-dev libxxf86vm-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libgtk-3-dev libdbus-1-dev git
```

**Fedora/RHEL**:
```bash
sudo dnf install -y \
  gcc-c++ cmake ninja-build pkg-config \
  glew-devel libXext-devel wavpack-devel boost-devel \
  libpng-devel python3-devel \
  alsa-lib-devel pulseaudio-libs-devel \
  libX11-devel libXrandr-devel libXcursor-devel libXinerama-devel libXi-devel \
  libXScrnSaver-devel libXxf86vm-devel libxkbcommon-devel wayland-devel \
  gtk3-devel dbus-devel git
```

### GCC Version Too Old

**Error**: C++23 features not supported

**Solution**: Install newer GCC or use Clang

```bash
# Install GCC 13
sudo apt-get install -y gcc-13 g++-13

# Configure CMake to use GCC 13
cmake --preset lin-dbg \
  -DCMAKE_C_COMPILER=gcc-13 \
  -DCMAKE_CXX_COMPILER=g++-13
```

---

## Common Build Errors

### "Target not found" Errors

**Error**: `CMake Error: Cannot specify link libraries for target "X" which is not built by this project`

**Causes**:
1. Target aliasing issues
2. Dependency order problems
3. Missing `find_package()` calls

**Solutions**:
1. Check `cmake/dependencies.cmake` for proper target exports
2. Ensure dependencies are included before they're used
3. Verify target names match (e.g., `grpc++` vs `gRPC::grpc++`)

### Protobuf Version Mismatch

**Error**: "Protobuf C++ gencode is built with an incompatible version"

**Cause**: System protoc version doesn't match bundled protobuf runtime

**Solution**: Use bundled protoc
```cmake
set(_gRPC_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
```

### compile_commands.json Not Generated

**Problem**: IntelliSense/clangd not working

**Solution**: Ensure preset uses Ninja Multi-Config generator
```bash
cmake --preset mac-dbg  # Uses Ninja Multi-Config
# compile_commands.json will be at build/compile_commands.json
```

### ImGui ID Collisions

**Error**: "Dear ImGui: Duplicate ID"

**Solution**: Add `PushID/PopID` scopes around widgets
```cpp
ImGui::PushID("unique_identifier");
// ... widgets here ...
ImGui::PopID();
```

### ASAR Library Build Errors

**Status**: Known issue with stubbed implementation

**Current State**: ASAR methods return `UnimplementedError`

**Workaround**: Assembly patching features are disabled until ASAR CMakeLists.txt macro errors are fixed

---

## Debugging Tips

### Enable Verbose Build Output

```bash
# Verbose CMake configuration
cmake --preset mac-dbg -- -LAH

# Verbose build
cmake --build --preset mac-dbg --verbose

# Very verbose build
cmake --build --preset mac-dbg -- -v VERBOSE=1
```

### Check Dependency Detection

```bash
# See what CMake found
cmake --preset mac-dbg 2>&1 | grep -E "(Found|Using|Detecting)"

# Check cache for specific variables
cmake -LA build/ | grep -i grpc
```

### Isolate Build Issues

```bash
# Build specific targets to isolate issues
cmake --build build --target yaze_canvas   # Just canvas library
cmake --build build --target yaze_gfx      # Just graphics library
cmake --build build --target protoc        # Just protobuf compiler
```

### Clean Builds

```bash
# Clean build directory (fast)
cmake --build build --target clean

# Remove build artifacts but keep dependencies (medium)
rm -rf build/bin build/lib

# Nuclear option - full rebuild (slow, 30+ minutes)
rm -rf build/
cmake --preset mac-dbg
```

---

## Getting Help

1. **Check existing documentation**:
   - BUILD-GUIDE.md - General build instructions
   - CLAUDE.md - Project overview
   - CI/CD logs - .github/workflows/ci.yml

2. **Search git history** for working configurations:
   ```bash
   git log --all --grep="grpc" --oneline
   git show <commit-hash>:cmake/grpc.cmake
   ```

3. **Enable debug logging**:
   ```bash
   YAZE_LOG_LEVEL=DEBUG ./build/bin/yaze 2>&1 | tee debug.log
   ```

4. **Create a minimal reproduction**:
   - Isolate the failing component
   - Create a minimal CMakeLists.txt
   - Test with minimal dependencies

5. **File an issue** with:
   - Platform and OS version
   - CMake preset used
   - Full error output
   - `cmake -LA build/` output
   - Relevant CMakeCache.txt entries
