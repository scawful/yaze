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
      unit/cli/resource_catalog_test.cc
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
      unit/zelda3/dungeon/room_object_encoding_test.cc
      zelda3/dungeon/room_manipulation_test.cc

      # CLI Services (for catalog serialization tests)
      ../src/cli/service/resources/resource_catalog.cc
      
      # Integration Tests
      integration/asar_integration_test.cc
      integration/asar_rom_test.cc
      integration/dungeon_editor_test.cc
      integration/dungeon_editor_test.h
      integration/dungeon_editor_v2_test.cc
      integration/dungeon_editor_v2_test.h
      integration/editor/tile16_editor_test.cc
      integration/editor/editor_integration_test.cc
      integration/editor/editor_integration_test.h
      
      # E2E Tests (excluded in CI builds)
      e2e/rom_dependent/e2e_rom_test.cc
      e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
      
      # Deprecated Tests (formerly legacy)
      deprecated/comprehensive_integration_test.cc
      deprecated/dungeon_integration_test.cc

      # Integration Tests (Zelda3)
      integration/zelda3/overworld_integration_test.cc
      integration/zelda3/dungeon_editor_system_integration_test.cc
      integration/zelda3/dungeon_object_renderer_integration_test.cc
      integration/zelda3/room_integration_test.cc
      
      # Mock/Unit Tests for Zelda3
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
      test_utils.cc
      
      # Unit Tests
      unit/core/asar_wrapper_test.cc
      unit/core/hex_test.cc
      unit/cli/resource_catalog_test.cc
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
      unit/zelda3/dungeon/room_object_encoding_test.cc
      zelda3/dungeon/room_manipulation_test.cc

      # CLI Services (for catalog serialization tests)
      ../src/cli/service/resources/resource_catalog.cc
      
      # Integration Tests
      integration/asar_integration_test.cc
      integration/asar_rom_test.cc
      integration/dungeon_editor_test.cc
      integration/dungeon_editor_test.h
      integration/dungeon_editor_v2_test.cc
      integration/dungeon_editor_v2_test.h
      integration/editor/tile16_editor_test.cc
      integration/editor/editor_integration_test.cc
      integration/editor/editor_integration_test.h
      
      # E2E Tests (included in development builds)
      e2e/canvas_selection_test.cc
      e2e/framework_smoke_test.cc
      e2e/dungeon_editor_smoke_test.cc
      e2e/rom_dependent/e2e_rom_test.cc
      e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc
      
      # Deprecated Tests (formerly legacy)
      deprecated/comprehensive_integration_test.cc
      deprecated/dungeon_integration_test.cc

      # Integration Tests (Zelda3)
      integration/zelda3/overworld_integration_test.cc
      integration/zelda3/dungeon_editor_system_integration_test.cc
      integration/zelda3/dungeon_object_renderer_integration_test.cc
      integration/zelda3/room_integration_test.cc
      
      # Mock/Unit Tests for Zelda3
      unit/zelda3/dungeon_object_renderer_mock_test.cc
      unit/zelda3/dungeon_object_rendering_tests.cc
      unit/zelda3/dungeon_room_test.cc

      # Benchmarks
      benchmarks/gfx_optimization_benchmarks.cc
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
    gmock_main
    gmock
    gtest_main
    gtest
  )

  # Link core library for essential functionality (BPS, ASAR, etc.)
  if(YAZE_BUILD_LIB)
    if(YAZE_USE_MODULAR_BUILD)
      target_link_libraries(yaze_test
        yaze_util
        yaze_gfx
        yaze_gui
        yaze_zelda3
        yaze_core_lib
        yaze_editor
      )

      if(TARGET yaze_agent)
        target_link_libraries(yaze_test yaze_agent)
      endif()

      if(YAZE_BUILD_EMU AND TARGET yaze_emulator)
        target_link_libraries(yaze_test yaze_emulator)
      endif()
    else()
      target_link_libraries(yaze_test yaze_core)
    endif()
  endif()

  if(TARGET yaze_test_support)
    target_link_libraries(yaze_test yaze_test_support)
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
      # Use 16MB stack to handle deep call stacks in tests and test discovery
      message(STATUS "Configuring Windows stack size for yaze_test to 16MB")
      if(MSVC)
        target_link_options(yaze_test PRIVATE /STACK:16777216)  # 16MB stack
        message(STATUS "  Using MSVC linker flag: /STACK:16777216")
      elseif(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(yaze_test PRIVATE -Wl,--stack,16777216)  # 16MB stack
        message(STATUS "  Using MinGW/GNU linker flag: -Wl,--stack,16777216")
      else()
        # Fallback for other compilers
        target_link_options(yaze_test PRIVATE -Wl,--stack,16777216)
        message(STATUS "  Using fallback linker flag: -Wl,--stack,16777216")
      endif()
  endif()
endif()

include(test.cmake)
