set(
  YAZE_APP_CORE_SRC
  app/core/controller.cc
  app/emu/emulator.cc
  app/core/project.cc
  app/core/window.cc
  app/core/asar_wrapper.cc
)

if (WIN32 OR MINGW OR (UNIX AND NOT APPLE))
  list(APPEND YAZE_APP_CORE_SRC
    app/core/platform/font_loader.cc
    app/core/platform/asset_loader.cc
  )
endif()

if(APPLE)
    list(APPEND YAZE_APP_CORE_SRC
      app/core/platform/file_dialog.mm
      app/core/platform/app_delegate.mm
      app/core/platform/font_loader.cc
      app/core/platform/font_loader.mm
      app/core/platform/asset_loader.cc
    )

    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa not found")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()

# ==============================================================================
# Yaze Core Library
# ==============================================================================
# This library contains core application functionality:
# - ROM management
# - Project management
# - Controller/Window management
# - Asar wrapper for assembly
# - Platform-specific utilities (file dialogs, fonts, clipboard)
# - Widget state capture for testing
# - Emulator interface
#
# Dependencies: yaze_util, yaze_gfx, asar, SDL2
# ==============================================================================

add_library(yaze_core_lib STATIC 
  app/rom.cc
  ${YAZE_APP_CORE_SRC}
)

target_include_directories(yaze_core_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_core_lib PUBLIC
  yaze_util
  yaze_gfx
  yaze_common
  ImGui
  asar-static
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${CMAKE_DL_LIBS}
)

target_sources(yaze_core_lib PRIVATE
  ${CMAKE_SOURCE_DIR}/src/cli/service/testing/test_workflow_generator.cc
)

if(YAZE_WITH_GRPC)
  target_include_directories(yaze_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_core_lib PRIVATE YAZE_WITH_JSON)

  # Add proto definitions for test harness and ROM service
  target_add_protobuf(yaze_core_lib
    ${PROJECT_SOURCE_DIR}/src/app/core/proto/imgui_test_harness.proto)
  target_add_protobuf(yaze_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto)

  # Add test harness implementation
  target_sources(yaze_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app/core/service/imgui_test_harness_service.cc
    ${CMAKE_SOURCE_DIR}/src/app/core/service/imgui_test_harness_service.h
    ${CMAKE_SOURCE_DIR}/src/app/core/service/screenshot_utils.cc
    ${CMAKE_SOURCE_DIR}/src/app/core/service/screenshot_utils.h
    ${CMAKE_SOURCE_DIR}/src/app/core/service/widget_discovery_service.cc
    ${CMAKE_SOURCE_DIR}/src/app/core/service/widget_discovery_service.h
    ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_recorder.cc
    ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_recorder.h
    ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_script_parser.cc
    ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_script_parser.h
    # Add unified gRPC server
    ${CMAKE_SOURCE_DIR}/src/app/core/service/unified_grpc_server.cc
    ${CMAKE_SOURCE_DIR}/src/app/core/service/unified_grpc_server.h
  )

  target_link_libraries(yaze_core_lib PUBLIC
    grpc++
    grpc++_reflection
    libprotobuf
  )
  
  message(STATUS "  - gRPC test harness + ROM service enabled")
endif()

# Platform-specific libraries
if(APPLE)
  target_link_libraries(yaze_core_lib PUBLIC ${COCOA_LIBRARY})
endif()

set_target_properties(yaze_core_lib PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_core_lib PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_core_lib PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_core_lib PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_core_lib library configured")
