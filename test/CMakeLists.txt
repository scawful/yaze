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
  yaze_test.cc
  emu/cpu_test.cc
  emu/spc700_test.cc
  emu/ppu_test.cc
  compression_test.cc
  snes_palette_test.cc
  room_object_test.cc
  ../src/cli/patch.cc
  ../src/cli/command_handler.cc
  ../src/app/rom.cc
  ../src/app/emu/cpu/cpu.cc
  ../src/app/emu/cpu/internal/instructions.cc
  ../src/app/emu/cpu/internal/addressing.cc
  ../src/app/emu/audio/internal/addressing.cc
  ../src/app/emu/audio/internal/instructions.cc
  ../src/app/emu/audio/apu.cc
  ../src/app/emu/video/ppu.cc
  ../src/app/emu/audio/dsp.cc
  ../src/app/emu/audio/spc700.cc
  ../src/app/gfx/bitmap.cc
  ../src/app/gfx/snes_tile.cc
  ../src/app/gfx/snes_palette.cc
  ../src/app/gfx/compression.cc
  ../src/app/core/common.cc
  ../src/app/core/labeling.cc
  # ${ASAR_STATIC_SRC}
)

target_include_directories(
  yaze_test PUBLIC
  ../src/
  ../src/lib/
  # ../src/lib/asar/src/asar/
  ${SDL2_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_test
  ${ABSL_TARGETS}
  SDL2::SDL2
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  # asar-static
  # snes_spc
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