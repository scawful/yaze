# Dependency Management for z3ed

**Last Updated**: October 1, 2025  
**Target Platforms**: macOS (arm64/x64), Linux (x64), Windows (x64)

## Overview

This document outlines the **careful and cautious** approach to managing dependencies for z3ed, particularly focusing on the optional gRPC/Protobuf integration for ImGuiTestHarness (IT-01).

## Philosophy

**Key Principles**:
1. ✅ **Optional by Default**: New dependencies are opt-in via CMake flags
2. ✅ **Cross-Platform First**: Every dependency must work on macOS, Linux, Windows
3. ✅ **Fail Gracefully**: Build succeeds even if optional deps unavailable
4. ✅ **Document Everything**: Clear instructions for each platform
5. ✅ **Minimal Footprint**: Prefer header-only or static linking

## Current Dependencies

### Core (Required)
Managed via vcpkg (`vcpkg.json`):
- **SDL2** (`sdl2`) - Cross-platform windowing and input
  - Version: 2.28.x via vcpkg baseline
  - Platform: All except UWP
  - Features: Vulkan support enabled

### Build Tools (Developer Environment)
- **CMake** 3.20+ - Build system
- **vcpkg** - C++ package manager (optional, used for SDL2)
- **Compiler**: Clang 14+ (macOS/Linux), MSVC 2019+ (Windows)
- **Git** - For CMake FetchContent (downloads source during build)

### Optional (Feature Flags)
- **gRPC + Protobuf** - For ImGuiTestHarness IPC (IT-01)
  - CMake Flag: `YAZE_WITH_GRPC=ON`
  - Status: **Infrastructure exists** in `cmake/grpc.cmake` (FetchContent)
  - Build Method: **Source build via FetchContent** (not vcpkg)
  - Risk Level: **Low** (builds from source, no dependency hell)
  - Build Time: ~15-20 minutes first time (downloads + compiles)

## Existing Build Infrastructure

### gRPC via CMake FetchContent (Already Present!)

**Good News**: YAZE already has comprehensive gRPC support in `cmake/grpc.cmake`!

**How It Works**:
1. CMake downloads gRPC + Protobuf source from GitHub
2. Builds from source during first configure (15-20 minutes)
3. Caches build artifacts (subsequent builds are fast)
4. **No external dependencies** (no vcpkg needed for gRPC)
5. Works identically on all platforms (macOS, Linux, Windows)

**Key Files**:
- `cmake/grpc.cmake` - FetchContent configuration
  - gRPC v1.70.1 (pinned version)
  - Protobuf v29.3 (pinned version)
  - Includes `target_add_protobuf()` helper function
- `cmake/absl.cmake` - Abseil (gRPC dependency, already present)

**Advantages Over vcpkg**:
- ✅ **Consistent across platforms** - Same build process everywhere
- ✅ **No external tools** - Just CMake + Git
- ✅ **Reproducible** - Pinned versions (v1.70.1, v29.3)
- ✅ **Contributors friendly** - Works out of box
- ✅ **No DLL hell** - Statically linked

**Why This Is Better**:
We don't need vcpkg for gRPC! The existing FetchContent approach:
- Downloads source during `cmake -B build`
- Builds gRPC/Protobuf from scratch (controlled environment)
- No version conflicts with system packages
- Same result on macOS, Linux, Windows

## Careful Integration Strategy for gRPC

### Phase 1: Test Existing Infrastructure (macOS - Current User)

**Goal**: Verify existing `cmake/grpc.cmake` works before enabling

```bash
cd /Users/scawful/Code/yaze

# Step 1: Create isolated build directory for testing
mkdir -p build-grpc-test

# Step 2: Configure with gRPC enabled
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON

# This will:
# - Download gRPC v1.70.1 from GitHub (~100MB download)
# - Download Protobuf v29.3 from GitHub (~50MB download)
# - Build both from source (~15-20 minutes first time)
# - Cache everything for future builds

# Watch for FetchContent progress:
#   -- Fetching grpc...
#   -- Fetching protobuf...
#   -- Building gRPC (this takes time)...

# Step 3: Build YAZE with gRPC
cmake --build build-grpc-test --target yaze -j8

# Expected outcome:
# - First run: 15-20 minutes (downloading + building gRPC)
# - Subsequent runs: ~30 seconds (using cached gRPC)
# - Binary size increases ~10-15MB (gRPC statically linked)
```

