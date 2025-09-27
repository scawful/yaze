
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

add_executable(
  yaze_test
  yaze_test.cc
  rom_test.cc
  test_editor.cc
  hex_test.cc
  core/asar_wrapper_test.cc
  gfx/snes_tile_test.cc
  gfx/compression_test.cc
  gfx/snes_palette_test.cc
  zelda3/message_test.cc
  zelda3/overworld_test.cc
  zelda3/overworld_integration_test.cc
  zelda3/comprehensive_integration_test.cc
  zelda3/dungeon_integration_test.cc
  zelda3/dungeon_object_renderer_integration_test.cc
  zelda3/dungeon_object_renderer_mock_test.cc
  zelda3/dungeon_editor_system_integration_test.cc
  zelda3/sprite_builder_test.cc
  zelda3/sprite_position_test.cc
  emu/cpu_test.cc
  emu/ppu_test.cc
  emu/spc700_test.cc
  emu/audio/apu_test.cc
  emu/audio/ipl_handshake_test.cc
  integration/dungeon_editor_test.cc
  dungeon_component_unit_test.cc
  integration/asar_integration_test.cc
  integration/asar_rom_test.cc
  editor/tile16_editor_test.cc
  zelda3/object_parser_test.cc
  zelda3/object_parser_structs_test.cc
  zelda3/test_dungeon_objects.cc
)

# Add vanilla value extraction utility (only for local development with ROM access)
if(NOT YAZE_MINIMAL_BUILD AND YAZE_ENABLE_ROM_TESTS)
  add_executable(
    extract_vanilla_values
    zelda3/extract_vanilla_values.cc
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
    ${SDL_TARGETS}
    asar-static
    ${ABSL_TARGETS}
    ${PNG_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CMAKE_DL_LIBS}
  )

  # Conditionally link yaze_c only when library is built
  if(YAZE_BUILD_LIB)
    target_link_libraries(extract_vanilla_values yaze_c)
  endif()
endif()

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

# ImGui Test Engine definitions are now handled conditionally above

# Platform-specific definitions
if(UNIX AND NOT APPLE)
    target_compile_definitions(yaze_test PRIVATE "linux" "stricmp=strcasecmp")
elseif(APPLE)
    target_compile_definitions(yaze_test PRIVATE "MACOS" "stricmp=strcasecmp")
elseif(WIN32)
    target_compile_definitions(yaze_test PRIVATE "WINDOWS")
endif()

include(GoogleTest)

# Configure test discovery with efficient labeling for CI/CD
include(GoogleTest)

# Discover all tests with default properties
gtest_discover_tests(yaze_test)

# Add test labels using a simpler approach
# Note: Test names might have prefixes, we'll use regex patterns for CI