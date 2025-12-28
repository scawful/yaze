# ==============================================================================
# Lab sandbox executable
# ==============================================================================

set(YAZE_LAB_SRC
  lab/main.cc
  lab/layout_designer/layout_designer_window.cc
  lab/layout_designer/layout_serialization.cc
  lab/layout_designer/layout_definition.cc
  lab/layout_designer/widget_definition.cc
  lab/layout_designer/widget_code_generator.cc
  lab/layout_designer/theme_properties.cc
  lab/layout_designer/yaze_widgets.cc
)

add_executable(yaze_lab ${YAZE_LAB_SRC})
set_target_properties(yaze_lab PROPERTIES OUTPUT_NAME "lab")

target_precompile_headers(yaze_lab PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_lab PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext
  ${CMAKE_SOURCE_DIR}/ext/imgui
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_lab PRIVATE
  yaze_editor_system_panels
  yaze_app_core_lib
  yaze_gui
  yaze_gfx
  yaze_util
  yaze_common
  ImGui
)

if(APPLE)
  target_link_libraries(yaze_lab PUBLIC "-framework Cocoa")
endif()

message(STATUS "✓ yaze_lab sandbox target configured")
