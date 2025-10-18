cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0074 NEW)

# Include FetchContent module
include(FetchContent)

# Try Windows-optimized path first
if(WIN32)
  include(${CMAKE_CURRENT_LIST_DIR}/grpc_windows.cmake)
  if(YAZE_GRPC_CONFIGURED)
    # Validate that grpc_windows.cmake properly exported required targets/variables
    if(NOT COMMAND target_add_protobuf)
      message(FATAL_ERROR "grpc_windows.cmake did not define target_add_protobuf function")
    endif()
    if(NOT DEFINED ABSL_TARGETS OR NOT ABSL_TARGETS)
      message(FATAL_ERROR "grpc_windows.cmake did not export ABSL_TARGETS")
    endif()
    if(NOT DEFINED YAZE_PROTOBUF_TARGETS OR NOT YAZE_PROTOBUF_TARGETS)
      message(FATAL_ERROR "grpc_windows.cmake did not export YAZE_PROTOBUF_TARGETS")
    endif()
    message(STATUS "✓ Windows vcpkg gRPC configuration validated")
    return()
  endif()
endif()

# Set minimum CMake version for subprojects (fixes c-ares compatibility)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(FETCHCONTENT_QUIET OFF)

# CRITICAL: Prevent CMake from finding system-installed protobuf/abseil
# This ensures gRPC uses its own bundled versions
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_gRPC TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_absl TRUE)

# Also prevent pkg-config from finding system packages
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)

# Add compiler flags for modern compiler compatibility
# These flags are scoped to gRPC and its dependencies only
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang 15+ compatibility for gRPC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=missing-template-arg-list-after-template-kw")
    add_compile_definitions(_LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS)
elseif(MSVC)
    # MSVC/Visual Studio compatibility for gRPC templates
    # v1.67.1 fixes most issues, but these flags help with large template instantiations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")  # Large object files
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")  # Standards conformance
    
    # Suppress common gRPC warnings on MSVC (don't use add_compile_options to avoid affecting user code)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267 /wd4244")
    
    # Increase template instantiation depth for complex promise chains (MSVC 2019+)
    if(MSVC_VERSION GREATER_EQUAL 1920)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /constexpr:depth2048")
    endif()
    
    # Prevent Windows macro pollution in protobuf-generated headers
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN  # Exclude rarely-used Windows headers
        NOMINMAX             # Don't define min/max macros
        NOGDI                # Exclude GDI (prevents DWORD and other macro conflicts)
    )
endif()

# Save YAZE's C++ standard and temporarily set to C++17 for gRPC
set(_SAVED_CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD 17)

# Configure gRPC build options before fetching
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

set(gRPC_BENCHMARK_PROVIDER "none" CACHE STRING "" FORCE)
set(gRPC_ZLIB_PROVIDER "module" CACHE STRING "" FORCE)

# Skip install rule generation inside gRPC's dependency graph. This avoids
# configure-time checks that require every transitive dependency (like Abseil
# compatibility shims) to participate in install export sets, which we do not
# need for the editor builds.
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)

# Let gRPC fetch and build its own protobuf and abseil
set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)

# Protobuf configuration
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)
set(protobuf_MSVC_STATIC_RUNTIME ON CACHE BOOL "" FORCE)

# Abseil configuration
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_MSVC_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
set(gRPC_MSVC_STATIC_RUNTIME ON CACHE BOOL "" FORCE)

# Disable x86-specific optimizations for ARM64 macOS builds
if(APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  set(ABSL_USE_EXTERNAL_GOOGLETEST OFF CACHE BOOL "" FORCE)
  set(ABSL_BUILD_TEST_HELPERS OFF CACHE BOOL "" FORCE)
  # Disable problematic random targets that use x86-specific instructions
  set(ABSL_RANDOM_HWAES_IMPL OFF CACHE BOOL "" FORCE)
  set(ABSL_RANDOM_HWAES OFF CACHE BOOL "" FORCE)
  # Disable all x86-specific random implementations
  set(ABSL_RANDOM_INTERNAL_RANDEN_HWAES_IMPL OFF CACHE BOOL "" FORCE)
  set(ABSL_RANDOM_INTERNAL_RANDEN_HWAES OFF CACHE BOOL "" FORCE)
  # Force use of portable random implementation
  set(ABSL_RANDOM_INTERNAL_PLATFORM_IMPL "portable" CACHE STRING "" FORCE)
endif()

# Declare gRPC version - using stable version with better protobuf compatibility
# v1.67.1 has good stability and protobuf compatibility
set(_GRPC_VERSION "v1.67.1")
set(_GRPC_VERSION_REASON "Stable version with good protobuf compatibility")

# Windows-specific: Disable BoringSSL ASM to avoid NASM build issues
if(WIN32)
  set(OPENSSL_NO_ASM ON CACHE BOOL "" FORCE)
  message(STATUS "Disabling BoringSSL ASM optimizations for Windows build compatibility")
endif()

message(STATUS "FetchContent gRPC version: ${_GRPC_VERSION} (${_GRPC_VERSION_REASON})")

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG        ${_GRPC_VERSION}
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
  USES_TERMINAL_DOWNLOAD TRUE
)