**Success Criteria**:
- ✅ CMake FetchContent downloads gRPC successfully
- ✅ gRPC builds without errors
- ✅ YAZE links against gRPC libraries
- ✅ `target_add_protobuf()` function available

**Rollback Plan**:
- If build fails, delete `build-grpc-test/` directory
- Original build untouched: `cmake --build build --target yaze -j8`
- No system changes (everything in build directory)

### Phase 2: CMake Integration (No vcpkg.json Changes Yet)

**Goal**: Add CMake support for gRPC detection without requiring it

```cmake
# Add to root CMakeLists.txt (around line 50, after project())

# Optional gRPC support for ImGuiTestHarness
option(YAZE_WITH_GRPC "Enable gRPC-based ImGuiTestHarness (experimental)" OFF)

if(YAZE_WITH_GRPC)
  # Try to find gRPC, but don't fail if missing
  find_package(gRPC CONFIG QUIET)
  find_package(Protobuf CONFIG QUIET)
  
  if(gRPC_FOUND AND Protobuf_FOUND)
    message(STATUS "✓ gRPC support enabled")
    message(STATUS "  gRPC version: ${gRPC_VERSION}")
    message(STATUS "  Protobuf version: ${Protobuf_VERSION}")
    
    set(YAZE_HAS_GRPC TRUE)
    
    # Helper function for .proto compilation (defined later)
    include(cmake/grpc.cmake)
  else()
    message(WARNING "⚠ YAZE_WITH_GRPC=ON but gRPC not found. Disabling gRPC features.")
    message(WARNING "   Install via: vcpkg install grpc protobuf")
    message(WARNING "   Or set: CMAKE_TOOLCHAIN_FILE to vcpkg toolchain")
    
    set(YAZE_HAS_GRPC FALSE)
  endif()
else()
  message(STATUS "○ gRPC support disabled (set YAZE_WITH_GRPC=ON to enable)")
  set(YAZE_HAS_GRPC FALSE)
endif()

# Pass to source code
if(YAZE_HAS_GRPC)
  add_compile_definitions(YAZE_WITH_GRPC)
endif()
```

**Key Design Choice**: `QUIET` flag on `find_package()`
- If gRPC not found, build continues **without errors**
- Clear warning message guides user to install gRPC
- Contributor can build YAZE without gRPC

### Phase 3: Create cmake/grpc.cmake Helper

```cmake
# cmake/grpc.cmake
# Helper functions for gRPC/Protobuf code generation

if(NOT YAZE_HAS_GRPC)
  # Guard: only define functions if gRPC available
  return()
endif()

# Function: yaze_add_grpc_service(target proto_file)
#   Generates C++ code from .proto and adds to target
#
# Example:
#   yaze_add_grpc_service(yaze 
#     ${CMAKE_CURRENT_SOURCE_DIR}/app/core/proto/test_harness.proto)
#
function(yaze_add_grpc_service target proto_file)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Target '${target}' does not exist")
  endif()
  
  if(NOT EXISTS ${proto_file})
    message(FATAL_ERROR "Proto file not found: ${proto_file}")
  endif()
  
  get_filename_component(proto_dir ${proto_file} DIRECTORY)
  get_filename_component(proto_name ${proto_file} NAME_WE)
  
  # Output files
  set(proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.cc")
  set(proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.h")
  set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc")
  set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h")
  
  # Custom command to run protoc
  add_custom_command(
    OUTPUT ${proto_srcs} ${proto_hdrs} ${grpc_srcs} ${grpc_hdrs}
    COMMAND protobuf::protoc
      --proto_path=${proto_dir}
      --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
      --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
      --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
      ${proto_file}
    DEPENDS ${proto_file} protobuf::protoc gRPC::grpc_cpp_plugin
    COMMENT "Generating C++ from ${proto_name}.proto"
    VERBATIM
  )
  
  # Add generated sources to target
  target_sources(${target} PRIVATE
    ${proto_srcs}
    ${grpc_srcs}
  )
  
  # Add include directory for generated headers
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
  )
  
  # Link gRPC libraries
  target_link_libraries(${target} PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
  )
  
  message(STATUS "  Added gRPC service: ${proto_name}.proto -> ${target}")
endfunction()
```

