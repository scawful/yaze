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
  app/rom.cc
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
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_app_core_lib PUBLIC
  yaze_core_lib    # Foundational core library with project management
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

# gRPC Services (Optional)
if(YAZE_WITH_GRPC)
  target_include_directories(yaze_app_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_app_core_lib PRIVATE YAZE_WITH_JSON)

  # Add proto definitions for ROM service, canvas automation, and test harness
  # Test harness proto is needed because widget_discovery_service.h includes it
  target_add_protobuf(yaze_app_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto)
  target_add_protobuf(yaze_app_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto)
  target_add_protobuf(yaze_app_core_lib
    ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto)

  # Add unified gRPC server (non-test services only)
  target_sources(yaze_app_core_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app/service/unified_grpc_server.cc
    ${CMAKE_SOURCE_DIR}/src/app/service/unified_grpc_server.h
  )

  target_link_libraries(yaze_app_core_lib PUBLIC
    grpc++
    grpc++_reflection
  )
  # NOTE: Do NOT link protobuf at library level on Windows - causes LNK1241
  # Executables will link it with /WHOLEARCHIVE to include internal symbols
  if(NOT WIN32 AND YAZE_PROTOBUF_TARGETS)
    target_link_libraries(yaze_app_core_lib PUBLIC ${YAZE_PROTOBUF_TARGETS})
  endif()
  
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

message(STATUS "✓ yaze_app_core_lib library configured (application layer)")

# ==============================================================================
# Yaze Application Executable
# ==============================================================================

if (APPLE)
  add_executable(yaze MACOSX_BUNDLE app/main.cc ${YAZE_RESOURCE_FILES})
  
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
else()
  add_executable(yaze app/main.cc)
  if(WIN32 OR UNIX)
    target_sources(yaze PRIVATE ${YAZE_RESOURCE_FILES})
  endif()
endif()

target_include_directories(yaze PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/incl
  ${PROJECT_BINARY_DIR}
)

target_sources(yaze PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h PROPERTIES GENERATED TRUE)

# Link modular libraries
target_link_libraries(yaze PRIVATE 
  yaze_editor 
  yaze_emulator 
  yaze_agent
  absl::failure_signal_handler
  absl::flags
  absl::flags_parse
)
if(YAZE_WITH_GRPC AND YAZE_PROTOBUF_TARGETS)
  # On Windows: Use /WHOLEARCHIVE instead of normal linking to include internal symbols
  # On Unix: Use normal linking (symbols resolve correctly without whole-archive)
  if(MSVC AND YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS)
    foreach(_yaze_proto_target IN LISTS YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS)
      target_link_options(yaze PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:${_yaze_proto_target}>)
    endforeach()
  else()
    target_link_libraries(yaze PRIVATE ${YAZE_PROTOBUF_TARGETS})
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

if(APPLE)
  target_link_libraries(yaze PUBLIC "-framework Cocoa")
endif()

# Post-build asset copying for non-macOS platforms
if(NOT APPLE)
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
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/agent $<TARGET_FILE_DIR:yaze>/assets/agent
      COMMENT "Copying agent assets"
    )
  endif()
endif()
