# ==============================================================================
# YAZE GFX Library: Tiered Graphics Architecture
#
# This file implements a layered graphics library to avoid circular dependencies
# and improve build times.
#
# IMPORTANT FOR BUILD_CLEANER:
# - Source lists marked with "build_cleaner:auto-maintain" are managed automatically
# - Paths MUST be relative to SOURCE_ROOT (src/) for consistency
# - All other sections (macros, link structure) are manually configured
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
    ${PROJECT_BINARY_DIR}
  )
  target_link_libraries(${name} PUBLIC
    yaze_util
    yaze_common
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
# SOURCE LISTS (auto-maintained by build_cleaner.py)
# Paths are relative to src/ directory
# ==============================================================================

# build_cleaner:auto-maintain
set(GFX_TYPES_SRC
  app/gfx/types/snes_color.cc
  app/gfx/types/snes_palette.cc
  app/gfx/types/snes_tile.cc
)

# build_cleaner:auto-maintain
set(GFX_BACKEND_SRC
  app/gfx/backend/sdl2_renderer.cc
)

# build_cleaner:auto-maintain
set(GFX_RESOURCE_SRC
  app/gfx/resource/memory_pool.cc
)

# build_cleaner:auto-maintain
set(GFX_CORE_SRC
  app/gfx/core/bitmap.cc
  app/gfx/render/background_buffer.cc
  app/gfx/resource/arena.cc
)

# build_cleaner:auto-maintain
set(GFX_UTIL_SRC
  app/gfx/util/bpp_format_manager.cc
  app/gfx/util/compression.cc
  app/gfx/util/palette_manager.cc
  app/gfx/util/scad_format.cc
)

# build_cleaner:auto-maintain
set(GFX_RENDER_SRC
  app/gfx/render/atlas_renderer.cc
  app/gfx/render/texture_atlas.cc
  app/gfx/render/tilemap.cc
)

# build_cleaner:auto-maintain
set(GFX_DEBUG_SRC
  app/gfx/debug/graphics_optimizer.cc
  app/gfx/debug/performance/performance_dashboard.cc
  app/gfx/debug/performance/performance_profiler.cc
)

# ==============================================================================
# LIBRARY DEFINITIONS AND LINK STRUCTURE (manually configured)
# DO NOT AUTO-MAINTAIN - Dependency order is critical to avoid circular deps
# ==============================================================================

# Layer 1: Foundation types (no dependencies)
add_library(yaze_gfx_types STATIC ${GFX_TYPES_SRC})
configure_gfx_library(yaze_gfx_types)
# Debug: message(STATUS "YAZE_SDL2_TARGETS for gfx_types: '${YAZE_SDL2_TARGETS}'")
target_link_libraries(yaze_gfx_types PUBLIC ${YAZE_SDL2_TARGETS})

# Layer 2: Backend (depends on types)
add_library(yaze_gfx_backend STATIC ${GFX_BACKEND_SRC})
configure_gfx_library(yaze_gfx_backend)
target_link_libraries(yaze_gfx_backend PUBLIC 
  yaze_gfx_types 
  ${YAZE_SDL2_TARGETS}
)

# Layer 3a: Resource management (depends on backend)
add_library(yaze_gfx_resource STATIC ${GFX_RESOURCE_SRC})
configure_gfx_library(yaze_gfx_resource)
target_link_libraries(yaze_gfx_resource PUBLIC yaze_gfx_backend)

# Layer 3b: Rendering (depends on types, NOT on core to avoid circular dep)
add_library(yaze_gfx_render STATIC ${GFX_RENDER_SRC})
configure_gfx_library(yaze_gfx_render)
target_link_libraries(yaze_gfx_render PUBLIC 
  yaze_gfx_types
  yaze_gfx_backend
  ${YAZE_SDL2_TARGETS}
)

# Layer 3c: Debug tools (depends on types only at this level)
add_library(yaze_gfx_debug STATIC ${GFX_DEBUG_SRC})
configure_gfx_library(yaze_gfx_debug)
target_link_libraries(yaze_gfx_debug PUBLIC 
  yaze_gfx_types
  yaze_gfx_resource
  ImGui
  ${YAZE_SDL2_TARGETS}
)

# Layer 4: Core bitmap (depends on render, debug)
add_library(yaze_gfx_core STATIC ${GFX_CORE_SRC})
configure_gfx_library(yaze_gfx_core)
target_link_libraries(yaze_gfx_core PUBLIC
  yaze_gfx_types
  yaze_gfx_render
  yaze_gfx_debug
)

# Layer 5: Utilities (depends on core)
add_library(yaze_gfx_util STATIC ${GFX_UTIL_SRC})
configure_gfx_library(yaze_gfx_util)
target_link_libraries(yaze_gfx_util PUBLIC yaze_gfx_core)

# Aggregate INTERFACE library for easy linking
add_library(yaze_gfx INTERFACE)
target_link_libraries(yaze_gfx INTERFACE
  yaze_gfx_types
  yaze_gfx_backend
  yaze_gfx_resource
  yaze_gfx_render
  yaze_gfx_debug
  yaze_gfx_core
  yaze_gfx_util
)

# Conditionally add PNG support
if(PNG_FOUND)
  target_include_directories(yaze_gfx INTERFACE ${PNG_INCLUDE_DIRS})
  target_link_libraries(yaze_gfx INTERFACE ${PNG_LIBRARIES})
endif()

message(STATUS "✓ yaze_gfx library configured with tiered architecture")
