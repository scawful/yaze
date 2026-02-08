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
  app/emu/debug/disassembler.cc
  app/emu/debug/disassembly_viewer.cc
  app/emu/debug/semantic_introspection.cc
  app/emu/debug/step_controller.cc
  app/emu/debug/symbol_provider.cc
  app/emu/debug/watchpoint_manager.cc
  app/emu/emulator.cc
  app/emu/input/input_backend.cc
  app/emu/input/input_manager.cc
  app/emu/memory/dma.cc
  app/emu/memory/memory.cc
  app/emu/snes.cc
  app/emu/video/ppu.cc
  app/emu/render/render_context.cc
  app/emu/render/emulator_render_service.cc
  app/emu/render/save_state_manager.cc
  app/emu/mesen/mesen_client_registry.cc
  app/emu/mesen/mesen_socket_client.cc
)

set(
  YAZE_EMU_GUI_SRC
  app/emu/ui/debugger_ui.cc
  app/emu/ui/emulator_ui.cc
  app/emu/ui/input_handler.cc
)

# Add SDL3-specific backends when SDL3 is enabled
if(YAZE_USE_SDL3)
  list(APPEND YAZE_APP_EMU_SRC app/emu/input/sdl3_input_backend.cc)
  list(APPEND YAZE_APP_EMU_SRC app/emu/audio/sdl3_audio_backend.cc)
endif()

# Add WASM-specific backends when building for Emscripten
if(EMSCRIPTEN)
  list(APPEND YAZE_APP_EMU_SRC app/emu/platform/wasm/wasm_audio.cc)
endif()

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

# Add foundational core library (project management, asar wrapper)
add_subdirectory(core)

# Add ROM library (generic SNES ROM handling)
add_subdirectory(rom)

# Add Zelda3-specific game logic (depends on yaze_rom)
include(zelda3/zelda3_library.cmake)

# App-specific libraries
include(app/gfx/gfx_library.cmake)

# Include net library for all builds (has WASM support now)
include(app/net/net_library.cmake)

include(app/gui/gui_library.cmake)
# NOTE: app/core/core_library.cmake merged into app/app.cmake

# Define core application library early (needed by agent)
include(app/app_core.cmake)

if(YAZE_BUILD_GUI OR YAZE_BUILD_Z3ED OR YAZE_BUILD_TESTS OR YAZE_BUILD_CLI)
  include(cli/cli_core.cmake)
endif()

# Include agent before test support (test support depends on agent)
if(YAZE_BUILD_GUI OR YAZE_BUILD_Z3ED OR YAZE_BUILD_TESTS)
  include(cli/agent.cmake)
endif()

# Include test support library BEFORE yaze_editor so it can link against it
# (yaze_editor needs TestManager for editor features)
# Test executables are only built when YAZE_BUILD_TESTS=ON (handled in test/CMakeLists.txt)
if(YAZE_BUILD_TESTS OR NOT YAZE_MINIMAL_BUILD)
  include(app/test/test.cmake)
endif()

if(YAZE_ENABLE_AGENT_CLI AND (YAZE_BUILD_CLI OR YAZE_BUILD_Z3ED))
  include(cli/cli_agent.cmake)
endif()

# Editor and emulator (depend on test support when tests are enabled)
include(app/editor/editor_library.cmake)
include(app/emu/emu_library.cmake)

# Include gRPC support library (consolidates all protobuf/gRPC usage)
# Needs yaze_emulator target, so include after emu_library.cmake.
if(YAZE_ENABLE_GRPC)
  include(app/service/grpc_support.cmake)
endif()

# Build standalone emulator
if(YAZE_BUILD_EMU)
  include(app/emu/emu.cmake)
endif()

# Build the GUI executable (yaze) after editor/emulator/agent targets exist.
# Note: yaze_app_core_lib is defined by app/app_core.cmake (included earlier).
if(YAZE_BUILD_GUI)
  include(app/app.cmake)
endif()

# Build lab sandbox target
if(YAZE_BUILD_LAB)
  include(lab/lab.cmake)
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
    yaze_rom
    yaze_gfx
    yaze_gui
    yaze_zelda3
    yaze_core_lib
    yaze_editor
    yaze_emulator
    ImGui
  )
endif()

