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

# Use CPM to fetch gRPC with bundled dependencies
CPMAddPackage(
  NAME grpc
  VERSION ${GRPC_VERSION}
  GITHUB_REPOSITORY grpc/grpc
  GIT_TAG v${GRPC_VERSION}
  OPTIONS
    "gRPC_BUILD_TESTS OFF"
    "gRPC_BUILD_CODEGEN ON"
    "gRPC_BUILD_GRPC_CPP_PLUGIN ON"
    "gRPC_BUILD_CSHARP_EXT OFF"
    "gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF"
    "gRPC_BUILD_GRPC_NODE_PLUGIN OFF"
    "gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF"
    "gRPC_BUILD_GRPC_PHP_PLUGIN OFF"
    "gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF"
    "gRPC_BUILD_GRPC_RUBY_PLUGIN OFF"
    "gRPC_BUILD_REFLECTION OFF"
    "gRPC_BUILD_GRPC_REFLECTION OFF"
    "gRPC_BUILD_GRPC_CPP_REFLECTION OFF"
    "gRPC_BUILD_GRPCPP_REFLECTION OFF"
    "gRPC_BENCHMARK_PROVIDER none"
    "gRPC_ZLIB_PROVIDER package"
    "gRPC_PROTOBUF_PROVIDER module"
    "gRPC_ABSL_PROVIDER module"
    "protobuf_BUILD_TESTS OFF"
    "protobuf_BUILD_CONFORMANCE OFF"
    "protobuf_BUILD_EXAMPLES OFF"
    "protobuf_BUILD_PROTOC_BINARIES ON"
    "protobuf_WITH_ZLIB ON"
    "ABSL_PROPAGATE_CXX_STD ON"
    "ABSL_ENABLE_INSTALL ON"
    "ABSL_BUILD_TESTING OFF"
    "utf8_range_BUILD_TESTS OFF"
    "utf8_range_INSTALL OFF"
)

# Verify gRPC targets are available
if(NOT TARGET grpc::grpc++)
  message(FATAL_ERROR "gRPC target not found after CPM fetch")
endif()

if(NOT TARGET protoc)
  message(FATAL_ERROR "protoc target not found after gRPC setup")
endif()

if(NOT TARGET grpc_cpp_plugin)
  message(FATAL_ERROR "grpc_cpp_plugin target not found after gRPC setup")
endif()

# Create convenience targets for the rest of the project
add_library(yaze_grpc_support INTERFACE)
target_link_libraries(yaze_grpc_support INTERFACE
  grpc::grpc++
  grpc::grpc++_reflection
  protobuf::libprotobuf
)

# Export gRPC targets for use in other CMake files
set(YAZE_GRPC_TARGETS
  grpc::grpc++
  grpc::grpc++_reflection
  protobuf::libprotobuf
  protoc
  grpc_cpp_plugin
  PARENT_SCOPE
)

message(STATUS "gRPC setup complete - targets available: ${YAZE_GRPC_TARGETS}")
