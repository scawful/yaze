# This file defines the yaze_emu standalone executable.
# Note: The yaze_emulator library is ALWAYS built (via emu_library.cmake)
#       because it's used by the main yaze app and test suites.
#       This file only controls the STANDALONE emulator executable.

if(YAZE_BUILD_EMU AND NOT YAZE_MINIMAL_BUILD AND NOT YAZE_PLATFORM_IOS)
  if(YAZE_PLATFORM_MACOS)
    # Note: controller.cc is included here (not via library) because it depends on
    # yaze_editor and yaze_gui. Including it in yaze_app_core_lib would create a cycle:
    # yaze_agent -> yaze_app_core_lib -> yaze_editor -> yaze_agent
    add_executable(yaze_emu MACOSX_BUNDLE
      app/emu/emu.cc
    )
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
    yaze_emulator_ui
    yaze_agent
    absl::flags
    absl::flags_parse
    absl::failure_signal_handler
    yaze_gui_core
    yaze_app_core_lib
  )

  # Link test support library (yaze_editor needs TestManager)
  if(TARGET yaze_test_support)
    target_link_libraries(yaze_emu PRIVATE yaze_test_support)
    message(STATUS "✓ yaze_emu executable linked to yaze_test_support")
  else()
    message(WARNING "yaze_emu needs yaze_test_support but TARGET not found")
  endif()

  # gRPC/protobuf linking is now handled by yaze_grpc_support library

  # Test engine is always available when tests are built
  # No need for conditional definitions

  # Headless Emulator Test Harness
  add_executable(yaze_emu_test emu_test.cc)
  target_include_directories(yaze_emu_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${PROJECT_BINARY_DIR}
  )
  target_link_libraries(yaze_emu_test PRIVATE
    yaze_emulator
    yaze_editor
    yaze_gui
    yaze_util
    absl::flags
    absl::flags_parse
    absl::status
    absl::strings
    absl::str_format
  )
  
  # gRPC/protobuf linking is now handled by yaze_grpc_support library
  
  message(STATUS "✓ yaze_emu_test: Headless emulator test harness configured")
  message(STATUS "✓ yaze_emu: Standalone emulator executable configured")
else()
  message(STATUS "○ Standalone emulator builds disabled (YAZE_BUILD_EMU=OFF, YAZE_MINIMAL_BUILD=ON, or iOS)")
  message(STATUS "  Note: yaze_emulator library still available for main app and tests")
endif()
