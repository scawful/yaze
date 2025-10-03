# ==============================================================================
# Yaze GUI Library
# ==============================================================================
# This library contains all GUI-related functionality:
# - Canvas system
# - ImGui widgets and utilities
# - Input handling
# - Theme management
# - Color utilities
# - Background rendering
#
# Dependencies: yaze_gfx, yaze_util, ImGui, SDL2
# ==============================================================================

add_library(yaze_gui STATIC ${YAZE_GUI_SRC})

target_include_directories(yaze_gui PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_gui PUBLIC
  yaze_gfx
  yaze_util
  yaze_common
  ImGui
  ${SDL_TARGETS}
)

set_target_properties(yaze_gui PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_gui PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_gui PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_gui PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_gui library configured")
