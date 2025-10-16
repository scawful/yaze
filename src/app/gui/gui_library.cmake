# ==============================================================================
# GUI Library Refactoring (see docs/gui-refactor.md)
# ==============================================================================
# The monolithic yaze_gui has been decomposed into smaller, layered libraries
# to improve build times and code organization. The yaze_gui target is now
# an INTERFACE library that aggregates these components for backward
# compatibility.
# ==============================================================================

# ==============================================================================
# SOURCE LISTS (auto-maintained by build_cleaner.py)
# Paths are relative to src/ directory
# ==============================================================================

# build_cleaner:auto-maintain
set(GUI_CORE_SRC
  app/gui/core/background_renderer.cc
  app/gui/core/color.cc
  app/gui/core/input.cc
  app/gui/core/layout_helpers.cc
  app/gui/core/style.cc
  app/gui/core/theme_manager.cc
  app/gui/core/ui_helpers.cc
)

# build_cleaner:auto-maintain
set(CANVAS_SRC
  app/gui/canvas/bpp_format_ui.cc
  app/gui/canvas/canvas.cc
  app/gui/canvas/canvas_automation_api.cc
  app/gui/canvas/canvas_context_menu.cc
  app/gui/canvas/canvas_geometry.cc
  app/gui/canvas/canvas_interaction.cc
  app/gui/canvas/canvas_interaction_handler.cc
  app/gui/canvas/canvas_modals.cc
  app/gui/canvas/canvas_performance_integration.cc
  app/gui/canvas/canvas_rendering.cc
  app/gui/canvas/canvas_usage_tracker.cc
  app/gui/canvas/canvas_utils.cc
)

# build_cleaner:auto-maintain
set(GUI_WIDGETS_SRC
  app/gui/widgets/asset_browser.cc
  app/gui/widgets/dungeon_object_emulator_preview.cc
  app/gui/widgets/palette_editor_widget.cc
  app/gui/widgets/text_editor.cc
  app/gui/widgets/themed_widgets.cc
  app/gui/widgets/tile_selector_widget.cc
)

# build_cleaner:auto-maintain
set(GUI_AUTOMATION_SRC
  app/gui/automation/widget_auto_register.cc
  app/gui/automation/widget_id_registry.cc
  app/gui/automation/widget_measurement.cc
  app/gui/automation/widget_state_capture.cc
)

# build_cleaner:auto-maintain
set(GUI_APP_SRC
  app/gui/app/agent_chat_widget.cc
  app/gui/app/collaboration_panel.cc
  app/gui/app/editor_layout.cc
)

# ==============================================================================
# LIBRARY DEFINITIONS AND LINK STRUCTURE (manually configured)
# DO NOT AUTO-MAINTAIN
# ==============================================================================

# 2. Create Static Libraries and Establish Link Dependencies
add_library(yaze_gui_core STATIC ${GUI_CORE_SRC})
add_library(yaze_canvas STATIC ${CANVAS_SRC})
add_library(yaze_gui_widgets STATIC ${GUI_WIDGETS_SRC})
add_library(yaze_gui_automation STATIC ${GUI_AUTOMATION_SRC})
add_library(yaze_gui_app STATIC ${GUI_APP_SRC})

# Link dependencies between the new libraries
target_link_libraries(yaze_gui_core PUBLIC yaze_util ImGui nlohmann_json::nlohmann_json)
target_link_libraries(yaze_canvas PUBLIC yaze_gui_core yaze_gfx)
target_link_libraries(yaze_gui_widgets PUBLIC yaze_gui_core yaze_gfx)
target_link_libraries(yaze_gui_automation PUBLIC yaze_gui_core)
target_link_libraries(yaze_gui_app PUBLIC yaze_gui_core yaze_gui_widgets yaze_gui_automation)

set(GUI_SUB_LIBS
  yaze_gui_core
  yaze_canvas
  yaze_gui_widgets
  yaze_gui_automation
  yaze_gui_app
)

# Apply common properties to all new sub-libraries
foreach(LIB ${GUI_SUB_LIBS})
  target_precompile_headers(${LIB} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
  )
  target_include_directories(${LIB} PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${CMAKE_SOURCE_DIR}/src/app/gui
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )
  set_target_properties(${LIB} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
  if(UNIX AND NOT APPLE)
    target_compile_definitions(${LIB} PRIVATE linux stricmp=strcasecmp)
  elseif(APPLE)
    target_compile_definitions(${LIB} PRIVATE MACOS)
  elseif(WIN32)
    target_compile_definitions(${LIB} PRIVATE WINDOWS)
  endif()
endforeach()

# 3. Create Aggregate INTERFACE library
add_library(yaze_gui INTERFACE)
target_link_libraries(yaze_gui INTERFACE
  yaze_gui_core
  yaze_canvas
  yaze_gui_widgets
  yaze_gui_automation
  yaze_gui_app
  # Link original public dependencies so downstream targets receive them
  yaze_gfx
  yaze_util
  yaze_common
  yaze_net
  ImGui
  ${SDL_TARGETS}
)

message(STATUS "✓ yaze_gui library refactored and configured")
