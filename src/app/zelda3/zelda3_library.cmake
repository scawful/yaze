set(
  YAZE_APP_ZELDA3_SRC
  app/zelda3/dungeon/dungeon_editor_system.cc
  app/zelda3/dungeon/dungeon_object_editor.cc
  app/zelda3/dungeon/object_drawer.cc
  app/zelda3/dungeon/object_parser.cc
  app/zelda3/dungeon/room.cc
  app/zelda3/dungeon/room_layout.cc
  app/zelda3/dungeon/room_object.cc
  app/zelda3/music/tracker.cc
  app/zelda3/overworld/overworld.cc
  app/zelda3/overworld/overworld_map.cc
  app/zelda3/screen/dungeon_map.cc
  app/zelda3/screen/inventory.cc
  app/zelda3/screen/title_screen.cc
  app/zelda3/sprite/sprite.cc
  app/zelda3/sprite/sprite_builder.cc
  app/zelda3/zelda3_labels.cc
)

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

target_precompile_headers(yaze_zelda3 PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

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
