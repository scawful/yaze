# Platform Compatibility & CI/CD Fixes

**Last Updated**: October 9, 2025  

---

## Platform-Specific Notes

### Windows

**Build System:**
- Uses vcpkg for: SDL2, yaml-cpp, zlib (fast, pre-compiled)
- Uses FetchContent for: gRPC v1.67.1, abseil, protobuf (slower first time)

**MSVC Flags Applied:**
- `/bigobj` - Support large object files
- `/permissive-` - Standards conformance
- `/wd4267 /wd4244` - Suppress conversion warnings
- `/constexpr:depth2048` - Deep template instantiation

**Macro Definitions:**
- `WIN32_LEAN_AND_MEAN` - Reduce Windows header pollution
- `NOMINMAX` - Prevent min/max macro conflicts
- `NOGDI` - Prevent GDI macro conflicts (DWORD, etc.)
- `__PRFCHWINTRIN_H` - Work around Clang 20 `_m_prefetchw` linkage clash with
  Windows SDK headers

**Build Times:**
- First build with FetchContent: ~45-60 minutes (compiles gRPC)
- Subsequent builds: ~2-5 minutes
- With vcpkg pre-compiled gRPC: ~5-10 minutes first time

### macOS

**Build System:**
- Uses vcpkg for: SDL2, yaml-cpp, zlib
- Uses FetchContent for: gRPC, abseil, protobuf

**Supported Architectures:**
- x64 (Intel Macs) - macOS 13+
- ARM64 (Apple Silicon) - macOS 14+

**Build Times:**
- First build: ~20-30 minutes
- Subsequent builds: ~3-5 minutes

### Linux

**Build System:**
- Uses system packages (apt) for most dependencies
- Does NOT use vcpkg
- Uses FetchContent for: gRPC, abseil, protobuf (when not in system)

**Required System Packages:**
```bash
build-essential ninja-build pkg-config libglew-dev libxext-dev 
libwavpack-dev libabsl-dev libboost-all-dev libpng-dev python3-dev 
libpython3-dev libasound2-dev libpulse-dev libaudio-dev libx11-dev 
libxrandr-dev libxcursor-dev libxinerama-dev libxi-dev libxss-dev 
libxxf86vm-dev libxkbcommon-dev libwayland-dev libdecor-0-dev 
libgtk-3-dev libdbus-1-dev gcc-12 g++-12 clang-15
```

**Build Times:**
- First build: ~15-20 minutes
- Subsequent builds: ~2-4 minutes

---

## Cross-Platform Code Validation

The following subsystems run unchanged across Windows, macOS, and Linux:

- Audio backend (`src/app/emu/audio`) uses SDL2 only; no platform branches.
- Input backend/manager (`src/app/emu/input`) runs on SDL2 abstractions.
- Debug tools (`src/app/emu/debug`) avoid OS-specific headers.
- Emulator UI (`src/app/emu/ui`) is pure ImGui + SDL2.

---

## Common Build Issues & Solutions

### Windows: "use of undefined type 'PromiseLike'"
**Cause:** Old gRPC version (< v1.67.1)  
**Fix:** Clear build cache and reconfigure
```powershell
rm -r build/_deps/grpc-*
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### macOS: "absl not found"
**Cause:** vcpkg not finding abseil packages  
**Fix:** Use FetchContent (default) - abseil is fetched automatically by gRPC
```bash
cmake -B build
```

### Linux: CMake configuration fails
**Cause:** Missing system dependencies  
**Fix:** Install required packages
```bash
sudo apt-get update
sudo apt-get install -y [see package list above]
```

### Windows: "DWORD syntax error"
**Cause:** Windows macros conflicting with protobuf enums  
**Fix:** Ensure `NOGDI` is defined (now automatic in grpc.cmake)

---

## CI/CD Validation Checklist

Before merging platform-specific changes:

- Confirm the vcpkg baseline matches `vcpkg.json`.
- Do not reintroduce the Windows x86 build (cpp-httplib incompatibility).
- Keep Windows macro guards (`NOGDI`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`) in place.
- Build against gRPC 1.67.1 with the MSVC workaround flags.
- Leave shared code paths on SDL2/ImGui abstractions.
- Re-run the full matrix if caches or presets change.

