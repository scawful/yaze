include(../cmake/gtest.cmake)

add_executable(
  yaze_test
  test/yaze_test.cc
  test/rom_test.cc
  test/gfx/compression_test.cc
  test/gfx/snes_palette_test.cc
  test/integration/test_editor.cc
  test/zelda3/overworld_test.cc
  test/zelda3/sprite_builder_test.cc
  app/rom.cc
  app/core/common.cc
  ${ASAR_STATIC_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
  ${IMGUI_TEST_ENGINE_SOURCES}
)

target_include_directories(
  yaze_test PUBLIC
  app/
  lib/
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${ASAR_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
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