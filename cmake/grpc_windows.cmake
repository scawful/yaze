# Windows-optimized gRPC configuration using vcpkg
# This file provides fast gRPC builds on Windows using pre-compiled packages

cmake_minimum_required(VERSION 3.16)

# Option to use vcpkg for gRPC on Windows
option(YAZE_USE_VCPKG_GRPC "Use vcpkg pre-compiled gRPC packages (Windows only)" ON)

if(WIN32 AND YAZE_USE_VCPKG_GRPC)
  message(STATUS "Attempting to use vcpkg gRPC packages for faster Windows builds...")
  
  # Try to find gRPC via vcpkg
  find_package(gRPC CONFIG QUIET)
  find_package(Protobuf CONFIG QUIET)
  
  if(gRPC_FOUND AND Protobuf_FOUND)
    message(STATUS "✓ Using vcpkg gRPC packages (fast build path)")
    
    # Verify required targets exist
    if(NOT TARGET grpc++)
      message(FATAL_ERROR "gRPC found but grpc++ target missing")
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
    set(_gRPC_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
    set(_gRPC_CPP_PLUGIN $<TARGET_FILE:grpc_cpp_plugin>)
    set(_gRPC_PROTO_GENS_DIR ${CMAKE_BINARY_DIR}/gens)
    file(MAKE_DIRECTORY ${_gRPC_PROTO_GENS_DIR})
    
    # Skip the FetchContent path
    set(YAZE_GRPC_CONFIGURED TRUE PARENT_SCOPE)
    return()
  else()
    message(WARNING "vcpkg gRPC not found. Install with: vcpkg install grpc:x64-windows")
    message(STATUS "Falling back to FetchContent build (this will be slow on first build)")
  endif()
endif()

# If we reach here, vcpkg wasn't used - fall back to standard grpc.cmake
message(STATUS "Using FetchContent for gRPC (standard path)")
set(YAZE_GRPC_CONFIGURED FALSE PARENT_SCOPE)
