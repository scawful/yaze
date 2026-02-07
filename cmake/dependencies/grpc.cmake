# gRPC and Protobuf dependency management
# Uses CPM.cmake for consistent cross-platform builds, with optional system package fallback

if(NOT YAZE_ENABLE_GRPC)
  return()
endif()

# Include CPM and dependencies lock
include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up gRPC ${GRPC_VERSION}")

#-----------------------------------------------------------------------
# Option: YAZE_PREFER_SYSTEM_GRPC - Use system-installed gRPC/protobuf/abseil
# when available (e.g., from Homebrew, apt, vcpkg).
#
# Benefits: Much faster configure/build times for local development
# Trade-off: May have version mismatches between system packages
#
# Example: cmake --preset mac-ai-fast (uses system packages)
#-----------------------------------------------------------------------
option(YAZE_PREFER_SYSTEM_GRPC "Prefer system-installed gRPC/protobuf over CPM" OFF)

if(YAZE_PREFER_SYSTEM_GRPC OR YAZE_USE_SYSTEM_DEPS)
  message(STATUS "Attempting to use system gRPC/protobuf packages...")

  # Ensure OpenSSL is discoverable for Homebrew gRPC on macOS.
  # gRPC's CMake config references OpenSSL::SSL and fails hard if missing.
  find_package(OpenSSL QUIET)
  if(APPLE AND (NOT OpenSSL_FOUND OR NOT TARGET OpenSSL::SSL))
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
    find_package(OpenSSL QUIET)
  endif()
  if(NOT TARGET OpenSSL::SSL)
    # Fallback: synthesize imported targets when OpenSSL is installed but the
    # FindOpenSSL module wasn't able to produce modern targets. Homebrew gRPC
    # exports OpenSSL::SSL in its link interface, so we must define it.
    if(NOT DEFINED OPENSSL_ROOT_DIR AND APPLE)
      set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
    endif()
    set(_YAZE_OPENSSL_HINT_LIBS "")
    set(_YAZE_OPENSSL_HINT_INCS "")
    if(DEFINED OPENSSL_ROOT_DIR)
      list(APPEND _YAZE_OPENSSL_HINT_LIBS "${OPENSSL_ROOT_DIR}/lib")
      list(APPEND _YAZE_OPENSSL_HINT_INCS "${OPENSSL_ROOT_DIR}/include")
    endif()
    if(APPLE)
      list(APPEND _YAZE_OPENSSL_HINT_LIBS "/opt/homebrew/opt/openssl@3/lib")
      list(APPEND _YAZE_OPENSSL_HINT_INCS "/opt/homebrew/opt/openssl@3/include")
    endif()

    find_library(_YAZE_OPENSSL_SSL_LIB NAMES ssl HINTS ${_YAZE_OPENSSL_HINT_LIBS})
    find_library(_YAZE_OPENSSL_CRYPTO_LIB NAMES crypto HINTS ${_YAZE_OPENSSL_HINT_LIBS})
    find_path(_YAZE_OPENSSL_INCLUDE_DIR openssl/ssl.h HINTS ${_YAZE_OPENSSL_HINT_INCS})

    if(_YAZE_OPENSSL_SSL_LIB AND _YAZE_OPENSSL_CRYPTO_LIB AND _YAZE_OPENSSL_INCLUDE_DIR)
      add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${_YAZE_OPENSSL_CRYPTO_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${_YAZE_OPENSSL_INCLUDE_DIR}"
      )

      add_library(OpenSSL::SSL UNKNOWN IMPORTED)
      set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${_YAZE_OPENSSL_SSL_LIB}"
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
        INTERFACE_INCLUDE_DIRECTORIES "${_YAZE_OPENSSL_INCLUDE_DIR}"
      )
    endif()
  endif()

  # Try CMake's find_package first (works with Homebrew on macOS)
  find_package(gRPC CONFIG QUIET)
  find_package(Protobuf CONFIG QUIET)
  find_package(absl CONFIG QUIET)

  if(NOT gRPC_FOUND OR NOT Protobuf_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      if(NOT gRPC_FOUND)
        pkg_check_modules(GRPCPP QUIET grpc++)
        pkg_check_modules(GRPCPP_REFLECTION QUIET grpc++_reflection)
      endif()
      if(NOT Protobuf_FOUND)
        pkg_check_modules(PROTOBUF QUIET protobuf)
      endif()
    endif()

    if(NOT Protobuf_FOUND AND PROTOBUF_FOUND AND NOT TARGET protobuf::libprotobuf)
      add_library(protobuf::libprotobuf INTERFACE IMPORTED)
      target_include_directories(protobuf::libprotobuf INTERFACE ${PROTOBUF_INCLUDE_DIRS})
      if(PROTOBUF_LIBRARY_DIRS)
        target_link_directories(protobuf::libprotobuf INTERFACE ${PROTOBUF_LIBRARY_DIRS})
      endif()
      target_link_libraries(protobuf::libprotobuf INTERFACE ${PROTOBUF_LIBRARIES})
      set(Protobuf_FOUND TRUE)
    endif()

    if(NOT gRPC_FOUND AND GRPCPP_FOUND AND NOT TARGET gRPC::grpc++)
      add_library(gRPC::grpc++ INTERFACE IMPORTED)
      target_include_directories(gRPC::grpc++ INTERFACE ${GRPCPP_INCLUDE_DIRS})
      if(GRPCPP_LIBRARY_DIRS)
        target_link_directories(gRPC::grpc++ INTERFACE ${GRPCPP_LIBRARY_DIRS})
      endif()
      target_link_libraries(gRPC::grpc++ INTERFACE ${GRPCPP_LIBRARIES})
      set(gRPC_FOUND TRUE)
    endif()

    if(NOT TARGET gRPC::grpc++_reflection)
      if(GRPCPP_REFLECTION_FOUND)
        add_library(gRPC::grpc++_reflection INTERFACE IMPORTED)
        target_include_directories(gRPC::grpc++_reflection INTERFACE ${GRPCPP_REFLECTION_INCLUDE_DIRS})
        if(GRPCPP_REFLECTION_LIBRARY_DIRS)
          target_link_directories(gRPC::grpc++_reflection INTERFACE ${GRPCPP_REFLECTION_LIBRARY_DIRS})
        endif()
        target_link_libraries(gRPC::grpc++_reflection INTERFACE ${GRPCPP_REFLECTION_LIBRARIES})
      else()
        find_library(_YAZE_GRPCPP_REFLECTION_LIB NAMES grpc++_reflection)
        if(_YAZE_GRPCPP_REFLECTION_LIB AND TARGET gRPC::grpc++)
          add_library(gRPC::grpc++_reflection INTERFACE IMPORTED)
          get_target_property(_YAZE_GRPCPP_INCLUDE_DIRS gRPC::grpc++ INTERFACE_INCLUDE_DIRECTORIES)
          if(_YAZE_GRPCPP_INCLUDE_DIRS)
            target_include_directories(gRPC::grpc++_reflection INTERFACE ${_YAZE_GRPCPP_INCLUDE_DIRS})
          endif()
          target_link_libraries(gRPC::grpc++_reflection INTERFACE ${_YAZE_GRPCPP_REFLECTION_LIB} gRPC::grpc++)
        endif()
      endif()
    endif()
  endif()

  if(gRPC_FOUND AND Protobuf_FOUND AND absl_FOUND)
    message(STATUS "✓ Found system gRPC: ${gRPC_VERSION}")
    message(STATUS "✓ Found system Protobuf: ${Protobuf_VERSION}")
    message(STATUS "✓ Found system Abseil")

    # Find protoc and grpc_cpp_plugin executables
    find_program(PROTOC_EXECUTABLE protoc)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)

    if(PROTOC_EXECUTABLE AND GRPC_CPP_PLUGIN)
      message(STATUS "✓ Found protoc: ${PROTOC_EXECUTABLE}")
      message(STATUS "✓ Found grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

      # Create imported targets for the executables if they don't exist
      if(NOT TARGET protoc)
        add_executable(protoc IMPORTED)
        set_target_properties(protoc PROPERTIES IMPORTED_LOCATION "${PROTOC_EXECUTABLE}")
      endif()

      if(NOT TARGET grpc_cpp_plugin)
        add_executable(grpc_cpp_plugin IMPORTED)
        set_target_properties(grpc_cpp_plugin PROPERTIES IMPORTED_LOCATION "${GRPC_CPP_PLUGIN}")
      endif()

      # Create convenience interface for basic gRPC linking
      add_library(yaze_grpc_deps INTERFACE)
      target_link_libraries(yaze_grpc_deps INTERFACE
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
      )

      # Ensure Abseil include directories are available
      # Homebrew's abseil may not properly export include dirs
      get_target_property(_ABSL_BASE_INCLUDE absl::base INTERFACE_INCLUDE_DIRECTORIES)
      if(_ABSL_BASE_INCLUDE)
        target_include_directories(yaze_grpc_deps INTERFACE ${_ABSL_BASE_INCLUDE})
        message(STATUS "  Added Abseil include: ${_ABSL_BASE_INCLUDE}")
      elseif(APPLE)
        # Fallback for Homebrew on macOS
        target_include_directories(yaze_grpc_deps INTERFACE /opt/homebrew/include)
        message(STATUS "  Added Homebrew Abseil include: /opt/homebrew/include")
      endif()

      # Create interface libraries for compatibility with CPM target names
      # CPM gRPC creates lowercase 'grpc++' targets
      # System gRPC (Homebrew) creates namespaced 'gRPC::grpc++' targets
      # We create interface libs (not aliases) so we can add include directories
      if(NOT TARGET grpc++)
        add_library(grpc++ INTERFACE)
        target_link_libraries(grpc++ INTERFACE gRPC::grpc++)
        # Add abseil includes for targets linking to grpc++
        if(_ABSL_BASE_INCLUDE)
          target_include_directories(grpc++ INTERFACE ${_ABSL_BASE_INCLUDE})
        elseif(APPLE)
          target_include_directories(grpc++ INTERFACE /opt/homebrew/include)
        endif()
      endif()
      if(NOT TARGET grpc++_reflection)
        add_library(grpc++_reflection INTERFACE)
        target_link_libraries(grpc++_reflection INTERFACE gRPC::grpc++_reflection)
      endif()
      if(NOT TARGET grpc::grpc++)
        add_library(grpc::grpc++ ALIAS gRPC::grpc++)
      endif()
      if(NOT TARGET grpc::grpc++_reflection)
        add_library(grpc::grpc++_reflection ALIAS gRPC::grpc++_reflection)
      endif()

      set(ABSL_TARGETS "")
      set(_YAZE_ABSL_CANDIDATES
        absl::base
        absl::config
        absl::core_headers
        absl::utility
        absl::memory
        absl::container_memory
        absl::strings
        absl::strings_internal
        absl::str_format
        absl::str_format_internal
        absl::cord
        absl::hash
        absl::time
        absl::time_zone
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
        absl::strerror
        absl::int128
      )
      foreach(_yaze_absl_target IN LISTS _YAZE_ABSL_CANDIDATES)
        if(TARGET ${_yaze_absl_target})
          list(APPEND ABSL_TARGETS ${_yaze_absl_target})
        endif()
      endforeach()

      # Export targets
      set(YAZE_GRPC_TARGETS
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
        protoc
        grpc_cpp_plugin
      )
      set(YAZE_PROTOBUF_TARGETS
        protobuf::libprotobuf
      )

      # Setup protobuf generation directory
      set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens CACHE INTERNAL "Protobuf generated files directory")
      file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gens)

      # Get protobuf include directory from package
      get_target_property(_PROTOBUF_INCLUDE_DIRS protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
      if(_PROTOBUF_INCLUDE_DIRS)
        list(GET _PROTOBUF_INCLUDE_DIRS 0 _gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR)
        set(_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR} CACHE INTERNAL "Protobuf include directory")
      endif()

      # Add global include directories for system packages
      # This ensures all targets can find abseil headers even if target propagation fails
      # Use add_compile_options for reliable include path propagation with Ninja Multi-Config
      if(_ABSL_BASE_INCLUDE)
        add_compile_options(-I${_ABSL_BASE_INCLUDE})
        message(STATUS "  Added Abseil include via compile options: ${_ABSL_BASE_INCLUDE}")
      elseif(APPLE)
        add_compile_options(-I/opt/homebrew/include)
        message(STATUS "  Added Homebrew include via compile options: /opt/homebrew/include")
      endif()

      message(STATUS "✓ Using SYSTEM gRPC stack - fast configure!")
      message(STATUS "  Protobuf gens dir: ${_gRPC_PROTO_GENS_DIR}")
      message(STATUS "  Protobuf include dir: ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR}")
      set(_YAZE_USING_SYSTEM_GRPC TRUE)
    else()
      message(STATUS "○ System gRPC found but protoc/grpc_cpp_plugin missing, falling back to CPM")
      set(_YAZE_USING_SYSTEM_GRPC FALSE)
    endif()
  else()
    message(STATUS "○ System gRPC/protobuf not found, falling back to CPM")
    set(_YAZE_USING_SYSTEM_GRPC FALSE)
  endif()
