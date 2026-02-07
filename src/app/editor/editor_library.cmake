set(
  YAZE_APP_EDITOR_SRC
  app/editor/agent/agent_chat_history_codec.cc
  app/editor/code/assembly_editor.cc
  app/editor/core/content_registry.cc
  app/editor/code/memory_editor.cc
  app/editor/code/project_file_editor.cc
  app/editor/dungeon/dungeon_canvas_viewer.cc
  app/editor/dungeon/dungeon_editor_v2.cc
  app/editor/dungeon/dungeon_object_interaction.cc
  app/editor/dungeon/dungeon_object_selector.cc
  app/editor/dungeon/object_selection.cc
  app/editor/dungeon/dungeon_room_loader.cc
  app/editor/dungeon/dungeon_room_selector.cc
  app/editor/dungeon/dungeon_toolset.cc
  app/editor/dungeon/dungeon_usage_tracker.cc
  app/editor/dungeon/widgets/dungeon_room_nav_widget.cc
  app/editor/dungeon/widgets/dungeon_workbench_toolbar.cc
  app/editor/dungeon/interaction/door_interaction_handler.cc
  app/editor/dungeon/interaction/item_interaction_handler.cc
  app/editor/dungeon/interaction/sprite_interaction_handler.cc
  app/editor/dungeon/interaction/interaction_coordinator.cc
  app/editor/dungeon/interaction/interaction_mode.cc
  app/editor/dungeon/panels/dungeon_room_graphics_panel.cc
  app/editor/dungeon/panels/dungeon_workbench_panel.cc
  app/editor/dungeon/panels/object_editor_panel.cc
  app/editor/dungeon/panels/minecart_track_editor_panel.cc
  app/editor/dungeon/panels/room_tag_editor_panel.cc
  app/editor/editor_manager.cc
  app/editor/session_types.cc
  app/editor/graphics/gfx_group_editor.cc
  app/editor/graphics/graphics_editor.cc
  app/editor/graphics/link_sprite_panel.cc
  app/editor/graphics/polyhedral_editor_panel.cc
  app/editor/graphics/palette_controls_panel.cc
  app/editor/graphics/paletteset_editor_panel.cc
  app/editor/graphics/pixel_editor_panel.cc
  app/editor/graphics/screen_editor.cc
  app/editor/graphics/sheet_browser_panel.cc
  app/editor/message/message_data.cc
  app/editor/message/message_editor.cc
  app/editor/message/message_preview.cc
  app/editor/music/music_editor.cc
  app/editor/music/music_player.cc
  app/editor/music/instrument_editor_view.cc
  app/editor/music/piano_roll_view.cc
  app/editor/music/sample_editor_view.cc
  app/editor/music/song_browser_view.cc
  app/editor/music/tracker_view.cc
  app/editor/overworld/automation.cc
  app/editor/overworld/debug_window_card.cc
  app/editor/overworld/entity.cc
  app/editor/overworld/entity_operations.cc
  app/editor/overworld/map_properties.cc
  app/editor/overworld/overworld_editor.cc
  app/editor/overworld/overworld_entity_renderer.cc
  app/editor/overworld/overworld_navigation.cc
  app/editor/overworld/overworld_sidebar.cc
  app/editor/overworld/overworld_toolbar.cc
  app/editor/overworld/panels/area_graphics_panel.cc
  app/editor/overworld/panels/tile16_selector_panel.cc
  app/editor/overworld/panels/map_properties_panel.cc
  app/editor/overworld/panels/overworld_canvas_panel.cc
  app/editor/overworld/panels/scratch_space_panel.cc
  app/editor/overworld/panels/usage_statistics_panel.cc
  app/editor/overworld/panels/tile8_selector_panel.cc
  app/editor/overworld/panels/debug_window_panel.cc
  app/editor/overworld/panels/gfx_groups_panel.cc
  app/editor/overworld/panels/v3_settings_panel.cc
  app/editor/overworld/panels/tile16_editor_panel.cc
  app/editor/overworld/scratch_space.cc
  app/editor/overworld/tile16_editor.cc
  app/editor/overworld/usage_statistics_card.cc
  app/editor/palette/palette_editor.cc
  app/editor/palette/palette_group_panel.cc
  app/editor/palette/palette_utility.cc
  app/editor/sprite/sprite_drawer.cc
  app/editor/sprite/sprite_editor.cc
  app/editor/menu/menu_orchestrator.cc
  app/editor/ui/popup_manager.cc
  app/editor/ui/about_panel.cc
  app/editor/ui/dashboard_panel.cc
  app/editor/ui/editor_selection_dialog.cc
  app/editor/menu/right_panel_manager.cc
  app/editor/menu/status_bar.cc
  app/editor/ui/settings_panel.cc
  app/editor/ui/selection_properties_panel.cc
  app/editor/ui/project_management_panel.cc
  app/editor/menu/menu_builder.cc
  app/editor/menu/activity_bar.cc
  app/editor/ui/rom_load_options_dialog.cc
  app/editor/ui/ui_coordinator.cc
  app/editor/ui/welcome_screen.cc
  app/editor/ui/workspace_manager.cc

  yaze.cc
)

