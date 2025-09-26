if (MINGW OR WIN32)
  add_subdirectory(src/lib/abseil-cpp)
elseif(YAZE_MINIMAL_BUILD)
  # For CI builds, always use submodule to avoid dependency issues
  add_subdirectory(src/lib/abseil-cpp)
else()
  # Try system package first, fallback to submodule
  find_package(absl QUIET)
  if(NOT absl_FOUND)
    message(STATUS "System Abseil not found, using submodule")
    add_subdirectory(src/lib/abseil-cpp)
  endif()
endif()
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_CXX_STANDARD 23)
set(ABSL_USE_GOOGLETEST_HEAD ON)
set(ABSL_ENABLE_INSTALL ON)
set(
  ABSL_TARGETS
  absl::strings
  absl::str_format
  absl::flags
  absl::status
  absl::statusor
  absl::examine_stack
  absl::stacktrace
  absl::base
  absl::config
  absl::core_headers
  absl::failure_signal_handler
  absl::flat_hash_map
  absl::cord
  absl::hash
  absl::synchronization
  absl::time
  absl::symbolize
  absl::flags_commandlineflag
  absl::flags_marshalling
  absl::flags_private_handle_accessor
  absl::flags_program_name
  absl::flags_config
  absl::flags_reflection
  absl::container_memory
  absl::memory
  absl::utility
)
