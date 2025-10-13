# ==============================================================================
# Yaze Emulator Library
# ==============================================================================
# This library contains SNES emulation functionality:
# - CPU (65C816) implementation
# - PPU (Picture Processing Unit) for graphics
# - APU (Audio Processing Unit) with SPC700 and DSP
# - DMA controller
# - Memory management
#
# Dependencies: yaze_util, SDL2
# ==============================================================================

add_library(yaze_emulator STATIC ${YAZE_APP_EMU_SRC})

target_precompile_headers(yaze_emulator PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_emulator PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_emulator PUBLIC
  yaze_util
  yaze_common
  yaze_core_lib
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
)

set_target_properties(yaze_emulator PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_emulator PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_emulator PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_emulator PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_emulator library configured")
