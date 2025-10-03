# Normalize Abseil's hardware AES flags when targeting macOS ARM64 only.
if(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  set(ABSL_RANDOM_HWAES_X64_FLAGS "" CACHE STRING "" FORCE)
  set(ABSL_RANDOM_HWAES_ARM64_FLAGS "-march=armv8-a+crypto" CACHE STRING "" FORCE)
endif()

if (MINGW OR WIN32 OR YAZE_FORCE_BUNDLED_ABSL)
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

# Silence C++23 deprecation warnings for Abseil int128
if(MSVC)
    add_definitions(-DSILENCE_CXX23_DEPRECATIONS)
else()
    add_definitions(-D_SILENCE_CXX23_DEPRECATION_WARNING)
endif()
# Define base Abseil targets
set(
  ABSL_TARGETS
  absl::strings
  absl::str_format
  absl::flags
  absl::flags_parse
  absl::flags_usage
  absl::flags_commandlineflag
  absl::flags_marshalling
  absl::flags_private_handle_accessor
  absl::flags_program_name
  absl::flags_config
  absl::flags_reflection
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
  absl::container_memory
  absl::memory
  absl::utility
)

# Add int128 only on non-Windows platforms to avoid C++23 deprecation issues
if(NOT WIN32)
  list(APPEND ABSL_TARGETS absl::int128)
  message(STATUS "Including absl::int128 (non-Windows platform)")
else()
  message(STATUS "Excluding absl::int128 on Windows to avoid C++23 deprecation issues")
endif()

if(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  foreach(_absl_target IN ITEMS absl_random_internal_randen_hwaes absl_random_internal_randen_hwaes_impl)
    if(TARGET ${_absl_target})
      get_target_property(_absl_opts ${_absl_target} COMPILE_OPTIONS)
      if(NOT _absl_opts STREQUAL "NOTFOUND")
        set(_absl_filtered_opts "")
        set(_absl_skip_next FALSE)
        foreach(_absl_opt IN LISTS _absl_opts)
          if(_absl_skip_next)
            set(_absl_skip_next FALSE)
            continue()
          endif()
          if(_absl_opt STREQUAL "-Xarch_x86_64")
            set(_absl_skip_next TRUE)
            continue()
          endif()
          if(_absl_opt STREQUAL "-maes" OR _absl_opt STREQUAL "-msse4.1")
            continue()
          endif()
          list(APPEND _absl_filtered_opts "${_absl_opt}")
        endforeach()
        set_target_properties(${_absl_target} PROPERTIES COMPILE_OPTIONS "${_absl_filtered_opts}")
      endif()
      target_compile_options(${_absl_target} PRIVATE "-Xarch_arm64" "-march=armv8-a+crypto")
    endif()
  endforeach()
endif()
