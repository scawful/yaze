# YAZE Build Guide

**Status**: CI/CD Overhaul Complete ✅
**Last Updated**: October 2025
**Platforms**: macOS (ARM64/Intel), Linux, Windows

## Quick Start

### macOS (Apple Silicon)
```bash
# Basic debug build
cmake --preset mac-dbg && cmake --build --preset mac-dbg

# With AI features (z3ed agent, gRPC, JSON)
cmake --preset mac-ai && cmake --build --preset mac-ai

# Release build
cmake --preset mac-rel && cmake --build --preset mac-rel
```

### Linux
```bash
# Debug build
cmake --preset lin-dbg && cmake --build --preset lin-dbg

# With AI features
cmake --preset lin-ai && cmake --build --preset lin-ai
```

### Windows (Visual Studio)
```bash
# Debug build
cmake --preset win-dbg && cmake --build --preset win-dbg

# With AI features
cmake --preset win-ai && cmake --build --preset win-ai
```

## Build System Overview

### CMake Presets
The project uses a streamlined preset system with short, memorable names:

| Preset | Platform | Features | Build Dir |
|--------|----------|----------|-----------|
| `mac-dbg`, `lin-dbg`, `win-dbg` | All | Basic debug builds | `build/` |
| `mac-ai`, `lin-ai`, `win-ai` | All | AI features (z3ed, gRPC, JSON) | `build_ai/` |
| `mac-rel`, `lin-rel`, `win-rel` | All | Release builds | `build/` |
| `mac-dev`, `win-dev` | Desktop | Development with ROM tests | `build/` |
| `mac-uni` | macOS | Universal binary (ARM64+x86_64) | `build/` |

Add `-v` suffix (e.g., `mac-dbg-v`) for verbose compiler warnings.

### Build Configuration
- **C++ Standard**: C++23 (required)
- **Generator**: Ninja Multi-Config (all platforms)
- **Dependencies**: Bundled via Git submodules or CMake FetchContent
- **Optional Features**:
  - gRPC: Enable with `-DYAZE_WITH_GRPC=ON` (for GUI automation)
  - AI Agent: Enable with `-DZ3ED_AI=ON` (requires JSON and gRPC)
  - ROM Tests: Enable with `-DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=/path/to/zelda3.sfc`

## CI/CD Build Fixes (October 2025)

### Issues Resolved

#### 1. CMake Integration ✅
**Problem**: Generator mismatch between `CMakePresets.json` and VSCode settings

**Fixes**:
- Updated `.vscode/settings.json` to use Ninja Multi-Config
- Fixed compile_commands.json path to `build/compile_commands.json`
- Created proper `.vscode/tasks.json` with preset-based tasks
- Updated `scripts/dev-setup.sh` for future setups

#### 2. gRPC Dependency ✅
**Problem**: CPM downloading but not building gRPC targets

**Fixes**:
- Fixed target aliasing for non-namespaced targets (grpc++ → grpc::grpc++)
- Exported `ABSL_TARGETS` for project-wide use
- Added `target_add_protobuf()` function for protobuf code generation
- Fixed protobuf generation paths and working directory

#### 3. Protobuf Code Generation ✅
**Problem**: `.pb.h` and `.grpc.pb.h` files weren't being generated

**Fixes**:
- Changed all `YAZE_WITH_GRPC` → `YAZE_ENABLE_GRPC` (compile definition vs CMake variable)
- Fixed variable scoping using `CACHE INTERNAL` for functions
- Set up proper include paths for generated files
- All proto files now generate successfully:
  - `rom_service.proto`
  - `canvas_automation.proto`
  - `imgui_test_harness.proto`
  - `emulator_service.proto`

#### 4. SDL2 Configuration ✅
**Problem**: SDL.h headers not found

**Fixes**:
- Changed all `SDL_TARGETS` → `YAZE_SDL2_TARGETS`
- Fixed variable export using `PARENT_SCOPE`
- Added Homebrew SDL2 include path (`/opt/homebrew/opt/sdl2/include/SDL2`)
- Fixed all library targets to link SDL2 properly

#### 5. ImGui Configuration ✅
**Problem**: Conflicting ImGui versions (bundled vs CPM download)

**Fixes**:
- Used bundled ImGui from `src/lib/imgui/` instead of downloading
- Created proper ImGui static library target
- Added `imgui_stdlib.cpp` for std::string support
- Exported with `PARENT_SCOPE`

#### 6. nlohmann_json Configuration ✅
**Problem**: JSON headers not found

**Fixes**:
- Created `cmake/dependencies/json.cmake`
- Set up bundled `third_party/json/`
- Added include directories to all targets that need JSON

#### 7. GTest and GMock ✅
**Problem**: GMock was disabled but test targets required it

**Fixes**:
- Changed `BUILD_GMOCK OFF` → `BUILD_GMOCK ON` in testing.cmake
- Added verification for both gtest and gmock targets
- Linked all four testing libraries: gtest, gtest_main, gmock, gmock_main
- Built ImGuiTestEngine from bundled source for GUI test automation