endif()

# If we're using system gRPC, skip CPM entirely and jump to function definition
if(_YAZE_USING_SYSTEM_GRPC)
  message(STATUS "Skipping CPM gRPC build - using system packages")
else()
  # CPM build path
  message(STATUS "Building gRPC from source via CPM (this takes 15-20 minutes on first build)")
  message(STATUS "  Tip: Install gRPC via Homebrew and use -DYAZE_PREFER_SYSTEM_GRPC=ON for faster builds")

#-----------------------------------------------------------------------
# Guard CMake's package lookup so CPM always downloads a consistent gRPC
# toolchain instead of picking up partially-installed Homebrew/apt copies.
#-----------------------------------------------------------------------
if(DEFINED CPM_USE_LOCAL_PACKAGES)
  set(_YAZE_GRPC_SAVED_CPM_USE_LOCAL_PACKAGES "${CPM_USE_LOCAL_PACKAGES}")
else()
  set(_YAZE_GRPC_SAVED_CPM_USE_LOCAL_PACKAGES "__YAZE_UNSET__")
endif()
set(CPM_USE_LOCAL_PACKAGES OFF)

foreach(_yaze_pkg IN ITEMS gRPC Protobuf absl)
  string(TOUPPER "CMAKE_DISABLE_FIND_PACKAGE_${_yaze_pkg}" _yaze_disable_var)
  if(DEFINED ${_yaze_disable_var})
    set("_YAZE_GRPC_SAVE_${_yaze_disable_var}" "${${_yaze_disable_var}}")
  else()
    set("_YAZE_GRPC_SAVE_${_yaze_disable_var}" "__YAZE_UNSET__")
  endif()
  set(${_yaze_disable_var} TRUE)
