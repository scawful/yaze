# Build System Improvements and Windows gRPC Solutions

## Overview

This document outlines the improvements made to fix cross-platform CI/CD builds and provides recommendations for stable Windows builds with gRPC support.

## Issues Fixed

### 1. Windows Build Failures with vcpkg and gRPC

**Problem:**
- CI workflow used vcpkg but `vcpkg.json` was missing gRPC dependency
- CMake forced gRPC ON by default but vcpkg couldn't provide it
- Fallback to FetchContent build took 45+ minutes and often failed on Windows runners

**Solution:**
- Added gRPC to `vcpkg.json` with Windows platform constraint
- Modified CMakeLists.txt to disable gRPC for `YAZE_MINIMAL_BUILD` (CI builds)
- Improved vcpkg action configuration with pinned version and proper directory setup

**Files Changed:**
- `vcpkg.json` - Added gRPC dependency for Windows
- `CMakeLists.txt` - Made gRPC respect YAZE_MINIMAL_BUILD flag
- `.github/workflows/ci.yml` - Improved vcpkg setup
- `.github/workflows/release.yml` - Matched CI improvements

### 2. Ubuntu Abseil Version Conflicts

**Problem:**
- System `libabsl-dev` package conflicted with gRPC's bundled Abseil
- Version mismatches caused compilation failures
- Different absl versions between system and gRPC dependencies

**Solution:**
- Removed `libabsl-dev` from Ubuntu dependency installation
- Modified CMakeLists.txt to force bundled Abseil when gRPC is enabled
- Ensures version consistency across all dependencies

**Files Changed:**
- `CMakeLists.txt` - Force bundled absl when YAZE_WITH_GRPC=ON
- `.github/workflows/ci.yml` - Removed libabsl-dev from apt packages
- `.github/workflows/release.yml` - Removed libabsl-dev from apt packages

### 3. Minimal Build Configuration

**Problem:**
- `YAZE_MINIMAL_BUILD` flag didn't actually minimize the build
- gRPC and AI features were always enabled, slowing CI significantly

**Solution:**
- Made feature flags respect YAZE_MINIMAL_BUILD:
  - gRPC: OFF for minimal builds (saves 15-45 minutes)
  - AI: OFF for minimal builds
  - JSON: ON even for minimal builds (header-only, negligible cost)

## Build Configuration Matrix

### CI Builds (YAZE_MINIMAL_BUILD=ON)
- **Features:** JSON only
- **gRPC:** Disabled (avoids 15-45 minute compile)
- **AI Agent:** Disabled
- **Build Time:** ~5-10 minutes per platform
- **Purpose:** Fast validation of core functionality

### Release Builds (Full Features)
- **Features:** JSON, gRPC, AI Agent
- **Windows:** Uses vcpkg pre-compiled gRPC (~5 minutes)
- **Linux/macOS:** Uses FetchContent gRPC (~15-20 minutes)
- **Build Time:** 10-30 minutes depending on platform
- **Purpose:** Full-featured releases

## Windows Build Recommendations

### Option 1: vcpkg (Recommended for Windows)

**Benefits:**
- Pre-compiled binaries (~5 minute setup)
- Automatic dependency management
- Tested and stable package versions

**Setup:**
```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# Install dependencies (one-time, cached for future builds)
./vcpkg install grpc:x64-windows sdl2[vulkan]:x64-windows yaml-cpp:x64-windows

# Configure project
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake
```

**Build Time:** ~10 minutes total (5 min vcpkg install, 5 min project build)

### Option 2: FetchContent (Cross-platform)

**Benefits:**
- No external dependencies
- Works on all platforms
- Version-controlled dependency sources

**Drawbacks:**
- First build takes 15-45 minutes to compile gRPC
- Subsequent builds cached (~5 minutes)

**Setup:**
```bash
# Just configure and build - CMake handles everything
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**Build Time:** 
- First build: ~45 minutes
- Subsequent builds: ~5 minutes (cached)

### Option 3: Minimal Build (Development/Testing)

**Benefits:**
- Fastest build times
- Ideal for core functionality testing
- No gRPC compilation needed

**Setup:**
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DYAZE_MINIMAL_BUILD=ON
cmake --build build --config Release
```

**Build Time:** ~5 minutes

**Limitations:**
- No gRPC networking features
- No AI agent features
- JSON support only

## Platform-Specific Notes

### Windows
- **Recommended:** Use vcpkg for gRPC (fastest)
- **Alternative:** FetchContent (works but slow first build)
- **CI:** Uses MINIMAL_BUILD (gRPC disabled)
- **Release:** Uses vcpkg with full features

### Ubuntu/Linux
- Uses FetchContent for Abseil and gRPC
- System packages avoided for consistency
- Build time: ~15-20 minutes first build

### macOS
- Uses FetchContent for all dependencies
- Homebrew only for build tools (ninja, cmake)
- Universal binaries supported (arm64 + x86_64)

## CI/CD Configuration

### Workflow Optimizations

1. **vcpkg Pinning:** Uses specific commit for reproducible builds
2. **Parallel Jobs:** fail-fast: false allows all platforms to complete
3. **Minimal Builds:** CI uses YAZE_MINIMAL_BUILD for speed
4. **Artifact Caching:** vcpkg packages cached between runs

### Build Matrix

```yaml
- Ubuntu 22.04 + GCC 12: Core platform validation
- macOS 14 + Clang: Apple Silicon + x86_64 universal binary
- Windows 2022 + MSVC: Windows x64 with vcpkg
```

## Troubleshooting

### Windows: "gRPC not found"
**Solution:** Install via vcpkg or allow first build to complete (45 min)

### Ubuntu: "Abseil version mismatch"
**Solution:** Ensure libabsl-dev is NOT installed, let FetchContent provide it

### macOS: "Architecture mismatch"
**Solution:** Abseil.cmake automatically handles arm64 architecture issues

### All Platforms: "Build too slow"
**Solution:** Use `-DYAZE_MINIMAL_BUILD=ON` for development/testing

## Performance Metrics

### Build Time Comparison (First Build)

| Platform | Configuration | Time | Notes |
|----------|--------------|------|-------|
| Windows | vcpkg + full | 10 min | Recommended |
| Windows | FetchContent | 45 min | Works, slower |
| Windows | Minimal | 5 min | Fast, limited features |
| Ubuntu | Full | 20 min | FetchContent only option |
| Ubuntu | Minimal | 5 min | CI default |
| macOS | Full | 18 min | FetchContent only option |
| macOS | Minimal | 5 min | CI default |

### Subsequent Builds (Cached)

All configurations: ~5 minutes (CMake/compiler cache hit)

## Future Improvements

### Potential Optimizations
1. **ccache Integration:** Cache compiled objects across builds
2. **Precompiled Headers:** Reduce template instantiation time
3. **Unity Builds:** Combine translation units (already supported via YAZE_UNITY_BUILD)
4. **vcpkg Binary Cache:** Share packages across CI runners

### Dependency Management
1. **Consider Conan:** Alternative to vcpkg with better binary caching
2. **Package Managers:** Submit packages to system package repositories
3. **Container Builds:** Docker images with pre-installed dependencies

## References

- [vcpkg Documentation](https://vcpkg.io/)
- [gRPC Build Guide](https://grpc.io/docs/languages/cpp/quickstart/)
- [CMake FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)
- [Abseil C++ Libraries](https://abseil.io/)
