# ==============================================================================
# Application Core Library (Platform, Controller, ROM, Services)
# ==============================================================================
# This library contains application-level core components:
# - ROM management (app/rom.cc)
# - Application controller (app/controller.cc)
# - Window/platform management (app/platform/)
# - gRPC services for AI automation (app/service/)
#
# Dependencies: yaze_core_lib (foundational), yaze_util, yaze_gfx, SDL2, ImGui
# ==============================================================================

set(
  YAZE_APP_CORE_SRC
  # Note: controller.cc is built directly into the yaze executable (not this library)
  # because it depends on yaze_editor and yaze_gui, which would create a cycle:
  # yaze_agent -> yaze_app_core_lib -> yaze_editor -> yaze_agent
  app/platform/window.cc
  app/platform/window_backend_factory.cc
)

# Window backend: SDL2 or SDL3 (mutually exclusive)
if(YAZE_USE_SDL3)
  list(APPEND YAZE_APP_CORE_SRC
    app/platform/sdl3_window_backend.cc
  )
else()
  list(APPEND YAZE_APP_CORE_SRC
    app/platform/sdl2_window_backend.cc
  )
endif()

# Platform-specific sources
if (WIN32 OR MINGW OR (UNIX AND NOT APPLE AND NOT EMSCRIPTEN))
  list(APPEND YAZE_APP_CORE_SRC
    app/platform/font_loader.cc
    app/platform/asset_loader.cc
    app/platform/file_dialog_nfd.cc  # NFD file dialog for Windows/Linux
    # Stub implementation for WASM worker pool
    app/platform/wasm/wasm_worker_pool.cc
  )
endif()

if (EMSCRIPTEN)
  list(APPEND YAZE_APP_CORE_SRC
    app/platform/font_loader.cc
    app/platform/asset_loader.cc
    app/platform/file_dialog_web.cc
    app/platform/wasm/wasm_error_handler.cc
    # WASM File System Layer (Phase 1)
    app/platform/wasm/wasm_storage.cc
    app/platform/wasm/wasm_file_dialog.cc
    # WASM Drag & Drop ROM Loading
    app/platform/wasm/wasm_drop_handler.cc
    # WASM Loading Manager (Phase 3)
    app/platform/wasm/wasm_loading_manager.cc
    # WASM AI Service Integration (Phase 5)
    app/platform/wasm/wasm_browser_storage.cc
    # WASM Local Storage Persistence (Phase 6)
    app/platform/wasm/wasm_settings.cc
    app/platform/wasm/wasm_autosave.cc
    # WASM Web Workers for Heavy Processing (Phase 7)
    app/platform/wasm/wasm_worker_pool.cc
    # WASM Patch Export functionality (Phase 8)
    app/platform/wasm/wasm_patch_export.cc
    # WASM Centralized Configuration
    app/platform/wasm/wasm_config.cc
    # WASM Real-time Collaboration
    app/platform/wasm/wasm_collaboration.cc
    # WASM Message Queue for offline support
    app/platform/wasm/wasm_message_queue.cc
    # WASM Async Guard for Asyncify operation serialization
    app/platform/wasm/wasm_async_guard.cc
    # WASM Bootstrap (Platform Init)
    app/platform/wasm/wasm_bootstrap.cc
    # WASM Control API for editor/UI control from browser
    app/platform/wasm/wasm_control_api.cc
    # WASM Session Bridge for z3ed integration
    app/platform/wasm/wasm_session_bridge.cc
  )
endif()

if(APPLE)
    list(APPEND YAZE_APP_CORE_SRC
      app/platform/font_loader.cc
      app/platform/asset_loader.cc
      # Stub implementation for WASM worker pool
      app/platform/wasm/wasm_worker_pool.cc
    )

    set(YAZE_APPLE_OBJCXX_SRC
      app/platform/file_dialog.mm
      app/platform/font_loader.mm
    )

    add_library(yaze_app_objcxx OBJECT ${YAZE_APPLE_OBJCXX_SRC})
    set_target_properties(yaze_app_objcxx PROPERTIES
      OBJCXX_STANDARD 20
      OBJCXX_STANDARD_REQUIRED ON
    )

    target_include_directories(yaze_app_objcxx PUBLIC
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/src/app
      ${CMAKE_SOURCE_DIR}/ext
      ${CMAKE_SOURCE_DIR}/ext/imgui
      ${CMAKE_SOURCE_DIR}/incl
      ${PROJECT_BINARY_DIR}
    )

    if(YAZE_ENABLE_JSON)
      target_include_directories(yaze_app_objcxx PUBLIC
        ${CMAKE_SOURCE_DIR}/ext/json/include)
      target_compile_definitions(yaze_app_objcxx PUBLIC YAZE_WITH_JSON)
    endif()

    target_link_libraries(yaze_app_objcxx PUBLIC ${ABSL_TARGETS} yaze_util ${YAZE_SDL2_TARGETS})
    target_compile_definitions(yaze_app_objcxx PUBLIC MACOS)

    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa not found")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()

