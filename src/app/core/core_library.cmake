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
  asar-static
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${CMAKE_DL_LIBS}
)

target_sources(yaze_core_lib PRIVATE
  ${CMAKE_SOURCE_DIR}/src/cli/service/testing/test_workflow_generator.cc
)

if(YAZE_WITH_GRPC)
  target_add_protobuf(yaze_core_lib
    ${CMAKE_SOURCE_DIR}/src/app/core/proto/imgui_test_harness.proto)

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
  )

  target_link_libraries(yaze_core_lib PUBLIC
    grpc++
    grpc++_reflection
    libprotobuf
  )
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
