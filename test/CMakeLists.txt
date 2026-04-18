# This file defines the yaze test suites and configures test discovery with labels.

if(YAZE_BUILD_TESTS)

  # Helper function to create and configure a test executable for a suite of tests.
  # This function adds the executable, links common dependencies, discovers the
  # tests using gtest_discover_tests, and assigns a label to all discovered tests.
  function(yaze_add_test_suite suite_name label is_gui_test)
    set(sources ${ARGN})
    
    # GUI tests need application.cc + controller.cc (full app context).
    # Non-GUI tests use a minimal Application::Instance() stub (app_instance_stub.cc)
    # to satisfy editor-library references without pulling in controller.cc, which
    # has circular dependencies that break non-GUI test link.
    if(is_gui_test)
      add_executable(${suite_name} yaze_test.cc ../src/app/application.cc ../src/app/controller.cc ${sources})
    else()
      add_executable(${suite_name} yaze_test.cc app_instance_stub.cc ${sources})
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
    if(YAZE_ENABLE_Z3DK AND YAZE_Z3DK_FOUND AND TARGET yaze::z3dk)
      target_link_libraries(${suite_name} PRIVATE yaze::z3dk)
      target_compile_definitions(${suite_name} PRIVATE YAZE_WITH_Z3DK=1)
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
    # gtest_discover_tests forwards PROPERTIES through a post-build discovery
    # script as a semicolon-delimited list of tokens (prop/value pairs). If the
    # LABELS value contains semicolons (multiple labels), it must be escaped so
    # it stays a single token when forwarded, otherwise it turns into two extra
    # tokens and breaks the prop/value pairing (and ctest won't be able to
    # filter by the missing labels).
    #
    # NOTE: GoogleTestAddTests.cmake forwards TEST_PROPERTIES through another
    # helper function call without quoting. That forwarding step will split on
    # literal ';' again, so we must *double escape* label separators here. This
    # ensures the discovery script ultimately emits:
    #   LABELS "stable;unit"
    # rather than the broken:
    #   LABELS stable unit
    string(REPLACE ";" "\\\\;" _suite_labels "${label}")
    # Avoid duplicate CTest names across multiple suites that compile the same
    # gtest sources (e.g. quick vs stable). Only the quick suites need a prefix.
    set(_test_prefix "")
    if(label MATCHES "(^|;)quick(;|$)")
      set(_test_prefix "${suite_name}::")
    endif()
    if(WIN32)
      set(_gtest_discover_args
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DISCOVERY_TIMEOUT 60
        PROPERTIES LABELS "${_suite_labels}"
      )
      if(NOT "${_test_prefix}" STREQUAL "")
        list(APPEND _gtest_discover_args TEST_PREFIX "${_test_prefix}")
      endif()
      gtest_discover_tests(${suite_name} ${_gtest_discover_args})
    else()
      set(_gtest_discover_args
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES LABELS "${_suite_labels}"
      )
      if(NOT "${_test_prefix}" STREQUAL "")
        list(APPEND _gtest_discover_args TEST_PREFIX "${_test_prefix}")
      endif()
      gtest_discover_tests(${suite_name} ${_gtest_discover_args})
    endif()
  endfunction()

  # --- Stable Test Suites (Valid Contracts) ---
  # Note: stable tests are split into unit + integration so you can run:
  #   ctest --test-dir build/presets/mac-ai-fast -L unit
  #   ctest --test-dir build/presets/mac-ai-fast -L integration
  # without running everything (still included in `-L stable`).
  set(STABLE_UNIT_TEST_SOURCES
    # Unit Tests
    unit/core/hex_test.cc
    unit/core/hack_manifest_test.cc
    unit/core/oracle_progression_test.cc
    unit/core/oracle_progression_loader_test.cc
    unit/core/oracle_menu_registry_test.cc
    unit/core/story_event_graph_test.cc
    unit/core/story_event_graph_query_test.cc
    unit/core/rom_hash_test.cc
    unit/core/project_paths_test.cc
    unit/core/project_bundle_test.cc
    unit/core/project_bundle_path_test.cc
    unit/util/lru_cache_test.cc
    unit/util/bps_test.cc
    unit/deps/dependency_smoke_test.cc
    unit/cli/resource_catalog_test.cc
    unit/cli/command_registry_test.cc
    unit/cli/rom_doctor_water_fill_test.cc
    unit/cli/dungeon_object_validate_test.cc
    unit/cli/dungeon_collision_json_commands_test.cc
    unit/cli/dungeon_edit_commands_test.cc
    unit/cli/editor_completion_command_handlers_test.cc
    unit/cli/dungeon_minecart_audit_test.cc
    unit/cli/dungeon_oracle_preflight_test.cc
    unit/cli/oracle_smoke_check_test.cc
    unit/cli/oracle_menu_commands_test.cc
    unit/cli/palette_commands_test.cc
    unit/cli/project_bundle_verify_test.cc
    unit/cli/project_bundle_archive_test.cc
    unit/rom/rom_diff_test.cc
    unit/rom/rom_test.cc
    unit/rom/write_fence_test.cc
    unit/emu/mesen_socket_client_test.cc
    unit/emu/input_backend_test.cc
    unit/gfx/snes_tile_test.cc
    unit/gfx/compression_test.cc
    unit/gfx/snes_palette_test.cc
    unit/gfx/usdasm_palette_loading_test.cc
    unit/gfx/bpp_conversion_test.cc
    unit/palette_json_test.cc
    unit/snes_color_test.cc
    unit/gui/tile_selector_widget_test.cc
    unit/gui/canvas_automation_api_test.cc
    unit/gui/canvas_coordinate_sync_test.cc
    unit/gui/canvas_zoom_anchor_test.cc
    unit/gui/window_clamp_test.cc
    unit/gui/panel_window_test.cc
    unit/gui/animator_test.cc
    unit/editor/message/message_data_test.cc
    unit/editor/message/message_id_resolver_test.cc
    unit/editor/message/message_expanded_write_test.cc
    unit/editor/oracle_validation_view_model_test.cc
    unit/editor/hack_workflow_backend_factory_test.cc
    unit/editor/rom_file_manager_test.cc
    unit/editor/project_manager_validator_test.cc
    unit/editor/panel_system_test.cc
    unit/editor/workspace_window_manager_policy_test.cc
    unit/editor/background_command_task_test.cc
    unit/editor/content_registry_workflow_history_test.cc
    unit/editor/command_palette_workflow_test.cc
    unit/editor/workflow_activity_widgets_test.cc
    unit/editor/layout_manager_persistence_test.cc
    unit/editor/right_drawer_manager_test.cc
    unit/editor/undo_manager_test.cc
    unit/editor/sprite_editor_undo_test.cc
    unit/editor/screen_editor_undo_test.cc
    unit/editor/dungeon_undo_actions_test.cc
    unit/editor/assembly_editor_symbol_jump_test.cc
    unit/editor/editor_manager_test.cc
    unit/editor/ui_coordinator_workflow_test.cc
    unit/editor/editor_manager_project_actions_test.cc
    unit/editor/editor_manager_rom_write_policy_test.cc
    unit/editor/editor_manager_oracle_rom_safety_test.cc
    unit/editor/editor_set_test.cc
    unit/editor/paint_util_test.cc
    unit/editor/map_properties_context_menu_test.cc
    unit/editor/layout_presets_test.cc
    unit/editor/user_settings_layout_defaults_test.cc
    unit/editor/sidebar_sort_test.cc
    unit/editor/activity_bar_actions_registry_test.cc
    unit/editor/theme_persistence_test.cc
    unit/editor/editor_registry_test.cc
    unit/editor/shortcut_manager_test.cc
    unit/editor/shortcut_configurator_test.cc
    unit/editor/window_sidebar_test.cc
    unit/editor/dungeon_canvas_viewer_navigation_test.cc
    unit/editor/dungeon_usage_tracker_test.cc
    unit/editor/dungeon_editor_v2_rom_safety_test.cc
    unit/editor/dungeon_overlay_controls_test.cc
    unit/editor/dungeon_workbench_toolbar_test.cc
    unit/editor/toast_dedup_test.cc
    unit/editor/tile_painting_manager_test.cc
    unit/editor/tile16_editor_action_state_test.cc
    unit/editor/tile16_editor_shortcuts_test.cc
    unit/editor/tile8_source_interaction_test.cc
    unit/editor/overworld_item_operations_test.cc
    unit/editor/canvas_navigation_manager_test.cc
    unit/editor/map_refresh_coordinator_test.cc
    unit/zelda3/overworld_test.cc
    unit/zelda3/overworld_regression_test.cc
    unit/zelda3/overworld_version_helper_test.cc
    unit/zelda3/tile16_metadata_test.cc
    unit/zelda3/tile16_stamp_test.cc
    unit/zelda3/tile16_renderer_test.cc
    unit/zelda3/tile16_usage_index_test.cc
    unit/zelda3/resource_labels_test.cc
    unit/zelda3/object_parser_test.cc
    unit/zelda3/object_parser_structs_test.cc
    unit/zelda3/sprite_builder_test.cc
    unit/zelda3/music_parser_test.cc
    unit/zelda3/dungeon_component_unit_test.cc
    unit/zelda3/dungeon/dungeon_limits_test.cc
    unit/zelda3/dungeon/room_limit_regression_test.cc
    unit/zelda3/dungeon/dungeon_validator_test.cc
    unit/zelda3/dungeon/room_object_encoding_test.cc
    unit/zelda3/dungeon/room_object_set_id_test.cc
    unit/zelda3/custom_object_test.cc
    unit/zelda3/dungeon/room_manipulation_test.cc
    unit/zelda3/dungeon/dungeon_save_test.cc
    unit/zelda3/dungeon/sprite_relocation_test.cc
    unit/zelda3/dungeon/bpp_conversion_test.cc
    unit/zelda3/dungeon/dimension_cross_validation_test.cc
    unit/zelda3/dungeon/dimension_service_test.cc
    unit/zelda3/dungeon/object_dimensions_test.cc
    unit/zelda3/dungeon/object_geometry_test.cc
    unit/zelda3/dungeon/track_collision_generator_test.cc
    unit/zelda3/dungeon/object_layer_semantics_test.cc
    unit/zelda3/dungeon/room_layer_manager_test.cc
    unit/zelda3/dungeon/water_fill_zone_test.cc
    unit/zelda3/dungeon/custom_collision_reserved_region_test.cc
    unit/zelda3/dungeon/custom_collision_json_test.cc
    unit/zelda3/dungeon/oracle_rom_safety_preflight_test.cc
    unit/zelda3/dungeon/save_all_collision_rom_presence_test.cc
    unit/zelda3/dungeon/room_water_fill_state_test.cc
    unit/zelda3/dungeon/dungeon_rom_addresses_guardrail_test.cc
    unit/zelda3/dungeon/draw_routine_mapping_test.cc
    unit/zelda3/dungeon/draw_routine_bothbg_metadata_test.cc
    unit/zelda3/dungeon/object_drawing_comprehensive_test.cc
    unit/zelda3/dungeon/object_tile_editor_test.cc
    unit/zelda3/dungeon/room_graphics_palette_test.cc
    unit/zelda3/dungeon/room_header_palette_test.cc
    unit/zelda3/dungeon/object_drawer_registry_replay_test.cc
    unit/zelda3/dungeon/custom_object_room_render_test.cc
    unit/zelda3/dungeon/room_object_rom_parity_test.cc
    unit/dungeon_object_drawer_test.cc
    unit/object_selection_test.cc
    unit/editor/tile_object_handler_test.cc
    unit/editor/sprite_door_handler_test.cc
    unit/editor/interaction_coordinator_test.cc
    # Platform Tests
    platform/sdl3_audio_backend_test.cc
    platform/window_backend_test.cc
    platform/wasm_async_guard_test.cc
    ../src/cli/service/resources/resource_catalog.cc
    cli/service/resources/command_context_test.cc
    cli/service/resources/command_handler_test.cc
  )
  set(STABLE_INTEGRATION_TEST_SOURCES
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
    integration/zelda3/dungeon_room_render_parity_test.cc
    integration/zelda3/dungeon_palette_test.cc
    integration/zelda3/dungeon_room_test.cc
    integration/zelda3/minecart_audit_integration_test.cc
    integration/zelda3/oracle_workflow_integration_test.cc
    integration/zelda3/oracle_smoke_check_integration_test.cc
    integration/zelda3/sprite_position_test.cc
    integration/zelda3/message_test.cc
    integration/palette_manager_test.cc
    integration/object_selection_integration_test.cc
    integration/interaction_delegation_test.cc
  )
  if(YAZE_ENABLE_AGENT_CLI)
    list(APPEND STABLE_UNIT_TEST_SOURCES
      unit/cli/conversational_agent_service_test.cc
      unit/cli/ai_config_utils_test.cc
      unit/cli/ai_service_factory_test.cc
      unit/cli/model_registry_test.cc
      unit/cli/http_api_handlers_test.cc
      unit/cli/api_handlers_test.cc
      unit/cli/vision_action_refiner_test.cc
      unit/tools/build_tool_test.cc
      unit/tools/filesystem_tool_test.cc
      unit/tools/memory_inspector_tool_test.cc
      unit/tools/visual_analysis_tool_test.cc
      unit/tools/project_tool_test.cc
      unit/tools/project_graph_tool_test.cc
      unit/tools/code_gen_tool_test.cc
    )
  else()
    message(STATUS "Skipping agent CLI/tool tests (YAZE_ENABLE_AGENT_CLI=OFF)")
  endif()

  if(YAZE_BUILD_AGENT_UI)
    list(APPEND STABLE_UNIT_TEST_SOURCES
      unit/editor/agent_chat_test.cc
    )
  else()
    message(STATUS "Skipping agent UI tests (YAZE_BUILD_AGENT_UI=OFF)")
  endif()

  if(YAZE_ENABLE_ASAR)
    list(APPEND STABLE_UNIT_TEST_SOURCES
      unit/core/asar_wrapper_test.cc
      unit/core/asar_compiler_test.cc
    )
  else()
    message(STATUS "Skipping Asar-dependent tests (YAZE_ENABLE_ASAR=OFF)")
  endif()

  if(YAZE_ENABLE_Z3DK AND YAZE_Z3DK_FOUND)
    list(APPEND STABLE_UNIT_TEST_SOURCES
      unit/core/z3dk_smoke_test.cc
      unit/core/z3dk_wrapper_test.cc
      unit/core/z3dk_lint_test.cc
    )
    list(APPEND STABLE_INTEGRATION_TEST_SOURCES
      integration/z3dk_patch_manager_integration_test.cc
    )

    # Regression: cmake/dependencies/z3dk.cmake must FATAL_ERROR when
    # YAZE_ENABLE_Z3DK=ON and no z3dk source tree is reachable. If that
    # guard gets weakened to a warning, mac-ai-z3dk builds would silently
    # ship with no assembler backend. This test spawns a child cmake
    # configure against a fixture project with Z3DK_SRC_DIR pointed at a
    # non-existent path and asserts both non-zero exit and the expected
    # error message.
    # Use the legacy (positional) add_test form: it does not emit the
    # Ninja Multi-Config if(CTEST_CONFIGURATION_TYPE ...) guard that the
    # NAME form does. This test invokes a config-independent cmake -P
    # script, so we want it to show up in `ctest --preset mac-ai-z3dk-unit`
    # without needing -C Debug on every invocation.
    add_test(Z3dkMissingCheckoutFailsFast
      "${CMAKE_COMMAND}"
      "-DYAZE_ROOT=${CMAKE_SOURCE_DIR}"
      "-DFIXTURE_BIN_DIR=${CMAKE_BINARY_DIR}/_z3dk_missing_checkout_fixture"
      -P "${CMAKE_SOURCE_DIR}/test/cmake/z3dk_missing_checkout_driver.cmake"
    )
    set_tests_properties(Z3dkMissingCheckoutFailsFast PROPERTIES
      LABELS "stable;unit;cmake"
    )
  endif()

  yaze_add_test_suite(yaze_test_unit "stable;unit" OFF ${STABLE_UNIT_TEST_SOURCES})
  yaze_add_test_suite(yaze_test_integration "stable;integration" OFF ${STABLE_INTEGRATION_TEST_SOURCES})
  if(YAZE_ENABLE_ROM_TESTS)
    target_compile_definitions(yaze_test_integration PRIVATE
      YAZE_ENABLE_ROM_TESTS=1
    )
    if(YAZE_TEST_ROM_PATH)
      target_compile_definitions(yaze_test_integration PRIVATE
        YAZE_TEST_ROM_PATH="${YAZE_TEST_ROM_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_VANILLA_PATH)
      target_compile_definitions(yaze_test_integration PRIVATE
        YAZE_TEST_ROM_VANILLA_PATH="${YAZE_TEST_ROM_VANILLA_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_US_PATH)
      target_compile_definitions(yaze_test_integration PRIVATE
        YAZE_TEST_ROM_US_PATH="${YAZE_TEST_ROM_US_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_JP_PATH)
      target_compile_definitions(yaze_test_integration PRIVATE
        YAZE_TEST_ROM_JP_PATH="${YAZE_TEST_ROM_JP_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_EU_PATH)
      target_compile_definitions(yaze_test_integration PRIVATE
        YAZE_TEST_ROM_EU_PATH="${YAZE_TEST_ROM_EU_PATH}"
      )
    endif()
    if(YAZE_TEST_ROM_EXPANDED_PATH)
      target_compile_definitions(yaze_test_integration PRIVATE
        YAZE_TEST_ROM_EXPANDED_PATH="${YAZE_TEST_ROM_EXPANDED_PATH}"
      )
    endif()
  endif()

  # --- Quick Test Suites (Fast Iteration) ---
  # These suites are intentionally NOT labeled "stable" to avoid duplicate runs
  # when running `ctest -L stable`. Use `ctest -L quick` for fast feedback.
  set(QUICK_UNIT_CORE_TEST_SOURCES
    unit/core/hack_manifest_test.cc
    unit/core/oracle_progression_test.cc
    unit/core/oracle_progression_loader_test.cc
    unit/core/story_event_graph_test.cc
    unit/core/project_bundle_test.cc
    unit/core/project_paths_test.cc
    unit/core/project_bundle_path_test.cc
    unit/rom/rom_diff_test.cc
    unit/rom/write_fence_test.cc
    unit/emu/mesen_socket_client_test.cc
    unit/zelda3/resource_labels_test.cc
  )
  set(QUICK_UNIT_EDITOR_TEST_SOURCES
    unit/editor/dungeon_editor_v2_rom_safety_test.cc
    unit/editor/assembly_editor_symbol_jump_test.cc
    unit/editor/message/message_expanded_write_test.cc
    unit/editor/undo_manager_test.cc
    unit/editor/background_command_task_test.cc
    unit/editor/content_registry_workflow_history_test.cc
    unit/editor/command_palette_workflow_test.cc
    unit/editor/dungeon_undo_actions_test.cc
    unit/editor/editor_registry_test.cc
    unit/editor/sidebar_sort_test.cc
    unit/editor/activity_bar_actions_registry_test.cc
    unit/editor/theme_persistence_test.cc
    unit/editor/editor_manager_test.cc
    unit/editor/ui_coordinator_workflow_test.cc
    unit/editor/editor_manager_project_actions_test.cc
    unit/editor/editor_manager_rom_write_policy_test.cc
    unit/editor/editor_manager_write_conflict_test.cc
    unit/editor/editor_manager_oracle_rom_safety_test.cc
    unit/editor/editor_set_test.cc
    unit/editor/paint_util_test.cc
    unit/editor/dungeon_canvas_viewer_navigation_test.cc
    unit/editor/dungeon_workbench_toolbar_test.cc
    unit/editor/project_manager_validator_test.cc
    unit/editor/tile_painting_manager_test.cc
    unit/editor/tile16_editor_shortcuts_test.cc
    unit/editor/canvas_navigation_manager_test.cc
    unit/editor/map_refresh_coordinator_test.cc
    unit/cli/dungeon_object_validate_test.cc
    unit/cli/dungeon_collision_json_commands_test.cc
    unit/cli/dungeon_edit_commands_test.cc
    unit/cli/editor_completion_command_handlers_test.cc
    unit/zelda3/dungeon/water_fill_zone_test.cc
    unit/zelda3/dungeon/custom_collision_reserved_region_test.cc
    unit/zelda3/dungeon/custom_collision_json_test.cc
    unit/zelda3/dungeon/oracle_rom_safety_preflight_test.cc
    unit/zelda3/dungeon/save_all_collision_rom_presence_test.cc
    unit/zelda3/dungeon/room_water_fill_state_test.cc
    unit/zelda3/dungeon/dungeon_rom_addresses_guardrail_test.cc
    unit/zelda3/dungeon/room_object_set_id_test.cc
    unit/zelda3/dungeon/dungeon_limits_test.cc
    unit/zelda3/dungeon/dungeon_validator_test.cc
    unit/zelda3/dungeon/object_layer_semantics_test.cc
    unit/zelda3/dungeon/draw_routine_bothbg_metadata_test.cc
    unit/zelda3/dungeon/room_graphics_palette_test.cc
    unit/zelda3/dungeon/room_header_palette_test.cc
  )
  set(QUICK_INTEGRATION_TEST_SOURCES
    integration/zelda3/dungeon_save_region_test.cc
  )
  if(YAZE_ENABLE_ASAR)
    list(APPEND QUICK_UNIT_CORE_TEST_SOURCES
      unit/core/asar_wrapper_test.cc
    )
  endif()
  yaze_add_test_suite(yaze_test_quick_unit_core "quick;fast_unit;fast_unit_core" OFF ${QUICK_UNIT_CORE_TEST_SOURCES})
  yaze_add_test_suite(yaze_test_quick_unit_editor "quick;fast_unit;fast_unit_editor" OFF ${QUICK_UNIT_EDITOR_TEST_SOURCES})
  yaze_add_test_suite(yaze_test_quick_integration "quick;fast_integration" OFF ${QUICK_INTEGRATION_TEST_SOURCES})

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
    e2e/workflow_activity_widgets_test.cc
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
      COMMAND $<TARGET_FILE:yaze_test_gui> -nogui --fast WindowBrowser,WindowSidebarToggle,RightDrawer,WorkflowActivityWidgets
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
