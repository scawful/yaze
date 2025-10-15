set(
  YAZE_APP_EDITOR_SRC
  app/editor/agent/agent_chat_history_codec.cc
  app/editor/code/assembly_editor.cc
  app/editor/code/memory_editor.cc
  app/editor/code/project_file_editor.cc
  app/editor/dungeon/dungeon_canvas_viewer.cc
  app/editor/dungeon/dungeon_editor_v2.cc
  app/editor/dungeon/dungeon_object_interaction.cc
  app/editor/dungeon/dungeon_object_selector.cc
  app/editor/dungeon/dungeon_room_loader.cc
  app/editor/dungeon/dungeon_room_selector.cc
  app/editor/dungeon/dungeon_toolset.cc
  app/editor/dungeon/dungeon_usage_tracker.cc
  app/editor/dungeon/object_editor_card.cc
  app/editor/editor_manager.cc
  app/editor/graphics/gfx_group_editor.cc
  app/editor/graphics/graphics_editor.cc
  app/editor/graphics/screen_editor.cc
  app/editor/message/message_data.cc
  app/editor/message/message_editor.cc
  app/editor/message/message_preview.cc
  app/editor/music/music_editor.cc
  app/editor/overworld/entity.cc
  app/editor/overworld/map_properties.cc
  app/editor/overworld/overworld_editor.cc
  app/editor/overworld/overworld_entity_renderer.cc
  app/editor/overworld/scratch_space.cc
  app/editor/overworld/tile16_editor.cc
  app/editor/palette/palette_editor.cc
  app/editor/palette/palette_group_card.cc
  app/editor/palette/palette_utility.cc
  app/editor/sprite/sprite_editor.cc
  app/editor/system/command_manager.cc
  app/editor/system/command_palette.cc
  app/editor/system/extension_manager.cc
  app/editor/system/popup_manager.cc
  app/editor/system/proposal_drawer.cc
  app/editor/system/session_card_registry.cc
  app/editor/system/settings_editor.cc
  app/editor/system/shortcut_manager.cc
  app/editor/system/user_settings.cc
  app/editor/system/window_delegate.cc
  app/editor/ui/editor_selection_dialog.cc
  app/editor/ui/menu_builder.cc
  app/editor/ui/session_coordinator.cc
  app/editor/ui/welcome_screen.cc
  app/editor/ui/workspace_manager.cc
)

if(YAZE_WITH_GRPC)
  list(APPEND YAZE_APP_EDITOR_SRC
    app/editor/agent/agent_editor.cc
    app/editor/agent/agent_chat_widget.cc
    app/editor/agent/agent_chat_history_popup.cc
    app/editor/agent/agent_ui_theme.cc
    app/editor/agent/agent_collaboration_coordinator.cc
    app/editor/agent/network_collaboration_coordinator.cc
    app/editor/agent/automation_bridge.cc
  )
endif()

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

target_precompile_headers(yaze_editor PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

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
  yaze_emulator  # Needed for emulator integration (APU, PPU, SNES)
  yaze_util
  yaze_common
  ImGui
)

# Link agent library for AI features (always available when not in minimal build)
if(NOT YAZE_MINIMAL_BUILD)
  if(TARGET yaze_agent)
    target_link_libraries(yaze_editor PUBLIC yaze_agent)
    message(STATUS "✓ yaze_editor linked to yaze_agent")
  endif()
endif()

# Note: yaze_test_support linking is deferred to test.cmake to ensure proper ordering

if(YAZE_WITH_JSON)
  target_include_directories(yaze_editor PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include)

  if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(yaze_editor PUBLIC nlohmann_json::nlohmann_json)
  endif()

  target_compile_definitions(yaze_editor PUBLIC YAZE_WITH_JSON)
endif()

# Link test infrastructure when tests are enabled
# The test infrastructure is integrated into the editor for test automation
if(YAZE_BUILD_TESTS)
  if(TARGET ImGuiTestEngine)
    target_link_libraries(yaze_editor PUBLIC ImGuiTestEngine)
    message(STATUS "✓ yaze_editor linked to ImGuiTestEngine")
  endif()
  
  if(TARGET yaze_test_support)
    target_link_libraries(yaze_editor PUBLIC yaze_test_support)
    message(STATUS "✓ yaze_editor linked to yaze_test_support")
  endif()
endif()

# Conditionally link gRPC if enabled
if(YAZE_WITH_GRPC)
  target_link_libraries(yaze_editor PRIVATE
    grpc++
    grpc++_reflection
  )
  if(YAZE_PROTOBUF_TARGETS)
    target_link_libraries(yaze_editor PRIVATE ${YAZE_PROTOBUF_TARGETS})
    if(MSVC)
      foreach(_yaze_proto_target IN LISTS YAZE_PROTOBUF_TARGETS)
        target_link_options(yaze_editor PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:${_yaze_proto_target}>)
      endforeach()
    endif()
  endif()
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
