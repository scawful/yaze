set(
  YAZE_GUI_SRC
  app/gui/canvas.cc
  app/gui/canvas/bpp_format_ui.cc
  app/gui/canvas/canvas_automation_api.cc
  app/gui/canvas/canvas_context_menu.cc
  app/gui/canvas/canvas_interaction_handler.cc
  app/gui/canvas/canvas_modals.cc
  app/gui/canvas/canvas_performance_integration.cc
  app/gui/canvas/canvas_usage_tracker.cc
  app/gui/canvas/canvas_utils.cc
  app/gui/color.cc
  app/gui/editor_card_manager.cc
  app/gui/editor_layout.cc
  app/gui/input.cc
  app/gui/modules/asset_browser.cc
  app/gui/modules/text_editor.cc
  app/gui/style.cc
  app/gui/theme_manager.cc
  app/gui/ui_helpers.cc
  app/gui/widgets/agent_chat_widget.cc
  app/gui/widgets/collaboration_panel.cc
  app/gui/widgets/dungeon_object_emulator_preview.cc
  app/gui/widgets/palette_editor_widget.cc
  app/gui/widgets/palette_widget.cc
  app/gui/widgets/tile_selector_widget.cc
  app/gui/widgets/widget_auto_register.cc
  app/gui/widgets/widget_id_registry.cc
  app/gui/widgets/widget_measurement.cc
  app/gui/widgets/widget_state_capture.cc
  # Canvas system components
)

# ==============================================================================
# Yaze GUI Library
# ==============================================================================
# This library contains all GUI-related functionality:
# - Canvas system
# - ImGui widgets and utilities
# - Input handling
# - Theme management
# - Color utilities
# - Background rendering
#
# Dependencies: yaze_gfx, yaze_util, ImGui, SDL2
# ==============================================================================

add_library(yaze_gui STATIC ${YAZE_GUI_SRC})

target_precompile_headers(yaze_gui PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_gui PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_gui PUBLIC
  yaze_gfx
  yaze_util
  yaze_common
  yaze_net
  ImGui
  ${SDL_TARGETS}
)

set_target_properties(yaze_gui PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_gui PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_gui PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_gui PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_gui library configured")
