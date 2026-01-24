# ==============================================================================
# Application Core Library and GUI Executable
# ==============================================================================
# This file builds:
# 1. yaze_app_core_lib (always, when included)
# 2. yaze executable (GUI application, only when YAZE_BUILD_GUI=ON)
# ==============================================================================

# Always create the application core library (needed by yaze_agent)
include(app/app_core.cmake)

# Only build GUI executable when explicitly requested
if(NOT YAZE_BUILD_GUI)
  return()
endif()

# ==============================================================================
# Yaze Application Executable
# ==============================================================================

# controller.cc is built here (not in yaze_app_core_lib) because it uses
# EditorManager, DockSpaceRenderer, and WidgetIdRegistry from yaze_editor/yaze_gui.
# Including it in yaze_app_core_lib would create a dependency cycle:
# yaze_agent -> yaze_app_core_lib -> yaze_editor -> yaze_agent
set(YAZE_APP_EXECUTABLE_SRC
  app/application.cc
  app/main.cc
  app/controller.cc
)

if(EMSCRIPTEN)
  list(APPEND YAZE_APP_EXECUTABLE_SRC web/debug/yaze_debug_inspector.cc)
endif()

if (YAZE_PLATFORM_MACOS)
  list(APPEND YAZE_APP_EXECUTABLE_SRC app/platform/app_delegate.mm)
  add_executable(yaze MACOSX_BUNDLE ${YAZE_APP_EXECUTABLE_SRC} ${YAZE_RESOURCE_FILES})

  set(ICON_FILE "${CMAKE_SOURCE_DIR}/assets/yaze.icns")
  target_sources(yaze PRIVATE ${ICON_FILE})
  set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  set_target_properties(yaze PROPERTIES
    MACOSX_BUNDLE_ICON_FILE "yaze.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "Yaze"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.scawful.yaze"
    MACOSX_BUNDLE_INFO_STRING "Yet Another Zelda3 Editor"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  )
elseif(EMSCRIPTEN)
  add_executable(yaze ${YAZE_APP_EXECUTABLE_SRC})
  # Set suffix to .html so Emscripten generates HTML output with shell template
  set_target_properties(yaze PROPERTIES SUFFIX ".html")
else()
  add_executable(yaze ${YAZE_APP_EXECUTABLE_SRC})
  if(WIN32 OR UNIX)
    target_sources(yaze PRIVATE ${YAZE_RESOURCE_FILES})
  endif()
endif()

target_include_directories(yaze PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/inc
  ${PROJECT_BINARY_DIR}
)

target_sources(yaze PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h PROPERTIES GENERATED TRUE)

# Add SDL version compile definitions
if(YAZE_USE_SDL3)
  target_compile_definitions(yaze PRIVATE YAZE_SDL3=1)
else()
  target_compile_definitions(yaze PRIVATE YAZE_SDL2=1)
endif()

# Link modular libraries
target_link_libraries(yaze PRIVATE
  yaze_editor
  yaze_emulator
  yaze_emulator_ui
  yaze_agent
  absl::failure_signal_handler
  absl::flags
  absl::flags_parse
)

# Force-load yaze_editor to ensure static REGISTER_PANEL objects are included.
# Without this, the linker strips panel object files since nothing references them.
if(APPLE)
  target_link_options(yaze PRIVATE
    "-Wl,-force_load,$<TARGET_LINKER_FILE:yaze_editor>"
  )
elseif(UNIX)
  target_link_options(yaze PRIVATE
    "-Wl,--whole-archive"
    "$<TARGET_LINKER_FILE:yaze_editor>"
    "-Wl,--no-whole-archive"
  )
elseif(MSVC)
  target_link_options(yaze PRIVATE
    "/WHOLEARCHIVE:$<TARGET_LINKER_FILE:yaze_editor>"
  )
endif()
# gRPC/protobuf linking is now handled by yaze_grpc_support library
if(YAZE_ENABLE_REMOTE_AUTOMATION)
  if(TARGET yaze_grpc_support)
    target_link_libraries(yaze PRIVATE yaze_grpc_support)
    message(STATUS "✓ yaze executable linked to yaze_grpc_support")
  else()
    message(FATAL_ERROR "YAZE_ENABLE_REMOTE_AUTOMATION=ON but yaze_grpc_support target missing")
  endif()
endif()

# Link test support library (yaze_editor needs TestManager)
if(TARGET yaze_test_support)
  target_link_libraries(yaze PRIVATE yaze_test_support)
  message(STATUS "✓ yaze executable linked to yaze_test_support")
else()
  message(WARNING "yaze needs yaze_test_support but TARGET not found")
endif()