endforeach()

if(DEFINED PKG_CONFIG_USE_CMAKE_PREFIX_PATH)
  set(_YAZE_GRPC_SAVED_PKG_CONFIG_USE_CMAKE_PREFIX_PATH "${PKG_CONFIG_USE_CMAKE_PREFIX_PATH}")
else()
  set(_YAZE_GRPC_SAVED_PKG_CONFIG_USE_CMAKE_PREFIX_PATH "__YAZE_UNSET__")
endif()
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)

set(_YAZE_GRPC_SAVED_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
set(CMAKE_PREFIX_PATH "")

if(DEFINED CMAKE_CROSSCOMPILING)
  set(_YAZE_GRPC_SAVED_CROSSCOMPILING "${CMAKE_CROSSCOMPILING}")
else()
  set(_YAZE_GRPC_SAVED_CROSSCOMPILING "__YAZE_UNSET__")
endif()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME
   AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL CMAKE_SYSTEM_PROCESSOR)
  set(CMAKE_CROSSCOMPILING FALSE)
endif()

if(DEFINED CMAKE_CXX_STANDARD)
  set(_YAZE_GRPC_SAVED_CXX_STANDARD "${CMAKE_CXX_STANDARD}")
else()
  set(_YAZE_GRPC_SAVED_CXX_STANDARD "__YAZE_UNSET__")
