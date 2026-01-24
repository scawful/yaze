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
  app/test/agent_tools_test.cc
)

set(YAZE_ENABLE_VISUAL_DIFF_ENGINE ON)
if(YAZE_PLATFORM_IOS)
  # Visual diff engine requires libpng which we don't bundle on iOS.
  set(YAZE_ENABLE_VISUAL_DIFF_ENGINE OFF)
  message(STATUS "○ visual_diff_engine disabled on iOS (libpng unavailable)")
else()
  find_package(PNG QUIET)
  if(NOT PNG_FOUND)
    set(YAZE_ENABLE_VISUAL_DIFF_ENGINE OFF)
    message(STATUS "○ visual_diff_engine disabled (libpng unavailable)")
  endif()
endif()

if(NOT YAZE_ENABLE_VISUAL_DIFF_ENGINE)
  list(REMOVE_ITEM YAZE_TEST_SOURCES app/test/visual_diff_engine.cc)
endif()

# gRPC test harness services are now in yaze_grpc_support library

add_library(yaze_test_support STATIC ${YAZE_TEST_SOURCES})

target_precompile_headers(yaze_test_support PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_test_support PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/inc
  ${CMAKE_SOURCE_DIR}/ext
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_test_support PUBLIC
  yaze_editor
  yaze_emulator_ui
  yaze_app_core_lib  # Changed from yaze_core_lib - app layer needs app core
  yaze_gui
  yaze_zelda3
  yaze_gfx
  yaze_util
  yaze_common
)

if(PNG_FOUND)
  target_link_libraries(yaze_test_support PUBLIC PNG::PNG)
endif()

# Add gRPC dependencies if test harness is enabled
if(YAZE_WITH_GRPC)
  target_compile_definitions(yaze_test_support PRIVATE YAZE_WITH_JSON)
  
  # Link to consolidated gRPC support library
  target_link_libraries(yaze_test_support PUBLIC yaze_grpc_support)
  
  message(STATUS "  - gRPC test harness service enabled in yaze_test_support")
endif()

# Link agent library if available (for z3ed test suites)
# yaze_agent contains all the CLI service code (tile16_proposal_generator, gui_automation_client, etc.)
if(TARGET yaze_agent)
  # Use normal linking to avoid circular dependencies
  # The previous force_load/whole-archive approach created a circular dependency:
  # yaze_test_support -> force_load(yaze_agent) -> yaze_test_support
  # This caused SIGSEGV during static initialization.
  # If specific agent symbols are not being pulled in, they should be explicitly
  # referenced in the test code or restructured into a separate test library.
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
