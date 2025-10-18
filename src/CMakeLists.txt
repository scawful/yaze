# This file orchestrates the build of the yaze application and its libraries.
# It includes component-specific .cmake files to keep the build system modular.

# Define emulator source files
# This list is auto-maintained by scripts/build_cleaner.py
set(
  YAZE_APP_EMU_SRC
  app/emu/audio/apu.cc
  app/emu/audio/audio_backend.cc
  app/emu/audio/dsp.cc
  app/emu/audio/internal/addressing.cc
  app/emu/audio/internal/instructions.cc
  app/emu/audio/spc700.cc
  app/emu/cpu/cpu.cc
  app/emu/cpu/internal/addressing.cc
  app/emu/cpu/internal/instructions.cc
  app/emu/debug/apu_debugger.cc
  app/emu/debug/breakpoint_manager.cc
  app/emu/debug/disassembly_viewer.cc
  app/emu/debug/watchpoint_manager.cc
  app/emu/emu.cc
  app/emu/emulator.cc
  app/emu/input/input_backend.cc
  app/emu/input/input_manager.cc
  app/emu/memory/dma.cc
  app/emu/memory/memory.cc
  app/emu/snes.cc
  app/emu/ui/debugger_ui.cc
  app/emu/ui/emulator_ui.cc
  app/emu/ui/input_handler.cc
  app/emu/video/ppu.cc
)

# Define resource files for bundling
set(YAZE_RESOURCE_FILES
  ${CMAKE_SOURCE_DIR}/assets/font/Karla-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Cousine-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/DroidSans.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/NotoSansJP.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/IBMPlexSansJP-Bold.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/MaterialIcons-Regular.ttf
)
file(GLOB YAZE_THEME_FILES "${CMAKE_SOURCE_DIR}/assets/themes/*.theme")
list(APPEND YAZE_RESOURCE_FILES ${YAZE_THEME_FILES})

# Configure assets for different platforms
foreach (FILE ${YAZE_RESOURCE_FILES})
  file(RELATIVE_PATH NEW_FILE "${CMAKE_SOURCE_DIR}/assets" ${FILE})
  get_filename_component(NEW_FILE_PATH ${NEW_FILE} DIRECTORY)
  
  if (APPLE)
    set_source_files_properties(${FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${NEW_FILE_PATH}")
  else()
    set_source_files_properties(${FILE} PROPERTIES VS_DEPLOYMENT_CONTENT 1 VS_DEPLOYMENT_LOCATION "assets/${NEW_FILE_PATH}")
  endif()
endforeach()

# Include modular libraries
include(util/util.cmake)
include(zelda3/zelda3_library.cmake)

# Add foundational core library (project management, asar wrapper)
add_subdirectory(core)

# App-specific libraries
include(app/gfx/gfx_library.cmake)
include(app/net/net_library.cmake)
include(app/gui/gui_library.cmake)
# NOTE: app/core/core_library.cmake merged into app/app.cmake

# Include test support library BEFORE yaze_editor so it can link against it
# (yaze_editor needs TestManager for editor features)
# Test executables are only built when YAZE_BUILD_TESTS=ON (handled in test/CMakeLists.txt)
if(YAZE_BUILD_TESTS OR NOT YAZE_MINIMAL_BUILD)
  include(app/test/test.cmake)
endif()

# Include gRPC support library (consolidates all protobuf/gRPC usage)
if(YAZE_WITH_GRPC)
  include(app/service/grpc_support.cmake)
endif()

# Include agent/CLI components (needed by yaze_editor for agent features)
if(YAZE_BUILD_APP OR YAZE_BUILD_Z3ED OR YAZE_BUILD_TESTS)
  include(cli/agent.cmake)
endif()

# Editor and emulator (depend on test support when tests are enabled)
include(app/editor/editor_library.cmake)
include(app/emu/emu_library.cmake)

# Build main application
if(YAZE_BUILD_APP)
  include(app/app.cmake)
endif()

# Build standalone emulator
if(YAZE_BUILD_EMU)
  include(app/emu/emu.cmake)
endif()

# Build z3ed CLI tool
if(YAZE_BUILD_Z3ED)
  include(cli/z3ed.cmake)
endif()

# Yaze Core Library (for testing and C API)
if (YAZE_BUILD_LIB)
  add_library(yaze_core INTERFACE)
  target_link_libraries(yaze_core INTERFACE
    yaze_util
    yaze_gfx
    yaze_gui
    yaze_zelda3
    yaze_core_lib
    yaze_editor
    yaze_emulator
    yaze_agent
    ImGui
  )
endif()
