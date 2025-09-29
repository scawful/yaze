set(YAZE_SRC_FILES "")
foreach (file 
  app/rom.cc
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_UTIL_SRC}
  ${YAZE_GUI_SRC})
  list(APPEND YAZE_SRC_FILES ${CMAKE_SOURCE_DIR}/src/${file})
endforeach()

# Only build test executable if tests are enabled
# Double-check to ensure tests are actually enabled
if(YAZE_BUILD_TESTS AND NOT YAZE_BUILD_TESTS STREQUAL "OFF")
  # Main test executable with enhanced argument handling for AI agents
  # Use CI version for minimal builds, full version for development
  if(YAZE_MINIMAL_BUILD)
    # CI/Minimal build: use simplified test executable
    add_executable(
      yaze_test
      yaze_test_ci.cc
      test_editor.cc
      test_editor.h
      testing.h
      test_utils.h
    
      # Unit Tests
      unit/core/asar_wrapper_test.cc
      unit/core/hex_test.cc
      unit/rom/rom_test.cc
      unit/gfx/snes_tile_test.cc
      unit/gfx/compression_test.cc
      unit/gfx/snes_palette_test.cc
      unit/zelda3/message_test.cc
      unit/zelda3/overworld_test.cc
      unit/zelda3/object_parser_test.cc
      unit/zelda3/object_parser_structs_test.cc
      unit/zelda3/sprite_builder_test.cc
      unit/zelda3/sprite_position_test.cc
      unit/zelda3/test_dungeon_objects.cc
      unit/zelda3/dungeon_component_unit_test.cc
      
      # Integration Tests
      integration/asar_integration_test.cc
      integration/asar_rom_test.cc
      integration/dungeon_editor_test.cc
      integration/dungeon_editor_test.h
      integration/editor/tile16_editor_test.cc
      integration/editor/editor_integration_test.cc
      integration/editor/editor_integration_test.h
      
      # E2E Tests (excluded in CI builds)
      e2e/rom_dependent/e2e_rom_test.cc
      e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
      
      # Legacy Integration Tests (to be migrated)
      unit/zelda3/comprehensive_integration_test.cc
      unit/zelda3/overworld_integration_test.cc
      unit/zelda3/dungeon_integration_test.cc
      unit/zelda3/dungeon_editor_system_integration_test.cc
      unit/zelda3/dungeon_object_renderer_integration_test.cc
      unit/zelda3/dungeon_object_renderer_mock_test.cc
      unit/zelda3/dungeon_object_rendering_tests.cc
      unit/zelda3/dungeon_room_test.cc
    )
  else()
    # Development build: use full-featured test executable
    add_executable(
      yaze_test
      yaze_test.cc
      test_editor.cc
      test_editor.h
      testing.h
      test_utils.h
      
      # Unit Tests
      unit/core/asar_wrapper_test.cc
      unit/core/hex_test.cc
      unit/rom/rom_test.cc
      unit/gfx/snes_tile_test.cc
      unit/gfx/compression_test.cc
      unit/gfx/snes_palette_test.cc
      unit/zelda3/message_test.cc
      unit/zelda3/overworld_test.cc
      unit/zelda3/object_parser_test.cc
      unit/zelda3/object_parser_structs_test.cc
      unit/zelda3/sprite_builder_test.cc
      unit/zelda3/sprite_position_test.cc
      unit/zelda3/test_dungeon_objects.cc
      unit/zelda3/dungeon_component_unit_test.cc
      
      # Integration Tests
      integration/asar_integration_test.cc
      integration/asar_rom_test.cc
      integration/dungeon_editor_test.cc
      integration/dungeon_editor_test.h
      integration/editor/tile16_editor_test.cc
      integration/editor/editor_integration_test.cc
      integration/editor/editor_integration_test.h
      
      # E2E Tests (included in development builds)
      e2e/rom_dependent/e2e_rom_test.cc
      e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
      
      # Legacy Integration Tests (to be migrated)
      unit/zelda3/comprehensive_integration_test.cc
      unit/zelda3/overworld_integration_test.cc
      unit/zelda3/dungeon_integration_test.cc
      unit/zelda3/dungeon_editor_system_integration_test.cc
      unit/zelda3/dungeon_object_renderer_integration_test.cc
      unit/zelda3/dungeon_object_renderer_mock_test.cc
      unit/zelda3/dungeon_object_rendering_tests.cc
      unit/zelda3/dungeon_room_test.cc
    )
  endif()

