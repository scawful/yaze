
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
# ./yaze_test --e2e --rom-path zelda3.sfc
# ./yaze_test --zscustomoverworld --verbose

# =============================================================================
# Test Source Groups for Visual Studio Organization
# =============================================================================

# Test Framework
source_group("Tests\\Framework" FILES
  testing.h
  yaze_test.cc
  yaze_test_ci.cc
  test_editor.cc
  test_editor.h
)

# Unit Tests
source_group("Tests\\Unit" FILES
  unit/core/asar_wrapper_test.cc
  unit/core/hex_test.cc
  unit/cli/resource_catalog_test.cc
  unit/rom/rom_test.cc
  unit/gfx/snes_tile_test.cc
  unit/gfx/compression_test.cc
  unit/gfx/snes_palette_test.cc
  unit/gui/tile_selector_widget_test.cc
  unit/zelda3/message_test.cc
  unit/zelda3/overworld_test.cc
  unit/zelda3/object_parser_test.cc
  unit/zelda3/object_parser_structs_test.cc
  unit/zelda3/sprite_builder_test.cc
  unit/zelda3/sprite_position_test.cc
  unit/zelda3/test_dungeon_objects.cc
  unit/zelda3/dungeon_component_unit_test.cc
      unit/zelda3/dungeon/room_object_encoding_test.cc
      zelda3/dungeon/room_manipulation_test.cc
  unit/zelda3/dungeon_object_renderer_mock_test.cc
  unit/zelda3/dungeon_object_rendering_tests.cc
  unit/zelda3/dungeon_room_test.cc
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
)

# Integration Tests (Zelda3)
source_group("Tests\\Integration\\Zelda3" FILES
  integration/zelda3/overworld_integration_test.cc
  integration/zelda3/dungeon_editor_system_integration_test.cc
  integration/zelda3/dungeon_object_renderer_integration_test.cc
  integration/zelda3/room_integration_test.cc
)

# End-to-End Tests
source_group("Tests\\E2E" FILES
  e2e/canvas_selection_test.cc
  e2e/framework_smoke_test.cc
  e2e/rom_dependent/e2e_rom_test.cc
  e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
)

# Deprecated Tests
source_group("Tests\\Deprecated" FILES
  deprecated/comprehensive_integration_test.cc
  deprecated/dungeon_integration_test.cc
)

# Benchmarks
source_group("Tests\\Benchmarks" FILES
  benchmarks/gfx_optimization_benchmarks.cc
)

# Test Utilities and Mocks
source_group("Tests\\Utilities" FILES
  test_utils.h
  test_utils.cc
  mocks/mock_rom.h
  mocks/mock_memory.h
)

# Test Assets
source_group("Tests\\Assets" FILES
  assets/test_patch.asm
)