endif()
set(CMAKE_CXX_STANDARD 17)

# Set gRPC options before adding package
set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_CODEGEN ON CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE BOOL "" FORCE)
set(gRPC_BUILD_CSHARP_EXT OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CPP_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPCPP_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BENCHMARK_PROVIDER "none" CACHE STRING "" FORCE)
set(gRPC_ZLIB_PROVIDER "module" CACHE STRING "" FORCE)
set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(utf8_range_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(utf8_range_INSTALL OFF CACHE BOOL "" FORCE)
set(utf8_range_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Force consistent MSVC runtime library across all gRPC components (Windows only)
# This ensures gRPC, protobuf, and Abseil all use the same CRT linking mode
if(WIN32 AND MSVC)
  # Use dynamic CRT (/MD for Release, /MDd for Debug) to avoid undefined math symbols
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
  # Also ensure protobuf doesn't try to use static runtime
  set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
  message(STATUS "Forcing dynamic MSVC runtime for gRPC dependencies: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif()

# Temporarily disable installation to prevent utf8_range export errors
# This is a workaround for gRPC 1.67.1 where utf8_range tries to install targets
# that depend on Abseil, but we have ABSL_ENABLE_INSTALL=OFF
set(CMAKE_SKIP_INSTALL_RULES TRUE)

# Use CPM to fetch gRPC with bundled dependencies
# GIT_SUBMODULES "" disables submodule recursion since gRPC handles its own deps via CMake

if(WIN32)
  set(GRPC_VERSION_TO_USE "1.67.1")
else()
  set(GRPC_VERSION_TO_USE "1.76.0")
endif()

message(STATUS "Selected gRPC version ${GRPC_VERSION_TO_USE} for platform ${CMAKE_SYSTEM_NAME}")

CPMAddPackage(
  NAME grpc
  VERSION ${GRPC_VERSION_TO_USE}
  GITHUB_REPOSITORY grpc/grpc
  GIT_TAG v${GRPC_VERSION_TO_USE}
  GIT_SUBMODULES ""
  GIT_SHALLOW TRUE
)

# Re-enable installation rules after gRPC is loaded
set(CMAKE_SKIP_INSTALL_RULES FALSE)

# Restore CPM lookup behaviour and toolchain detection environment early so
# subsequent dependency configuration isn't polluted even if we hit errors.
if("${_YAZE_GRPC_SAVED_CPM_USE_LOCAL_PACKAGES}" STREQUAL "__YAZE_UNSET__")
  unset(CPM_USE_LOCAL_PACKAGES)
else()
  set(CPM_USE_LOCAL_PACKAGES "${_YAZE_GRPC_SAVED_CPM_USE_LOCAL_PACKAGES}")
endif()

foreach(_yaze_pkg IN ITEMS gRPC Protobuf absl)
  string(TOUPPER "CMAKE_DISABLE_FIND_PACKAGE_${_yaze_pkg}" _yaze_disable_var)
  string(TOUPPER "_YAZE_GRPC_SAVE_${_yaze_disable_var}" _yaze_saved_key)
  if(NOT DEFINED ${_yaze_saved_key})
    continue()
  endif()
  if("${${_yaze_saved_key}}" STREQUAL "__YAZE_UNSET__")
    unset(${_yaze_disable_var})
  else()
    set(${_yaze_disable_var} "${${_yaze_saved_key}}")
  endif()
  unset(${_yaze_saved_key})
endforeach()

if("${_YAZE_GRPC_SAVED_PKG_CONFIG_USE_CMAKE_PREFIX_PATH}" STREQUAL "__YAZE_UNSET__")
  unset(PKG_CONFIG_USE_CMAKE_PREFIX_PATH)
else()
  set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH "${_YAZE_GRPC_SAVED_PKG_CONFIG_USE_CMAKE_PREFIX_PATH}")
endif()
unset(_YAZE_GRPC_SAVED_PKG_CONFIG_USE_CMAKE_PREFIX_PATH)

set(CMAKE_PREFIX_PATH "${_YAZE_GRPC_SAVED_PREFIX_PATH}")
unset(_YAZE_GRPC_SAVED_PREFIX_PATH)

if("${_YAZE_GRPC_SAVED_CROSSCOMPILING}" STREQUAL "__YAZE_UNSET__")
  unset(CMAKE_CROSSCOMPILING)
else()
  set(CMAKE_CROSSCOMPILING "${_YAZE_GRPC_SAVED_CROSSCOMPILING}")
endif()
unset(_YAZE_GRPC_SAVED_CROSSCOMPILING)

if("${_YAZE_GRPC_SAVED_CXX_STANDARD}" STREQUAL "__YAZE_UNSET__")
  unset(CMAKE_CXX_STANDARD)
else()
  set(CMAKE_CXX_STANDARD "${_YAZE_GRPC_SAVED_CXX_STANDARD}")
endif()
unset(_YAZE_GRPC_SAVED_CXX_STANDARD)

# Check which target naming convention is used
if(TARGET grpc++)
  message(STATUS "Found non-namespaced gRPC target grpc++")
  if(NOT TARGET grpc::grpc++)
    add_library(grpc::grpc++ ALIAS grpc++)
  endif()
  if(NOT TARGET grpc::grpc++_reflection AND TARGET grpc++_reflection)
    add_library(grpc::grpc++_reflection ALIAS grpc++_reflection)
  endif()
endif()

set(_YAZE_GRPC_ERRORS "")

if(NOT TARGET grpc++ AND NOT TARGET grpc::grpc++)
  list(APPEND _YAZE_GRPC_ERRORS "gRPC target not found after CPM fetch")
endif()

if(NOT TARGET protoc)
  list(APPEND _YAZE_GRPC_ERRORS "protoc target not found after gRPC setup")
endif()

if(NOT TARGET grpc_cpp_plugin)
  list(APPEND _YAZE_GRPC_ERRORS "grpc_cpp_plugin target not found after gRPC setup")
endif()

if(_YAZE_GRPC_ERRORS)
  list(JOIN _YAZE_GRPC_ERRORS "\n" _YAZE_GRPC_ERROR_MESSAGE)
  message(FATAL_ERROR "${_YAZE_GRPC_ERROR_MESSAGE}")
endif()

# Create convenience interface for basic gRPC linking (renamed to avoid conflict with yaze_grpc_support STATIC library)
add_library(yaze_grpc_deps INTERFACE)
target_link_libraries(yaze_grpc_deps INTERFACE
  grpc::grpc++
  grpc::grpc++_reflection
  protobuf::libprotobuf
)

# Define Windows macro guards once so protobuf-generated headers stay clean
if(WIN32)
  add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    NOGDI
  )
endif()

# Export Abseil targets from gRPC's bundled Abseil
# When gRPC_ABSL_PROVIDER is "module", gRPC fetches and builds Abseil
# All Abseil targets are available, we just need to list them
# Note: All targets are available even if not listed here, but listing ensures consistency
set(ABSL_TARGETS
  absl::base
  absl::config
  absl::core_headers
  absl::utility
  absl::memory
  absl::container_memory
  absl::strings
  absl::strings_internal
  absl::str_format
  absl::str_format_internal
  absl::cord
  absl::hash
  absl::time
  absl::time_zone
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
  absl::strerror
)

# Only expose absl::int128 when it's supported without warnings
if(NOT WIN32)
  list(APPEND ABSL_TARGETS absl::int128)
endif()

# Export gRPC targets for use in other CMake files
set(YAZE_GRPC_TARGETS
  grpc::grpc++
  grpc::grpc++_reflection
  protobuf::libprotobuf
  protoc
  grpc_cpp_plugin
)

message(STATUS "gRPC setup complete - targets available: ${YAZE_GRPC_TARGETS}")

# Setup protobuf generation directory (use CACHE so it's available in functions)
set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens CACHE INTERNAL "Protobuf generated files directory")
# Ensure stale protobuf outputs don't survive protoc version changes.
file(REMOVE_RECURSE ${_gRPC_PROTO_GENS_DIR})
file(MAKE_DIRECTORY ${_gRPC_PROTO_GENS_DIR})