### Build Statistics

**Main Application**:
- Compilation Units: 310 targets
- Executable: `build/bin/Debug/yaze.app/Contents/MacOS/yaze` (macOS)
- Size: 120MB (ARM64 Mach-O)
- Status: ✅ Successfully built

**Test Suites**:
- `yaze_test_stable`: 126MB - Unit + Integration tests for CI/CD
- `yaze_test_gui`: 123MB - GUI automation tests
- `yaze_test_experimental`: 121MB - Experimental features
- `yaze_test_benchmark`: 121MB - Performance benchmarks
- Status: ✅ All test executables built successfully

## Test Execution

### Build Tests
```bash
# Build tests
cmake --build build --target yaze_test

# Run all tests
./build/bin/yaze_test

# Run specific categories
./build/bin/yaze_test --unit              # Unit tests only
./build/bin/yaze_test --integration       # Integration tests
./build/bin/yaze_test --e2e --show-gui    # End-to-end GUI tests

# Run with ROM-dependent tests
./build/bin/yaze_test --rom-dependent --rom-path zelda3.sfc

# Run specific test by name
./build/bin/yaze_test "*Asar*"
```

### Using CTest
```bash
# Run all stable tests
ctest --preset stable --output-on-failure

# Run all tests
ctest --preset all --output-on-failure

# Run unit tests only
ctest --preset unit

# Run integration tests only
ctest --preset integration
```

## Platform-Specific Notes

### macOS
- Supports both Apple Silicon (ARM64) and Intel (x86_64)
- Use `mac-uni` preset for universal binaries
- Bundled Abseil used by default to avoid deployment target mismatches
- Requires Xcode Command Line Tools

**ARM64 Considerations**:
- gRPC v1.67.1 is the tested stable version
- Abseil SSE flags are handled automatically
- See docs/BUILD-TROUBLESHOOTING.md for gRPC ARM64 issues

### Windows
- Requires Visual Studio 2022 with "Desktop development with C++" workload
- Run `scripts\verify-build-environment.ps1` before building
- gRPC builds take 15-20 minutes first time (use vcpkg for faster builds)
- Watch for path length limits: Enable long paths with `git config --global core.longpaths true`

**vcpkg Integration**:
- Optional: Use `-DYAZE_USE_VCPKG_GRPC=ON` for pre-built packages
- Faster builds (~5-10 min vs 30-40 min)
- See docs/BUILD-TROUBLESHOOTING.md for vcpkg setup

### Linux
- Requires GCC 13+ or Clang 16+
- Install dependencies: `libgtk-3-dev`, `libdbus-1-dev`, `pkg-config`
- See `.github/workflows/ci.yml` for complete dependency list

## Build Verification

After a successful build, verify:

- ✅ CMake configuration completes successfully
- ✅ `compile_commands.json` generated (62,066 lines, 10,344 source files indexed)
- ✅ Main executable links successfully
- ✅ All test executables build successfully
- ✅ IntelliSense working with full codebase indexing

## Troubleshooting

For platform-specific issues, dependency problems, and error resolution, see:
- **docs/BUILD-TROUBLESHOOTING.md** - Comprehensive troubleshooting guide
- **docs/ci-cd/LOCAL-CI-TESTING.md** - Local testing strategies

## Files Modified (CI/CD Overhaul)

### Core Build System (9 files)
1. `cmake/dependencies/grpc.cmake` - gRPC setup, protobuf generation
2. `cmake/dependencies/sdl2.cmake` - SDL2 configuration
3. `cmake/dependencies/imgui.cmake` - ImGui + ImGuiTestEngine
4. `cmake/dependencies/json.cmake` - nlohmann_json setup
5. `cmake/dependencies/testing.cmake` - GTest + GMock
6. `cmake/dependencies.cmake` - Dependency coordination
7. `src/yaze_pch.h` - Removed Abseil includes
8. `CMakeLists.txt` - Top-level configuration
9. `CMakePresets.json` - Preset definitions

### VSCode/CMake Integration (4 files)
10. `.vscode/settings.json` - CMake integration
11. `.vscode/c_cpp_properties.json` - Compile commands path
12. `.vscode/tasks.json` - Build tasks
13. `scripts/dev-setup.sh` - VSCode config generation

### Library Configuration (6 files)
14. `src/app/gfx/gfx_library.cmake` - SDL2 variable names
15. `src/app/net/net_library.cmake` - JSON includes
16. `src/app/app.cmake` - SDL2 targets for macOS
17. `src/app/gui/gui_library.cmake` - SDL2 targets
18. `src/app/emu/emu_library.cmake` - SDL2 targets
19. `src/app/service/grpc_support.cmake` - SDL2 targets

**Total: 26 files modified/created**

## See Also

- **CLAUDE.md** - Project overview and development guidelines
- **docs/BUILD-TROUBLESHOOTING.md** - Platform-specific troubleshooting
- **docs/ci-cd/CI-SETUP.md** - CI/CD pipeline configuration
- **docs/testing/TEST-GUIDE.md** - Testing strategies and execution
