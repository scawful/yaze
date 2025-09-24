# Try to find Abseil as a system package first
find_package(absl QUIET)
if(NOT absl_FOUND)
  # If not found, try to use bundled Abseil
  if(EXISTS "${CMAKE_SOURCE_DIR}/src/lib/abseil-cpp/CMakeLists.txt")
    add_subdirectory(src/lib/abseil-cpp)
  else()
    message(FATAL_ERROR "Abseil not found and no bundled Abseil available. Please install libabsl-dev")
  endif()
endif()
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
