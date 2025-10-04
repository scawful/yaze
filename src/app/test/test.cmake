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

add_library(yaze_test_support STATIC app/test/test_manager.cc)

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

message(STATUS "✓ yaze_test_support library configured")