### Phase 4: Test on Second Platform (Linux VM)

**Goal**: Validate cross-platform before committing

```bash
# On Linux VM (Ubuntu 22.04 or similar)
sudo apt update
sudo apt install -y build-essential cmake git

# Install vcpkg
cd ~/
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Install gRPC
./vcpkg install grpc:x64-linux protobuf:x64-linux

# Clone YAZE (your branch)
cd ~/
git clone https://github.com/scawful/yaze.git
cd yaze
git checkout feature/it-01-grpc

# Build with gRPC
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DYAZE_WITH_GRPC=ON
cmake --build build -j$(nproc)

# Expected: Same result as macOS (successful build)
```

### Phase 5: Test on Windows (VM or Contributor)

**Goal**: Validate most complex platform

```powershell
# On Windows (PowerShell as Administrator)

# Install vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install gRPC (takes 10-15 minutes first time)
.\vcpkg install grpc:x64-windows protobuf:x64-windows

# Clone YAZE
cd C:\Users\YourName\Code
git clone https://github.com/scawful/yaze.git
cd yaze
git checkout feature/it-01-grpc

# Configure with Visual Studio generator
cmake -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DYAZE_WITH_GRPC=ON `
  -A x64

# Build (Release config)
cmake --build build --config Release

# Expected: Successful build, yaze.exe in build/bin/Release/
```

**Windows-Specific Concerns**:
- ⚠️ **Build Time**: gRPC takes 10-15 minutes to compile on Windows (one-time)
- ⚠️ **DLL Paths**: vcpkg handles this via `vcpkg integrate install`
- ⚠️ **MSVC Version**: Requires Visual Studio 2019+ with C++ workload

### Phase 6: Only Then Add to vcpkg.json

**Trigger**: All 3 platforms validated (macOS ✅, Linux ✅, Windows ✅)

```json
{
  "name": "yaze",
  "version": "0.3.2",
  "dependencies": [
    {
      "name": "sdl2",
      "platform": "!uwp",
      "features": ["vulkan"]
    },
    {
      "name": "grpc",
      "features": ["codegen"],
      "platform": "!android & !uwp"
    },
    "protobuf"
  ],
  "builtin-baseline": "4bee3f5aae7aefbc129ca81c33d6a062b02fcf3b"
}
```

**Documentation Update**: Add to `docs/02-build-instructions.md`:
```markdown
### Building with gRPC Support (Optional)

gRPC enables the ImGuiTestHarness for automated GUI testing.

**Prerequisites**:
- vcpkg installed and integrated
- CMake 3.20+
- 15-20 minutes for first-time gRPC build

**Build Steps**:
```bash
# macOS/Linux
cmake -B build -DYAZE_WITH_GRPC=ON
cmake --build build -j8

# Windows (PowerShell)
cmake -B build -DYAZE_WITH_GRPC=ON -A x64
cmake --build build --config Release
```

