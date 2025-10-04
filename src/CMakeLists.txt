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

set(
  YAZE_UTIL_SRC
  util/bps.cc
  util/flag.cc
  util/hex.cc
)

set(YAZE_RESOURCE_FILES
  ${CMAKE_SOURCE_DIR}/assets/font/Karla-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Cousine-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/DroidSans.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/NotoSansJP.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/IBMPlexSansJP-Bold.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/MaterialIcons-Regular.ttf
)

# Add theme files for macOS bundle (replacing the glob pattern with explicit files)
file(GLOB YAZE_THEME_FILES "${CMAKE_SOURCE_DIR}/assets/themes/*.theme")
list(APPEND YAZE_RESOURCE_FILES ${YAZE_THEME_FILES})

# Configure assets for different platforms
foreach (FILE ${YAZE_RESOURCE_FILES})
  file(RELATIVE_PATH NEW_FILE "${CMAKE_SOURCE_DIR}/assets" ${FILE})
  get_filename_component(NEW_FILE_PATH ${NEW_FILE} DIRECTORY)
  
  if (APPLE)
    # macOS: Set bundle location
    set_source_files_properties(${FILE}
      PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/${NEW_FILE_PATH}"
    )
  else()
    # Windows/Linux: Copy to output directory
    set_source_files_properties(${FILE}
      PROPERTIES
      VS_DEPLOYMENT_CONTENT 1
      VS_DEPLOYMENT_LOCATION "assets/${NEW_FILE_PATH}"
    )
  endif()
endforeach()

# Conditionally add native file dialog (optional for CI builds)
if(NOT YAZE_MINIMAL_BUILD)
  # Check if we can build NFD before adding it
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND AND UNIX AND NOT APPLE)
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
    if(GTK3_FOUND)
      add_subdirectory(lib/nativefiledialog-extended)
      set(YAZE_HAS_NFD ON)
      message(STATUS "NFD enabled with GTK3 support")
    else()
      set(YAZE_HAS_NFD OFF)
      message(STATUS "NFD disabled - GTK3 not found")
    endif()
  elseif(WIN32 OR APPLE)
    add_subdirectory(lib/nativefiledialog-extended)
    set(YAZE_HAS_NFD ON)
    message(STATUS "NFD enabled for Windows/macOS")
  else()
    set(YAZE_HAS_NFD OFF)
    message(STATUS "NFD disabled - no platform support")
  endif()
else()
  set(YAZE_HAS_NFD OFF)
  message(STATUS "NFD disabled for minimal build")
endif()

if(YAZE_BUILD_APP OR YAZE_BUILD_Z3ED)
  include(cli/agent.cmake)
endif()

if(YAZE_USE_MODULAR_BUILD)
  message(STATUS "Using modular build system")

  if(YAZE_BUILD_LIB OR YAZE_BUILD_APP OR YAZE_BUILD_Z3ED)
    include(app/gfx/gfx.cmake)
    include(app/gui/gui.cmake)
    include(app/zelda3/zelda3.cmake)
    include(app/core/core.cmake)
    include(app/editor/editor.cmake)

    include(util/util.cmake)
    include(app/gfx/gfx_library.cmake)
    include(app/gui/gui_library.cmake)
    include(app/zelda3/zelda3_library.cmake)

    if(YAZE_BUILD_EMU)
      include(app/emu/emu_library.cmake)
    endif()

    include(app/core/core_library.cmake)
    include(app/editor/editor_library.cmake)
  endif()
else()
  message(STATUS "Using traditional monolithic build system")
endif()

if (YAZE_BUILD_APP)
  include(app/app.cmake)
endif()
# Conditionally build the emulator, but not when gRPC is enabled for app-only testing
# Conditionally build the emulator, but not when gRPC is enabled for app-only testing
if(YAZE_BUILD_EMU AND NOT YAZE_WITH_GRPC)
  include(app/emu/emu.cmake)
endif()
if (YAZE_BUILD_Z3ED)
  include(cli/z3ed.cmake)
endif()

if(MACOS)
  set_target_properties(yaze
    PROPERTIES
    BUNDLE True
    OUTPUT_NAME "yaze"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/yaze.plist.in
    RESOURCE ${YAZE_RESOURCE_FILES}
  )
