# GoogleTest ------------------------------------------------------------------------------------
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
  rom_test.cc
  ../src/app/rom.cc
  ../src/app/asm/script.cc
  ../src/app/gfx/bitmap.cc
  ../src/app/gfx/snes_tile.cc
  ../src/app/gfx/snes_palette.cc
  ../src/app/core/common.cc
  ../src/lib/asar/src/asar-dll-bindings/c/asardll.c
)

target_include_directories(
  yaze_test PUBLIC
  ../src/
  ../src/lib/
  ../src/lib/asar/src/asar-dll-bindings/c
  ${SDL_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_test
  SDL2::SDL2
  ${ABSL_TARGETS}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  gmock_main
  gmock
  gtest_main
  gtest
)

include(GoogleTest)
gtest_discover_tests(yaze_test)