set(
  YAZE_EDITOR_SYSTEM_PANELS_SRC
  app/editor/layout/layout_coordinator.cc
  app/editor/layout/layout_manager.cc
  app/editor/layout/layout_orchestrator.cc
  app/editor/layout/layout_presets.cc
  app/editor/layout/window_delegate.cc
  app/editor/system/editor_activator.cc
  app/editor/system/editor_registry.cc
  app/editor/system/panel_manager.cc
  app/editor/system/file_browser.cc
  app/editor/system/proposal_drawer.cc
)

set(
  YAZE_EDITOR_SYSTEM_SESSION_SRC
  app/editor/system/extension_manager.cc
  app/editor/system/project_manager.cc
  app/editor/system/rom_file_manager.cc
  app/editor/system/session_coordinator.cc
  app/editor/system/user_settings.cc
)

set(
  YAZE_EDITOR_SYSTEM_SHORTCUTS_SRC
  app/editor/system/command_manager.cc
  app/editor/system/command_palette.cc
  app/editor/system/shortcut_manager.cc
  app/editor/system/shortcut_configurator.cc
)

# Editor system split targets (panels/session/shortcuts)
add_library(yaze_editor_system_panels STATIC ${YAZE_EDITOR_SYSTEM_PANELS_SRC})
add_library(yaze_editor_system_session STATIC ${YAZE_EDITOR_SYSTEM_SESSION_SRC})
add_library(yaze_editor_system_shortcuts STATIC ${YAZE_EDITOR_SYSTEM_SHORTCUTS_SRC})

