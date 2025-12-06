# Canvas system CMake configuration
# This file configures the canvas system components

# Canvas core components
set(CANVAS_SOURCES
  bpp_format_ui.cc
  canvas_extensions.cc
  canvas_modals.cc
  canvas_context_menu.cc
  canvas_usage_tracker.cc
  canvas_performance_integration.cc
  canvas_interaction_handler.cc
  canvas_utils.cc
)

set(CANVAS_HEADERS
  bpp_format_ui.h
  canvas_extensions.h
  canvas_modals.h
  canvas_context_menu.h
  canvas_usage_tracker.h
  canvas_performance_integration.h
  canvas_interaction_handler.h
  canvas_utils.h
)

# Create canvas library
add_library(yaze_canvas STATIC
  ${CANVAS_SOURCES}
  ${CANVAS_HEADERS}
)

# Set target properties
set_target_properties(yaze_canvas PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(yaze_canvas PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../incl
)

# Link dependencies
target_link_libraries(yaze_canvas PUBLIC
  yaze_gfx
  yaze_gui_common
  absl::status
  absl::statusor
  absl::strings
  imgui
  SDL2::SDL2
)

# Compiler-specific options (respect global warning settings)
if(NOT YAZE_SUPPRESS_WARNINGS)
  if(MSVC)
    target_compile_options(yaze_canvas PRIVATE /W4)
  else()
    target_compile_options(yaze_canvas PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# Add canvas to parent GUI library
target_sources(yaze_gui PRIVATE
  ${CANVAS_SOURCES}
  ${CANVAS_HEADERS}
)

# Install rules
install(TARGETS yaze_canvas
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

install(FILES ${CANVAS_HEADERS}
  DESTINATION include/yaze/app/gui/canvas
)
