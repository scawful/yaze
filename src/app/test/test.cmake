# ==============================================================================
# Yaze Test Support Library
# ==============================================================================
# This library contains the core test manager and infrastructure for running
# tests within the application.
#
# It is intended to be linked by test executables, not by the main
# application itself.
#
# Dependencies: All major yaze libraries.
# ==============================================================================

set(YAZE_TEST_SOURCES
  app/test/test_manager.cc
  app/test/z3ed_test_suite.cc
)

# Add gRPC test harness services if enabled (depend on TestManager)
if(YAZE_WITH_GRPC)
  list(APPEND YAZE_TEST_SOURCES
    app/service/imgui_test_harness_service.cc
    app/service/screenshot_utils.cc
    app/service/widget_discovery_service.cc
    app/test/test_recorder.cc
    app/test/test_script_parser.cc
  )
endif()

add_library(yaze_test_support STATIC ${YAZE_TEST_SOURCES})

target_precompile_headers(yaze_test_support PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_test_support PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/incl
  ${CMAKE_SOURCE_DIR}/src/lib
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_test_support PUBLIC
  yaze_editor
  yaze_app_core_lib  # Changed from yaze_core_lib - app layer needs app core
  yaze_gui
  yaze_zelda3
  yaze_gfx
  yaze_util
  yaze_common
)

# Add gRPC dependencies if test harness is enabled
if(YAZE_WITH_GRPC)
  target_include_directories(yaze_test_support PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_test_support PRIVATE YAZE_WITH_JSON)
  
  # Add test harness proto definition
  target_add_protobuf(yaze_test_support
    ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto)
  
  target_link_libraries(yaze_test_support PUBLIC
    grpc++
    grpc++_reflection
  )
  
  message(STATUS "  - gRPC test harness service enabled in yaze_test_support")
endif()

# Link agent library if available (for z3ed test suites)
# yaze_agent contains all the CLI service code (tile16_proposal_generator, gui_automation_client, etc.)
if(TARGET yaze_agent)
  target_link_libraries(yaze_test_support PUBLIC yaze_agent)
  if(YAZE_WITH_GRPC)
    message(STATUS "✓ z3ed test suites enabled with gRPC support")
  else()
    message(STATUS "✓ z3ed test suites enabled (without gRPC)")
  endif()
else()
  message(STATUS "○ z3ed test suites disabled (yaze_agent not built)")
endif()

message(STATUS "✓ yaze_test_support library configured")

# Note: yaze_editor needs yaze_test_support for TestManager, but we can't link it here
# because this happens BEFORE yaze and yaze_emu are configured.
# Instead, each executable (yaze, yaze_emu) must explicitly link yaze_test_support
# in their respective .cmake files (app.cmake, emu.cmake).