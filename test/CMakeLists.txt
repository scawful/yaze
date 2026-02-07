# This file defines the yaze test suites and configures test discovery with labels.

if(YAZE_BUILD_TESTS)

  # Helper function to create and configure a test executable for a suite of tests.
  # This function adds the executable, links common dependencies, discovers the
  # tests using gtest_discover_tests, and assigns a label to all discovered tests.
  function(yaze_add_test_suite suite_name label is_gui_test)
    set(sources ${ARGN})
    
    # Only GUI tests need controller.cc (yaze_test.cc uses Controller when config.enable_ui_tests is true)
    # Controller is legacy code with circular dependencies - avoid it in non-GUI tests
    if(is_gui_test)
      add_executable(${suite_name} yaze_test.cc ../src/app/application.cc ../src/app/controller.cc ${sources})
    else()
      add_executable(${suite_name} yaze_test.cc ${sources})
    endif()

    target_include_directories(${suite_name} PUBLIC
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/inc
      ${CMAKE_SOURCE_DIR}/test
      ${CMAKE_SOURCE_DIR}/test/framework
      ${CMAKE_SOURCE_DIR}/ext
      ${CMAKE_SOURCE_DIR}/ext/imgui
      ${CMAKE_SOURCE_DIR}/ext/imgui/backends
      ${CMAKE_SOURCE_DIR}/ext/imgui_test_engine
      ${CMAKE_SOURCE_DIR}/ext/SDL/include
      ${CMAKE_BINARY_DIR}/ext/SDL/include
      ${PROJECT_BINARY_DIR}
    )

    set(_yaze_test_core_libs
      yaze_test_support
      yaze_editor
      yaze_emulator
      yaze_emulator_ui
      yaze_core_lib
    )

    # Linux static linking needs a rescan to resolve yaze_test_support/editor
    # and emulator/emulator_ui circular dependencies.
    if(UNIX AND NOT APPLE)
      # Ensure transitive shared libs (absl flags/hash) stay linked on Linux.
      target_link_options(${suite_name} PRIVATE -Wl,--no-as-needed)
      target_link_libraries(${suite_name} PRIVATE
        -Wl,--start-group
        ${_yaze_test_core_libs}
        -Wl,--end-group
        gmock_main
        gtest_main
        absl::failure_signal_handler
        absl::flags
        absl::flags_parse
        absl::hash
        ImGui
        ${YAZE_SDL2_TARGETS}
      )
    else()
      target_link_libraries(${suite_name} PRIVATE
        ${_yaze_test_core_libs}
        gmock_main
        gtest_main
        absl::failure_signal_handler
        absl::flags
        absl::flags_parse
        absl::hash
        ImGui
        ${YAZE_SDL2_TARGETS}
      )
    endif()

    if(YAZE_ENABLE_JSON AND TARGET nlohmann_json::nlohmann_json)
      target_link_libraries(${suite_name} PRIVATE nlohmann_json::nlohmann_json)
    endif()
    if(TARGET httplib::httplib)
      target_link_libraries(${suite_name} PRIVATE httplib::httplib)
    endif()
    if(TARGET yaze_cli_core)
      target_link_libraries(${suite_name} PRIVATE yaze_cli_core)
    endif()

    # Link ImGui Test Engine for GUI tests (always available when tests are built)
    if(is_gui_test AND TARGET ImGuiTestEngine)
      target_link_libraries(${suite_name} PRIVATE ImGuiTestEngine)
      target_compile_definitions(${suite_name} PRIVATE 
        IMGUI_ENABLE_TEST_ENGINE=1 
        IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
        YAZE_GUI_TEST_TARGET=1)  # Mark this as a GUI test target
    endif()

    # Link protobuf with /WHOLEARCHIVE on Windows (instead of via libraries)
    if(WIN32 AND MSVC AND YAZE_WITH_GRPC AND YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS)
      foreach(_yaze_proto_target IN LISTS YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS)
        if(TARGET ${_yaze_proto_target})
          get_target_property(_target_type ${_yaze_proto_target} TYPE)
          if(_target_type MATCHES ".*_LIBRARY")
            target_link_options(${suite_name} PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:${_yaze_proto_target}>)
          endif()
        endif()
      endforeach()
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
    if(WIN32)
      gtest_discover_tests(${suite_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DISCOVERY_TIMEOUT 60
        PROPERTIES LABELS "${label}"
      )
    else()
      gtest_discover_tests(${suite_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES LABELS "${label}"
      )
    endif()
  endfunction()

  # --- Stable Test Suite (Valid Contracts) ---
  set(STABLE_TEST_SOURCES
    # Unit Tests
    unit/core/asar_wrapper_test.cc
    unit/core/hex_test.cc
    unit/core/hack_manifest_test.cc
    unit/deps/dependency_smoke_test.cc
    unit/cli/resource_catalog_test.cc
    unit/cli/command_registry_test.cc
    unit/cli/dungeon_object_validate_test.cc
    unit/rom/rom_test.cc
    unit/emu/input_backend_test.cc
    unit/gfx/snes_tile_test.cc
    unit/gfx/compression_test.cc
    unit/gfx/snes_palette_test.cc
    unit/gfx/bpp_conversion_test.cc
    unit/palette_json_test.cc
    unit/snes_color_test.cc
    unit/gui/tile_selector_widget_test.cc
    unit/gui/canvas_automation_api_test.cc
    unit/gui/window_clamp_test.cc
    unit/gui/panel_window_test.cc
    unit/editor/message/message_data_test.cc
    unit/editor/panel_system_test.cc
    unit/editor/editor_manager_test.cc
    unit/core/asar_compiler_test.cc
    unit/zelda3/overworld_test.cc
    unit/zelda3/overworld_regression_test.cc
    unit/zelda3/overworld_version_helper_test.cc
    unit/zelda3/resource_labels_test.cc
    unit/zelda3/object_parser_test.cc
    unit/zelda3/object_parser_structs_test.cc
    unit/zelda3/sprite_builder_test.cc
    unit/zelda3/music_parser_test.cc
    unit/zelda3/dungeon_component_unit_test.cc
    unit/zelda3/dungeon/room_object_encoding_test.cc
    unit/zelda3/custom_object_test.cc
    unit/zelda3/dungeon/room_manipulation_test.cc
    unit/zelda3/dungeon/dungeon_save_test.cc
    unit/zelda3/dungeon/bpp_conversion_test.cc
    unit/zelda3/dungeon/dimension_cross_validation_test.cc
    unit/zelda3/dungeon/dimension_service_test.cc
    unit/zelda3/dungeon/object_dimensions_test.cc
    unit/zelda3/dungeon/object_geometry_test.cc
    unit/zelda3/dungeon/room_layer_manager_test.cc
    unit/zelda3/dungeon/draw_routine_mapping_test.cc
    unit/zelda3/dungeon/object_drawing_comprehensive_test.cc
    unit/dungeon_object_drawer_test.cc
    unit/object_selection_test.cc
    # Platform Tests
    platform/sdl3_audio_backend_test.cc
    platform/window_backend_test.cc
    platform/wasm_async_guard_test.cc
    ../src/cli/service/resources/resource_catalog.cc
    cli/service/resources/command_context_test.cc
    cli/service/resources/command_handler_test.cc
    # Integration Tests
    integration/asar_integration_test.cc
    integration/dungeon_editor_test.cc
    integration/dungeon_editor_v2_test.cc
    integration/overworld_editor_test.cc
    integration/editor/tile16_editor_test.cc
    # NOTE: editor_integration_test.cc removed - legacy code with Controller dependency
    integration/zelda3/overworld_integration_test.cc
    integration/zelda3/dungeon_editor_system_integration_test.cc
    integration/zelda3/room_integration_test.cc
    integration/zelda3/dungeon_save_region_test.cc
    integration/zelda3/dungeon_object_rendering_tests.cc
    integration/zelda3/dungeon_object_rom_validation_test.cc
    integration/zelda3/dungeon_palette_test.cc
    integration/zelda3/dungeon_room_test.cc
    integration/zelda3/sprite_position_test.cc
    integration/zelda3/message_test.cc
    integration/palette_manager_test.cc
    integration/object_selection_integration_test.cc
  )
  if(YAZE_ENABLE_AGENT_CLI)
    list(APPEND STABLE_TEST_SOURCES
      unit/cli/conversational_agent_service_test.cc
      unit/cli/ai_config_utils_test.cc
      unit/cli/ai_service_factory_test.cc
      unit/cli/model_registry_test.cc
      unit/cli/http_api_handlers_test.cc
      unit/cli/vision_action_refiner_test.cc
      unit/tools/build_tool_test.cc
      unit/tools/filesystem_tool_test.cc
      unit/tools/memory_inspector_tool_test.cc
      unit/tools/visual_analysis_tool_test.cc
      unit/tools/project_tool_test.cc
      unit/tools/code_gen_tool_test.cc
    )
  else()
    message(STATUS "Skipping agent CLI/tool tests (YAZE_ENABLE_AGENT_CLI=OFF)")
  endif()

  if(YAZE_BUILD_AGENT_UI)
    list(APPEND STABLE_TEST_SOURCES
      unit/editor/agent_chat_test.cc
    )
  else()
    message(STATUS "Skipping agent UI tests (YAZE_BUILD_AGENT_UI=OFF)")
  endif()

  yaze_add_test_suite(yaze_test_stable "stable" OFF ${STABLE_TEST_SOURCES})

  # --- ROM Dependent Test Suite ---
  if(YAZE_ENABLE_ROM_TESTS)
    set(ROM_DEPENDENT_TEST_SOURCES
      integration/asar_rom_test.cc
      integration/zelda3/dungeon_graphics_transparency_test.cc
      integration/zelda3/dungeon_object_rom_validation_test.cc
      integration/zelda3/music_integration_test.cc
      integration/emulator_object_preview_test.cc
      integration/emulator_render_service_test.cc
      integration/save_state_generation_test.cc
      # Audio timing and debugging tests
      integration/audio/audio_timing_test.cc
      integration/audio/music_player_headless_test.cc
      integration/audio/headless_audio_debug_test.cc
      # E2E ROM-dependent editor save tests
      e2e/rom_dependent/e2e_rom_test.cc
      e2e/rom_dependent/graphics_editor_save_test.cc
      e2e/rom_dependent/tile16_editor_save_test.cc
      e2e/rom_dependent/dungeon_editor_save_test.cc
      e2e/rom_dependent/palette_editor_save_test.cc
      e2e/rom_dependent/screen_editor_save_test.cc
      e2e/rom_dependent/rom_version_test.cc
      e2e/rom_dependent/zscustomoverworld_save_test.cc
      e2e/rom_dependent/cross_editor_integrity_test.cc
      # Overworld E2E tests
      e2e/overworld/overworld_e2e_test.cc
      e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
    )
    yaze_add_test_suite(yaze_test_rom_dependent "rom_dependent" OFF ${ROM_DEPENDENT_TEST_SOURCES})
    if(TARGET overworld_golden_data_extractor)
      add_dependencies(yaze_test_rom_dependent overworld_golden_data_extractor)
    endif()
    target_compile_definitions(yaze_test_rom_dependent PRIVATE
      YAZE_ENABLE_ROM_TESTS=1
    )
    if(YAZE_TEST_ROM_PATH)
      target_compile_definitions(yaze_test_rom_dependent PRIVATE
        YAZE_TEST_ROM_PATH="${YAZE_TEST_ROM_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_VANILLA_PATH)
      target_compile_definitions(yaze_test_rom_dependent PRIVATE
        YAZE_TEST_ROM_VANILLA_PATH="${YAZE_TEST_ROM_VANILLA_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_US_PATH)
      target_compile_definitions(yaze_test_rom_dependent PRIVATE
        YAZE_TEST_ROM_US_PATH="${YAZE_TEST_ROM_US_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_JP_PATH)
      target_compile_definitions(yaze_test_rom_dependent PRIVATE
        YAZE_TEST_ROM_JP_PATH="${YAZE_TEST_ROM_JP_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_EU_PATH)
      target_compile_definitions(yaze_test_rom_dependent PRIVATE
        YAZE_TEST_ROM_EU_PATH="${YAZE_TEST_ROM_EU_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_EXPANDED_PATH)
      target_compile_definitions(yaze_test_rom_dependent PRIVATE
        YAZE_TEST_ROM_EXPANDED_PATH="${YAZE_TEST_ROM_EXPANDED_PATH}"
      )
    endif()
  endif()

  # Experimental & GUI Test Suites ---
  # GUI tests always available when tests are built (uses ImGui Test Engine)
  set(GUI_TEST_SOURCES
    gui_test_utils.cc
    e2e/framework_smoke_test.cc
    e2e/dungeon_editor_smoke_test.cc
    e2e/canvas_selection_test.cc
    e2e/dungeon_e2e_tests.cc
    e2e/dungeon_visual_verification_test.cc
    e2e/dungeon_object_drawing_test.cc
    e2e/dungeon_canvas_interaction_test.cc
    e2e/dungeon_layer_rendering_test.cc
    e2e/editor_smoke_tests.cc
    # AI Multimodal Testing Framework
    e2e/ai_multimodal_test.cc
    e2e/emulator_stepping_test.cc
    # ImGui Test Engine feature demos
    e2e/imgui_test_engine_demo.cc
  )

  if(YAZE_ENABLE_AI_RUNTIME)
    list(APPEND GUI_TEST_SOURCES integration/ai/ai_gui_controller_test.cc)
  else()
    message(STATUS "Skipping AI GUI controller tests (YAZE_ENABLE_AI_RUNTIME=OFF)")
  endif()

  yaze_add_test_suite(yaze_test_gui "gui;experimental" ON ${GUI_TEST_SOURCES})

  # Add a single test entry to run the entire GUI suite headlessly
  add_test(
      NAME headless_gui_suite
      COMMAND $<TARGET_FILE:yaze_test_gui> -nogui
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties(headless_gui_suite PROPERTIES LABELS "headless_gui;experimental")

  if(YAZE_ENABLE_AI_RUNTIME)
    set(EXPERIMENTAL_TEST_SOURCES
      integration/ai/test_ai_tile_placement.cc
      integration/ai/test_gemini_vision.cc
    )
    yaze_add_test_suite(yaze_test_experimental "experimental" OFF ${EXPERIMENTAL_TEST_SOURCES})
  else()
    message(STATUS "Skipping experimental AI suites (YAZE_ENABLE_AI_RUNTIME=OFF)")
  endif()

  # --- Benchmark Test Suite ---
  set(BENCHMARK_TEST_SOURCES
    benchmarks/gfx_optimization_benchmarks.cc
  )
  yaze_add_test_suite(yaze_test_benchmark "benchmark" OFF ${BENCHMARK_TEST_SOURCES})

  # --- z3ed CLI Smoke Test ---
  # Add z3ed self-test as a ctest entry if z3ed target exists
  if(TARGET z3ed)
    add_test(
      NAME z3ed_self_test
      COMMAND $<TARGET_FILE:z3ed> --self-test
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(z3ed_self_test PROPERTIES 
      LABELS "stable;cli;z3ed"
      TIMEOUT 30
    )
  endif()

endif()
