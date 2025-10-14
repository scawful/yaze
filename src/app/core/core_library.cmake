set(
  YAZE_APP_CORE_SRC
  app/core/asar_wrapper.cc
  app/core/controller.cc
  app/core/project.cc
  app/core/window.cc
)

if (WIN32 OR MINGW OR (UNIX AND NOT APPLE))
  list(APPEND YAZE_APP_CORE_SRC
    app/platform/font_loader.cc
    app/platform/asset_loader.cc
    app/platform/file_dialog_nfd.cc  # NFD file dialog for Windows/Linux
  )
endif()

if(APPLE)
    list(APPEND YAZE_APP_CORE_SRC
      app/platform/font_loader.cc
      app/platform/asset_loader.cc
    )

    set(YAZE_APPLE_OBJCXX_SRC
      app/platform/file_dialog.mm
      app/platform/app_delegate.mm
      app/platform/font_loader.mm
    )

    add_library(yaze_core_objcxx OBJECT ${YAZE_APPLE_OBJCXX_SRC})
    set_target_properties(yaze_core_objcxx PROPERTIES
      OBJCXX_STANDARD 20
      OBJCXX_STANDARD_REQUIRED ON
    )

    target_include_directories(yaze_core_objcxx PUBLIC
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
    target_link_libraries(yaze_core_objcxx PUBLIC ${ABSL_TARGETS} yaze_util)
    target_compile_definitions(yaze_core_objcxx PUBLIC MACOS)

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
  $<$<BOOL:${APPLE}>:$<TARGET_OBJECTS:yaze_core_objcxx>>
)

target_precompile_headers(yaze_core_lib PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
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
  yaze_zelda3  # Needed for Zelda3Labels in project.cc
  yaze_common
  ImGui
  asar-static
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${CMAKE_DL_LIBS}
)

# Link nativefiledialog-extended for Windows/Linux file dialogs
if(WIN32 OR (UNIX AND NOT APPLE))
  add_subdirectory(${CMAKE_SOURCE_DIR}/src/lib/nativefiledialog-extended ${CMAKE_BINARY_DIR}/nfd EXCLUDE_FROM_ALL)
  target_link_libraries(yaze_core_lib PUBLIC nfd)
  target_include_directories(yaze_core_lib PUBLIC ${CMAKE_SOURCE_DIR}/src/lib/nativefiledialog-extended/src/include)
endif()

if(YAZE_WITH_GRPC)
  target_include_directories(yaze_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_core_lib PRIVATE YAZE_WITH_JSON)

  # Add proto definitions for test harness, ROM service, and canvas automation
  target_add_protobuf(yaze_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto)
  target_add_protobuf(yaze_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto)
  target_add_protobuf(yaze_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto)

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
  )
  if(YAZE_PROTOBUF_TARGET)
    target_link_libraries(yaze_core_lib PUBLIC ${YAZE_PROTOBUF_TARGET})
  endif()
  
  # On Windows, force whole-archive linking for protobuf to ensure all symbols are included
if(MSVC AND YAZE_PROTOBUF_TARGET)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_link_options(yaze_core_lib PUBLIC 
      /WHOLEARCHIVE:$<TARGET_FILE:${YAZE_PROTOBUF_TARGET}>)
  else()
    message(STATUS "○ Skipping /WHOLEARCHIVE for libprotobuf in yaze_core_lib (clang-cl)")
  endif()
endif()
  
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
