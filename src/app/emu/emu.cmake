# Yaze Emulator Standalone Application (skip in minimal builds)
if (NOT YAZE_MINIMAL_BUILD AND APPLE)
  add_executable(
    yaze_emu
    MACOSX_BUNDLE
    app/main.cc
    app/rom.cc
    app/core/platform/app_delegate.mm
    ${YAZE_APP_EMU_SRC}
    ${YAZE_APP_CORE_SRC}
    ${YAZE_APP_EDITOR_SRC}
    ${YAZE_APP_GFX_SRC}
    ${YAZE_APP_ZELDA3_SRC}
    ${YAZE_UTIL_SRC}
    ${YAZE_GUI_SRC}
    ${IMGUI_SRC}
  )
  target_link_libraries(yaze_emu PUBLIC ${COCOA_LIBRARY})
elseif(NOT YAZE_MINIMAL_BUILD)
  add_executable(
    yaze_emu
    app/rom.cc
    app/emu/emu.cc
    ${YAZE_APP_EMU_SRC}
    ${YAZE_APP_CORE_SRC}
    ${YAZE_APP_EDITOR_SRC}
    ${YAZE_APP_GFX_SRC}
    ${YAZE_APP_ZELDA3_SRC}
    ${YAZE_UTIL_SRC}
    ${YAZE_GUI_SRC}
    ${IMGUI_SRC}
  )
endif()

# Only configure emulator target if it was created
if(NOT YAZE_MINIMAL_BUILD)
  target_include_directories(
    yaze_emu PUBLIC
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${PNG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )

  target_link_libraries(
    yaze_emu PUBLIC 
    ${ABSL_TARGETS} 
    ${SDL_TARGETS} 
    ${PNG_LIBRARIES} 
    ${CMAKE_DL_LIBS} 
    ImGui
    asar-static
  )

  # Conditionally link ImGui Test Engine
  if(YAZE_ENABLE_UI_TESTS)
    target_link_libraries(yaze_emu PUBLIC ImGuiTestEngine)
    target_compile_definitions(yaze_emu PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=1)
  else()
    target_compile_definitions(yaze_emu PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
  endif()
endif()
