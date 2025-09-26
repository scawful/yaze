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
  
  # Add the app icon to the macOS bundle
  set(ICON_FILE "${CMAKE_SOURCE_DIR}/assets/yaze.icns")
  target_sources(yaze PRIVATE ${ICON_FILE})
  set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  
  # Set macOS bundle properties
  set_target_properties(yaze PROPERTIES
    MACOSX_BUNDLE_ICON_FILE "yaze.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "Yaze"
    MACOSX_BUNDLE_EXECUTABLE_NAME "yaze"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.scawful.yaze"
    MACOSX_BUNDLE_INFO_STRING "Yet Another Zelda3 Editor"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 scawful. All rights reserved."
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
)

# Conditionally link ImGui Test Engine
if(YAZE_ENABLE_UI_TESTS)
  if(TARGET ImGuiTestEngine)
    target_include_directories(yaze PUBLIC ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine)
    target_link_libraries(yaze PUBLIC ImGuiTestEngine)
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=1)
  else()
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
  endif()
else()
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
endif()

# Link Google Test if available for integrated testing
if(YAZE_BUILD_TESTS AND TARGET gtest AND TARGET gtest_main)
  target_link_libraries(yaze PRIVATE gtest gtest_main)
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_GTEST=1)
else()
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_GTEST=0)
endif()

# Conditionally link PNG if available
if(PNG_FOUND)
  target_link_libraries(yaze PUBLIC ${PNG_LIBRARIES})
endif()

if (APPLE)
  target_link_libraries(yaze PUBLIC ${COCOA_LIBRARY})
endif()

