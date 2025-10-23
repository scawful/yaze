cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0074 NEW)

# Include FetchContent module
include(FetchContent)

# Set minimum CMake version for subprojects (fixes c-ares compatibility)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(FETCHCONTENT_QUIET OFF)

# CRITICAL: Prevent CMake from finding system-installed protobuf/abseil
# This ensures gRPC uses its own bundled versions
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_absl TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_gRPC TRUE)

# Also prevent pkg-config from finding system packages
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)

# Save YAZE's C++ standard and temporarily set to C++17 for gRPC
set(_SAVED_CMAKE_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD 17)

find_package(ZLIB REQUIRED)

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
# Disable C++ reflection support (avoids extra proto generation)
set(gRPC_BUILD_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CPP_REFLECTION OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPCPP_REFLECTION OFF CACHE BOOL "" FORCE)

set(gRPC_BENCHMARK_PROVIDER "none" CACHE STRING "" FORCE)
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)

# Let gRPC fetch and build its own protobuf and abseil
set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)

# Protobuf configuration
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)

# Abseil configuration
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Additional protobuf settings to avoid export conflicts
set(protobuf_BUILD_LIBPROTOC ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBPROTOBUF ON CACHE BOOL "" FORCE)
set(protobuf_BUILD_LIBPROTOBUF_LITE ON CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)

set(utf8_range_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(utf8_range_INSTALL OFF CACHE BOOL "" FORCE)

# Declare gRPC with platform-specific versions
if(WIN32 AND MSVC)
  set(_GRPC_VERSION "v1.67.1")
  set(_GRPC_VERSION_REASON "MSVC-compatible, avoids linker regressions")
else()
  set(_GRPC_VERSION "v1.75.1")
  set(_GRPC_VERSION_REASON "ARM64 macOS + modern Clang compatibility")
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

# Some toolchain presets set CMAKE_CROSSCOMPILING even when building for the
# host (macOS arm64). gRPC treats that as a signal to locate host-side protoc
# binaries via find_program, which fails since we rely on the bundled targets.
# Suppress the flag when the host and target platforms match so the generator
# expressions remain intact.
set(_SAVED_CMAKE_CROSSCOMPILING ${CMAKE_CROSSCOMPILING})
if(CMAKE_HOST_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME
   AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL CMAKE_SYSTEM_PROCESSOR)
  set(CMAKE_CROSSCOMPILING FALSE)
endif()

# Download and build in isolation
FetchContent_MakeAvailable(grpc)

# Restore cross-compiling flag
set(CMAKE_CROSSCOMPILING ${_SAVED_CMAKE_CROSSCOMPILING})

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

set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens)
file(REMOVE_RECURSE ${_gRPC_PROTO_GENS_DIR})
file(MAKE_DIRECTORY ${_gRPC_PROTO_GENS_DIR})

get_target_property(_PROTOBUF_INCLUDE_DIRS libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
list(GET _PROTOBUF_INCLUDE_DIRS 0 _gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR)

message(STATUS "gRPC setup complete")

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
        COMMAND $<TARGET_FILE:protoc>
        ARGS --grpc_out=generate_mock_code=true:${_gRPC_PROTO_GENS_DIR}
            --cpp_out=${_gRPC_PROTO_GENS_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
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
