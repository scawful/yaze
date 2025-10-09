cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0074 NEW)

# Include FetchContent module
include(FetchContent)

# Try Windows-optimized path first
if(WIN32)
  include(${CMAKE_CURRENT_LIST_DIR}/grpc_windows.cmake)
  if(YAZE_GRPC_CONFIGURED)
    return()
  endif()
endif()

# Set minimum CMake version for subprojects (fixes c-ares compatibility)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(FETCHCONTENT_QUIET OFF)

# CRITICAL: Prevent CMake from finding system-installed protobuf
# This ensures gRPC uses its own bundled versions
# NOTE: We allow gRPC to use our FetchContent abseil to keep versions in sync
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_gRPC TRUE)

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

set(gRPC_BENCHMARK_PROVIDER "none" CACHE STRING "" FORCE)
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)

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

# Abseil configuration
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Declare gRPC - use v1.67.1 which fixes MSVC template issues and is compatible with modern compilers
# v1.67.1 includes:
# - MSVC/Visual Studio compatibility fixes (template instantiation errors)
# - Clang 18+ compatibility
# - Abseil compatibility updates
FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG        v1.67.1
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

message(STATUS "gRPC setup complete")

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
