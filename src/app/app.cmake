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
  
  # Add asset files for Windows/Linux builds
  if(WIN32 OR LINUX)
    target_sources(yaze PRIVATE ${YAZE_RESOURCE_FILES})
    
    # Set up asset deployment for Visual Studio
    if(WIN32)
      foreach(ASSET_FILE ${YAZE_RESOURCE_FILES})
        file(RELATIVE_PATH ASSET_REL_PATH "${CMAKE_SOURCE_DIR}/assets" ${ASSET_FILE})
        get_filename_component(ASSET_DIR ${ASSET_REL_PATH} DIRECTORY)
        
        set_source_files_properties(${ASSET_FILE}
          PROPERTIES
          VS_DEPLOYMENT_CONTENT 1
          VS_DEPLOYMENT_LOCATION "assets/${ASSET_DIR}"
        )
      endforeach()
    endif()
  endif()
endif()

target_include_directories(
  yaze PUBLIC
  ${CMAKE_SOURCE_DIR}/src/lib/
  ${CMAKE_SOURCE_DIR}/src/app/
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${SDL2_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROJECT_BINARY_DIR}
)

target_sources(yaze PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)

# 4) Tell the IDE it’s generated
set_source_files_properties(
  ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h
  PROPERTIES GENERATED TRUE
)

# (Optional) put it under a neat filter in VS Solution Explorer
source_group(TREE ${CMAKE_CURRENT_BINARY_DIR}
             FILES ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)

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
    target_compile_definitions(yaze PRIVATE 
      YAZE_ENABLE_IMGUI_TEST_ENGINE=1
      ${IMGUI_TEST_ENGINE_DEFINITIONS})
  else()
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
  endif()
else()
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
endif()

# Link Google Test if available for integrated testing (but NOT gtest_main to avoid main() conflicts)
if(YAZE_BUILD_TESTS AND TARGET gtest)
  target_link_libraries(yaze PRIVATE gtest)
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_GTEST=1)
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_TESTING=1)
else()
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_GTEST=0)
  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_TESTING=0)
endif()

# Conditionally link PNG if available
if(PNG_FOUND)
  target_link_libraries(yaze PUBLIC ${PNG_LIBRARIES})
endif()

if (APPLE)
  target_link_libraries(yaze PUBLIC ${COCOA_LIBRARY})
endif()

# Post-build step to copy assets to output directory (Windows/Linux)
if(NOT APPLE)
  # Add post-build commands directly to the yaze target
  # Copy fonts
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:yaze>/assets/font
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/font
    $<TARGET_FILE_DIR:yaze>/assets/font
    COMMENT "Copying font assets"
  )
  
  # Copy themes
  add_custom_command(TARGET yaze POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:yaze>/assets/themes
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/themes
    $<TARGET_FILE_DIR:yaze>/assets/themes
    COMMENT "Copying theme assets"
  )
  
  # Copy other assets if they exist
  if(EXISTS ${CMAKE_SOURCE_DIR}/assets/layouts)
    add_custom_command(TARGET yaze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      $<TARGET_FILE_DIR:yaze>/assets/layouts
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/assets/layouts
      $<TARGET_FILE_DIR:yaze>/assets/layouts
      COMMENT "Copying layout assets"
    )
  endif()
  
  if(EXISTS ${CMAKE_SOURCE_DIR}/assets/lib)
    add_custom_command(TARGET yaze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      $<TARGET_FILE_DIR:yaze>/assets/lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/assets/lib
      $<TARGET_FILE_DIR:yaze>/assets/lib
      COMMENT "Copying library assets"
    )
  endif()
endif()