foreach(target_name IN ITEMS
  yaze_editor_system_panels
  yaze_editor_system_session
  yaze_editor_system_shortcuts
)
  target_precompile_headers(${target_name} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
  )

  target_include_directories(${target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/ext/imgui
    ${CMAKE_SOURCE_DIR}/ext/imgui_test_engine
    ${CMAKE_SOURCE_DIR}/inc
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )
endforeach()

target_link_libraries(yaze_editor_system_panels PUBLIC
  yaze_app_core_lib
  yaze_gfx
  yaze_gui
  yaze_util
  yaze_common
  ImGui
)

target_link_libraries(yaze_editor_system_session PUBLIC
  yaze_editor_system_panels
  yaze_rom
  yaze_zelda3
  yaze_gui
  yaze_util
  yaze_common
  ImGui
)

target_link_libraries(yaze_editor_system_shortcuts PUBLIC
  yaze_gui
  yaze_util
  yaze_common
  ImGui
)

if(YAZE_BUILD_AGENT_UI)
  list(APPEND YAZE_APP_EDITOR_SRC
    app/editor/agent/agent_chat.cc
    app/editor/agent/agent_collaboration_coordinator.cc
    app/editor/agent/agent_editor.cc
    app/editor/agent/agent_proposals_panel.cc
    app/editor/agent/agent_session.cc
    app/editor/agent/agent_ui_controller.cc
    app/editor/agent/asm_follow_service.cc
    app/editor/agent/automation_bridge.cc
    app/editor/agent/network_collaboration_coordinator.cc
    app/editor/agent/oracle_ram_panel.cc
    app/editor/agent/panels/agent_automation_panel.cc
    app/editor/agent/panels/agent_configuration_panel.cc
    app/editor/agent/panels/agent_editor_panels.cc
    app/editor/agent/panels/feature_flag_editor_panel.cc
    app/editor/agent/panels/manifest_panel.cc
    app/editor/agent/panels/agent_knowledge_panel.cc
    app/editor/agent/panels/agent_rom_sync_panel.cc
    app/editor/agent/panels/agent_z3ed_command_panel.cc
    app/editor/agent/panels/mesen_debug_panel.cc
    app/editor/agent/panels/mesen_screenshot_panel.cc
    app/editor/agent/panels/oracle_state_library_panel.cc
    app/editor/agent/panels/sram_viewer_panel.cc
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
# Dependencies: yaze_app_core_lib, yaze_gfx, yaze_gui, yaze_zelda3, ImGui
# ==============================================================================

add_library(yaze_editor STATIC ${YAZE_APP_EDITOR_SRC})

target_link_libraries(yaze_editor PUBLIC
  yaze_editor_system_panels
  yaze_editor_system_session
  yaze_editor_system_shortcuts
  yaze_app_core_lib
  yaze_rom
  yaze_gfx
  yaze_gui
  yaze_zelda3
  yaze_emulator  # Needed for emulator integration (APU, PPU, SNES)
  yaze_util
  yaze_common
  ImGui
)

target_precompile_headers(yaze_editor PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_editor PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext
  ${CMAKE_SOURCE_DIR}/ext/imgui
  ${CMAKE_SOURCE_DIR}/ext/imgui_test_engine
  ${CMAKE_SOURCE_DIR}/inc
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

# Link agent runtime only when agent UI panels are enabled
if(YAZE_BUILD_AGENT_UI AND NOT YAZE_MINIMAL_BUILD)
  if(TARGET yaze_agent)
    target_link_libraries(yaze_editor PUBLIC yaze_agent)
    message(STATUS "✓ yaze_editor linked to yaze_agent (UI panels)")
  else()
    message(WARNING "Agent UI requested but yaze_agent target not found")
  endif()

  # MesenScreenshotPanel needs libpng for PNG decode
  find_package(PNG QUIET)
  if(PNG_FOUND)
    target_link_libraries(yaze_editor PUBLIC PNG::PNG)
    message(STATUS "✓ yaze_editor linked to PNG::PNG (screenshot panel)")
  else()
    message(STATUS "○ libpng not found — MesenScreenshotPanel PNG decode unavailable")
  endif()
endif()

# Note: yaze_test_support linking is deferred to test.cmake to ensure proper ordering

if(YAZE_ENABLE_JSON)
  if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(yaze_editor_system_panels PUBLIC nlohmann_json::nlohmann_json)
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

  # NOTE: yaze_editor should NOT force-load yaze_test_support to avoid circular dependency.
  # The chain yaze_editor -> force_load(yaze_test_support) -> yaze_editor causes SIGSEGV
  # during static initialization.
  #
  # Test executables should link yaze_test_support directly, which provides all needed
  # symbols through its own dependencies (including yaze_editor via regular linking).
endif()

# Conditionally link gRPC if enabled
if(YAZE_WITH_GRPC)
  target_link_libraries(yaze_editor PUBLIC yaze_grpc_support)
  # Add protobuf generated headers directory
  target_include_directories(yaze_editor PUBLIC ${PROJECT_BINARY_DIR}/gens)
endif()

set_target_properties(yaze_editor PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_editor PRIVATE linux stricmp=strcasecmp)
elseif(YAZE_PLATFORM_MACOS)
  target_compile_definitions(yaze_editor PRIVATE MACOS)
elseif(YAZE_PLATFORM_IOS)
  target_compile_definitions(yaze_editor PRIVATE YAZE_IOS)
elseif(WIN32)
  target_compile_definitions(yaze_editor PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_editor library configured")
