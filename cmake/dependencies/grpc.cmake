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
set(utf8_range_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Temporarily disable installation to prevent utf8_range export errors
# This is a workaround for gRPC 1.67.1 where utf8_range tries to install targets
# that depend on Abseil, but we have ABSL_ENABLE_INSTALL=OFF
set(CMAKE_SKIP_INSTALL_RULES TRUE)

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

