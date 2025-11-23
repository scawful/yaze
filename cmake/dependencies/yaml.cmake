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
  set(_YAZE_YAML_PREFIX_CANDIDATES
    /opt/homebrew/opt/yaml-cpp
    /usr/local/opt/yaml-cpp)

  foreach(_prefix IN LISTS _YAZE_YAML_PREFIX_CANDIDATES)
    if(EXISTS "${_prefix}")
      list(APPEND CMAKE_PREFIX_PATH "${_prefix}")
      message(STATUS "Added Homebrew yaml-cpp prefix: ${_prefix}")
      set(_YAZE_USE_SYSTEM_YAML ON)
      break()
    endif()
  endforeach()

  if(NOT _YAZE_USE_SYSTEM_YAML)
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
      execute_process(
        COMMAND "${HOMEBREW_EXECUTABLE}" --prefix yaml-cpp
        OUTPUT_VARIABLE HOMEBREW_YAML_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE HOMEBREW_YAML_RESULT
        ERROR_QUIET)
      if(HOMEBREW_YAML_RESULT EQUAL 0 AND EXISTS "${HOMEBREW_YAML_PREFIX}")
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_YAML_PREFIX}")
        message(STATUS "Added Homebrew yaml-cpp prefix: ${HOMEBREW_YAML_PREFIX}")
        set(_YAZE_USE_SYSTEM_YAML ON)
      endif()
    endif()
  endif()
endif()

# Try to use system packages first
if(_YAZE_USE_SYSTEM_YAML)
  find_package(yaml-cpp QUIET)
  if(yaml-cpp_FOUND)
    message(STATUS "Using system yaml-cpp")
    add_library(yaze_yaml INTERFACE)
    if(TARGET yaml-cpp::yaml-cpp)
      message(STATUS "Linking yaze_yaml against yaml-cpp::yaml-cpp")
      target_link_libraries(yaze_yaml INTERFACE yaml-cpp::yaml-cpp)
      
      # HACK: Explicitly add the library directory for Homebrew if detected
      # This fixes 'ld: library not found for -lyaml-cpp' when the imported target
      # doesn't propagate the library path correctly to the linker command line
      if(EXISTS "/opt/homebrew/opt/yaml-cpp/lib")
        link_directories("/opt/homebrew/opt/yaml-cpp/lib")
        message(STATUS "Added yaml-cpp link directory: /opt/homebrew/opt/yaml-cpp/lib")
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
