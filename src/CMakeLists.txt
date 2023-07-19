# yaze source files -----------------------------------------------------------
set(
  YAZE_APP_CORE_SRC
  app/core/common.cc
  app/core/controller.cc
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
)

set(
  YAZE_APP_GFX_SRC
  app/gfx/bitmap.cc
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
)

set(
  YAZE_GUI_SRC
  app/gui/canvas.cc
  app/gui/input.cc
  app/gui/style.cc
  app/gui/widgets.cc
  app/gui/color.cc
)

set(
  YAZE_VIEWER_SRC
  app/viewer/cgx_viewer.cc
)

# executable creation ---------------------------------------------------------

add_executable(
  yaze
  app/yaze.cc
  app/rom.cc
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${YAZE_VIEWER_SRC}
  # ${ASAR_STATIC_SRC}
  # ${SNES_SPC_SOURCES}
  ${IMGUI_SRC}
)

# including libraries ---------------------------------------------------------

target_include_directories(
  yaze PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  lib/SDL_mixer/include/
  ${GLEW_INCLUDE_DIRS}
  # lib/asar/src/asar/
  # lib/snes_spc/snes_spc/
)

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main)
    add_definitions(-DSDL_MAIN_HANDLED)
endif()

# linking libraries -----------------------------------------------------------

target_link_libraries(
  yaze PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${SDLMIXER_LIBRARY}
  SDL2_mixer
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  # asar-static
  # snes_spc
  ImGui
)

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
      # MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/yaze.plist.in
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