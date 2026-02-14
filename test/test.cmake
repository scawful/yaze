
# Configure test discovery with efficient labeling for CI/CD
# Only discover tests if tests are enabled
if(YAZE_BUILD_TESTS AND NOT YAZE_BUILD_TESTS STREQUAL "OFF")
  include(GoogleTest)
  
  # Discover all tests and apply default labels using PROPERTIES argument
  # This ensures all tests get the ALL_TESTS label immediately
  if(WIN32)
    gtest_discover_tests(yaze_test
      TEST_LIST yaze_discovered_tests
      DISCOVERY_TIMEOUT 60
      NO_PRETTY_TYPES
      PROPERTIES
        TIMEOUT 300
        LABELS "ALL_TESTS;UNIT_TEST;STABLE;ASAR_TEST;INTEGRATION_TEST;E2E_TEST;ROM_TEST;ZSCUSTOM_TEST;CLI_TEST;MISC_TEST"
      TEST_PREFIX ""
      TEST_SUFFIX ""
    )
  else()
    gtest_discover_tests(yaze_test
      TEST_LIST yaze_discovered_tests
      PROPERTIES
        LABELS "ALL_TESTS;UNIT_TEST;STABLE;ASAR_TEST;INTEGRATION_TEST;E2E_TEST;ROM_TEST;ZSCUSTOM_TEST;CLI_TEST;MISC_TEST"
      TEST_PREFIX ""
      TEST_SUFFIX ""
    )
  endif()
  
  # Note: Due to CMake's bracket argument syntax limitations, we cannot dynamically
  # apply labels to tests with bracket-quoted names in post-processing scripts.
  # All tests get all possible labels initially, and can be filtered using test presets
  # in CMakePresets.json which use label-based filtering via ctest -L option.
  #
  # Test categorization is done via naming conventions:
  # - Tests matching "*IntegrationTest*" -> Integration tests  
  # - Tests matching "E2ERomDependentTest.*" -> E2E + ROM tests
  # - Tests matching "ZSCustomOverworldUpgradeTest.*" -> E2E + ROM + ZSCustom tests
  # - Tests matching "RomTest.*" or "*RomIntegrationTest*" -> ROM tests
  # - Tests matching "*Asar*" -> Asar tests
  # - Tests matching "ResourceCatalogTest*" -> CLI tests
  # - All others -> Unit tests
  #
  # Test presets use these labels for filtering (see CMakePresets.json)
else()
  # Tests are disabled - don't build test executable or discover tests
  message(STATUS "Tests disabled - skipping test executable and discovery")
endif()

# Test organization and labeling for CI/CD
# Note: Test labeling is handled through the enhanced yaze_test executable
# which supports filtering by test categories using command line arguments:
# --unit, --integration, --e2e, --rom-dependent, --zscustomoverworld, etc.
#
# For CI/CD, use the test runner with appropriate filters:
# ./yaze_test --unit --verbose
# ./yaze_test --e2e --rom-vanilla roms/alttp_vanilla.sfc
# ./yaze_test --zscustomoverworld --verbose

# =============================================================================
# Test Source Groups for Visual Studio Organization
# =============================================================================

# Test Framework
source_group("Tests\\Framework" FILES
  yaze_test.cc
  yaze_test_ci.cc
  test_editor.cc
  test_editor.h
  browser_ai_test.cc
  test_conversation_minimal.cc
)

# Unit Tests
source_group("Tests\\Unit" FILES
  unit/core/asar_wrapper_test.cc
  unit/core/asm_patch_test.cc
  unit/core/hex_test.cc
  unit/cli/resource_catalog_test.cc
  unit/cli/rom_debug_agent_test.cc
  unit/cli/tile16_proposal_generator_test.cc
  unit/rom/rom_test.cc
  unit/gfx/snes_tile_test.cc
  unit/gfx/compression_test.cc
  unit/gfx/snes_palette_test.cc
  unit/snes_color_test.cc
  unit/gui/tile_selector_widget_test.cc
  unit/gui/canvas_automation_api_test.cc
  unit/gui/canvas_coordinate_sync_test.cc
  unit/zelda3/overworld_test.cc
  unit/zelda3/overworld_regression_test.cc
  unit/zelda3/overworld_version_helper_test.cc
  unit/zelda3/resource_labels_test.cc
  unit/diggable_tiles_test.cc
  unit/zelda3/object_parser_test.cc
  unit/zelda3/object_parser_structs_test.cc
  unit/zelda3/sprite_builder_test.cc
  unit/zelda3/music_parser_test.cc
  unit/zelda3/dungeon_component_unit_test.cc
  unit/zelda3/dungeon/room_object_encoding_test.cc
  unit/zelda3/dungeon/room_manipulation_test.cc
  unit/zelda3/dungeon/dungeon_save_test.cc
  unit/zelda3/dungeon/sprite_relocation_test.cc
  unit/zelda3/dungeon/object_geometry_test.cc
  unit/zelda3/dungeon/bpp_conversion_test.cc
  unit/zelda3/test_dungeon_objects.h
  unit/emu/disassembler_test.cc
  unit/emu/step_controller_test.cc
  unit/emu/apu_dsp_test.cc
  unit/emu/apu_ipl_handshake_test.cc
  unit/emu/spc700_reset_test.cc
  unit/tools/build_tool_test.cc
  unit/tools/filesystem_tool_test.cc
  unit/tools/memory_inspector_tool_test.cc
  unit/editor/message/message_data_test.cc
  unit/sdl3_audio_backend_test.cc
  unit/wasm_patch_export_test.cc
)

