# This file defines the yaze_emu standalone executable.
# Note: The yaze_emulator library is ALWAYS built (via emu_library.cmake)
#       because it's used by the main yaze app and test suites.
#       This file only controls the STANDALONE emulator executable.

if(YAZE_BUILD_EMU AND NOT YAZE_MINIMAL_BUILD)
  if(APPLE)
    add_executable(yaze_emu MACOSX_BUNDLE app/emu/emu.cc app/platform/app_delegate.mm)
    target_link_libraries(yaze_emu PUBLIC "-framework Cocoa")
  else()
    add_executable(yaze_emu app/emu/emu.cc)
  endif()

  target_include_directories(yaze_emu PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${PROJECT_BINARY_DIR}
  )

  target_link_libraries(yaze_emu PRIVATE
    yaze_editor
    yaze_emulator
    yaze_agent
    absl::flags
    absl::flags_parse
    absl::failure_signal_handler
  )

  # Link test support if tests are enabled (yaze_editor needs TestManager)
  if(YAZE_BUILD_TESTS AND TARGET yaze_test_support)
    target_link_libraries(yaze_emu PRIVATE yaze_test_support)
  endif()

  if(YAZE_ENABLE_UI_TESTS)
    target_compile_definitions(yaze_emu PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=1)
  endif()

  # Headless Emulator Test Harness
  add_executable(yaze_emu_test emu_test.cc)
  target_include_directories(yaze_emu_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${PROJECT_BINARY_DIR}
  )
  target_link_libraries(yaze_emu_test PRIVATE
    yaze_emulator
    yaze_util
    absl::flags
    absl::flags_parse
    absl::status
    absl::strings
    absl::str_format
  )
  message(STATUS "✓ yaze_emu_test: Headless emulator test harness configured")
  message(STATUS "✓ yaze_emu: Standalone emulator executable configured")
else()
  message(STATUS "○ Standalone emulator builds disabled (YAZE_BUILD_EMU=OFF or YAZE_MINIMAL_BUILD=ON)")
  message(STATUS "  Note: yaze_emulator library still available for main app and tests")
endif()
