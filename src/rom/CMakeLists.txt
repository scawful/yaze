# ==============================================================================
# YAZE ROM Library
# ==============================================================================
# Generic SNES ROM handling library.
# Provides ROM loading, saving, patching, and raw data access.
#
# Dependencies: yaze_util, yaze_gfx, yaze_core, absl
# ==============================================================================

set(YAZE_ROM_LIB_SOURCES
  rom.cc
  rom_diff.cc
  rom_diagnostics.cc
  hm_support.cc
)

add_library(yaze_rom STATIC ${YAZE_ROM_LIB_SOURCES})

target_compile_features(yaze_rom PUBLIC cxx_std_20)

target_include_directories(yaze_rom PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_rom PUBLIC
  yaze_util
  yaze_core_lib
  yaze_gfx
  ${ABSL_TARGETS}
  ${YAZE_SDL2_TARGETS}
)

target_precompile_headers(yaze_rom PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

set_target_properties(yaze_rom PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "✓ yaze_rom library configured")
