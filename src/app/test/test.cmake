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

add_library(yaze_test_support STATIC ${YAZE_TEST_SOURCES})

target_precompile_headers(yaze_test_support PRIVATE
  <memory>
  <set>
  <string>
  <string_view>
  <vector>
)

target_include_directories(yaze_test_support PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/incl
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_test_support PUBLIC
  yaze_editor
  yaze_core_lib
  yaze_gui
  yaze_zelda3
  yaze_gfx
  yaze_util
  yaze_common
)

# Link agent library if gRPC is enabled (for z3ed test suites)
# yaze_agent contains all the CLI service code (tile16_proposal_generator, gui_automation_client, etc.)
if(YAZE_WITH_GRPC)
  target_link_libraries(yaze_test_support PUBLIC yaze_agent)
  message(STATUS "✓ z3ed test suites enabled (YAZE_WITH_GRPC=ON)")
endif()

message(STATUS "✓ yaze_test_support library configured")