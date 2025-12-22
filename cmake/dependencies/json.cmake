# nlohmann_json dependency management

if(NOT YAZE_ENABLE_JSON)
  return()
endif()

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up nlohmann_json ${NLOHMANN_JSON_VERSION}")

set(_YAZE_USE_SYSTEM_JSON ${YAZE_USE_SYSTEM_DEPS})

if(NOT _YAZE_USE_SYSTEM_JSON)
  unset(nlohmann_json_DIR CACHE)
endif()

# Try to use system packages first
if(_YAZE_USE_SYSTEM_JSON)
  find_package(nlohmann_json QUIET)
  if(nlohmann_json_FOUND)
    message(STATUS "Using system nlohmann_json")
    set(YAZE_JSON_TARGETS nlohmann_json::nlohmann_json CACHE INTERNAL "nlohmann_json targets")
    return()
  elseif(YAZE_USE_SYSTEM_DEPS)
    message(WARNING "System nlohmann_json not found despite YAZE_USE_SYSTEM_DEPS=ON; falling back to CPM download")
  endif()
endif()

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)

CPMAddPackage(
  NAME nlohmann_json
  VERSION ${NLOHMANN_JSON_VERSION}
  GITHUB_REPOSITORY nlohmann/json
  GIT_TAG v${NLOHMANN_JSON_VERSION}
  OPTIONS
    "JSON_BuildTests OFF"
    "JSON_Install OFF"
    "JSON_MultipleHeaders OFF"
)

# Verify target is available
if(TARGET nlohmann_json::nlohmann_json)
  message(STATUS "nlohmann_json target found")
elseif(TARGET nlohmann_json)
  # Create alias if only non-namespaced target exists
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  message(STATUS "Created nlohmann_json::nlohmann_json alias")
else()
  message(FATAL_ERROR "nlohmann_json target not found after CPM fetch")
endif()

# Export for use in other CMake files
set(YAZE_JSON_TARGETS nlohmann_json::nlohmann_json)
set(YAZE_JSON_TARGETS nlohmann_json::nlohmann_json CACHE INTERNAL "nlohmann_json targets")

message(STATUS "nlohmann_json setup complete")
