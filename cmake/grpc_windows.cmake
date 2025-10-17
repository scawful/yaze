# Windows-optimized gRPC configuration using vcpkg
# This file provides fast gRPC builds on Windows using pre-compiled packages
#
# Benefits:
# - vcpkg build: ~5 minutes (pre-compiled)
# - FetchContent build: ~45 minutes (compile from source)
#
# To use vcpkg (recommended):
#   vcpkg install grpc:x64-windows
#   cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake ..

cmake_minimum_required(VERSION 3.16)

# Option to use vcpkg for gRPC on Windows
option(YAZE_USE_VCPKG_GRPC "Use vcpkg pre-compiled gRPC packages (Windows only)" ON)

if(WIN32 AND YAZE_USE_VCPKG_GRPC)
  message(STATUS "Attempting to use vcpkg gRPC packages for faster Windows builds...")
  message(STATUS "  Note: If gRPC not in vcpkg.json, will fallback to FetchContent (recommended)")
  
  # Debug: Check if vcpkg toolchain is being used
  if(DEFINED VCPKG_TOOLCHAIN)
    message(STATUS "  vcpkg toolchain detected: ${VCPKG_TOOLCHAIN}")
  endif()
  if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "  CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
  endif()
  
  # Try to find gRPC via vcpkg (try both gRPC and grpc package names)
  find_package(gRPC CONFIG QUIET)
  if(NOT gRPC_FOUND)
    find_package(grpc CONFIG QUIET)
    if(grpc_FOUND)
      set(gRPC_FOUND TRUE)
    endif()
  endif()
  
  find_package(Protobuf CONFIG QUIET)
  
  if(gRPC_FOUND AND Protobuf_FOUND)
    message(STATUS "✓ Using vcpkg gRPC packages (fast build path)")

    # Prevent Windows macro pollution in protobuf-generated headers
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN  # Exclude rarely-used Windows headers
        NOMINMAX             # Don't define min/max macros
        NOGDI                # Exclude GDI (prevents DWORD and other macro conflicts)
    )

    
    # Verify required targets exist (check both with and without gRPC:: namespace)
    set(_grpc_target_found FALSE)
    if(TARGET gRPC::grpc++)
      message(STATUS "  Found gRPC::grpc++ target")
      set(_grpc_target_found TRUE)
      # Create aliases without namespace for compatibility with existing code
      if(NOT TARGET grpc++)
        add_library(grpc++ ALIAS gRPC::grpc++)
      endif()
      if(TARGET gRPC::grpc++_reflection AND NOT TARGET grpc++_reflection)
        add_library(grpc++_reflection ALIAS gRPC::grpc++_reflection)
      endif()
    elseif(TARGET grpc++)
      message(STATUS "  Found grpc++ target")
      set(_grpc_target_found TRUE)
    endif()
    
    if(NOT _grpc_target_found)
      message(WARNING "gRPC found but grpc++ target missing - falling back to FetchContent")
      message(STATUS "  Available targets containing 'grpc':")
      get_property(_all_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
      foreach(_target ${_all_targets})
        if(_target MATCHES "grpc" OR _target MATCHES "gRPC")
          message(STATUS "    - ${_target}")
        endif()
      endforeach()
      set(YAZE_GRPC_CONFIGURED FALSE PARENT_SCOPE)
      return()
    endif()
    
    # Handle protoc (check for both protoc and protobuf::protoc)
    if(NOT TARGET protoc)
      if(TARGET protobuf::protoc)
        get_target_property(PROTOC_LOCATION protobuf::protoc IMPORTED_LOCATION_RELEASE)
        if(NOT PROTOC_LOCATION)
          get_target_property(PROTOC_LOCATION protobuf::protoc IMPORTED_LOCATION)
        endif()
        if(PROTOC_LOCATION)
          add_executable(protoc IMPORTED)
          set_target_properties(protoc PROPERTIES IMPORTED_LOCATION "${PROTOC_LOCATION}")
          message(STATUS "  Found protoc at: ${PROTOC_LOCATION}")
        else()
          message(FATAL_ERROR "protoc executable not found in vcpkg package")
        endif()
      else()
        message(FATAL_ERROR "protoc target not found in vcpkg gRPC package")
      endif()
    endif()
    
    # Handle grpc_cpp_plugin (check for both grpc_cpp_plugin and gRPC::grpc_cpp_plugin)
    if(NOT TARGET grpc_cpp_plugin)
      if(TARGET gRPC::grpc_cpp_plugin)
        get_target_property(PLUGIN_LOCATION gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
        if(NOT PLUGIN_LOCATION)
          get_target_property(PLUGIN_LOCATION gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
        endif()
        if(PLUGIN_LOCATION)
          add_executable(grpc_cpp_plugin IMPORTED)
          set_target_properties(grpc_cpp_plugin PROPERTIES IMPORTED_LOCATION "${PLUGIN_LOCATION}")
          message(STATUS "  Found grpc_cpp_plugin at: ${PLUGIN_LOCATION}")
        endif()
      else()
        # Try find_program as fallback
        find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin HINTS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/grpc)
        if(GRPC_CPP_PLUGIN)
          add_executable(grpc_cpp_plugin IMPORTED)
          set_target_properties(grpc_cpp_plugin PROPERTIES IMPORTED_LOCATION "${GRPC_CPP_PLUGIN}")
          message(STATUS "  Found grpc_cpp_plugin at: ${GRPC_CPP_PLUGIN}")
        else()
          message(FATAL_ERROR "grpc_cpp_plugin not found in vcpkg gRPC package")
        endif()
      endif()
    endif()
    
    # Set variables for compatibility with rest of build system
    set(_gRPC_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc> PARENT_SCOPE)
    set(_gRPC_CPP_PLUGIN $<TARGET_FILE:grpc_cpp_plugin> PARENT_SCOPE)
    set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens)
    file(MAKE_DIRECTORY ${_gRPC_PROTO_GENS_DIR})
    set(_gRPC_PROTO_GENS_DIR ${_gRPC_PROTO_GENS_DIR} PARENT_SCOPE)
    
    # Export gRPC library targets (vcpkg uses gRPC:: namespace)
    # Use the namespaced targets directly
    set(_GRPC_GRPCPP_LIBRARY gRPC::grpc++)
    set(_GRPC_REFLECTION_LIBRARY gRPC::grpc++_reflection)
    
    # Export Abseil targets from vcpkg (critical for linking!)
    # Note: Abseil targets use absl:: namespace consistently
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
      PARENT_SCOPE
    )
    
    # Export protobuf targets (vcpkg uses protobuf:: namespace)
    set(YAZE_PROTOBUF_TARGETS protobuf::libprotobuf PARENT_SCOPE)
    set(YAZE_PROTOBUF_WHOLEARCHIVE_TARGETS protobuf::libprotobuf PARENT_SCOPE)
    
    # Get protobuf include directories for proto generation
    get_target_property(_PROTOBUF_INCLUDE_DIRS protobuf::libprotobuf 
                        INTERFACE_INCLUDE_DIRECTORIES)
    if(_PROTOBUF_INCLUDE_DIRS)
      list(GET _PROTOBUF_INCLUDE_DIRS 0 _gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR)
      set(_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR ${_gRPC_PROTOBUF_WELLKNOWN_INCLUDE_DIR} PARENT_SCOPE)
    endif()
    
    # Define target_add_protobuf() function for proto compilation (needed by vcpkg path)
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
    
    # Skip the FetchContent path
    set(YAZE_GRPC_CONFIGURED TRUE PARENT_SCOPE)
    message(STATUS "gRPC setup complete via vcpkg (includes bundled Abseil)")
    return()
  else()
    message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    message(STATUS "  vcpkg gRPC not found (expected if removed from vcpkg.json)")
    message(STATUS "  Using FetchContent build (faster with caching)")
    message(STATUS "  First build: ~10-15 min, subsequent: <1 min (cached)")
    message(STATUS "  Using gRPC v1.75.1 with Windows compatibility fixes")
    message(STATUS "  Note: BoringSSL ASM disabled for clang-cl compatibility")
    message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
  endif()
endif()

# If we reach here, vcpkg wasn't used - fall back to standard grpc.cmake
message(STATUS "Using FetchContent for gRPC (recommended path for Windows)")
set(YAZE_GRPC_CONFIGURED FALSE PARENT_SCOPE)
