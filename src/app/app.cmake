include(app/core/core.cmake)
include(app/editor/editor.cmake)
include(app/gfx/gfx.cmake)
include(app/gui/gui.cmake)
include(app/zelda3/zelda3.cmake)

if (APPLE)
  add_executable(
    yaze
    MACOSX_BUNDLE
    app/main.cc
    app/rom.cc
    ${YAZE_APP_EMU_SRC}
    ${YAZE_APP_CORE_SRC}
    ${YAZE_APP_EDITOR_SRC}
    ${YAZE_APP_GFX_SRC}
    ${YAZE_APP_ZELDA3_SRC}
    ${YAZE_UTIL_SRC}
    ${YAZE_GUI_SRC}
    ${IMGUI_SRC}
    # Bundled Resources
    ${YAZE_RESOURCE_FILES}
  )
else()
  add_executable(
    yaze
    app/main.cc
    app/rom.cc
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
  yaze PUBLIC
  lib/
  app/
  ${ASAR_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

# Conditionally add PNG include dirs if available
if(PNG_FOUND)
  target_include_directories(yaze PUBLIC ${PNG_INCLUDE_DIRS})
endif()

# Conditionally link nfd if available
if(YAZE_HAS_NFD)
  target_link_libraries(yaze PRIVATE nfd)
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_NFD=1)
else()
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_NFD=0)
endif()

target_link_libraries(
  yaze PUBLIC
  asar-static
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${CMAKE_DL_LIBS}
  ImGui
  ImGuiTestEngine
)

# Conditionally link PNG if available
if(PNG_FOUND)
  target_link_libraries(yaze PUBLIC ${PNG_LIBRARIES})
endif()

if (APPLE)
  target_link_libraries(yaze PUBLIC ${COCOA_LIBRARY})
endif()