# Save the current CMAKE_PREFIX_PATH and clear it temporarily
# This prevents system packages from interfering
set(_SAVED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "")

# Download and build in isolation
FetchContent_MakeAvailable(grpc)

# Restore CMAKE_PREFIX_PATH
set(CMAKE_PREFIX_PATH ${_SAVED_CMAKE_PREFIX_PATH})

# Restore YAZE's C++ standard
set(CMAKE_CXX_STANDARD ${_SAVED_CMAKE_CXX_STANDARD})

# Verify targets
if(NOT TARGET protoc)
    message(FATAL_ERROR "Can not find target protoc")
endif()
if(NOT TARGET grpc_cpp_plugin)
    message(FATAL_ERROR "Can not find target grpc_cpp_plugin")
endif()

set(_gRPC_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
set(_gRPC_CPP_PLUGIN $<TARGET_FILE:grpc_cpp_plugin>)
set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens)
file(MAKE_DIRECTORY ${_gRPC_PROTO_GENS_DIR})

get_target_property(_PROTOBUF_INCLUDE_DIRS libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
list(GET _PROTOBUF_INCLUDE_DIRS 0 _gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR)

# Export Abseil targets from gRPC's bundled abseil for use by the rest of the project
# This ensures version compatibility between gRPC and our project
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

# Only expose absl::int128 when it's supported without warnings
if(NOT WIN32)
  list(APPEND ABSL_TARGETS absl::int128)
endif()

# ABSL_TARGETS is now available to the rest of the project via include()

# Fix Abseil ARM64 macOS compile flags (remove x86-specific flags)
if(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
  # List of all Abseil targets that might have x86-specific flags
  set(_absl_targets_with_x86_flags
    absl_random_internal_randen_hwaes
    absl_random_internal_randen_hwaes_impl
    absl_random_internal_randen_hwaes_impl
    absl_random_internal_randen_hwaes
  )
  
  foreach(_absl_target IN LISTS _absl_targets_with_x86_flags)
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
          if(_absl_opt STREQUAL "-maes" OR _absl_opt STREQUAL "-msse4.1" OR _absl_opt STREQUAL "-msse2")
            continue()
          endif()
          list(APPEND _absl_filtered_opts ${_absl_opt})
        endforeach()
        set_property(TARGET ${_absl_target} PROPERTY COMPILE_OPTIONS ${_absl_filtered_opts})
        message(STATUS "Fixed ARM64 flags for ${_absl_target}")
      endif()
    endif()
  endforeach()
endif()

message(STATUS "gRPC setup complete (includes bundled Abseil)")

function(target_add_protobuf target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Target ${target} doesn't exist")
    endif()
    if(NOT ARGN)
        message(SEND_ERROR "Error: target_add_protobuf() called without any proto files")
        return()
    endif()

    set(_protobuf_include_path -I . -I ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR})
    foreach(FIL ${ARGN})
        get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
        get_filename_component(FIL_WE ${FIL} NAME_WE)
        file(RELATIVE_PATH REL_FIL ${CMAKE_CURRENT_SOURCE_DIR} ${ABS_FIL})
        get_filename_component(REL_DIR ${REL_FIL} DIRECTORY)
        if(NOT REL_DIR)
            set(RELFIL_WE "${FIL_WE}")
        else()
            set(RELFIL_WE "${REL_DIR}/${FIL_WE}")
        endif()

        add_custom_command(
        OUTPUT  "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.grpc.pb.cc"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.grpc.pb.h"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}_mock.grpc.pb.h"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.cc"
                "${_gRPC_PROTO_GENS_DIR}/${RELFIL_WE}.pb.h"
        COMMAND ${_gRPC_PROTOBUF_PROTOC_EXECUTABLE}
        ARGS --grpc_out=generate_mock_code=true:${_gRPC_PROTO_GENS_DIR}
            --cpp_out=${_gRPC_PROTO_GENS_DIR}
            --plugin=protoc-gen-grpc=${_gRPC_CPP_PLUGIN}
            ${_protobuf_include_path}
            ${REL_FIL}
        DEPENDS ${ABS_FIL} protoc grpc_cpp_plugin
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
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