# Create the application core library
add_library(yaze_app_core_lib STATIC 
  ${YAZE_APP_CORE_SRC}
  $<$<BOOL:${APPLE}>:$<TARGET_OBJECTS:yaze_app_objcxx>>
)

target_precompile_headers(yaze_app_core_lib PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_app_core_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/ext
  ${CMAKE_SOURCE_DIR}/ext/imgui
  ${CMAKE_SOURCE_DIR}/ext/json/include
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

if(YAZE_ENABLE_JSON)
  target_compile_definitions(yaze_app_core_lib PUBLIC YAZE_WITH_JSON)
endif()

target_link_libraries(yaze_app_core_lib PUBLIC
  yaze_core_lib    # Foundational core library with project management
  yaze_util
  yaze_gfx
  yaze_gui         # Safe to include - yaze_gui doesn't link to yaze_agent
  yaze_zelda3
  yaze_common
  yaze_rom         # Generic ROM library
  # Note: yaze_editor is linked at executable level to avoid dependency cycle:
  # yaze_agent -> yaze_app_core_lib -> yaze_editor -> yaze_agent
  ImGui
  ${ABSL_TARGETS}
  ${YAZE_SDL2_TARGETS}
  ${CMAKE_DL_LIBS}
)

# Link nativefiledialog-extended for Windows/Linux file dialogs
if(WIN32 OR (UNIX AND NOT APPLE AND NOT EMSCRIPTEN))
  add_subdirectory(${CMAKE_SOURCE_DIR}/ext/nativefiledialog-extended ${CMAKE_BINARY_DIR}/nfd EXCLUDE_FROM_ALL)
  target_link_libraries(yaze_app_core_lib PUBLIC nfd)
  target_include_directories(yaze_app_core_lib PUBLIC ${CMAKE_SOURCE_DIR}/ext/nativefiledialog-extended/src/include)
endif()

# gRPC Services (Optional)
if(YAZE_WITH_GRPC)
  target_include_directories(yaze_app_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/ext/json/include)
  target_compile_definitions(yaze_app_core_lib PRIVATE YAZE_WITH_JSON)
  # Link to consolidated gRPC support library
  target_link_libraries(yaze_app_core_lib PUBLIC yaze_grpc_support)
  
  message(STATUS "  - gRPC ROM service + canvas automation enabled")
endif()

# Platform-specific libraries
if(APPLE)
  target_link_libraries(yaze_app_core_lib PUBLIC ${COCOA_LIBRARY})
endif()

set_target_properties(yaze_app_core_lib PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_app_core_lib PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_app_core_lib PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_app_core_lib PRIVATE WINDOWS)
endif()

# Copy web resources for WASM builds
if(EMSCRIPTEN)
  # Copy JavaScript and CSS files for loading indicators
  configure_file(
    ${CMAKE_SOURCE_DIR}/src/web/core/loading_indicator.js
    ${CMAKE_BINARY_DIR}/loading_indicator.js
    COPYONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/src/web/styles/loading_indicator.css
    ${CMAKE_BINARY_DIR}/loading_indicator.css
    COPYONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/src/web/core/error_handler.js
    ${CMAKE_BINARY_DIR}/error_handler.js
    COPYONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/src/web/styles/error_handler.css
    ${CMAKE_BINARY_DIR}/error_handler.css
    COPYONLY
  )
  # Copy drag and drop zone resources
  configure_file(
    ${CMAKE_SOURCE_DIR}/src/web/components/drop_zone.js
    ${CMAKE_BINARY_DIR}/drop_zone.js
    COPYONLY
  )
  configure_file(
    ${CMAKE_SOURCE_DIR}/src/web/styles/drop_zone.css
    ${CMAKE_BINARY_DIR}/drop_zone.css
    COPYONLY
  )
  message(STATUS "  - WASM web resources copied (loading_indicator, error_handler, drop_zone)")
endif()

message(STATUS "✓ yaze_app_core_lib library configured (application layer)")
