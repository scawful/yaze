# ==============================================================================
# YAZE GFX Library Refactoring: Tiered Graphics Architecture
#
# This file implements the tiered graphics architecture as proposed in
# docs/gfx-refactor.md. The monolithic yaze_gfx library is decomposed
# into smaller, layered static libraries to improve build times and clarify
# dependencies.
# ==============================================================================

# ==============================================================================
# Helper Macro to Configure a GFX Library
# ==============================================================================
macro(configure_gfx_library name)
  target_precompile_headers(${name} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
  )
  target_include_directories(${name} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/incl
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )
  target_link_libraries(${name} PUBLIC
    yaze_util
    yaze_common
    ${ABSL_TARGETS}
  )
  set_target_properties(${name} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  if(UNIX AND NOT APPLE)
    target_compile_definitions(${name} PRIVATE linux stricmp=strcasecmp)
  elseif(APPLE)
    target_compile_definitions(${name} PRIVATE MACOS)
  elseif(WIN32)
    target_compile_definitions(${name} PRIVATE WINDOWS)
  endif()
endmacro()

# ==============================================================================
# 3.1. gfx_types (Foundation)
# Responsibility: Pure data structures for SNES graphics primitives.
# Dependencies: None
# ==============================================================================
set(GFX_TYPES_SRC
  app/gfx/types/snes_color.cc
  app/gfx/types/snes_palette.cc
  app/gfx/types/snes_tile.cc
)
add_library(yaze_gfx_types STATIC ${GFX_TYPES_SRC})
configure_gfx_library(yaze_gfx_types)
message(STATUS "  - GFX Tier: gfx_types configured")

# ==============================================================================
# 3.2. gfx_backend (Rendering Abstraction)
# Responsibility: Low-level rendering interface and SDL2 implementation.
# Dependencies: SDL2
# ==============================================================================
set(GFX_BACKEND_SRC
  app/gfx/backend/sdl2_renderer.cc
)
add_library(yaze_gfx_backend STATIC ${GFX_BACKEND_SRC})
configure_gfx_library(yaze_gfx_backend)
target_link_libraries(yaze_gfx_backend PUBLIC ${SDL_TARGETS})
message(STATUS "  - GFX Tier: gfx_backend configured")

# ==============================================================================
# 3.3. gfx_resource (Resource Management)
# Responsibility: Manages memory and GPU resources.
# Dependencies: gfx_backend
# ==============================================================================
set(GFX_RESOURCE_SRC
  app/gfx/resource/arena.cc
  app/gfx/resource/memory_pool.cc
)
add_library(yaze_gfx_resource STATIC ${GFX_RESOURCE_SRC})
configure_gfx_library(yaze_gfx_resource)
target_link_libraries(yaze_gfx_resource PUBLIC yaze_gfx_backend)
message(STATUS "  - GFX Tier: gfx_resource configured")

# ==============================================================================
# 3.4. gfx_core (Core Graphics Object)
# Responsibility: The central Bitmap class.
# Dependencies: gfx_types, gfx_resource
# ==============================================================================
set(GFX_CORE_SRC
  app/gfx/core/bitmap.cc
)
add_library(yaze_gfx_core STATIC ${GFX_CORE_SRC})
configure_gfx_library(yaze_gfx_core)
target_link_libraries(yaze_gfx_core PUBLIC
  yaze_gfx_types
  yaze_gfx_resource
)
message(STATUS "  - GFX Tier: gfx_core configured")

# ==============================================================================
# 3.5. gfx_util (Utilities)
# Responsibility: Standalone graphics data conversion and compression.
# Dependencies: gfx_core
# ==============================================================================
set(GFX_UTIL_SRC
  app/gfx/util/bpp_format_manager.cc
  app/gfx/util/compression.cc
  app/gfx/util/scad_format.cc
  app/gfx/util/palette_manager.cc
)
add_library(yaze_gfx_util STATIC ${GFX_UTIL_SRC})
configure_gfx_library(yaze_gfx_util)
target_link_libraries(yaze_gfx_util PUBLIC yaze_gfx_core)
message(STATUS "  - GFX Tier: gfx_util configured")

# ==============================================================================
# 3.6. gfx_render (High-Level Rendering)
# Responsibility: Advanced rendering strategies.
# Dependencies: gfx_core, gfx_backend
# ==============================================================================
set(GFX_RENDER_SRC
  app/gfx/render/atlas_renderer.cc
  app/gfx/render/background_buffer.cc
  app/gfx/render/texture_atlas.cc
  app/gfx/render/tilemap.cc
)
add_library(yaze_gfx_render STATIC ${GFX_RENDER_SRC})
configure_gfx_library(yaze_gfx_render)
target_link_libraries(yaze_gfx_render PUBLIC
  yaze_gfx_core
  yaze_gfx_backend
)
message(STATUS "  - GFX Tier: gfx_render configured")

# ==============================================================================
# 3.7. gfx_debug (Performance & Analysis)
# Responsibility: Profiling, debugging, and optimization tools.
# Dependencies: gfx_util, gfx_render
# ==============================================================================
set(GFX_DEBUG_SRC
  app/gfx/debug/performance/performance_dashboard.cc
  app/gfx/debug/performance/performance_profiler.cc
  app/gfx/debug/graphics_optimizer.cc
)
add_library(yaze_gfx_debug STATIC ${GFX_DEBUG_SRC})
configure_gfx_library(yaze_gfx_debug)
target_link_libraries(yaze_gfx_debug PUBLIC
  yaze_gfx_util
  yaze_gfx_render
)
message(STATUS "  - GFX Tier: gfx_debug configured")

# ==============================================================================
# Aggregate INTERFACE Library (yaze_gfx)
# Provides a single link target for external consumers (e.g., yaze_gui).
# ==============================================================================
add_library(yaze_gfx INTERFACE)
target_link_libraries(yaze_gfx INTERFACE
  yaze_gfx_types
  yaze_gfx_backend
  yaze_gfx_resource
  yaze_gfx_core
  yaze_gfx_util
  yaze_gfx_render
  yaze_gfx_debug
  yaze_util
  yaze_common
  ${ABSL_TARGETS}
)

# Conditionally add PNG support
if(PNG_FOUND)
  target_include_directories(yaze_gfx INTERFACE ${PNG_INCLUDE_DIRS})
  target_link_libraries(yaze_gfx INTERFACE ${PNG_LIBRARIES})
endif()

message(STATUS "✓ yaze_gfx library configured with tiered architecture")
