# CMake Specifications --------------------------------------------------------
cmake_minimum_required(VERSION 3.10)

# Yet Another Zelda3 Editor
# by scawful
project(yaze VERSION 0.01)

# C++ Standard Specifications -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_LINKER_FLAGS \"-Wl,--no-undefined -Wl,--no-undefined\")
set(BUILD_SHARED_LIBS ON)

# Abseil Standard Specifications ----------------------------------------------
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_CXX_STANDARD 17)
set(ABSL_USE_GOOGLETEST_HEAD ON)
set(ABSL_ENABLE_INSTALL ON)
set(
  ABSL_TARGETS   
  absl::strings
  absl::flags
  absl::status
  absl::statusor
  absl::examine_stack
  absl::stacktrace
  absl::base
  absl::config
  absl::core_headers
  absl::raw_logging_internal
  absl::failure_signal_handler
  absl::flat_hash_map
)

# Video Libraries -------------------------------------------------------------
find_package(PNG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Project Files
add_subdirectory(src/lib/abseil-cpp)
add_subdirectory(src/lib/SDL)
add_subdirectory(src)
add_subdirectory(test)