# Get protobuf include directories (extract from generator expression or direct path)
if(TARGET libprotobuf)
  get_target_property(_PROTOBUF_INCLUDE_DIRS libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
  # Handle generator expressions
  string(REGEX REPLACE "\\$<BUILD_INTERFACE:([^>]+)>" "\\1" _PROTOBUF_INCLUDE_DIR_CLEAN "${_PROTOBUF_INCLUDE_DIRS}")
  list(GET _PROTOBUF_INCLUDE_DIR_CLEAN 0 _gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR)
  set(_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR} CACHE INTERNAL "Protobuf include directory")
elseif(TARGET protobuf::libprotobuf)
  get_target_property(_PROTOBUF_INCLUDE_DIRS protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
  string(REGEX REPLACE "\\$<BUILD_INTERFACE:([^>]+)>" "\\1" _PROTOBUF_INCLUDE_DIR_CLEAN "${_PROTOBUF_INCLUDE_DIRS}")
  list(GET _PROTOBUF_INCLUDE_DIR_CLEAN 0 _gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR)
  set(_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR} CACHE INTERNAL "Protobuf include directory")
endif()

# Remove x86-only Abseil compile flags when building on ARM64 macOS runners
set(_YAZE_PATCH_ABSL_FOR_APPLE FALSE)
if(APPLE)
  if(CMAKE_OSX_ARCHITECTURES)
    string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" _yaze_osx_archs)
    if(_yaze_osx_archs MATCHES "arm64")
      set(_YAZE_PATCH_ABSL_FOR_APPLE TRUE)
    endif()
  else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _yaze_proc)
    if(_yaze_proc MATCHES "arm64" OR _yaze_proc MATCHES "aarch64")
      set(_YAZE_PATCH_ABSL_FOR_APPLE TRUE)
    endif()
  endif()
