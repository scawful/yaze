include(app/core/core.cmake)
include(app/editor/editor.cmake)
include(app/gfx/gfx.cmake)
include(app/gui/gui.cmake)
include(app/zelda3/zelda3.cmake)

if (APPLE)
add_executable(
  yaze
  MACOSX_BUNDLE
  app/yaze.cc
  app/rom.cc
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}

  # Bundled Resources
  ${YAZE_RESOURCE_FILES}
)
else()
add_executable(
  yaze
  app/yaze.cc
  app/rom.cc
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
)
endif()

target_include_directories(
  yaze PUBLIC
  lib/
  app/
  ${ASAR_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
)

target_link_libraries(
  yaze PUBLIC
  asar-static
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ImGui
  ImGuiTestEngine
)

if (APPLE)
  target_link_libraries(yaze PUBLIC ${COCOA_LIBRARY})
endif()

if (WIN32 OR MINGW)
  target_link_libraries(
    yaze PUBLIC
    ${CMAKE_SOURCE_DIR}/build/build-windows/bin/libpng16.dll
    zlib
    mingw32
    ws2_32)
endif()
