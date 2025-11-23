# FTXUI dependency management for CLI tools
# Uses CPM.cmake for consistent cross-platform builds

if(NOT YAZE_BUILD_CLI)
  return()
endif()

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up FTXUI ${FTXUI_VERSION} with CPM.cmake")

# Use CPM to fetch FTXUI
CPMAddPackage(
  NAME ftxui
  VERSION ${FTXUI_VERSION}
  GITHUB_REPOSITORY ArthurSonzogni/ftxui
  GIT_TAG v${FTXUI_VERSION}
  OPTIONS
    "FTXUI_BUILD_EXAMPLES OFF"
    "FTXUI_BUILD_TESTS OFF"
    "FTXUI_ENABLE_INSTALL OFF"
)

# FTXUI targets are created during the build phase
# We'll create our own interface target and link when available
add_library(yaze_ftxui INTERFACE)

# Link to the actual FTXUI targets
if(TARGET ftxui::screen AND TARGET ftxui::dom AND TARGET ftxui::component)
  target_link_libraries(yaze_ftxui INTERFACE
    ftxui::screen
    ftxui::dom
    ftxui::component
  )
else()
  # Fallback for when targets aren't namespaced
  target_link_libraries(yaze_ftxui INTERFACE
    screen
    dom
    component
  )
endif()

# Add include path with compile options for Ninja Multi-Config compatibility
# The -isystem-after flag doesn't work properly with some generator/compiler combinations
if(ftxui_SOURCE_DIR)
  add_compile_options(-I${ftxui_SOURCE_DIR}/include)
  message(STATUS "  Added FTXUI include: ${ftxui_SOURCE_DIR}/include")
endif()

# Export FTXUI targets for use in other CMake files
set(YAZE_FTXUI_TARGETS yaze_ftxui)

message(STATUS "FTXUI setup complete")