elseif(UNIX)
  set_target_properties(yaze
    PROPERTIES
    BUNDLE True
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
  target_compile_definitions(yaze PRIVATE "linux")
  target_compile_definitions(yaze PRIVATE "stricmp=strcasecmp")
else()
  if(YAZE_MINIMAL_BUILD)
    # Skip Windows resource file in CI/minimal builds to avoid architecture conflicts
    set_target_properties(yaze
      PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
  else()
    # Windows resource file - only for x64 architecture
    set_target_properties(yaze
      PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # Only use resource file for x64 Windows builds
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
      # Generate yaze.res from yaze.rc and yaze.ico for Windows x64 builds
      set(YAZE_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/win32/yaze.rc")
      set(YAZE_ICO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/win32/yaze.ico")
      set(YAZE_RES_FILE "${CMAKE_CURRENT_BINARY_DIR}/yaze.res")

      # Add a custom command to generate the .res file from the .rc and .ico
      add_custom_command(
        OUTPUT ${YAZE_RES_FILE}
        COMMAND ${CMAKE_RC_COMPILER} /fo ${YAZE_RES_FILE} ${YAZE_RC_FILE}
        DEPENDS ${YAZE_RC_FILE} ${YAZE_ICO_FILE}
        COMMENT "Generating yaze.res from yaze.rc and yaze.ico"
        VERBATIM
      )

      # Make sure the resource file is built before linking
      add_custom_target(yaze_res ALL DEPENDS ${YAZE_RES_FILE})

      # Link the generated .res file to the yaze target
      target_sources(yaze PRIVATE ${YAZE_RES_FILE})
    endif()
  endif()
endif()

# Yaze Core Library (for testing and C API)
if (YAZE_BUILD_LIB)
  # Sources shared by the C API library
  set(YAZE_C_SOURCES
    ./yaze.cc
    cli/service/gui/gui_automation_client.cc
  )

  if(YAZE_USE_MODULAR_BUILD)
    # Aggregate modular libraries into an interface target for backward compatibility
    if(NOT TARGET yaze_core)
      add_library(yaze_core INTERFACE)
    endif()

    target_include_directories(
      yaze_core INTERFACE
      ${CMAKE_SOURCE_DIR}/src/lib/
      ${CMAKE_SOURCE_DIR}/src/app/
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
      ${CMAKE_SOURCE_DIR}/incl/
      ${CMAKE_SOURCE_DIR}/src/
      ${CMAKE_SOURCE_DIR}/src/lib/imgui
      ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
      ${SDL2_INCLUDE_DIR}
      ${PROJECT_BINARY_DIR}
    )

    target_link_libraries(
      yaze_core INTERFACE
      yaze_util
      yaze_gfx
      yaze_gui
      yaze_zelda3
      yaze_core_lib
      yaze_editor
      ImGui
    )

    if(TARGET yaze_agent)
      target_link_libraries(yaze_core INTERFACE yaze_agent)
    endif()

    if(YAZE_BUILD_EMU AND TARGET yaze_emulator)
      target_link_libraries(yaze_core INTERFACE yaze_emulator)
    endif()

    if(YAZE_ENABLE_UI_TESTS AND TARGET ImGuiTestEngine)
      target_link_libraries(yaze_core INTERFACE ImGuiTestEngine)
    endif()

    if(YAZE_WITH_GRPC)
      target_link_libraries(yaze_core INTERFACE
        grpc++
        grpc++_reflection
        libprotobuf)
    endif()
  else()
    # Create core library for testing (includes editor and zelda3 components needed by tests)
    set(YAZE_CORE_SOURCES
      app/rom.cc
      ${YAZE_APP_CORE_SRC}
      ${YAZE_APP_GFX_SRC} 
      ${YAZE_APP_EDITOR_SRC}
      ${YAZE_APP_ZELDA3_SRC}
      ${YAZE_APP_EMU_SRC}
      ${YAZE_GUI_SRC}
      ${YAZE_UTIL_SRC}
      # cli/service/gui_automation_client.cc # Moved to yaze_c
      cli/service/testing/test_workflow_generator.cc
    )

    add_library(yaze_core STATIC ${YAZE_CORE_SOURCES})

    target_include_directories(
      yaze_core PUBLIC
      ${CMAKE_SOURCE_DIR}/src/lib/
      ${CMAKE_SOURCE_DIR}/src/app/
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
      ${CMAKE_SOURCE_DIR}/incl/
      ${CMAKE_SOURCE_DIR}/src/
      ${CMAKE_SOURCE_DIR}/src/lib/imgui
      ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
      ${SDL2_INCLUDE_DIR}
      ${PROJECT_BINARY_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(
      yaze_core PUBLIC
      asar-static
      yaze_agent
      ${ABSL_TARGETS}
      ${SDL_TARGETS}
      ${CMAKE_DL_LIBS}
      ImGui
    )

    if(YAZE_WITH_GRPC)
      target_add_protobuf(yaze_core 
        ${CMAKE_SOURCE_DIR}/src/app/core/proto/imgui_test_harness.proto)

      target_link_libraries(yaze_core PRIVATE
        grpc++
        grpc++_reflection
        libprotobuf)
    endif()
  endif()

  # Create the full C API library (static for CI, shared for release)
  if(YAZE_MINIMAL_BUILD)
    add_library(yaze_c STATIC ${YAZE_C_SOURCES})
  else()
    add_library(yaze_c SHARED ${YAZE_C_SOURCES})
  endif()

  target_include_directories(
    yaze_c PUBLIC
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )

  if(PNG_FOUND)
    target_include_directories(yaze_c PUBLIC ${PNG_INCLUDE_DIRS})
    if(NOT YAZE_USE_MODULAR_BUILD)
      target_include_directories(yaze_core PUBLIC ${PNG_INCLUDE_DIRS})
    endif()
  endif()

  if(YAZE_USE_MODULAR_BUILD)
    target_link_libraries(
      yaze_c PRIVATE
      yaze_util
      yaze_gfx
      yaze_gui
      yaze_zelda3
      yaze_core_lib
      yaze_editor
      ImGui
    )

    if(TARGET yaze_agent)
      target_link_libraries(yaze_c PRIVATE yaze_agent)
    endif()

    if(YAZE_BUILD_EMU AND TARGET yaze_emulator)
      target_link_libraries(yaze_c PRIVATE yaze_emulator)
    endif()
  else()
    target_link_libraries(
      yaze_c PRIVATE
      yaze_core
      ImGui
    )
  endif()

  if(YAZE_WITH_GRPC)
    target_add_protobuf(yaze_c 
      ${CMAKE_SOURCE_DIR}/src/app/core/proto/imgui_test_harness.proto)

    target_link_libraries(yaze_c PRIVATE
      grpc++
      grpc++_reflection
      libprotobuf)
  endif()

  if(YAZE_ENABLE_UI_TESTS AND TARGET ImGuiTestEngine)
    target_link_libraries(yaze_c PRIVATE ImGuiTestEngine)
    target_compile_definitions(yaze_c PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=1)
  else()
    target_compile_definitions(yaze_c PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
  endif()

  if(PNG_FOUND)
    target_link_libraries(yaze_c PRIVATE ${PNG_LIBRARIES})
    if(NOT YAZE_USE_MODULAR_BUILD)
      target_link_libraries(yaze_core PRIVATE ${PNG_LIBRARIES})
    endif()
  endif()

  if (YAZE_INSTALL_LIB)
    install(TARGETS yaze_c
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static)

    install(
      FILES
        incl/yaze.h
        incl/zelda.h
      DESTINATION
        include
    )
  endif()
endif()

# =============================================================================
# Visual Studio Source Groups Organization
# =============================================================================
# These source groups will organize files in Visual Studio Solution Explorer
# into logical, hierarchical folders for better navigation and development.

# Core Application Structure
source_group("Application\\Core" FILES
  app/main.cc
  app/rom.cc
  app/rom.h
  app/snes.h
  app/transaction.h
  yaze.cc
)

# App Core Components
source_group("Application\\Core\\Controller" FILES
  app/core/controller.cc
  app/core/controller.h
  app/core/window.cc
  app/core/window.h
)

source_group("Application\\Core\\Project" FILES
  app/core/project.cc
  app/core/project.h
  app/core/features.h
)

source_group("Application\\Core\\Asar" FILES
  app/core/asar_wrapper.cc
  app/core/asar_wrapper.h
)

# Platform-specific files
source_group("Application\\Core\\Platform" FILES
  app/core/platform/app_delegate.h
  app/core/platform/app_delegate.mm
  app/core/platform/clipboard.cc
  app/core/platform/clipboard.h
  app/core/platform/clipboard.mm
  app/core/platform/file_dialog.cc
  app/core/platform/file_dialog.h
  app/core/platform/file_dialog.mm
  app/core/platform/font_loader.cc
  app/core/platform/font_loader.h
  app/core/platform/font_loader.mm
  app/core/platform/view_controller.h
)

# Editor System
source_group("Application\\Editor" FILES
  app/editor/editor_manager.cc
  app/editor/editor_manager.h
  app/editor/editor.h
  app/editor/editor_safeguards.h
)

# Code Editor
source_group("Application\\Editor\\Code" FILES
  app/editor/code/assembly_editor.cc
  app/editor/code/assembly_editor.h
  app/editor/code/memory_editor.h
)

# Dungeon Editor
source_group("Application\\Editor\\Dungeon" FILES
  app/editor/dungeon/dungeon_editor.cc
  app/editor/dungeon/dungeon_editor.h
  app/editor/dungeon/dungeon_map_editor.cc
  app/editor/dungeon/dungeon_map_editor.h
  app/editor/dungeon/dungeon_room_editor.cc
  app/editor/dungeon/dungeon_room_editor.h
  app/editor/dungeon/dungeon_sprite_editor.cc
  app/editor/dungeon/dungeon_sprite_editor.h
  app/editor/dungeon/dungeon_tile_editor.cc
  app/editor/dungeon/dungeon_tile_editor.h
  app/editor/dungeon/room_data_editor.cc
  app/editor/dungeon/room_data_editor.h
  app/editor/dungeon/room_properties_editor.cc
  app/editor/dungeon/room_properties_editor.h
  app/editor/dungeon/room_sprite_editor.cc
  app/editor/dungeon/room_sprite_editor.h
  app/editor/dungeon/room_tile_editor.cc
  app/editor/dungeon/room_tile_editor.h
)

# Graphics Editor
source_group("Application\\Editor\\Graphics" FILES
  app/editor/graphics/graphics_editor.cc
  app/editor/graphics/graphics_editor.h
  app/editor/graphics/palette_editor.cc
  app/editor/graphics/palette_editor.h
  app/editor/graphics/sprite_editor.cc
  app/editor/graphics/sprite_editor.h
  app/editor/graphics/tile_editor.cc
  app/editor/graphics/tile_editor.h
)

# Message Editor
source_group("Application\\Editor\\Message" FILES
  app/editor/message/message_editor.cc
  app/editor/message/message_editor.h
  app/editor/message/text_editor.cc
  app/editor/message/text_editor.h
  app/editor/message/translation_editor.cc
  app/editor/message/translation_editor.h
)

# Music Editor
source_group("Application\\Editor\\Music" FILES
  app/editor/music/music_editor.cc
  app/editor/music/music_editor.h
)

# Overworld Editor
source_group("Application\\Editor\\Overworld" FILES
  app/editor/overworld/overworld_editor.cc
  app/editor/overworld/overworld_editor.h
  app/editor/overworld/overworld_map_editor.cc
  app/editor/overworld/overworld_map_editor.h
  app/editor/overworld/overworld_sprite_editor.cc
  app/editor/overworld/overworld_sprite_editor.h
  app/editor/overworld/overworld_tile_editor.cc
  app/editor/overworld/overworld_tile_editor.h
  app/editor/overworld/overworld_transport_editor.cc
  app/editor/overworld/overworld_transport_editor.h
  app/editor/overworld/overworld_entrance_editor.cc
  app/editor/overworld/overworld_entrance_editor.h
)

# Sprite Editor
source_group("Application\\Editor\\Sprite" FILES
  app/editor/sprite/sprite_editor.cc
  app/editor/sprite/sprite_editor.h
  app/editor/sprite/sprite_properties_editor.cc
  app/editor/sprite/sprite_properties_editor.h
)

# System Editor
source_group("Application\\Editor\\System" FILES
  app/editor/system/asm_editor.cc
  app/editor/system/asm_editor.h
  app/editor/system/config_editor.cc
  app/editor/system/config_editor.h
  app/editor/system/debug_console.cc
  app/editor/system/debug_console.h
  app/editor/system/hex_editor.cc
  app/editor/system/hex_editor.h
  app/editor/system/log_viewer.cc
  app/editor/system/log_viewer.h
  app/editor/system/memory_editor.cc
  app/editor/system/memory_editor.h
  app/editor/system/proposal_drawer.cc
  app/editor/system/proposal_drawer.h
  app/editor/system/rom_analyzer.cc
  app/editor/system/rom_analyzer.h
)

# Emulator
source_group("Application\\Emulator" FILES
  app/emu/emu.cc
  app/emu/emulator.cc
  app/emu/emulator.h
  app/emu/snes.cc
  app/emu/snes.h
)

# Audio System
source_group("Application\\Emulator\\Audio" FILES
  app/emu/audio/apu.cc
  app/emu/audio/apu.h
  app/emu/audio/spc700.cc
  app/emu/audio/spc700.h
  app/emu/audio/dsp.cc
  app/emu/audio/dsp.h
  app/emu/audio/internal/addressing.cc
  app/emu/audio/internal/addressing.h
  app/emu/audio/internal/instructions.cc
  app/emu/audio/internal/instructions.h
)

# CPU System
source_group("Application\\Emulator\\CPU" FILES
  app/emu/cpu/cpu.cc
  app/emu/cpu/cpu.h
  app/emu/cpu/internal/addressing.cc
  app/emu/cpu/internal/addressing.h
  app/emu/cpu/internal/instructions.cc
  app/emu/cpu/internal/instructions.h
)

# Memory System
source_group("Application\\Emulator\\Memory" FILES
  app/emu/memory/dma.cc
  app/emu/memory/dma.h
  app/emu/memory/memory.cc
  app/emu/memory/memory.h
  app/emu/memory/mock_memory.h
)

# Video System
source_group("Application\\Emulator\\Video" FILES
  app/emu/video/ppu.cc
  app/emu/video/ppu.h
  app/emu/video/ppu_registers.h
)

# Graphics System
source_group("Application\\Graphics" FILES
  app/gfx/arena.cc
  app/gfx/arena.h
  app/gfx/background_buffer.cc
  app/gfx/background_buffer.h
  app/gfx/bitmap.cc
  app/gfx/bitmap.h
  app/gfx/compression.cc
  app/gfx/compression.h
  app/gfx/performance_profiler.cc
  app/gfx/performance_profiler.h
  app/gfx/scad_format.cc
  app/gfx/scad_format.h
  app/gfx/snes_color.cc
  app/gfx/snes_color.h
  app/gfx/snes_palette.cc
  app/gfx/snes_palette.h
  app/gfx/snes_tile.cc
  app/gfx/snes_tile.h
  app/gfx/tilemap.cc
  app/gfx/tilemap.h
)

# GUI System
source_group("Application\\GUI" FILES
  app/gui/background_renderer.cc
  app/gui/background_renderer.h
  app/gui/canvas_utils.cc
  app/gui/canvas_utils.h
  app/gui/canvas.cc
  app/gui/canvas.h
  app/gui/color.cc
  app/gui/color.h
  app/gui/enhanced_palette_editor.cc
  app/gui/enhanced_palette_editor.h
  app/gui/icons.h
  app/gui/input.cc
  app/gui/input.h
  app/gui/style.cc
  app/gui/style.h
  app/gui/theme_manager.cc
  app/gui/theme_manager.h
  app/gui/zeml.cc
  app/gui/zeml.h
)

# GUI Modules
source_group("Application\\GUI\\Modules" FILES
  app/gui/modules/about_dialog.cc
  app/gui/modules/about_dialog.h
  app/gui/modules/preferences_dialog.cc
  app/gui/modules/preferences_dialog.h
  app/gui/modules/project_dialog.cc
  app/gui/modules/project_dialog.h
)

# Zelda3 Specific
source_group("Application\\Zelda3" FILES
  app/zelda3/common.h
  app/zelda3/hyrule_magic.cc
  app/zelda3/hyrule_magic.h
)

# Zelda3 Dungeon
source_group("Application\\Zelda3\\Dungeon" FILES
  app/zelda3/dungeon/dungeon_data.cc
  app/zelda3/dungeon/dungeon_data.h
  app/zelda3/dungeon/dungeon_loader.cc
  app/zelda3/dungeon/dungeon_loader.h
  app/zelda3/dungeon/dungeon_room.cc
  app/zelda3/dungeon/dungeon_room.h
  app/zelda3/dungeon/dungeon_sprite.cc
  app/zelda3/dungeon/dungeon_sprite.h
  app/zelda3/dungeon/dungeon_tile.cc
  app/zelda3/dungeon/dungeon_tile.h
  app/zelda3/dungeon/room_data.cc
  app/zelda3/dungeon/room_data.h
  app/zelda3/dungeon/room_properties.cc
  app/zelda3/dungeon/room_properties.h
  app/zelda3/dungeon/room_sprite.cc
  app/zelda3/dungeon/room_sprite.h
  app/zelda3/dungeon/room_tile.cc
  app/zelda3/dungeon/room_tile.h
)

# Zelda3 Music
source_group("Application\\Zelda3\\Music" FILES
  app/zelda3/music/music_data.cc
  app/zelda3/music/music_data.h
)

# Zelda3 Overworld
source_group("Application\\Zelda3\\Overworld" FILES
  app/zelda3/overworld/overworld_data.cc
  app/zelda3/overworld/overworld_data.h
  app/zelda3/overworld/overworld_loader.cc
  app/zelda3/overworld/overworld_loader.h
  app/zelda3/overworld/overworld_sprite.cc
  app/zelda3/overworld/overworld_sprite.h
  app/zelda3/overworld/overworld_tile.cc
  app/zelda3/overworld/overworld_tile.h
  app/zelda3/overworld/overworld_transport.cc
  app/zelda3/overworld/overworld_transport.h
)

# Zelda3 Screen
source_group("Application\\Zelda3\\Screen" FILES
  app/zelda3/screen/dungeon_map.cc
  app/zelda3/screen/dungeon_map.h
  app/zelda3/screen/inventory.cc
  app/zelda3/screen/inventory.h
  app/zelda3/screen/title_screen.cc
  app/zelda3/screen/title_screen.h
)

# Zelda3 Sprite
source_group("Application\\Zelda3\\Sprite" FILES
  app/zelda3/sprite/sprite_data.cc
  app/zelda3/sprite/sprite_data.h
  app/zelda3/sprite/sprite_loader.cc
  app/zelda3/sprite/sprite_loader.h
  app/zelda3/sprite/sprite.cc
  app/zelda3/sprite/sprite.h
)

# Testing
source_group("Application\\Testing" FILES
  app/test/test_manager.cc
  app/test/test_manager.h
  app/test/e2e_test_suite.h
  app/test/integrated_test_suite.h
  app/test/rom_dependent_test_suite.h
  app/test/unit_test_suite.h
  app/test/zscustomoverworld_test_suite.h
)

# CLI Tools
source_group("CLI" FILES
  cli/cli_main.cc
  cli/tui.cc
  cli/tui.h
  cli/z3ed.cc
  cli/z3ed.h
)

source_group("CLI\\Handlers" FILES
  cli/handlers/compress.cc
  cli/handlers/patch.cc
  cli/handlers/tile16_transfer.cc
)

# Utilities
source_group("Utilities" FILES
  util/bps.cc
  util/bps.h
  util/flag.cc
  util/flag.h
  util/hex.cc
  util/hex.h
  util/log.h
  util/macro.h
  util/notify.h
)

# API
source_group("API" FILES
  api/service_handler.cc
  api/yaze.proto
)

source_group("API\\Python" FILES
  api/python/yaze_py.cc
)

# Platform-specific Resources
source_group("Platform\\Windows" FILES
  win32/yaze.ico
  win32/yaze.rc
  win32/yaze.res
)

source_group("Platform\\iOS" FILES
  ios/main.mm
  ios/iOS/Info-iOS.plist
  ios/iOS/LaunchScreen.storyboard
  ios/macOS/Info-macOS.plist
  ios/macOS/MainMenu.storyboard
)

source_group("Platform\\iOS\\Assets" FILES
  ios/Media.xcassets/Contents.json
)

# Configuration
source_group("Configuration" FILES
  yaze_config.h.in
)

# Assets
source_group("Assets\\Fonts" FILES
  ${CMAKE_SOURCE_DIR}/assets/font/Karla-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Cousine-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/DroidSans.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/NotoSansJP.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/IBMPlexSansJP-Bold.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/MaterialIcons-Regular.ttf
)

source_group("Assets\\Themes" FILES
  ${YAZE_THEME_FILES}
)

source_group("Assets\\Layouts" FILES
  ${CMAKE_SOURCE_DIR}/assets/layouts/ow_toolset.zeml
)

source_group("Assets\\Library" FILES
  ${CMAKE_SOURCE_DIR}/assets/lib/libasar.dll
)