# Add vanilla value extraction utility (only for local development with ROM access)
# IMPORTANT: Do not build in CI/release - this is a development-only utility
if(NOT YAZE_MINIMAL_BUILD AND YAZE_ENABLE_ROM_TESTS AND NOT DEFINED ENV{GITHUB_ACTIONS})
  add_executable(
    extract_vanilla_values
    unit/zelda3/extract_vanilla_values.cc
    ${YAZE_SRC_FILES}
  )

  target_include_directories(
    extract_vanilla_values PUBLIC
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${SDL2_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
  )

  target_link_libraries(
    extract_vanilla_values
    yaze_core
    ${SDL_TARGETS}
    asar-static
    ${ABSL_TARGETS}
    ${PNG_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ImGui
  )
  
  # Add rom_patch_utility as a separate executable
  add_executable(
    rom_patch_utility
    unit/zelda3/rom_patch_utility.cc
    ${YAZE_SRC_FILES}
  )
  
  target_include_directories(
    rom_patch_utility PUBLIC
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${SDL2_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
  )
  
  target_link_libraries(
    rom_patch_utility
    yaze_core
    ${SDL_TARGETS}
    asar-static
    ${ABSL_TARGETS}
    ${PNG_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ImGui
  )
endif()

  # Configure test executable only when tests are enabled
  target_include_directories(
    yaze_test PUBLIC
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/test/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${SDL2_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
  )

  target_link_libraries(
    yaze_test
    ${SDL_TARGETS}
    asar-static
    ${ABSL_TARGETS}
    ${PNG_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ImGui
    gmock_main
    gmock
    gtest_main
    gtest
  )

  # Link core library for essential functionality (BPS, ASAR, etc.)
  if(YAZE_BUILD_LIB)
    target_link_libraries(yaze_test yaze_core)
  endif()

  # Conditionally link ImGuiTestEngine only when UI tests are enabled
  if(YAZE_ENABLE_UI_TESTS)
    target_link_libraries(yaze_test ${IMGUI_TEST_ENGINE_TARGET})
    target_compile_definitions(yaze_test PRIVATE ${IMGUI_TEST_ENGINE_DEFINITIONS})
  endif()

  # ROM Testing Configuration
  if(YAZE_ENABLE_ROM_TESTS)
      target_compile_definitions(yaze_test PRIVATE 
          YAZE_ENABLE_ROM_TESTS=1
          YAZE_TEST_ROM_PATH="${YAZE_TEST_ROM_PATH}"
      )
  endif()

  # Platform-specific definitions
  if(UNIX AND NOT APPLE)
      target_compile_definitions(yaze_test PRIVATE "linux" "stricmp=strcasecmp")
  elseif(APPLE)
      target_compile_definitions(yaze_test PRIVATE "MACOS" "stricmp=strcasecmp")
  elseif(WIN32)
      target_compile_definitions(yaze_test PRIVATE "WINDOWS")
      # Increase stack size on Windows to prevent stack overflow during tests
      if(MSVC)
        target_link_options(yaze_test PRIVATE /STACK:8388608)  # 8MB stack
      elseif(MINGW)
        target_link_options(yaze_test PRIVATE -Wl,--stack,8388608)
      endif()
  endif()
endif()

# Configure test discovery with efficient labeling for CI/CD
# Only discover tests if tests are enabled
if(YAZE_BUILD_TESTS AND NOT YAZE_BUILD_TESTS STREQUAL "OFF")
  include(GoogleTest)
  
  # Discover all tests with default properties
  gtest_discover_tests(yaze_test)
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
)

# Unit Tests
source_group("Tests\\Unit" FILES
  unit/test_asar_wrapper.cc
  unit/test_rom_loading.cc
  unit/test_snes_tiles.cc
  unit/test_palettes.cc
  unit/test_hex_utils.cc
  unit/test_flag_utils.cc
  unit/test_bps_utils.cc
  unit/test_color_conversion.cc
  unit/test_tile_compression.cc
  unit/test_memory_management.cc
  unit/test_project_structure.cc
  unit/test_editor_basic.cc
  unit/test_dungeon_data.cc
  unit/test_overworld_data.cc
  unit/test_sprite_data.cc
  unit/test_music_data.cc
  unit/test_graphics_rendering.cc
  unit/test_gui_components.cc
  unit/test_emulator_core.cc
  unit/test_cpu_instructions.cc
  unit/test_ppu_rendering.cc
  unit/test_audio_processing.cc
  unit/test_compression_algorithms.cc
  unit/test_hex_editor.cc
)

# Integration Tests
source_group("Tests\\Integration" FILES
  integration/test_editor_integration.cc
  integration/test_rom_integration.cc
  integration/test_project_workflow.cc
  integration/test_asar_integration.cc
  integration/test_graphics_pipeline.cc
  integration/test_emulator_integration.cc
)

# End-to-End Tests
source_group("Tests\\E2E" FILES
  e2e/test_full_workflow.cc
  e2e/test_user_scenarios.cc
)

# Test Utilities and Mocks
source_group("Tests\\Utilities" FILES
  test_utils.h
  mocks/mock_rom.h
  mocks/mock_editor.h
)

# Test Assets
source_group("Tests\\Assets" FILES
  assets/test_rom.asm
)