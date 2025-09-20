
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
  gfx/snes_tile_test.cc
  gfx/compression_test.cc
  gfx/snes_palette_test.cc
  zelda3/message_test.cc
  zelda3/overworld_test.cc
  zelda3/sprite_builder_test.cc
  unit/object_parser_test.cc
  unit/improved_object_renderer_test.cc
  integration/dungeon_editor_test.cc
  integration/dungeon_object_rendering_integration_test.cc
  ${ASAR_STATIC_SRC}
  ${IMGUI_SRC}
  ${IMGUI_TEST_ENGINE_SOURCES}
  ${YAZE_SRC_FILES}
)

target_include_directories(
  yaze_test PUBLIC
  app/
  lib/
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${ASAR_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  yaze_test
  SDL2::SDL2
  asar-static
  ${ABSL_TARGETS}
  ${PNG_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  yaze_c
  ImGuiTestEngine
  ImGui
  gmock_main
  gmock
  gtest_main
  gtest
)
target_compile_definitions(yaze_test PRIVATE "linux")
target_compile_definitions(yaze_test PRIVATE "stricmp=strcasecmp")
target_compile_definitions(yaze_test PRIVATE "IMGUI_ENABLE_TEST_ENGINE")

include(GoogleTest)
gtest_discover_tests(yaze_test)