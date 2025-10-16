# ==============================================================================
# Application Core Library (Platform, Controller, Services)
# ==============================================================================
# This file defines the application-level core components that were moved
# from the monolithic app/core structure. This is distinct from the foundational
# yaze_core_lib in src/core/ which handles project management and asar.
#
# Components:
# - ROM management (app/rom.cc)
# - Application controller (app/controller.cc)
# - Window/platform management (app/platform/)
# - gRPC services for AI automation (app/service/)
# - Test harness (app/test/)
#
# Dependencies: yaze_core_lib (foundational), yaze_util, yaze_gfx, SDL2, ImGui
# ==============================================================================

set(
  YAZE_APP_CORE_SRC
  app/controller.cc
  app/platform/window.cc
)

# Platform-specific sources
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

    add_library(yaze_app_objcxx OBJECT ${YAZE_APPLE_OBJCXX_SRC})
    set_target_properties(yaze_app_objcxx PROPERTIES
      OBJCXX_STANDARD 20
      OBJCXX_STANDARD_REQUIRED ON
    )

    target_include_directories(yaze_app_objcxx PUBLIC
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/src/app
      ${CMAKE_SOURCE_DIR}/src/lib
      ${CMAKE_SOURCE_DIR}/src/lib/imgui
      ${CMAKE_SOURCE_DIR}/incl
      ${SDL2_INCLUDE_DIR}
      ${PROJECT_BINARY_DIR}
    )
    target_link_libraries(yaze_app_objcxx PUBLIC ${ABSL_TARGETS} yaze_util)
    target_compile_definitions(yaze_app_objcxx PUBLIC MACOS)

    find_library(COCOA_LIBRARY Cocoa)
    if(NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa not found")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "-framework ServiceManagement -framework Foundation -framework Cocoa")
endif()

# ==============================================================================
# Application Core Library
# ==============================================================================
# Renamed from old yaze_core_lib to yaze_app_core_lib to distinguish from
# the foundational yaze_core_lib in src/core/
# ==============================================================================

add_library(yaze_app_core_lib STATIC 
  app/rom.cc
  ${YAZE_APP_CORE_SRC}
  $<$<BOOL:${APPLE}>:$<TARGET_OBJECTS:yaze_app_objcxx>>
)

target_precompile_headers(yaze_app_core_lib PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_app_core_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_app_core_lib PUBLIC
  yaze_core_lib    # NEW foundational core library with project management
  yaze_util
  yaze_gfx
  yaze_zelda3
  yaze_common
  ImGui
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${CMAKE_DL_LIBS}
)

# Link nativefiledialog-extended for Windows/Linux file dialogs
if(WIN32 OR (UNIX AND NOT APPLE))
  add_subdirectory(${CMAKE_SOURCE_DIR}/src/lib/nativefiledialog-extended ${CMAKE_BINARY_DIR}/nfd EXCLUDE_FROM_ALL)
  target_link_libraries(yaze_app_core_lib PUBLIC nfd)
  target_include_directories(yaze_app_core_lib PUBLIC ${CMAKE_SOURCE_DIR}/src/lib/nativefiledialog-extended/src/include)
endif()

# ==============================================================================
# gRPC Services and Test Harness (Optional)
# ==============================================================================

if(YAZE_WITH_GRPC)
  target_include_directories(yaze_app_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_app_core_lib PRIVATE YAZE_WITH_JSON)

  # Add proto definitions for test harness, ROM service, and canvas automation
  target_add_protobuf(yaze_app_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto)
  target_add_protobuf(yaze_app_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto)
  target_add_protobuf(yaze_app_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto)

  # Add service and testing implementation (now in app/service/ and app/test/)
  target_sources(yaze_app_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app/service/imgui_test_harness_service.cc
    ${CMAKE_SOURCE_DIR}/src/app/service/imgui_test_harness_service.h
    ${CMAKE_SOURCE_DIR}/src/app/service/screenshot_utils.cc
    ${CMAKE_SOURCE_DIR}/src/app/service/screenshot_utils.h
    ${CMAKE_SOURCE_DIR}/src/app/service/widget_discovery_service.cc
    ${CMAKE_SOURCE_DIR}/src/app/service/widget_discovery_service.h
    ${CMAKE_SOURCE_DIR}/src/app/test/test_recorder.cc
    ${CMAKE_SOURCE_DIR}/src/app/test/test_recorder.h
    ${CMAKE_SOURCE_DIR}/src/app/test/test_script_parser.cc
    ${CMAKE_SOURCE_DIR}/src/app/test/test_script_parser.h
    # Add unified gRPC server
    ${CMAKE_SOURCE_DIR}/src/app/service/unified_grpc_server.cc
    ${CMAKE_SOURCE_DIR}/src/app/service/unified_grpc_server.h
  )

  target_link_libraries(yaze_app_core_lib PUBLIC
    grpc++
    grpc++_reflection
  )
  if(YAZE_PROTOBUF_TARGETS)
    target_link_libraries(yaze_app_core_lib PUBLIC ${YAZE_PROTOBUF_TARGETS})
    if(MSVC AND YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS)
      foreach(_yaze_proto_target IN LISTS YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS)
        target_link_options(yaze_app_core_lib PUBLIC /WHOLEARCHIVE:$<TARGET_FILE:${_yaze_proto_target}>)
      endforeach()
    endif()
  endif()
  
  message(STATUS "  - gRPC test harness + ROM service enabled")
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

message(STATUS "✓ yaze_app_core_lib library configured (application layer)")
