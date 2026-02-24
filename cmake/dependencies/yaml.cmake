# yaml-cpp dependency management
# Uses CPM.cmake for consistent cross-platform builds

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

if(NOT YAZE_ENABLE_AI AND NOT YAZE_ENABLE_AI_RUNTIME)
  message(STATUS "Skipping yaml-cpp (AI runtime and CLI agent features disabled)")
  set(YAZE_YAML_TARGETS "")
  return()
endif()

message(STATUS "Setting up yaml-cpp ${YAML_CPP_VERSION} with CPM.cmake")

set(_YAZE_USE_SYSTEM_YAML ${YAZE_USE_SYSTEM_DEPS})

# Detect Homebrew installation automatically (helps offline builds)
if(APPLE AND NOT _YAZE_USE_SYSTEM_YAML)
  include(cmake/platform/homebrew.cmake)
  yaze_homebrew_find_package(yaml-cpp RESULT_VAR _yaze_yaml_hb)
  if(_yaze_yaml_hb)
    list(APPEND CMAKE_PREFIX_PATH "${_yaze_yaml_hb}")
    set(_YAZE_USE_SYSTEM_YAML ON)
  endif()
endif()

# Try to use system packages first
if(_YAZE_USE_SYSTEM_YAML)
  find_package(yaml-cpp QUIET)
  if(yaml-cpp_FOUND)
    message(STATUS "Using system yaml-cpp")
    add_library(yaze_yaml INTERFACE)
    target_compile_definitions(yaze_yaml INTERFACE YAZE_HAS_YAML_CPP=1)
    if(TARGET yaml-cpp::yaml-cpp)
      message(STATUS "Linking yaze_yaml against yaml-cpp::yaml-cpp")
      target_link_libraries(yaze_yaml INTERFACE yaml-cpp::yaml-cpp)
      
      # HACK: Explicitly add the library directory for Homebrew if detected
      # This fixes 'ld: library not found for -lyaml-cpp' when the imported target
      # doesn't propagate the library path correctly to the linker command line
      if(_yaze_yaml_hb AND EXISTS "${_yaze_yaml_hb}/lib")
        link_directories("${_yaze_yaml_hb}/lib")
        message(STATUS "Added yaml-cpp link directory: ${_yaze_yaml_hb}/lib")
      endif()
    else()
      message(STATUS "Linking yaze_yaml against yaml-cpp (legacy)")
      target_link_libraries(yaze_yaml INTERFACE yaml-cpp)
    endif()
    set(YAZE_YAML_TARGETS yaze_yaml)
    return()
  elseif(YAZE_USE_SYSTEM_DEPS)
    message(WARNING "System yaml-cpp not found despite YAZE_USE_SYSTEM_DEPS=ON; falling back to CPM download")
  endif()
endif()

# Use CPM to fetch yaml-cpp
CPMAddPackage(
  NAME yaml-cpp
  VERSION ${YAML_CPP_VERSION}
  GITHUB_REPOSITORY jbeder/yaml-cpp
  GIT_TAG ${YAML_CPP_VERSION}
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
target_compile_definitions(yaze_yaml INTERFACE YAZE_HAS_YAML_CPP=1)

# Export yaml-cpp targets for use in other CMake files
set(YAZE_YAML_TARGETS yaze_yaml)

message(STATUS "yaml-cpp setup complete")
