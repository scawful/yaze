# This file centralizes the management of all third-party dependencies.
# It provides functions to find or fetch dependencies and creates alias targets
# for consistent usage throughout the project.

include(FetchContent)

# ============================================================================
# Helper function to add a dependency
# ============================================================================
function(yaze_add_dependency name)
  set(options)
  set(oneValueArgs GIT_REPOSITORY GIT_TAG URL)
  set(multiValueArgs)
  cmake_parse_arguments(DEP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(TARGET yaze::${name})
    return()
  endif()

  # Try to find the package via find_package first
  find_package(${name} QUIET)

  if(${name}_FOUND)
    message(STATUS "Found ${name} via find_package")
    if(TARGET ${name}::${name})
        add_library(yaze::${name} ALIAS ${name}::${name})
    else()
        # Handle cases where find_package doesn't create an imported target
        # This is a simplified approach; more logic may be needed for specific packages
        add_library(yaze::${name} INTERFACE IMPORTED)
        target_include_directories(yaze::${name} INTERFACE ${${name}_INCLUDE_DIRS})
        target_link_libraries(yaze::${name} INTERFACE ${${name}_LIBRARIES})
    endif()
    return()
  endif()

  # If not found, use FetchContent
  message(STATUS "Could not find ${name}, fetching from source.")
  FetchContent_Declare(
    ${name}
    GIT_REPOSITORY ${DEP_GIT_REPOSITORY}
    GIT_TAG ${DEP_GIT_TAG}
  )

  FetchContent_GetProperties(${name})
  if(NOT ${name}_POPULATED)
    FetchContent_Populate(${name})
    add_subdirectory(${${name}_SOURCE_DIR} ${${name}_BINARY_DIR})
  endif()

  if(TARGET ${name})
    add_library(yaze::${name} ALIAS ${name})
  elseif(TARGET ${name}::${name})
    add_library(yaze::${name} ALIAS ${name}::${name})
  else()
    message(FATAL_ERROR "Failed to create target for ${name}")
  endif()
endfunction()

# ============================================================================
# Dependency Declarations
# ============================================================================

# gRPC (must come before Abseil - provides its own compatible Abseil)
if(YAZE_WITH_GRPC)
  include(cmake/grpc.cmake)
  # Verify ABSL_TARGETS was populated by gRPC
  list(LENGTH ABSL_TARGETS _absl_count)
  if(_absl_count EQUAL 0)
    message(FATAL_ERROR "ABSL_TARGETS is empty after including grpc.cmake!")
  else()
    message(STATUS "gRPC provides ${_absl_count} Abseil targets for linking")
  endif()
endif()

# Abseil (only if gRPC didn't provide it)
if(NOT YAZE_WITH_GRPC)
  include(cmake/absl.cmake)
  # Verify ABSL_TARGETS was populated
  list(LENGTH ABSL_TARGETS _absl_count)
  if(_absl_count EQUAL 0)
    message(FATAL_ERROR "ABSL_TARGETS is empty after including absl.cmake!")
  else()
    message(STATUS "Abseil provides ${_absl_count} targets for linking")
  endif()
endif()

set(YAZE_PROTOBUF_TARGETS)

if(TARGET protobuf::libprotobuf)
  list(APPEND YAZE_PROTOBUF_TARGETS protobuf::libprotobuf)
else()
  if(TARGET libprotobuf)
    list(APPEND YAZE_PROTOBUF_TARGETS libprotobuf)
  endif()
endif()

set(YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS ${YAZE_PROTOBUF_TARGETS})

if(YAZE_PROTOBUF_TARGETS)
  list(GET YAZE_PROTOBUF_TARGETS 0 YAZE_PROTOBUF_TARGET)
else()
  set(YAZE_PROTOBUF_TARGET "")
endif()

# SDL2
include(cmake/sdl2.cmake)

# Asar
include(cmake/asar.cmake)

# Google Test
if(YAZE_BUILD_TESTS)
  include(cmake/gtest.cmake)
endif()

# ImGui
include(cmake/imgui.cmake)

# FTXUI (for z3ed)
if(YAZE_BUILD_Z3ED)
    FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
        GIT_TAG v5.0.0
    )
    FetchContent_MakeAvailable(ftxui)
endif()

# yaml-cpp (always available for configuration files)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "Disable yaml-cpp format target" FORCE)

# yaml-cpp (uses CMAKE_POLICY_VERSION_MINIMUM set in root CMakeLists.txt)
FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# Fix MSVC exception handling warning for yaml-cpp
if(MSVC AND TARGET yaml-cpp)
  target_compile_options(yaml-cpp PRIVATE /EHsc)
endif()

# nlohmann_json (header only)
if(YAZE_WITH_JSON)
    set(JSON_BuildTests OFF CACHE INTERNAL "Disable nlohmann_json tests")
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/json ${CMAKE_BINARY_DIR}/third_party/json EXCLUDE_FROM_ALL)
endif()

# httplib (header only)
# No action needed here as it's included directly.
