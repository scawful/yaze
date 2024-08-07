# GoogleTest ------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()


add_executable(
  yaze_test
  test/yaze_test.cc
  test/libc_test.cc
  test/rom_test.cc
  test/emu/cpu_test.cc
  test/emu/spc700_test.cc
  test/emu/ppu_test.cc
  test/gfx/compression_test.cc
  test/gfx/snes_palette_test.cc
  test/zelda3/room_object_test.cc
  test/zelda3/sprite_builder_test.cc
  cli/command_handler.cc
  app/rom.cc
  app/core/common.cc
  app/core/labeling.cc
  app/editor/utils/gfx_context.cc
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_GUI_SRC}
  lib/imgui/misc/cpp/imgui_stdlib.cpp
  # ${ASAR_STATIC_SRC}
)

target_include_directories(
  yaze_test PUBLIC
  app/
  lib/
  ${SDL2_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_test
  SDL2::SDL2
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