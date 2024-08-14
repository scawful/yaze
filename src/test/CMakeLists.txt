include(../cmake/gtest.cmake)

add_executable(
  yaze_test
  test/yaze_test.cc
  test/libc_test.cc
  test/rom_test.cc
  test/gfx/compression_test.cc
  test/gfx/snes_palette_test.cc
  test/integration/test_editor.cc
  test/zelda3/room_object_test.cc
  test/zelda3/sprite_builder_test.cc
  cli/command_handler.cc
  app/rom.cc
  app/core/common.cc
  app/core/labeling.cc
  app/editor/utils/gfx_context.cc
  ${ASAR_STATIC_SRC}
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
  ${IMGUI_TEST_ENGINE_SOURCES}
)

target_include_directories(
  yaze_test PUBLIC
  app/
  lib/
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
  ${GLEW_LIBRARIES}
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

include(GoogleTest)
gtest_discover_tests(yaze_test)