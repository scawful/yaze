# gRPC and Protobuf dependency management
# Uses CPM.cmake for consistent cross-platform builds

if(NOT YAZE_ENABLE_GRPC)
  return()
endif()

# Include CPM and dependencies lock
include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up gRPC ${GRPC_VERSION} with CPM.cmake")

# Try to use system packages first if requested
if(YAZE_USE_SYSTEM_DEPS)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GRPC_PC grpc++)
    if(GRPC_PC_FOUND)
      message(STATUS "Using system gRPC via pkg-config")
      add_library(grpc::grpc++ INTERFACE IMPORTED)
      target_include_directories(grpc::grpc++ INTERFACE ${GRPC_PC_INCLUDE_DIRS})
      target_link_libraries(grpc::grpc++ INTERFACE ${GRPC_PC_LIBRARIES})
      target_compile_options(grpc::grpc++ INTERFACE ${GRPC_PC_CFLAGS_OTHER})
      return()
    endif()
  endif()
endif()

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
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)
set(gRPC_PROTOBUF_PROVIDER "module" CACHE STRING "" FORCE)
set(gRPC_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
set(protobuf_WITH_ZLIB ON CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(utf8_range_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(utf8_range_INSTALL OFF CACHE BOOL "" FORCE)

# Use CPM to fetch gRPC with bundled dependencies
# GIT_SUBMODULES "" disables submodule recursion since gRPC handles its own deps via CMake
CPMAddPackage(
  NAME grpc
  VERSION ${GRPC_VERSION}
  GITHUB_REPOSITORY grpc/grpc
  GIT_TAG v${GRPC_VERSION}
  GIT_SUBMODULES ""
  GIT_SHALLOW TRUE
)

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

# Verify gRPC targets are available
if(NOT TARGET grpc++ AND NOT TARGET grpc::grpc++)
  message(FATAL_ERROR "gRPC target not found after CPM fetch")
endif()

if(NOT TARGET protoc)
  message(FATAL_ERROR "protoc target not found after gRPC setup")
endif()

if(NOT TARGET grpc_cpp_plugin)
  message(FATAL_ERROR "grpc_cpp_plugin target not found after gRPC setup")
endif()

# Create convenience interface for basic gRPC linking (renamed to avoid conflict with yaze_grpc_support STATIC library)
add_library(yaze_grpc_deps INTERFACE)
target_link_libraries(yaze_grpc_deps INTERFACE
  grpc::grpc++
  grpc::grpc++_reflection
  protobuf::libprotobuf
)

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
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gens)

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

message(STATUS "Protobuf gens dir: ${_gRPC_PROTO_GENS_DIR}")
message(STATUS "Protobuf include dir: ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR}")

# Export protobuf targets
set(YAZE_PROTOBUF_TARGETS
  protobuf::libprotobuf
)

# Function to add protobuf/gRPC code generation to a target
function(target_add_protobuf target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Target ${target} doesn't exist")
    endif()
    if(NOT ARGN)
        message(SEND_ERROR "Error: target_add_protobuf() called without any proto files")
        return()
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
        COMMAND $<TARGET_FILE:protoc>
        ARGS --grpc_out=generate_mock_code=true:${_gRPC_PROTO_GENS_DIR}
            --cpp_out=${_gRPC_PROTO_GENS_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
            ${_protobuf_include_path}
            ${ABS_FIL}
        DEPENDS ${ABS_FIL} protoc grpc_cpp_plugin
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

