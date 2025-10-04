# Build Modularization Plan for Yaze

## Executive Summary

This document outlines a comprehensive plan to modularize the yaze build system, reducing build times by 40-60% through the creation of intermediate libraries and improved dependency management.

## Current Problems

1. **Monolithic Build**: All subsystems compile directly into final executables
2. **Repeated Compilation**: Same sources compiled multiple times for different targets
3. **Poor Incremental Builds**: Small changes trigger large rebuilds
4. **Long Link Times**: Large monolithic targets take longer to link
5. **Parallel Build Limitations**: Cannot parallelize subsystem builds effectively

## Proposed Architecture

### Library Hierarchy

```
yaze_common (INTERFACE)
    ├── yaze_util (STATIC)
    │   ├── util/bps.cc
    │   ├── util/flag.cc
    │   └── util/hex.cc
    │
    ├── yaze_gfx (STATIC)
    │   ├── All YAZE_APP_GFX_SRC files
    │   └── Depends: yaze_util
    │
    ├── yaze_gui (STATIC)
    │   ├── All YAZE_GUI_SRC files
    │   └── Depends: yaze_gfx, ImGui
    │
    ├── yaze_zelda3 (STATIC)
    │   ├── All YAZE_APP_ZELDA3_SRC files
    │   └── Depends: yaze_gfx, yaze_util
    │
    ├── yaze_emulator (STATIC)
    │   ├── All YAZE_APP_EMU_SRC files
    │   └── Depends: yaze_util
    │
    ├── yaze_core_lib (STATIC)
    │   ├── All YAZE_APP_CORE_SRC files
    │   └── Depends: yaze_util, yaze_gfx, asar
    │
    ├── yaze_editor (STATIC)
    │   ├── All YAZE_APP_EDITOR_SRC files
    │   └── Depends: yaze_core_lib, yaze_gfx, yaze_gui, yaze_zelda3
    │
    ├── yaze_agent (STATIC - already exists)
    │   └── Depends: yaze_util
    │
    └── Final Executables
        ├── yaze (APP) - Links all above
        ├── yaze_emu - Links subset
        ├── z3ed (CLI) - Links subset
        └── yaze_test - Links all for testing
```

## Implementation Guide

This section provides step-by-step instructions for implementing the modularized build system for yaze.

### Quick Start

#### Option 1: Enable Modular Build (Recommended)

Add this option to enable the new modular build system:

```bash
cmake -B build -DYAZE_USE_MODULAR_BUILD=ON
cmake --build build
```

#### Option 2: Keep Existing Build (Default)

The existing build system remains the default for stability:

```bash
cmake -B build
cmake --build build
```

### Implementation Steps

#### Step 1: Add Build Option

Add to **root `CMakeLists.txt`** (around line 45, after other options):

```cmake
# Build system options
option(YAZE_USE_MODULAR_BUILD "Use modularized library build system for faster builds" OFF)
```

#### Step 2: Update src/CMakeLists.txt

Replace the current monolithic build with a library-based approach. Add near the top of `src/CMakeLists.txt`:

```cmake
# ==============================================================================
# Modular Build System
# ==============================================================================
if(YAZE_USE_MODULAR_BUILD)
  message(STATUS "Using modular build system")
  
  # Build subsystem libraries in dependency order
  if(YAZE_BUILD_LIB OR YAZE_BUILD_APP OR YAZE_BUILD_Z3ED)
    include(util/util.cmake)
    include(app/gfx/gfx_library.cmake)
    include(app/gui/gui_library.cmake)
    include(app/zelda3/zelda3_library.cmake)
    
    if(YAZE_BUILD_EMU AND NOT YAZE_WITH_GRPC)
      include(app/emu/emu_library.cmake)
    endif()
    
    include(app/core/core_library.cmake)
    include(app/editor/editor_library.cmake)
  endif()
  
  # Create yaze_core as an INTERFACE library that aggregates all modules
  if(YAZE_BUILD_LIB)
    add_library(yaze_core INTERFACE)
    target_link_libraries(yaze_core INTERFACE
      yaze_util
      yaze_gfx
      yaze_gui
      yaze_zelda3
      yaze_core_lib
      yaze_editor
      yaze_agent
    )
    
    if(YAZE_BUILD_EMU AND NOT YAZE_WITH_GRPC)
      target_link_libraries(yaze_core INTERFACE yaze_emulator)
    endif()
    
    # Add gRPC support if enabled
    if(YAZE_WITH_GRPC)
      target_add_protobuf(yaze_core 
        ${CMAKE_SOURCE_DIR}/src/app/core/proto/imgui_test_harness.proto)
      target_link_libraries(yaze_core INTERFACE
        grpc++
        grpc++_reflection
        libprotobuf)
    endif()
  endif()
else()
  message(STATUS "Using traditional monolithic build system")
  # Keep existing build (current code in src/CMakeLists.txt)
endif()
```

#### Step 3: Update yaze Application Linking

In `src/app/app.cmake`, update the main yaze target:

```cmake
if(YAZE_USE_MODULAR_BUILD)
  target_link_libraries(yaze PRIVATE
    yaze_util
    yaze_gfx
    yaze_gui
    yaze_zelda3
    yaze_core_lib
    yaze_editor
  )
  if(YAZE_BUILD_EMU AND NOT YAZE_WITH_GRPC)
    target_link_libraries(yaze PRIVATE yaze_emulator)
  endif()
else()
  target_link_libraries(yaze PRIVATE yaze_core)
endif()
```

#### Step 4: Update Test Executable

In `test/CMakeLists.txt`, update the test linking:

```cmake
if(YAZE_BUILD_LIB)
  if(YAZE_USE_MODULAR_BUILD)
    # Link individual libraries for faster incremental test builds
    target_link_libraries(yaze_test
      yaze_util
      yaze_gfx
      yaze_gui
      yaze_zelda3
      yaze_core_lib
      yaze_editor
    )
    if(YAZE_BUILD_EMU AND NOT YAZE_WITH_GRPC)
      target_link_libraries(yaze_test yaze_emulator)
    endif()
  else()
    # Use monolithic core library
    target_link_libraries(yaze_test yaze_core)
  endif()
endif()
```

## Expected Performance Improvements

### Build Time Reductions

| Scenario | Current | With Modularization | Improvement |
|----------|---------|---------------------|-------------|
| Clean build | 100% | 100% | 0% (first time) |
| Change util file | ~80% rebuild | ~15% rebuild | **81% faster** |
| Change gfx file | ~70% rebuild | ~30% rebuild | **57% faster** |
| Change gui file | ~60% rebuild | ~25% rebuild | **58% faster** |
| Change editor file | ~50% rebuild | ~20% rebuild | **60% faster** |
| Change zelda3 file | ~40% rebuild | ~15% rebuild | **62% faster** |
| Incremental test | 100% relink | Cached libs | **70% faster** |

### CI/CD Benefits

- **Cached Artifacts**: Libraries can be cached between CI runs
- **Selective Testing**: Only rebuild/test changed modules
- **Faster Iteration**: Developers see results sooner

## Rollback Procedure

If issues arise, you can revert to the traditional build:

```bash
# Option 1: Just disable the flag
cmake -B build -DYAZE_USE_MODULAR_BUILD=OFF
cmake --build build

# Option 2: Delete build and start fresh
rm -rf build
cmake -B build  # OFF is the default
cmake --build build
```