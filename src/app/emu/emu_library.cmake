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

if(YAZE_ENABLE_GRPC)
  list(APPEND YAZE_APP_EMU_SRC
    app/emu/internal_emulator_adapter.cc
    app/emu/mesen/mesen_emulator_adapter.cc
  )
endif()

add_library(yaze_emulator STATIC ${YAZE_APP_EMU_SRC})

target_precompile_headers(yaze_emulator PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_emulator PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/inc
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

# Link to SDL (version-dependent)
if(YAZE_USE_SDL3)
  set(SDL_TARGETS ${YAZE_SDL3_TARGETS})
else()
  set(SDL_TARGETS ${YAZE_SDL2_TARGETS})
endif()

target_link_libraries(yaze_emulator PUBLIC
  yaze_util
  yaze_common
  yaze_app_core_lib
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
)

if(YAZE_ENABLE_JSON AND TARGET nlohmann_json::nlohmann_json)
  target_link_libraries(yaze_emulator PUBLIC nlohmann_json::nlohmann_json)
endif()

# Emulator UI library (depends on app core and GUI)
add_library(yaze_emulator_ui STATIC ${YAZE_EMU_GUI_SRC})

target_precompile_headers(yaze_emulator_ui PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_emulator_ui PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/inc
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_emulator_ui PUBLIC
  yaze_emulator
  yaze_app_core_lib
  yaze_gui
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
)

set_target_properties(yaze_emulator PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set_target_properties(yaze_emulator_ui PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_emulator PRIVATE linux stricmp=strcasecmp)
elseif(YAZE_PLATFORM_MACOS)
  target_compile_definitions(yaze_emulator PRIVATE MACOS)
elseif(YAZE_PLATFORM_IOS)
  target_compile_definitions(yaze_emulator PRIVATE YAZE_IOS)
elseif(WIN32)
  target_compile_definitions(yaze_emulator PRIVATE WINDOWS)
endif()

# SDL version compile definitions
if(YAZE_USE_SDL3)
  target_compile_definitions(yaze_emulator PRIVATE YAZE_USE_SDL3=1 YAZE_SDL3=1)
else()
  target_compile_definitions(yaze_emulator PRIVATE YAZE_SDL2=1)
endif()

# Handle circular dependency: yaze_emulator calls UI functions
target_link_libraries(yaze_emulator PUBLIC yaze_emulator_ui)

# If adapters are included, we need generated proto headers
if(YAZE_ENABLE_GRPC)
  if(TARGET yaze_proto_gen)
    add_dependencies(yaze_emulator yaze_proto_gen)
    target_include_directories(yaze_emulator PRIVATE ${CMAKE_BINARY_DIR}/gens)
  endif()
endif()

message(STATUS "✓ yaze_emulator library configured")
