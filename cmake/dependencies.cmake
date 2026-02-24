# YAZE Dependencies Management
# Centralized dependency management using CPM.cmake

# Include CPM and options
include(cmake/CPM.cmake)
include(cmake/options.cmake)
include(cmake/dependencies.lock)

message(STATUS "=== Setting up YAZE dependencies with CPM.cmake ===")

# Only prefer local/system packages when explicitly requested.
set(CPM_USE_LOCAL_PACKAGES ${YAZE_USE_SYSTEM_DEPS} CACHE BOOL "" FORCE)

# Clear any previous dependency targets
set(YAZE_ALL_DEPENDENCIES "")
set(YAZE_SDL2_TARGETS "")
set(YAZE_SDL3_TARGETS "")
set(YAZE_WINDOW_TARGETS "")
set(YAZE_YAML_TARGETS "")
set(YAZE_IMGUI_TARGETS "")
set(YAZE_IMPLOT_TARGETS "")
set(YAZE_JSON_TARGETS "")
set(YAZE_HTTPLIB_TARGETS "")
set(YAZE_GRPC_TARGETS "")
set(YAZE_FTXUI_TARGETS "")
set(YAZE_TESTING_TARGETS "")

# Core dependencies (always required)
# Window backend selection is centralized in one place for easier backend swaps.
include(cmake/dependencies/window_backend.cmake)
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_WINDOW_TARGETS})

include(cmake/dependencies/yaml.cmake)
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_YAML_TARGETS})

include(cmake/dependencies/imgui.cmake)
# Debug: message(STATUS "After ImGui setup, YAZE_IMGUI_TARGETS = '${YAZE_IMGUI_TARGETS}'")
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_IMGUI_TARGETS})

include(cmake/dependencies/implot.cmake)
list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_IMPLOT_TARGETS})

# Abseil is required for failure_signal_handler, status, and other utilities
# Only include standalone Abseil when gRPC is disabled - when gRPC is enabled,
# it provides its own bundled Abseil via CPM
if(NOT YAZE_ENABLE_GRPC)
  include(cmake/absl.cmake)
endif()

# Optional dependencies based on feature flags
if(YAZE_ENABLE_JSON)
  include(cmake/dependencies/json.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_JSON_TARGETS})
endif()

# Native HTTP/HTTPS (cpp-httplib) for non-WASM builds
if(NOT EMSCRIPTEN)
  include(cmake/dependencies/httplib.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_HTTPLIB_TARGETS})
endif()

# CRITICAL: Load testing dependencies BEFORE gRPC when both are enabled
# This ensures gmock is available before Abseil (bundled with gRPC) tries to export test_allocator
# which depends on gmock. This prevents CMake export errors.
if(YAZE_BUILD_TESTS AND YAZE_ENABLE_GRPC)
  include(cmake/dependencies/testing.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_TESTING_TARGETS})
endif()

if(YAZE_ENABLE_GRPC)
  include(cmake/dependencies/grpc.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_GRPC_TARGETS})
endif()

if(YAZE_BUILD_CLI AND NOT EMSCRIPTEN)
  include(cmake/dependencies/ftxui.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_FTXUI_TARGETS})
endif()

# Load testing dependencies after gRPC if tests are enabled but gRPC is not
if(YAZE_BUILD_TESTS AND NOT YAZE_ENABLE_GRPC)
  include(cmake/dependencies/testing.cmake)
  list(APPEND YAZE_ALL_DEPENDENCIES ${YAZE_TESTING_TARGETS})
endif()

# ASAR dependency (for ROM assembly)
if(YAZE_ENABLE_ASAR AND NOT EMSCRIPTEN)
  include(cmake/asar.cmake)
  if(ASAR_FOUND)
    list(APPEND YAZE_ALL_DEPENDENCIES ${ASAR_LIBRARIES})
  endif()
endif()

# Print dependency summary
message(STATUS "=== YAZE Dependencies Summary ===")
message(STATUS "Total dependencies: ${YAZE_ALL_DEPENDENCIES}")
message(STATUS "Window backend: ${YAZE_WINDOW_BACKEND} (${YAZE_WINDOW_TARGETS})")
message(STATUS "SDL2-compat targets: ${YAZE_SDL2_TARGETS}")
message(STATUS "YAML: ${YAZE_YAML_TARGETS}")
message(STATUS "ImGui: ${YAZE_IMGUI_TARGETS}")
message(STATUS "ImPlot: ${YAZE_IMPLOT_TARGETS}")
if(YAZE_ENABLE_JSON)
  message(STATUS "JSON: ${YAZE_JSON_TARGETS}")
endif()
if(YAZE_ENABLE_GRPC)
  message(STATUS "gRPC: ${YAZE_GRPC_TARGETS}")
endif()
if(NOT EMSCRIPTEN)
  message(STATUS "httplib: ${YAZE_HTTPLIB_TARGETS}")
endif()
if(YAZE_BUILD_CLI AND NOT EMSCRIPTEN)
  message(STATUS "FTXUI: ${YAZE_FTXUI_TARGETS}")
endif()
if(YAZE_BUILD_TESTS)
  message(STATUS "Testing: ${YAZE_TESTING_TARGETS}")
endif()
if(YAZE_ENABLE_ASAR AND NOT EMSCRIPTEN)
  message(STATUS "ASAR: ${ASAR_FOUND} (${ASAR_LIBRARIES})")
endif()
message(STATUS "=================================")

# Export all dependency targets for use in other CMake files
set(YAZE_ALL_DEPENDENCIES ${YAZE_ALL_DEPENDENCIES})
