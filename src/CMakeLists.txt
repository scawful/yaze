set(
  YAZE_APP_CORE_SRC
  app/core/common.cc
  app/core/controller.cc
  app/core/labeling.cc
  app/emu/emulator.cc
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
  YAZE_GUI_SRC
  app/gui/asset_browser.cc
  app/gui/canvas.cc
  app/gui/input.cc
  app/gui/style.cc
  app/gui/color.cc
  app/gui/zeml.cc
)

set(
  YAZE_APP_EMU_SRC
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
    app/core/platform/file_dialog.cc
  )
endif()

if(APPLE)
    list(APPEND YAZE_APP_CORE_SRC 
      app/core/platform/file_dialog.mm
      app/core/platform/app_delegate.mm
      app/core/platform/font_loader.mm
      app/core/platform/clipboard.mm
      app/core/platform/file_path.mm
    )

    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa not found")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()

if (YAZE_BUILD_APP)
  include(app/CMakeLists.txt)
endif()

if (YAZE_BUILD_Z3ED)
  include(cli/CMakeLists.txt) 
endif()

if (YAZE_BUILD_EXTENSIONS)
  include(ext/CMakeLists.txt)
endif()

if (YAZE_BUILD_PYTHON)
  include(py/CMakeLists.txt)
endif()

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

include(test/CMakeLists.txt)

# Yaze C API
add_library(yaze_c SHARED
  ./yaze.cc
  app/rom.cc
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
  ${IMGUI_TEST_ENGINE_SOURCES}
)

target_include_directories(
  yaze_c PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
)

target_link_libraries(
  yaze_c PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ImGuiTestEngine
  ImGui
)