# YAZE Dependencies Management
# Centralized dependency management using CPM.cmake

# Include CPM and options
include(cmake/CPM.cmake)
include(cmake/options.cmake)
include(cmake/dependencies.lock)

message(STATUS "=== Setting up YAZE dependencies with CPM.cmake ===")

# Clear any previous dependency targets
set(YAZE_ALL_DEPENDENCIES "")
set(YAZE_SDL2_TARGETS "")
set(YAZE_YAML_TARGETS "")
set(YAZE_IMGUI_TARGETS "")
set(YAZE_JSON_TARGETS "")
set(YAZE_GRPC_TARGETS "")
set(YAZE_FTXUI_TARGETS "")
set(YAZE_TESTING_TARGETS "")

# Core dependencies (always required)
include(cmake/dependencies/sdl2.cmake)
# Debug: message(STATUS "After SDL2 setup, YAZE_SDL2_TARGETS = '${YAZE_SDL2_TARGETS}'")
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_SDL2_TARGETS})

include(cmake/dependencies/yaml.cmake)
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_YAML_TARGETS})

include(cmake/dependencies/imgui.cmake)
# Debug: message(STATUS "After ImGui setup, YAZE_IMGUI_TARGETS = '${YAZE_IMGUI_TARGETS}'")
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_IMGUI_TARGETS})

# Optional dependencies based on feature flags
if(YAZE_ENABLE_JSON)
  include(cmake/dependencies/json.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_JSON_TARGETS})
endif()

if(YAZE_ENABLE_GRPC)
  include(cmake/dependencies/grpc.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_GRPC_TARGETS})
endif()

if(YAZE_BUILD_CLI)
  include(cmake/dependencies/ftxui.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_FTXUI_TARGETS})
endif()

if(YAZE_BUILD_TESTS)
  include(cmake/dependencies/testing.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_TESTING_TARGETS})
endif()

# ASAR dependency (for ROM assembly) - temporarily disabled
# TODO: Add CMakeLists.txt to bundled ASAR or find working repository
message(STATUS "ASAR dependency temporarily disabled - will be added later")

# Print dependency summary
message(STATUS "=== YAZE Dependencies Summary ===")
message(STATUS "Total dependencies: ${YAZE_ALL_DEPENDENCIES}")
message(STATUS "SDL2: ${YAZE_SDL2_TARGETS}")
message(STATUS "YAML: ${YAZE_YAML_TARGETS}")
message(STATUS "ImGui: ${YAZE_IMGUI_TARGETS}")
if(YAZE_ENABLE_JSON)
  message(STATUS "JSON: ${YAZE_JSON_TARGETS}")
endif()
if(YAZE_ENABLE_GRPC)
  message(STATUS "gRPC: ${YAZE_GRPC_TARGETS}")
endif()
if(YAZE_BUILD_CLI)
  message(STATUS "FTXUI: ${YAZE_FTXUI_TARGETS}")
endif()
if(YAZE_BUILD_TESTS)
  message(STATUS "Testing: ${YAZE_TESTING_TARGETS}")
endif()
message(STATUS "=================================")

# Export all dependency targets for use in other CMake files
set(YAZE_ALL_DEPENDENCIES ${YAZE_ALL_DEPENDENCIES})