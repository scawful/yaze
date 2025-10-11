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
  yaze_core_lib
  yaze_gui
  yaze_zelda3
  yaze_gfx
  yaze_util
  yaze_common
)

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

# Link yaze_editor back to yaze_test_support (for TestManager usage in editor_manager.cc)
# Use PRIVATE linkage to avoid propagating test dependencies to editor consumers
if(TARGET yaze_editor)
  target_link_libraries(yaze_editor PRIVATE yaze_test_support)
  message(STATUS "✓ yaze_editor linked to yaze_test_support for TestManager access")
endif()

message(STATUS "✓ yaze_test_support library configured")