endif()

if(_YAZE_PATCH_ABSL_FOR_APPLE)
  set(_YAZE_ABSL_X86_TARGETS
    absl_random_internal_randen_hwaes
    absl_random_internal_randen_hwaes_impl
    absl_crc_internal_cpu_detect
  )

  foreach(_yaze_absl_target IN LISTS _YAZE_ABSL_X86_TARGETS)
    if(TARGET ${_yaze_absl_target})
      get_target_property(_yaze_absl_opts ${_yaze_absl_target} COMPILE_OPTIONS)
      if(_yaze_absl_opts AND NOT _yaze_absl_opts STREQUAL "NOTFOUND")
        set(_yaze_filtered_opts)
        foreach(_yaze_opt IN LISTS _yaze_absl_opts)
          if(_yaze_opt STREQUAL "-Xarch_x86_64")
            continue()
          endif()
          if(_yaze_opt MATCHES "^-m(sse|avx)")
            continue()
          endif()
          if(_yaze_opt STREQUAL "-maes")
            continue()
          endif()
          list(APPEND _yaze_filtered_opts "${_yaze_opt}")
        endforeach()
        set_property(TARGET ${_yaze_absl_target} PROPERTY COMPILE_OPTIONS ${_yaze_filtered_opts})
        message(STATUS "Patched ${_yaze_absl_target} compile options for ARM64 macOS")
      endif()
    endif()
  endforeach()
