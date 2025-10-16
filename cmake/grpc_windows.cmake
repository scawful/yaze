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
  message(STATUS "  Note: This is only for full builds with YAZE_WITH_GRPC=ON")
  
  # Try to find gRPC via vcpkg
  find_package(gRPC CONFIG QUIET)
  find_package(Protobuf CONFIG QUIET)
  
  if(gRPC_FOUND AND Protobuf_FOUND)
    message(STATUS "✓ Using vcpkg gRPC packages (fast build path)")

    # Prevent Windows macro pollution in protobuf-generated headers
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN  # Exclude rarely-used Windows headers
        NOMINMAX             # Don't define min/max macros
        NOGDI                # Exclude GDI (prevents DWORD and other macro conflicts)
    )

    
    # Verify required targets exist
    if(NOT TARGET grpc++)
      message(WARNING "gRPC found but grpc++ target missing - falling back to FetchContent")
      set(YAZE_GRPC_CONFIGURED FALSE PARENT_SCOPE)
      return()
    endif()
    
    if(NOT TARGET protoc)
      # Create protoc target if it doesn't exist
      get_target_property(PROTOC_LOCATION protobuf::protoc LOCATION)
      if(PROTOC_LOCATION)
        add_executable(protoc IMPORTED)
        set_target_properties(protoc PROPERTIES IMPORTED_LOCATION "${PROTOC_LOCATION}")
      else()
        message(FATAL_ERROR "protoc executable not found in vcpkg gRPC package")
      endif()
    endif()
    
    if(NOT TARGET grpc_cpp_plugin)
      # Find grpc_cpp_plugin
      find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
      if(GRPC_CPP_PLUGIN)
        add_executable(grpc_cpp_plugin IMPORTED)
        set_target_properties(grpc_cpp_plugin PROPERTIES IMPORTED_LOCATION "${GRPC_CPP_PLUGIN}")
      else()
        message(FATAL_ERROR "grpc_cpp_plugin not found in vcpkg gRPC package")
      endif()
    endif()
    
    # Set variables for compatibility with rest of build system
    set(_gRPC_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc> PARENT_SCOPE)
    set(_gRPC_CPP_PLUGIN $<TARGET_FILE:grpc_cpp_plugin> PARENT_SCOPE)
    set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens)
    file(MAKE_DIRECTORY ${_gRPC_PROTO_GENS_DIR})
    set(_gRPC_PROTO_GENS_DIR ${_gRPC_PROTO_GENS_DIR} PARENT_SCOPE)
    
    # Export Abseil targets from vcpkg (critical for linking!)
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
    
    # Export protobuf targets
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
    message(WARNING "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    message(WARNING "  vcpkg gRPC not found")
    message(WARNING "  For faster builds (5 min vs 45 min), install:")
    message(WARNING "  ")
    message(WARNING "    vcpkg install grpc:x64-windows")
    message(WARNING "  ")
    message(WARNING "  Then configure with:")
    message(WARNING "    cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake ..")
    message(WARNING "  ")
    message(WARNING "  Falling back to FetchContent (slow but works)")
    message(WARNING "  Using gRPC v1.67.1 (MSVC-compatible)")
    message(WARNING "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
  endif()
endif()

# If we reach here, vcpkg wasn't used - fall back to standard grpc.cmake
message(STATUS "Using FetchContent for gRPC (standard path)")
set(YAZE_GRPC_CONFIGURED FALSE PARENT_SCOPE)
