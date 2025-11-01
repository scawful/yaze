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

# Note: FTXUI targets will be available after the build phase
# For now, we'll create a placeholder that can be linked later

# Export FTXUI targets for use in other CMake files
set(YAZE_FTXUI_TARGETS yaze_ftxui)

message(STATUS "FTXUI setup complete")
