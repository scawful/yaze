# Abseil release to use when fetching from source
set(YAZE_ABSL_GIT_TAG "20240116.2" CACHE STRING "Abseil release tag used when fetching from source")

# Attempt to use the system package unless the build explicitly requests the
# bundled (fetched) copy or we're on platforms where prebuilt packages are often
# missing the required components (e.g. macOS).
set(_yaze_use_fetched_absl ${YAZE_FORCE_BUNDLED_ABSL})
if(NOT _yaze_use_fetched_absl)
  # Try to find via vcpkg first on Windows
  if(WIN32 AND DEFINED CMAKE_TOOLCHAIN_FILE)
    find_package(absl CONFIG QUIET)
  else()
    find_package(absl QUIET CONFIG)
  endif()
  
  if(absl_FOUND)
    message(STATUS "✓ Using system/vcpkg Abseil package")
  else()
    set(_yaze_use_fetched_absl TRUE)
    message(STATUS "○ System Abseil not found. Will fetch release ${YAZE_ABSL_GIT_TAG}")
  endif()
endif()

if(_yaze_use_fetched_absl)
  include(FetchContent)
  FetchContent_GetProperties(absl)
  if(NOT absl_POPULATED)
    FetchContent_Declare(
      absl
      GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
      GIT_TAG        ${YAZE_ABSL_GIT_TAG}
      GIT_SHALLOW    TRUE
      GIT_PROGRESS   TRUE
      USES_TERMINAL_DOWNLOAD TRUE
    )
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(absl)
    message(STATUS "Fetched Abseil ${YAZE_ABSL_GIT_TAG}")
  endif()
endif()

if(NOT TARGET absl::strings)
  message(FATAL_ERROR "Abseil was not found or failed to configure correctly.")
else()
  message(STATUS "✓ Abseil configured successfully (standalone)")
  # Verify critical targets exist
  foreach(_check_target IN ITEMS absl::status absl::statusor absl::str_format absl::flags)
    if(NOT TARGET ${_check_target})
      message(WARNING "Expected Abseil target ${_check_target} not found")
    endif()
  endforeach()
endif()

# Canonical list of Abseil targets that the rest of the project links against.
# Note: Order matters for some linkers - put base libraries first
set(
  ABSL_TARGETS
  absl::base
  absl::config
  absl::core_headers
  absl::utility
  absl::memory
  absl::container_memory
  absl::strings
  absl::str_format
  absl::cord
  absl::hash
  absl::time
  absl::status
  absl::statusor
  absl::flags
  absl::flags_parse
  absl::flags_usage
  absl::flags_commandlineflag
  absl::flags_marshalling
  absl::flags_private_handle_accessor
  absl::flags_program_name
  absl::flags_config
  absl::flags_reflection
  absl::examine_stack
  absl::stacktrace
  absl::failure_signal_handler
  absl::flat_hash_map
  absl::synchronization
  absl::symbolize
)

# Only expose absl::int128 when it's supported without warnings.
if(NOT WIN32)
  list(APPEND ABSL_TARGETS absl::int128)
  message(STATUS "Including absl::int128 target")
else()
  message(STATUS "Skipping absl::int128 target on Windows")
endif()

# ABSL_TARGETS is now available to the rest of the project via include()

# Explicitly find and cache Abseil include directory for standalone Abseil
if(TARGET absl::base)
  get_target_property(_absl_base_source_dir absl::base SOURCE_DIR)
  if(_absl_base_source_dir AND NOT _absl_base_source_dir STREQUAL "NOTFOUND")
    get_filename_component(_absl_base_dir "${_absl_base_source_dir}" DIRECTORY)
    get_filename_component(_absl_root_dir "${_absl_base_dir}" DIRECTORY)
    if(EXISTS "${_absl_root_dir}")
      set(YAZE_ABSL_INCLUDE_DIR "${_absl_root_dir}" CACHE INTERNAL "Abseil include directory (standalone)")
      message(STATUS "Found standalone Abseil include directory: ${_absl_root_dir}")
    endif()
  endif()
  
  # Also try to get from interface include directories
  get_target_property(_absl_interface_includes absl::base INTERFACE_INCLUDE_DIRECTORIES)
  if(_absl_interface_includes AND NOT _absl_interface_includes STREQUAL "NOTFOUND")
    foreach(_inc IN LISTS _absl_interface_includes)
      if(_inc MATCHES "\\$<BUILD_INTERFACE:([^>]+)>")
        set(_absl_inc_path "${CMAKE_MATCH_1}")
        if(EXISTS "${_absl_inc_path}")
          set(YAZE_ABSL_INCLUDE_DIR "${_absl_inc_path}" CACHE INTERNAL "Abseil include directory (standalone)")
          message(STATUS "Found standalone Abseil include directory: ${_absl_inc_path}")
          break()
        endif()
      elseif(NOT _inc MATCHES "\\$<" AND EXISTS "${_inc}")
        set(YAZE_ABSL_INCLUDE_DIR "${_inc}" CACHE INTERNAL "Abseil include directory (standalone)")
        message(STATUS "Found standalone Abseil include directory: ${_inc}")
        break()
      endif()
    endforeach()
  endif()
endif()

yaze_refresh_absl_usage_metadata()

if(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  foreach(_absl_target IN ITEMS absl_random_internal_randen_hwaes absl_random_internal_randen_hwaes_impl)
    if(TARGET ${_absl_target})
      get_target_property(_absl_opts ${_absl_target} COMPILE_OPTIONS)
  if(_absl_opts AND NOT _absl_opts STREQUAL "NOTFOUND")
        set(_absl_filtered_opts)
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
          list(APPEND _absl_filtered_opts ${_absl_opt})
        endforeach()
        set_property(TARGET ${_absl_target} PROPERTY COMPILE_OPTIONS ${_absl_filtered_opts})
      endif()
    endif()
  endforeach()
endif()

# Silence C++23 deprecation warnings for Abseil int128
if(MSVC)
    add_definitions(-DSILENCE_CXX23_DEPRECATIONS)
else()
    add_definitions(-D_SILENCE_CXX23_DEPRECATION_WARNING)
endif()