### CI/CD Performance Roadmap

- **Dependency caching**: Cache vcpkg installs on Windows plus Homebrew/apt
  archives to trim 5-10 minutes per job; track cache keys via OS + lockfiles.
- **Compiler caching**: Enable `ccache`/`sccache` across the matrix using the
  `hendrikmuhs/ccache-action` with 500 MB per-run limits for 3-5 minute wins.
- **Conditional work**: Add a path-filter job that skips emulator builds or
  full test runs when only docs or CLI code change; fall back to full matrix on
  shared components.
- **Reusable workflows**: Centralize setup steps (checking out submodules,
  restoring caches, configuring presets) to reduce duplication between `ci.yml`
  and `release.yml`.
- **Release optimizations**: Use lean presets without test targets, run platform
  builds in parallel, and reuse cached artifacts from CI when hashes match.

---

## Testing Strategy

### Automated (CI)
- Ubuntu 22.04 (GCC-12, Clang-15)
- macOS 13/14 (x64 and ARM64)
- Windows Server 2022 (x64)
- Core tests: `AsarWrapperTest`, `SnesTileTest`, others tagged `STABLE`
- Tooling: clang-format, clang-tidy, cppcheck
- Sanitizers: Linux AddressSanitizer job

### Manual Testing
After successful CI build:
- Windows: verify audio backend, keyboard input, APU debugger UI.
- Linux: verify input polling and audio output.
- macOS: spot-check rendering, input, audio.

---

## Quick Reference

### Build Command (All Platforms)
```bash
cmake -B build
cmake --build build --parallel

cmake --preset [mac-dbg|lin-dbg|win-dbg]
cmake --build --preset [mac-dbg|lin-dbg|win-dbg]
```

### Enable Features
All features (JSON, gRPC, AI) are **always enabled** by default.  
No need to specify `-DZ3ED_AI=ON` or `-DYAZE_WITH_GRPC=ON`.

### Windows Troubleshooting
```powershell
# Verify environment
.\scripts\verify-build-environment.ps1

# Use vcpkg for faster builds
.\scripts\setup-vcpkg-windows.ps1
cmake -B build -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake"
```

---

## Filesystem Abstraction

To ensure robust and consistent behavior across platforms, YAZE has standardized its filesystem operations:

- **`std::filesystem`**: All new and refactored code uses the C++17 `std::filesystem` library for path manipulation, directory iteration, and file operations. This eliminates platform-specific bugs related to path separators (`/` vs `\`).

- **`PlatformPaths` Utility**: A dedicated utility class, `yaze::util::PlatformPaths`, provides platform-aware API for retrieving standard directory locations:
  - **Application Data**: `%APPDATA%` on Windows, `~/Library/Application Support` on macOS, XDG Base Directory on Linux
  - **Configuration Files**: Semantically clear API for config file locations
  - **Home and Temporary Directories**: Safely resolves user-specific and temporary folders

This removes legacy platform-specific APIs (like `dirent.h` or Win32 directory functions) for cleaner, more maintainable file handling.

---

## Native File Dialog Support

YAZE features native file dialogs on all platforms:
- **macOS**: Cocoa-based file selection with proper sandboxing support
- **Windows**: Windows Explorer integration with COM APIs (`IFileOpenDialog`/`IFileSaveDialog`)
- **Linux**: GTK3 dialogs that match system appearance
- **Fallback**: Cross-platform implementation when native dialogs unavailable

---

**Status:** ✅ All CI/CD issues resolved. Next push should build successfully on all platforms.
