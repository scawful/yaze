# nlohmann_json dependency management

if(NOT YAZE_ENABLE_JSON)
  return()
endif()

message(NOTICE "Configuring nlohmann_json support (YAZE_ENABLE_JSON=${YAZE_ENABLE_JSON})")

set(_yaze_json_version "3.11.3")
set(_yaze_json_ext_dir "${CMAKE_SOURCE_DIR}/ext/json")

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_MultipleHeaders OFF CACHE BOOL "" FORCE)

if(EXISTS "${_yaze_json_ext_dir}/CMakeLists.txt")
  message(NOTICE "Setting up nlohmann_json from ext/json bundle")
  add_subdirectory(${_yaze_json_ext_dir} EXCLUDE_FROM_ALL)
else()
  message(NOTICE "ext/json missing CMakeLists.txt; fetching nlohmann_json ${_yaze_json_version} via CPM")
  CPMAddPackage(
    NAME nlohmann_json
    VERSION ${_yaze_json_version}
    GITHUB_REPOSITORY nlohmann/json
    OPTIONS "JSON_BuildTests OFF"
            "JSON_Install OFF"
            "JSON_MultipleHeaders OFF"
  )
endif()

if(TARGET nlohmann_json::nlohmann_json)
  message(STATUS "nlohmann_json target found")
elseif(TARGET nlohmann_json)
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  message(STATUS "Created nlohmann_json::nlohmann_json alias")
else()
  message(FATAL_ERROR "nlohmann_json target not found after setup")
endif()

set(YAZE_JSON_TARGETS nlohmann_json::nlohmann_json CACHE INTERNAL "nlohmann_json targets")

message(STATUS "nlohmann_json setup complete")
