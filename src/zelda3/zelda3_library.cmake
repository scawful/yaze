set(
  YAZE_APP_ZELDA3_SRC
  zelda3/common.cc
  zelda3/game_data.cc
  zelda3/dungeon/door_position.cc
  zelda3/dungeon/dungeon_editor_system.cc
  zelda3/dungeon/custom_object.cc
  zelda3/dungeon/dungeon_object_editor.cc
  zelda3/dungeon/dungeon_object_registry.cc
  zelda3/dungeon/dungeon_validator.cc
  zelda3/dungeon/object_dimensions.cc
  zelda3/dungeon/geometry/object_geometry.cc
  zelda3/dungeon/object_drawer.cc
  zelda3/dungeon/object_parser.cc
  zelda3/dungeon/object_templates.cc
  zelda3/dungeon/room.cc
  zelda3/dungeon/room_layer_manager.cc
  zelda3/dungeon/palette_debug.cc
  zelda3/dungeon/room_layout.cc
  zelda3/dungeon/room_object.cc
  # Draw routine modules (Phase 2 modularization)
  zelda3/dungeon/draw_routines/draw_routine_types.cc
  zelda3/dungeon/draw_routines/draw_routine_registry.cc
  zelda3/dungeon/draw_routines/rightwards_routines.cc
  zelda3/dungeon/draw_routines/downwards_routines.cc
  zelda3/dungeon/draw_routines/diagonal_routines.cc
  zelda3/dungeon/draw_routines/corner_routines.cc
  zelda3/dungeon/draw_routines/special_routines.cc
  zelda3/formats/offsets.cc
  zelda3/music/asm_exporter.cc
  zelda3/music/asm_importer.cc
  zelda3/music/music_bank.cc
  zelda3/music/spc_parser.cc
  zelda3/music/spc_serializer.cc
  zelda3/music/tracker.cc
  zelda3/overworld/diggable_tiles.cc
  zelda3/overworld/diggable_tiles_patch.cc
  zelda3/overworld/overworld.cc
  zelda3/overworld/overworld_map.cc
  zelda3/overworld/overworld_entrance.cc
  zelda3/overworld/overworld_exit.cc
  zelda3/overworld/overworld_item.cc
  zelda3/palette_constants.cc
  zelda3/screen/dungeon_map.cc
  zelda3/screen/inventory.cc
  zelda3/screen/title_screen.cc
  zelda3/screen/overworld_map_screen.cc
  zelda3/sprite/sprite.cc
  zelda3/sprite/sprite_builder.cc
  zelda3/sprite/sprite_names.h
  zelda3/sprite/sprite_oam_tables.cc
  zelda3/zelda3_labels.cc
  zelda3/resource_labels.cc
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
  yaze_rom
  yaze_gfx
  yaze_util
  yaze_common
  ${ABSL_TARGETS}
  nlohmann_json::nlohmann_json
  ImGui
  ${YAZE_IMPLOT_TARGETS}
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
