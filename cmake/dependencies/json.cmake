# nlohmann_json dependency management

if(NOT YAZE_ENABLE_JSON)
  return()
endif()

message(STATUS "Setting up nlohmann_json with local ext directory")

# Use the bundled nlohmann_json from ext/json
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/ext/json EXCLUDE_FROM_ALL)

# Verify target is available
if(TARGET nlohmann_json::nlohmann_json)
  message(STATUS "nlohmann_json target found")
elseif(TARGET nlohmann_json)
  # Create alias if only non-namespaced target exists
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  message(STATUS "Created nlohmann_json::nlohmann_json alias")
else()
  message(FATAL_ERROR "nlohmann_json target not found after add_subdirectory")
endif()

# Export for use in other CMake files
set(YAZE_JSON_TARGETS nlohmann_json::nlohmann_json CACHE INTERNAL "nlohmann_json targets")

message(STATUS "nlohmann_json setup complete")

