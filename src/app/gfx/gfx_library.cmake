set(
  YAZE_APP_GFX_SRC
  app/gfx/arena.cc
  app/gfx/atlas_renderer.cc
  app/gfx/background_buffer.cc
  app/gfx/bitmap.cc
  app/gfx/compression.cc
  app/gfx/memory_pool.cc
  app/gfx/performance_dashboard.cc
  app/gfx/performance_profiler.cc
  app/gfx/scad_format.cc
  app/gfx/snes_palette.cc
  app/gfx/snes_tile.cc
  app/gfx/snes_color.cc
  app/gfx/tilemap.cc
  app/gfx/graphics_optimizer.cc
  app/gfx/bpp_format_manager.cc
)

# ==============================================================================
# Yaze Graphics Library
# ==============================================================================
# This library contains all graphics-related functionality:
# - Bitmap manipulation
# - SNES tile/palette handling
# - Compression/decompression
# - Arena memory management
# - Atlas rendering
# - Performance profiling
#
# Dependencies: yaze_util, SDL2, Abseil
# ==============================================================================

add_library(yaze_gfx STATIC ${YAZE_APP_GFX_SRC})

target_precompile_headers(yaze_gfx PRIVATE
  <vector>
  <string>
  <memory>
)

target_include_directories(yaze_gfx PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_gfx PUBLIC
  yaze_util
  yaze_common
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
)

# Conditionally add PNG support
if(PNG_FOUND)
  target_include_directories(yaze_gfx PUBLIC ${PNG_INCLUDE_DIRS})
  target_link_libraries(yaze_gfx PUBLIC ${PNG_LIBRARIES})
endif()

set_target_properties(yaze_gfx PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_gfx PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_gfx PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_gfx PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_gfx library configured")
