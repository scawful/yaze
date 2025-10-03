# ==============================================================================
# Yaze Editor Library
# ==============================================================================
# This library contains all editor functionality:
# - Editor manager and coordination
# - Dungeon editor (room selector, object editor, renderer)
# - Overworld editor (map, tile16, entities)
# - Sprite editor
# - Music editor
# - Message editor
# - Assembly editor
# - Graphics/palette editors
# - System editors (settings, commands, extensions)
# - Testing infrastructure
#
# Dependencies: yaze_core_lib, yaze_gfx, yaze_gui, yaze_zelda3, ImGui
# ==============================================================================

add_library(yaze_editor STATIC ${YAZE_APP_EDITOR_SRC})

target_include_directories(yaze_editor PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_editor PUBLIC
  yaze_core_lib
  yaze_gfx
  yaze_gui
  yaze_zelda3
  yaze_util
  yaze_common
  ImGui
)

# Conditionally link ImGui Test Engine
if(YAZE_ENABLE_UI_TESTS AND TARGET ImGuiTestEngine)
  target_link_libraries(yaze_editor PUBLIC ImGuiTestEngine)
  target_compile_definitions(yaze_editor PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=1)
else()
  target_compile_definitions(yaze_editor PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
endif()

# Conditionally link gRPC if enabled
if(YAZE_WITH_GRPC)
  target_link_libraries(yaze_editor PRIVATE
    grpc++
    grpc++_reflection
    libprotobuf
  )
endif()

set_target_properties(yaze_editor PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_editor PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_editor PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_editor PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_editor library configured")
