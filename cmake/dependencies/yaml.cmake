# yaml-cpp dependency management
# Uses CPM.cmake for consistent cross-platform builds

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up yaml-cpp ${YAML_CPP_VERSION} with CPM.cmake")

# Try to use system packages first if requested
if(YAZE_USE_SYSTEM_DEPS)
  find_package(yaml-cpp QUIET)
  if(yaml-cpp_FOUND)
    message(STATUS "Using system yaml-cpp")
    add_library(yaze_yaml INTERFACE IMPORTED)
    target_link_libraries(yaze_yaml INTERFACE yaml-cpp)
    set(YAZE_YAML_TARGETS yaze_yaml PARENT_SCOPE)
    return()
  endif()
endif()

# Use CPM to fetch yaml-cpp
CPMAddPackage(
  NAME yaml-cpp
  VERSION ${YAML_CPP_VERSION}
  GITHUB_REPOSITORY jbeder/yaml-cpp
  GIT_TAG 0.8.0
  OPTIONS
    "YAML_CPP_BUILD_TESTS OFF"
    "YAML_CPP_BUILD_CONTRIB OFF"
    "YAML_CPP_BUILD_TOOLS OFF"
    "YAML_CPP_INSTALL OFF"
)

# Verify yaml-cpp targets are available
if(NOT TARGET yaml-cpp)
  message(FATAL_ERROR "yaml-cpp target not found after CPM fetch")
endif()

# Create convenience targets for the rest of the project
add_library(yaze_yaml INTERFACE)
target_link_libraries(yaze_yaml INTERFACE yaml-cpp)

# Export yaml-cpp targets for use in other CMake files
set(YAZE_YAML_TARGETS yaze_yaml)

message(STATUS "yaml-cpp setup complete")