**Troubleshooting**:
- If gRPC not found: `vcpkg install grpc protobuf`
- On Windows: Ensure Developer Command Prompt for VS
- Build errors: See `docs/z3ed/DEPENDENCY_MANAGEMENT.md`
```

## Rollback Strategy

If gRPC integration causes issues:

### Immediate Rollback (During Development)
```bash
# Disable gRPC, revert to working build
cmake -B build -DYAZE_WITH_GRPC=OFF
cmake --build build -j8
```

### Full Rollback (If Committing Breaks CI)
```bash
git revert <commit-hash>  # Revert CMake changes
# Edit vcpkg.json to remove grpc/protobuf
git add vcpkg.json CMakeLists.txt cmake/grpc.cmake
git commit -m "Rollback: Remove gRPC integration (build issues)"
git push
```

## Dependency Testing Checklist

Before merging gRPC integration:

### macOS (Developer Machine)
- [ ] Clean build with `YAZE_WITH_GRPC=OFF` succeeds
- [ ] Clean build with `YAZE_WITH_GRPC=ON` succeeds
- [ ] Binary runs and starts without crashes
- [ ] File size reasonable (~50MB app bundle + ~5MB gRPC overhead)

### Linux (VM or CI)
- [ ] Ubuntu 22.04 clean build succeeds
- [ ] Fedora/RHEL clean build succeeds (optional)
- [ ] Binary links against system glibc correctly

### Windows (VM or Contributor)
- [ ] Visual Studio 2019 build succeeds
- [ ] Visual Studio 2022 build succeeds
- [ ] Release build runs without DLL errors
- [ ] Installer (CPack) includes gRPC DLLs if needed

### CI/CD
- [ ] GitHub Actions workflow passes on all platforms
- [ ] Build artifacts uploaded successfully
- [ ] No increased build time for default builds (gRPC off)

## Communication Plan

### Contributors Without gRPC
Update `docs/B1-contributing.md`:
```markdown
### Optional Features

Some YAZE features are optional and require additional dependencies:

- **gRPC Test Harness** (`YAZE_WITH_GRPC=ON`): For automated GUI testing
  - Not required for general YAZE development
  - Adds 10-15 minutes to initial build time
  - See `docs/z3ed/DEPENDENCY_MANAGEMENT.md` for setup

You can build and contribute to YAZE without these optional features.
```

### PR Description Template
```markdown
## Dependency Changes

This PR adds optional gRPC support for ImGuiTestHarness.

**Impact**:
- ✅ Existing builds unaffected (opt-in via `-DYAZE_WITH_GRPC=ON`)
- ✅ Cross-platform validated (macOS ✅, Linux ✅, Windows ✅)
- ⚠️ First build with gRPC takes 10-15 minutes (one-time)

**Testing**:
- [x] macOS arm64 build successful
- [x] Linux x64 build successful
- [x] Windows x64 build successful
- [x] Default build (gRPC off) unchanged

**Documentation**:
- Updated: `docs/02-build-instructions.md`
- Created: `docs/z3ed/DEPENDENCY_MANAGEMENT.md`
- Updated: `docs/z3ed/IT-01-getting-started-grpc.md`
```

## Future Considerations

### Other Optional Dependencies (Lessons Learned)
- **ImPlot** - For graphing/visualization
- **libcurl** - For HTTP requests (alternative to gRPC)
- **SQLite** - For persistent state (alternative to JSON files)

**Pattern to Follow**:
1. Test locally with isolated vcpkg first
2. Add CMake `option()` with `QUIET` find_package
3. Validate on 3 platforms minimum
4. Document setup and troubleshooting
5. Only then add to vcpkg.json

### Dependency Pinning
Consider pinning gRPC version after validation:
```json
{
  "overrides": [
    {
      "name": "grpc",
      "version": "1.60.0"
    }
  ]
}
```

**When to Pin**:
- After successful Windows validation
- Before announcing feature to contributors
- To ensure reproducible builds

---

**Summary**: We're taking a **careful, incremental approach** to adding gRPC:
1. ✅ Test locally on macOS (isolated vcpkg)
2. ✅ Add CMake support with graceful fallback
3. ✅ Validate on Linux
4. ✅ Validate on Windows
5. ✅ Only then commit to vcpkg.json
6. ✅ Document everything

This ensures existing contributors aren't impacted while we experiment with gRPC.
