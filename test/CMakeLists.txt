# This file defines the yaze test suites and configures test discovery with labels.

if(YAZE_BUILD_TESTS)

  # Helper function to create and configure a test executable for a suite of tests.
  # This function adds the executable, links common dependencies, discovers the
  # tests using gtest_discover_tests, and assigns a label to all discovered tests.
  function(yaze_add_test_suite suite_name label is_gui_test)
    set(sources ${ARGN})
    add_executable(${suite_name} yaze_test.cc ${sources})

    target_include_directories(${suite_name} PUBLIC
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/incl
      ${CMAKE_SOURCE_DIR}/test
      ${CMAKE_SOURCE_DIR}/src/lib
      ${CMAKE_SOURCE_DIR}/src/lib/imgui
      ${CMAKE_SOURCE_DIR}/src/lib/imgui/backends
      ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
      ${CMAKE_SOURCE_DIR}/src/lib/SDL/include
      ${CMAKE_SOURCE_DIR}/third_party/json/include
      ${CMAKE_BINARY_DIR}/src/lib/SDL/include
      ${PROJECT_BINARY_DIR}
    )

    target_link_libraries(${suite_name} PRIVATE
      yaze_test_support
      gmock_main
      gtest_main
      absl::failure_signal_handler
      absl::flags
      absl::flags_parse
      ImGui
      ${SDL_TARGETS}
    )

    # Link ImGui Test Engine for GUI tests (always available when tests are built)
    if(is_gui_test AND TARGET ImGuiTestEngine)
      target_link_libraries(${suite_name} PRIVATE ImGuiTestEngine)
      target_compile_definitions(${suite_name} PRIVATE 
        IMGUI_ENABLE_TEST_ENGINE=1 
        IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
        YAZE_GUI_TEST_TARGET=1)  # Mark this as a GUI test target
    endif()

    if(WIN32)
      message(STATUS "Configuring Windows stack size for ${suite_name} to 16MB")
      if(MSVC)
        target_link_options(${suite_name} PRIVATE /STACK:16777216)
      else()
        target_link_options(${suite_name} PRIVATE -Wl,--stack,16777216)
      endif()
    endif()

    include(GoogleTest)
    gtest_discover_tests(${suite_name})

    # Get the list of tests discovered for the target and apply the label.
    get_target_property(suite_tests ${suite_name} TESTS)
    if(suite_tests)
      set_tests_properties(${suite_tests} PROPERTIES LABELS "${label}")
    endif()
  endfunction()

  # --- Stable Test Suite (Valid Contracts) ---
  set(STABLE_TEST_SOURCES
    test_editor.cc
    test_utils.cc
    # Unit Tests
    unit/core/asar_wrapper_test.cc
    unit/core/hex_test.cc
    unit/cli/resource_catalog_test.cc
    unit/rom/rom_test.cc
    unit/gfx/snes_tile_test.cc
    unit/gfx/compression_test.cc
    unit/gfx/snes_palette_test.cc
    unit/snes_color_test.cc
    unit/gui/tile_selector_widget_test.cc
    unit/gui/canvas_automation_api_test.cc
    unit/zelda3/overworld_test.cc
    unit/zelda3/object_parser_test.cc
    unit/zelda3/object_parser_structs_test.cc
    unit/zelda3/sprite_builder_test.cc
    unit/zelda3/dungeon_component_unit_test.cc
    unit/zelda3/dungeon/room_object_encoding_test.cc
    unit/zelda3/dungeon/room_manipulation_test.cc
    ../src/cli/service/resources/resource_catalog.cc
    cli/service/resources/command_context_test.cc
    # Integration Tests
    integration/asar_integration_test.cc
    integration/dungeon_editor_test.cc
    integration/dungeon_editor_v2_test.cc
    integration/editor/tile16_editor_test.cc
    integration/editor/editor_integration_test.cc
    integration/zelda3/overworld_integration_test.cc
    integration/zelda3/dungeon_editor_system_integration_test.cc
    integration/zelda3/room_integration_test.cc
    integration/zelda3/dungeon_object_rendering_tests.cc
    integration/zelda3/dungeon_room_test.cc
    integration/zelda3/sprite_position_test.cc
    integration/zelda3/message_test.cc
    integration/palette_manager_test.cc
  )
  yaze_add_test_suite(yaze_test_stable "stable" OFF ${STABLE_TEST_SOURCES})

  # --- ROM Dependent Test Suite ---
  if(YAZE_ENABLE_ROM_TESTS)
    set(ROM_DEPENDENT_TEST_SOURCES
      integration/asar_rom_test.cc
      e2e/rom_dependent/e2e_rom_test.cc
      e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
    )
    yaze_add_test_suite(yaze_test_rom_dependent "rom_dependent" OFF ${ROM_DEPENDENT_TEST_SOURCES})
    target_compile_definitions(yaze_test_rom_dependent PRIVATE
      YAZE_ENABLE_ROM_TESTS=1
      YAZE_TEST_ROM_PATH="${YAZE_TEST_ROM_PATH}"
    )
  endif()

  # --- Experimental & GUI Test Suites ---
  # GUI tests always available when tests are built (uses ImGui Test Engine)
  set(GUI_TEST_SOURCES
    test_utils.cc
    e2e/framework_smoke_test.cc
    e2e/dungeon_editor_smoke_test.cc
    e2e/canvas_selection_test.cc
    integration/ai/ai_gui_controller_test.cc
  )
  yaze_add_test_suite(yaze_test_gui "gui;experimental" ON ${GUI_TEST_SOURCES})

  # Add a single test entry to run the entire GUI suite headlessly
  add_test(
      NAME headless_gui_suite
      COMMAND $<TARGET_FILE:yaze_test_gui> -nogui
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties(headless_gui_suite PROPERTIES LABELS "headless_gui;experimental")

  set(EXPERIMENTAL_TEST_SOURCES
    test_utils.cc
    integration/ai/test_ai_tile_placement.cc
    integration/ai/test_gemini_vision.cc
  )
  yaze_add_test_suite(yaze_test_experimental "experimental" OFF ${EXPERIMENTAL_TEST_SOURCES})

  # --- Benchmark Test Suite ---
  set(BENCHMARK_TEST_SOURCES
    benchmarks/gfx_optimization_benchmarks.cc
  )
  yaze_add_test_suite(yaze_test_benchmark "benchmark" OFF ${BENCHMARK_TEST_SOURCES})

endif()