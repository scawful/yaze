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
  ../src/app/gfx/bitmap.cc
  ../src/app/gfx/snes_tile.cc
  ../src/app/gfx/snes_palette.cc
  ../src/app/core/common.cc
)

target_include_directories(
  yaze_test PUBLIC
  ../src/lib/
  ../src/
  ${SDL_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_test
  absl::strings
  absl::flags
  absl::status
  absl::statusor
  absl::failure_signal_handler
  absl::examine_stack
  absl::stacktrace
  absl::base
  absl::config
  absl::core_headers
  absl::raw_logging_internal
  ${SDL2_LIBRARIES}
  ${OPENGL_LIBRARIES}
  gtest_main
  gtest
)

include(GoogleTest)
gtest_discover_tests(yaze_test)