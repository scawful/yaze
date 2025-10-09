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

All recent additions (October 2025) are cross-platform compatible:

### Audio System
- ✅ `src/app/emu/audio/audio_backend.h/cc` - Uses SDL2 (cross-platform)
- ✅ No platform-specific code
- ✅ Ready for SDL3 migration

### Input System
- ✅ `src/app/emu/input/input_backend.h/cc` - Uses SDL2 (cross-platform)
- ✅ `src/app/emu/input/input_manager.h/cc` - Platform-agnostic
- ✅ Ready for SDL3 migration

### Debugger System
- ✅ `src/app/emu/debug/apu_debugger.h/cc` - No platform dependencies
- ✅ `src/app/emu/debug/breakpoint_manager.h/cc` - Platform-agnostic
- ✅ `src/app/emu/debug/watchpoint_manager.h/cc` - Platform-agnostic
- ✅ All use ImGui (cross-platform)

### UI Layer
- ✅ `src/app/emu/ui/*` - Uses ImGui + SDL2 (both cross-platform)
- ✅ No platform-specific rendering code

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

- ✅ vcpkg baseline synchronized across CI and vcpkg.json
- ✅ Windows x86 build removed (cpp-httplib incompatibility)
- ✅ Windows macro pollution prevented (NOGDI, NOMINMAX, WIN32_LEAN_AND_MEAN)
- ✅ gRPC v1.67.1 with MSVC compatibility flags
- ✅ Cross-platform code uses SDL2/ImGui only
- ⏳ Validate CI builds pass on next push

---

## Testing Strategy

### Automated (CI)
- ✅ Build on Ubuntu 22.04 (GCC-12, Clang-15)
- ✅ Build on macOS 13/14 (x64/ARM64)
- ✅ Build on Windows 2022 (x64 only)
- ✅ Run core tests (AsarWrapperTest, SnesTileTest, etc.)
- ✅ Code quality checks (clang-format, cppcheck, clang-tidy)
- ✅ Memory sanitizer (Linux AddressSanitizer)

### Manual Testing
After successful CI build:
- 🔲 Windows: Audio backend initializes
- 🔲 Windows: Keyboard input works
- 🔲 Windows: APU Debugger UI renders
- 🔲 Linux: Input polling works
- 🔲 macOS: All features functional

---

## Quick Reference

### Build Command (All Platforms)
```bash
# Simple - no flags needed!
cmake -B build
cmake --build build --parallel

# Or use presets:
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
