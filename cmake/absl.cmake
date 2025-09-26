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
  absl::str_format_internal
  absl::cord
  absl::hash
  absl::synchronization
  absl::time
  absl::symbolize
  absl::debugging_internal
  absl::demangle_internal
  absl::strings_internal
  absl::city
  absl::cordz_functions
  absl::malloc_internal
  absl::graphcycles_internal
  absl::str_format_internal
  absl::cord_internal
  absl::cordz_handle
  absl::cordz_info
  absl::flags_commandlineflag
  absl::flags_commandlineflag_internal
  absl::flags_marshalling
  absl::flags_private_handle_accessor
  absl::flags_program_name
  absl::flags_config
  absl::flags_reflection
  absl::flags_internal
  absl::hashtablez_sampler
  absl::raw_hash_set
  absl::int128
  absl::time_zone
  absl::exponential_biased
  absl::civil_time
  absl::bad_optional_access
  absl::bad_variant_access
  absl::throw_delegate
  absl::log_severity
  absl::spinlock_wait
  absl::strerror
  absl::raw_hash_set
  absl::flags_internal
  absl::cord
  absl::city
  absl::hash
  absl::strings_internal
)