# Integration Tests
source_group("Tests\\Integration" FILES
  integration/asar_integration_test.cc
  integration/asar_rom_test.cc
  integration/dungeon_editor_test.cc
  integration/dungeon_editor_test.h
  integration/dungeon_editor_v2_test.cc
  integration/dungeon_editor_v2_test.h
  integration/editor/tile16_editor_test.cc
  integration/editor/editor_integration_test.cc
  integration/editor/editor_integration_test.h
  integration/agent/tool_dispatcher_test.cc
  integration/palette_manager_test.cc
  integration/memory_debugging_test.cc
  integration/wasm_message_queue_test.cc
  integration/emulator_object_preview_test.cc
  integration/emulator_render_service_test.cc
)

# Integration Tests (Zelda3)
source_group("Tests\\Integration\\Zelda3" FILES
  integration/zelda3/overworld_integration_test.cc
  integration/zelda3/dungeon_editor_system_integration_test.cc
  integration/zelda3/dungeon_object_rendering_tests.cc
  integration/zelda3/dungeon_object_rendering_tests_new.cc
  integration/zelda3/room_integration_test.cc
  integration/zelda3/dungeon_save_region_test.cc
  integration/zelda3/dungeon_room_test.cc
  integration/zelda3/dungeon_palette_test.cc
  integration/zelda3/sprite_position_test.cc
  integration/zelda3/message_test.cc
)

# End-to-End Tests
source_group("Tests\\E2E" FILES
  e2e/canvas_selection_test.cc
  e2e/canvas_selection_test.h
  e2e/dungeon_canvas_interaction_test.cc
  e2e/dungeon_canvas_interaction_test.h
  e2e/dungeon_e2e_tests.cc
  e2e/dungeon_e2e_tests.h
  e2e/dungeon_editor_smoke_test.cc
  e2e/dungeon_editor_smoke_test.h
  e2e/dungeon_layer_rendering_test.cc
  e2e/dungeon_layer_rendering_test.h
  e2e/dungeon_visual_verification_test.cc
  e2e/dungeon_visual_verification_test.h
  e2e/dungeon_object_drawing_test.cc
  e2e/dungeon_object_drawing_test.h
  e2e/framework_smoke_test.cc
  e2e/framework_smoke_test.h
  e2e/imgui_test_engine_demo.cc
  e2e/imgui_test_engine_demo.h
  e2e/ai_multimodal_test.cc
  e2e/ai_multimodal_test.h
  e2e/emulator_stepping_test.cc
  e2e/emulator_stepping_test.h
  e2e/test_helpers.h
  e2e/overworld/overworld_e2e_test.cc
  e2e/rom_dependent/e2e_rom_test.cc
  e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
)

# Deprecated Tests
# These files exist but are marked for deprecation/archival.
# They are excluded from the main test suite but kept for reference.
# See individual file headers for deprecation reasons and replacements.
source_group("Tests\\Deprecated" FILES
  # Deprecated Nov 2025 - replaced by integration/zelda3/dungeon_object_rendering_tests.cc
  integration/zelda3/dungeon_rendering_test.cc
  unit/zelda3/dungeon/object_rendering_test.cc
  # Deprecated Nov 2025 - outdated DungeonEditor architecture, see dungeon_editor_smoke_test.cc
  e2e/dungeon_object_rendering_e2e_tests.cc
)

# Benchmarks
source_group("Tests\\Benchmarks" FILES
  benchmarks/gfx_optimization_benchmarks.cc
)

# Test Utilities and Mocks
source_group("Tests\\Utilities" FILES
  testing.h
  test_utils.h
  gui_test_utils.cc
  mocks/mock_rom.h
  mocks/mock_memory.h
  test_utils/rom_integrity_validator.h
  test_utils/mock_rom_generator.h
)

# AI Tests
source_group("Tests\\AI" FILES
  integration/ai/ai_gui_controller_test.cc
  integration/ai/test_gemini_vision.cc
  integration/ai/test_ai_tile_placement.cc
)

# Platform Tests
source_group("Tests\\Platform" FILES
  platform/wasm_error_handler_test.cc
  standalone/test_sdl3_audio_compile.cc
)

# CLI Tests
source_group("Tests\\CLI" FILES
  cli/service/resources/command_context_test.cc
)

# Inspection Tests
source_group("Tests\\Inspection" FILES
  inspection/dungeon_palette_inspection_test.cc
)

# Test Assets
source_group("Tests\\Assets" FILES
  assets/test_patch.asm
)