endif()

unset(_YAZE_GRPC_SAVED_CPM_USE_LOCAL_PACKAGES)
unset(_YAZE_GRPC_ERRORS)
unset(_YAZE_GRPC_ERROR_MESSAGE)

message(STATUS "Protobuf gens dir: ${_gRPC_PROTO_GENS_DIR}")
message(STATUS "Protobuf include dir: ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR}")

# Export protobuf targets
set(YAZE_PROTOBUF_TARGETS
  protobuf::libprotobuf
)

endif() # End of CPM build path (if NOT _YAZE_USING_SYSTEM_GRPC)

# Function to add protobuf/gRPC code generation to a target
# This function works with both system and CPM-built gRPC
function(target_add_protobuf target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Target ${target} doesn't exist")
    endif()
    if(NOT ARGN)
        message(SEND_ERROR "Error: target_add_protobuf() called without any proto files")
        return()
    endif()

    # Determine protoc and grpc_cpp_plugin paths
    # For IMPORTED targets (system gRPC), use IMPORTED_LOCATION
    # For built targets (CPM gRPC), use TARGET_FILE generator expression
    get_target_property(_PROTOC_IMPORTED protoc IMPORTED)
    get_target_property(_GRPC_PLUGIN_IMPORTED grpc_cpp_plugin IMPORTED)

    if(_PROTOC_IMPORTED)
        get_target_property(_PROTOC_EXECUTABLE protoc IMPORTED_LOCATION)
        set(_PROTOC_DEPENDS "")  # No build dependency for system protoc
    else()
        set(_PROTOC_EXECUTABLE "$<TARGET_FILE:protoc>")
        set(_PROTOC_DEPENDS "protoc")
    endif()

    if(_GRPC_PLUGIN_IMPORTED)
        get_target_property(_GRPC_PLUGIN_EXECUTABLE grpc_cpp_plugin IMPORTED_LOCATION)
        set(_GRPC_PLUGIN_DEPENDS "")  # No build dependency for system plugin
    else()
        set(_GRPC_PLUGIN_EXECUTABLE "$<TARGET_FILE:grpc_cpp_plugin>")
        set(_GRPC_PLUGIN_DEPENDS "grpc_cpp_plugin")
    endif()

    set(_protobuf_include_path -I ${CMAKE_SOURCE_DIR}/src -I ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR})
    foreach(FIL ${ARGN})
        get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
        get_filename_component(FIL_WE ${FIL} NAME_WE)
        file(RELATIVE_PATH REL_FIL ${CMAKE_SOURCE_DIR}/src ${ABS_FIL})
        get_filename_component(REL_DIR ${REL_FIL} DIRECTORY)
        if(NOT REL_DIR OR REL_DIR STREQUAL ".")
            set(RELFIL_WE "${FIL_WE}")
        else()
            set(RELFIL_WE "${REL_DIR}/${FIL_WE}")
        endif()

        message(STATUS "  Proto file: ${FIL_WE}")
        message(STATUS "    ABS_FIL = ${ABS_FIL}")
        message(STATUS "    REL_FIL = ${REL_FIL}")
        message(STATUS "    REL_DIR = ${REL_DIR}")
        message(STATUS "    RELFIL_WE = ${RELFIL_WE}")
        message(STATUS "    Output = ${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.h")

        add_custom_command(
        OUTPUT  "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.grpc.pb.cc"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.grpc.pb.h"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}_mock.grpc.pb.h"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.cc"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.h"
        COMMAND ${_PROTOC_EXECUTABLE}
        ARGS --grpc_out=generate_mock_code=true:${_gRPC_PROTO_GENS_DIR}
            --cpp_out=${_gRPC_PROTO_GENS_DIR}
            --plugin=protoc-gen-grpc=${_GRPC_PLUGIN_EXECUTABLE}
            ${_protobuf_include_path}
            ${ABS_FIL}
        DEPENDS ${ABS_FIL} ${_PROTOC_DEPENDS} ${_GRPC_PLUGIN_DEPENDS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
        COMMENT "Running gRPC C++ protocol buffer compiler on ${FIL}"
        VERBATIM)

        target_sources(${target} PRIVATE
            "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.grpc.pb.cc"
            "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.grpc.pb.h"
            "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}_mock.grpc.pb.h"
            "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.cc"
            "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.h"
        )
        target_include_directories(${target} PUBLIC
            $<BUILD_INTERFACE:${_gRPC_PROTO_GENS_DIR}>
            $<BUILD_INTERFACE:${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR}>
        )
    endforeach()
endfunction()
