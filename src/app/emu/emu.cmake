# Yaze Emulator Standalone Application
if (APPLE)
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
else()
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

target_include_directories(
  yaze_emu PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
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
  ImGuiTestEngine
)