# Platform-specific settings
if(WIN32)
  if(MSVC)
    target_link_options(yaze PRIVATE /STACK:8388608 /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  elseif(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(yaze PRIVATE -Wl,--stack,8388608 -Wl,--subsystem,windows -Wl,-emain)
  endif()
endif()

if(YAZE_PLATFORM_MACOS)
  target_link_libraries(yaze PUBLIC
    "-framework Cocoa"
    "-framework Security"
    "-framework CoreFoundation"
  )
endif()

# Emscripten-specific linker flags for yaze (not yaze_emu)
if(EMSCRIPTEN)
  # Export functions for web interface (only in yaze, not yaze_emu)
  # Use set_target_properties with LINK_FLAGS (similar to z3ed.cmake)
  # Note: Functions marked with EMSCRIPTEN_KEEPALIVE must also be listed here
  # MODULARIZE=1 allows async initialization via createYazeModule()
  #
  # Memory configuration for optimal WASM performance:
  # - INITIAL_MEMORY: Start with 256MB to reduce heap resizing during load
  # - ALLOW_MEMORY_GROWTH: Allow heap to grow beyond initial size
  # - STACK_SIZE: 8MB stack for recursive operations (overworld loading, etc.)
  # - MAXIMUM_MEMORY: Cap at 1GB to prevent runaway allocations
  # Create pre-js file for asyncifyStubs initialization (needed for workers)
  # Use CMAKE_CURRENT_BINARY_DIR which works in both local builds and CI
  # This file will be prepended to yaze.js, ensuring asyncifyStubs is available
  # in both the main thread and Web Workers before Emscripten's code runs
  set(ASYNCIFY_PRE_JS "${CMAKE_CURRENT_BINARY_DIR}/asyncify_pre.js")
  file(WRITE ${ASYNCIFY_PRE_JS}
    "// Auto-generated: Initialize asyncifyStubs for ASYNCIFY support\n"
    "// This is needed in both main thread and Web Workers\n"
    "// Emscripten's generated code with MODULARIZE=1 and ASYNCIFY accesses this during script initialization\n"
    "if (typeof asyncifyStubs === 'undefined') {\n"
    "  var asyncifyStubs = {};\n"
    "}\n"
    "if (typeof Module === 'undefined') {\n"
    "  var Module = {};\n"
    "}\n"
    "if (!Module.asyncifyStubs) {\n"
    "  Module.asyncifyStubs = asyncifyStubs;\n"
    "}\n"
  )
  
  # Append --pre-js to CMAKE_EXE_LINKER_FLAGS to ensure it's included even when presets set it
  # This ensures the pre-js file is prepended to the generated yaze.js
  # CMake will merge this with any flags set by presets (like wasm-ai)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --pre-js ${ASYNCIFY_PRE_JS}" CACHE STRING "Linker flags" FORCE)
  
  set_target_properties(yaze PROPERTIES
    LINK_FLAGS "--bind -s MODULARIZE=1 -s EXPORT_NAME='createYazeModule' -s INITIAL_MEMORY=268435456 -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1073741824 -s STACK_SIZE=16777216 -s USE_OFFSET_CONVERTER=1 -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"stringToUTF8\",\"UTF8ToString\",\"lengthBytesUTF8\",\"FS\",\"IDBFS\",\"allocateUTF8\",\"getValue\",\"setValue\",\"Asyncify\"]' -s EXPORTED_FUNCTIONS='[\"_main\",\"_SetFileSystemReady\",\"_SyncFilesystem\",\"_LoadRomFromWeb\",\"_yazeHandleDroppedFile\",\"_yazeHandleDropError\",\"_yazeHandleDragEnter\",\"_yazeHandleDragLeave\",\"_yazeEmergencySave\",\"_yazeRecoverSession\",\"_yazeHasRecoveryData\",\"_yazeClearRecoveryData\",\"_Z3edProcessCommand\",\"_Z3edIsReady\",\"_Z3edGetCompletions\",\"_Z3edSetApiKey\",\"_Z3edLoadRomData\",\"_Z3edGetRomInfo\",\"_Z3edQueryResource\",\"_OnTouchEvent\",\"_OnGestureEvent\",\"_malloc\",\"_free\",\"_emscripten_stack_get_base\",\"_emscripten_stack_get_end\"]' --shell-file ${CMAKE_SOURCE_DIR}/src/web/shell.html"
  )
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:yaze>
            $<TARGET_FILE_DIR:yaze>/index.html
    COMMENT "Adding index.html alias for local HTTP servers"
  )
endif()

# Post-build asset copying for non-macOS platforms
if(NOT APPLE)
  # Create assets directory first to avoid Windows copy_directory issues
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:yaze>/assets
    COMMENT "Creating assets directory"
  )
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/font $<TARGET_FILE_DIR:yaze>/assets/font
    COMMENT "Copying font assets"
  )
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/themes $<TARGET_FILE_DIR:yaze>/assets/themes
    COMMENT "Copying theme assets"
  )
  if(EXISTS ${CMAKE_SOURCE_DIR}/assets/agent)
    add_custom_command(TARGET yaze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:yaze>/assets/agent
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/agent $<TARGET_FILE_DIR:yaze>/assets/agent
      COMMENT "Copying agent assets"
    )
  endif()
endif()

# ==============================================================================
# Test Runner Executable
# ==============================================================================
if(YAZE_BUILD_TESTS)
  add_executable(yaze_test app/test/main_test.cc)
  target_link_libraries(yaze_test PRIVATE
    yaze_test_support
    yaze_app_core_lib
    absl::flags
    absl::flags_parse
  )
  target_include_directories(yaze_test PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/inc
    ${PROJECT_BINARY_DIR}
  )
endif()

