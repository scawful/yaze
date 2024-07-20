set(
  YAZE_APP_CORE_SRC
  app/core/common.cc
  app/core/controller.cc
  app/core/labeling.cc
)

set(
  YAZE_APP_EDITOR_SRC
  app/editor/dungeon_editor.cc
  app/editor/master_editor.cc
  app/editor/overworld_editor.cc
  app/editor/sprite/sprite_editor.cc
  app/editor/music/music_editor.cc
  app/editor/code/assembly_editor.cc
  app/editor/graphics/screen_editor.cc
  app/editor/graphics/graphics_editor.cc
  app/editor/graphics/palette_editor.cc
  app/editor/graphics/tile16_editor.cc
  app/editor/graphics/gfx_group_editor.cc
  app/editor/utils/gfx_context.cc
)

set(
  YAZE_APP_GFX_SRC
  app/gfx/bitmap.cc
  app/gfx/compression.cc
  app/gfx/scad_format.cc
  app/gfx/snes_palette.cc
  app/gfx/snes_tile.cc
  app/gfx/snes_color.cc
  app/gfx/tilesheet.cc
)

set(
  YAZE_APP_ZELDA3_SRC
  app/zelda3/overworld/overworld_map.cc
  app/zelda3/overworld/overworld.cc
  app/zelda3/screen/inventory.cc
  app/zelda3/screen/title_screen.cc
  app/zelda3/sprite/sprite.cc
  app/zelda3/music/tracker.cc
  app/zelda3/dungeon/room.cc
  app/zelda3/dungeon/room_object.cc
  app/zelda3/dungeon/object_renderer.cc
)

set(
  YAZE_GUI_SRC
  app/gui/canvas.cc
  app/gui/input.cc
  app/gui/style.cc
  app/gui/widgets.cc
  app/gui/color.cc
  app/gui/pipeline.cc
  app/gui/zeml.cc
)

set(
  YAZE_APP_EMU_SRC
  app/emu/emulator.cc
  app/emu/audio/apu.cc
  app/emu/audio/spc700.cc
  app/emu/audio/dsp.cc
  app/emu/audio/internal/addressing.cc
  app/emu/audio/internal/instructions.cc
  app/emu/cpu/internal/addressing.cc
  app/emu/cpu/internal/instructions.cc
  app/emu/cpu/cpu.cc
  app/emu/video/ppu.cc
  app/emu/memory/dma.cc
  app/emu/memory/memory.cc
  app/emu/snes.cc
)

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions(-DSDL_MAIN_HANDLED)
endif()

if (WIN32 OR MINGW OR UNIX AND NOT APPLE)
  list(APPEND YAZE_APP_CORE_SRC
    app/core/platform/font_loader.cc
    app/core/platform/clipboard.cc
  )
endif()

if(APPLE)
    list(APPEND YAZE_APP_CORE_SRC 
      app/core/platform/file_dialog.mm
      app/core/platform/app_delegate.mm
      app/core/platform/font_loader.mm
      app/core/platform/clipboard.mm
    )

    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa not found")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()

include(app/CMakeLists.txt)
# include(cli/CMakeLists.txt) Excluded for now, macOS include breaks action build

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