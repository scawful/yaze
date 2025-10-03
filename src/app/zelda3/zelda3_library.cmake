# ==============================================================================
# Yaze Zelda3 Library
# ==============================================================================
# This library contains all Zelda3-specific game logic:
# - Overworld system (maps, tiles, sprites)
# - Dungeon system (rooms, objects, sprites)
# - Screen modules (title, inventory, dungeon map)
# - Sprite management
# - Music/tracker system
#
# Dependencies: yaze_gfx, yaze_util
# ==============================================================================

add_library(yaze_zelda3 STATIC ${YAZE_APP_ZELDA3_SRC})

target_include_directories(yaze_zelda3 PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/incl
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_zelda3 PUBLIC
  yaze_gfx
  yaze_util
  yaze_common
  ${ABSL_TARGETS}
)

set_target_properties(yaze_zelda3 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_zelda3 PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_zelda3 PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_zelda3 PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_zelda3 library configured")
