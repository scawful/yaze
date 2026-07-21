# ==============================================================================
# Yaze Test Support Library (build-system module)
# ==============================================================================
# This file lives under src/ so src/CMakeLists.txt can include it without
# reaching out-of-tree. The actual test harness sources live under app/testing/.
#
# Public include layout stays unchanged: headers remain co-located with sources.
# ==============================================================================

include_guard(GLOBAL)

set(YAZE_TEST_SOURCES
  app/testing/test_manager.cc
  app/testing/test_recorder.cc
  app/testing/test_script_parser.cc
  app/testing/screenshot_assertion.cc
  app/testing/visual_diff_engine.cc
  app/testing/dungeon_ui_tests.cc
  app/testing/overworld_ui_tests.cc
  app/testing/z3ed_test_suite.cc
  app/testing/agent_tools_test.cc
  app/testing/ai_vision_verifier.cc
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
  list(REMOVE_ITEM YAZE_TEST_SOURCES app/testing/visual_diff_engine.cc)
endif()

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
  # yaze_editor # Removed to avoid circular dep with yaze_editor->yaze_test_support
  yaze_emulator_ui
  yaze_app_core_lib  # Changed from yaze_core_lib - app layer needs app core
  yaze_gui
  yaze_zelda3
  yaze_gfx
  yaze_util
  yaze_common
)

if(TARGET ImGuiTestEngine)
  target_link_libraries(yaze_test_support PUBLIC ImGuiTestEngine)
  target_compile_definitions(yaze_test_support PUBLIC
    YAZE_ENABLE_IMGUI_TEST_ENGINE=1
    IMGUI_DEFINE_MATH_OPERATORS=1
    IMGUI_ENABLE_TEST_ENGINE=1
    IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
  )
  message(STATUS "  - ImGui test engine enabled in yaze_test_support")
endif()

if(YAZE_ENABLE_VISUAL_DIFF_ENGINE)
  target_compile_definitions(yaze_test_support PUBLIC
    YAZE_HAS_VISUAL_DIFF_ENGINE=1
  )
  target_link_libraries(yaze_test_support PUBLIC PNG::PNG)
endif()

# Add gRPC dependencies if test harness is enabled
if(YAZE_WITH_GRPC)
  target_compile_definitions(yaze_test_support PRIVATE YAZE_WITH_JSON)
  target_link_libraries(yaze_test_support PUBLIC yaze_grpc_support)
  message(STATUS "  - gRPC test harness service enabled in yaze_test_support")
endif()

# Link agent library if available (for z3ed test suites)
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