if(YAZE_PLATFORM_IOS)
  if(NOT DEFINED YAZE_IOS_SDK)
    set(YAZE_IOS_SDK "iphoneos" CACHE STRING "iOS SDK name for xcrun/libtool")
  endif()

  set(YAZE_IOS_BUNDLE_OUTPUT "${CMAKE_BINARY_DIR}/ios/libyaze_ios_bundle.a")
  set(YAZE_IOS_BUNDLE_TARGETS
    yaze_app_core_lib
    yaze_editor
    yaze_editor_system_panels
    yaze_editor_system_session
    yaze_editor_system_shortcuts
    yaze_emulator
    yaze_emulator_ui
    yaze_gui_core
    yaze_canvas
    yaze_gui_widgets
    yaze_gui_automation
    yaze_gui_app
    yaze_net
    yaze_gfx_types
    yaze_gfx_backend
    yaze_gfx_resource
    yaze_gfx_render
    yaze_gfx_debug
    yaze_gfx_core
    yaze_gfx_util
    yaze_zelda3
    yaze_rom
    yaze_core_lib
    yaze_util
    ImGui
  )

  if(TARGET ImPlot)
    list(APPEND YAZE_IOS_BUNDLE_TARGETS ImPlot)
  endif()

  if(TARGET yaze_test_support)
    list(APPEND YAZE_IOS_BUNDLE_TARGETS yaze_test_support)
  endif()

  if(TARGET asar-static)
    list(APPEND YAZE_IOS_BUNDLE_TARGETS asar-static)
  endif()

  if(TARGET SDL2-static)
    list(APPEND YAZE_IOS_BUNDLE_TARGETS SDL2-static)
  elseif(TARGET SDL2::SDL2-static)
    list(APPEND YAZE_IOS_BUNDLE_TARGETS SDL2::SDL2-static)
  elseif(TARGET SDL2::SDL2)
    list(APPEND YAZE_IOS_BUNDLE_TARGETS SDL2::SDL2)
  endif()

  list(REMOVE_DUPLICATES YAZE_IOS_BUNDLE_TARGETS)

  set(YAZE_IOS_BUNDLE_INPUTS "")
  foreach(target IN LISTS YAZE_IOS_BUNDLE_TARGETS)
    if(TARGET ${target})
      get_target_property(_target_type ${target} TYPE)
      if(NOT _target_type STREQUAL "INTERFACE_LIBRARY")
        list(APPEND YAZE_IOS_BUNDLE_INPUTS "$<TARGET_FILE:${target}>")
      endif()
    endif()
  endforeach()

  list(REMOVE_DUPLICATES YAZE_IOS_BUNDLE_INPUTS)

  if(YAZE_IOS_BUNDLE_INPUTS)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ios")

    # Generate a base filelist from known CMake targets. We then append Abseil
    # static libs at build time so the final bundle is self-contained.
    set(YAZE_IOS_BUNDLE_FILELIST_BASE "${CMAKE_BINARY_DIR}/ios/bundle_inputs_base.txt")
    set(YAZE_IOS_BUNDLE_FILELIST "${CMAKE_BINARY_DIR}/ios/bundle_inputs.txt")
    set(YAZE_IOS_BUNDLE_GEN_SCRIPT "${CMAKE_BINARY_DIR}/ios/gen_ios_bundle_filelist.cmake")

    file(GENERATE
      OUTPUT ${YAZE_IOS_BUNDLE_FILELIST_BASE}
      CONTENT "$<JOIN:${YAZE_IOS_BUNDLE_INPUTS},\n>"
    )

    file(GENERATE
      OUTPUT ${YAZE_IOS_BUNDLE_GEN_SCRIPT}
      CONTENT [=[
if(NOT DEFINED BASE_FILELIST OR NOT DEFINED OUTPUT_FILELIST OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "Missing BASE_FILELIST/OUTPUT_FILELIST/BINARY_DIR")
endif()

file(READ "${BASE_FILELIST}" base)
string(REPLACE "\r\n" "\n" base "${base}")
string(REPLACE "\r" "\n" base "${base}")
string(REPLACE "\n" ";" inputs "${base}")
list(FILTER inputs EXCLUDE REGEX "^$")

# Abseil is a transitive dependency that doesn't appear as a direct CMake target
# in our bundle target list. Include it so the final archive links cleanly.
file(GLOB_RECURSE absl_libs
  "${BINARY_DIR}/_deps/absl-build/absl/*/libabsl_*.a"
  "${BINARY_DIR}/_deps/abseil-cpp-build/absl/*/libabsl_*.a"
)
list(REMOVE_DUPLICATES absl_libs)
list(APPEND inputs ${absl_libs})
list(REMOVE_DUPLICATES inputs)

list(LENGTH inputs inputs_len)
list(LENGTH absl_libs absl_len)
string(REPLACE ";" "\n" out "${inputs}")
file(WRITE "${OUTPUT_FILELIST}" "${out}\n")
message(STATUS "iOS bundle inputs: ${inputs_len} (absl: ${absl_len})")
]=]
    )

    add_custom_command(
      OUTPUT ${YAZE_IOS_BUNDLE_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/ios
      COMMAND ${CMAKE_COMMAND}
        -DBASE_FILELIST=${YAZE_IOS_BUNDLE_FILELIST_BASE}
        -DOUTPUT_FILELIST=${YAZE_IOS_BUNDLE_FILELIST}
        -DBINARY_DIR=${CMAKE_BINARY_DIR}
        -P ${YAZE_IOS_BUNDLE_GEN_SCRIPT}
      COMMAND xcrun --sdk ${YAZE_IOS_SDK} libtool -static -o ${YAZE_IOS_BUNDLE_OUTPUT} -filelist ${YAZE_IOS_BUNDLE_FILELIST}
      DEPENDS ${YAZE_IOS_BUNDLE_TARGETS}
      COMMAND_EXPAND_LISTS
      COMMENT "Bundling yaze iOS static libraries"
    )
    add_custom_target(yaze_ios_bundle ALL DEPENDS ${YAZE_IOS_BUNDLE_OUTPUT})
  endif()
endif()
