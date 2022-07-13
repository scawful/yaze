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

find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)

set(SNESHACKING_PATH "../src/lib/sneshacking/src")

add_executable(
  yaze_test
  yaze_test.cc
  rom_test.cc
  ../src/app/rom.cc
  ../src/app/gfx/snes_tile.cc
  ../src/app/gfx/snes_palette.cc
  ../src/app/core/common.cc
  ${SNESHACKING_PATH}/compressions/alttpcompression.c
  ${SNESHACKING_PATH}/compressions/stdnintendo.c
  ${SNESHACKING_PATH}/tile.c
  ${SNESHACKING_PATH}/tilepng.c
  ${SNESHACKING_PATH}/palette.c
  ${SNESHACKING_PATH}/rommapping.c
  ${SNESHACKING_PATH}/mapping_lorom.c
)

target_include_directories(
  yaze_test PUBLIC
  ../src/lib/
  ../src/
  ${SNESHACKING_PATH}
  ${SDL_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_test
  ${PNG_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${OPENGL_LIBRARIES}
  NintendoCompression
  gtest_main
  gtest
)

include(GoogleTest)
gtest_discover_tests(yaze_test)