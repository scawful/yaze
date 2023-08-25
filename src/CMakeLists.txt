set(
  YAZE_APP_CORE_SRC
  app/core/common.cc
  app/core/controller.cc
  app/core/emulator.cc
  app/core/pipeline.cc
)

set(
  YAZE_APP_EDITOR_SRC
  app/editor/assembly_editor.cc
  app/editor/dungeon_editor.cc
  app/editor/graphics_editor.cc
  app/editor/master_editor.cc
  app/editor/music_editor.cc
  app/editor/overworld_editor.cc
  app/editor/palette_editor.cc
  app/editor/screen_editor.cc
  app/editor/sprite_editor.cc
)

set(
  YAZE_APP_GFX_SRC
  app/gfx/bitmap.cc
  app/gfx/compression.cc
  app/gfx/scad_format.cc
  app/gfx/snes_palette.cc
  app/gfx/snes_tile.cc
)

set(
  YAZE_APP_ZELDA3_SRC
  app/zelda3/overworld_map.cc
  app/zelda3/overworld.cc
  app/zelda3/screen/inventory.cc
  app/zelda3/screen/title_screen.cc
  app/zelda3/sprite/sprite.cc
  app/zelda3/music/tracker.cc
  app/zelda3/dungeon/room.cc
  app/zelda3/dungeon/room_object.cc
)

set(
  YAZE_GUI_SRC
  app/gui/canvas.cc
  app/gui/input.cc
  app/gui/style.cc
  app/gui/widgets.cc
  app/gui/color.cc
)

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main)
    add_definitions(-DSDL_MAIN_HANDLED)
endif()

include(app/CMakeLists.txt)
include(cli/CMakeLists.txt)

if (UNIX)
  target_compile_definitions(yaze PRIVATE "linux")
  target_compile_definitions(yaze PRIVATE "stricmp=strcasecmp")
endif()

if(MACOS)         
  set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/yaze.ico)
  set_target_properties(yaze 
      PROPERTIES
      BUNDLE True
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/yaze.plist.in
  )
elseif(UNIX)
  set_target_properties(yaze 
    PROPERTIES
    BUNDLE True
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
else()
set_target_properties(yaze
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/yaze.res"
)
endif()