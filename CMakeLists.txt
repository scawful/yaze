# CMake Specifications --------------------------------------------------------
cmake_minimum_required(VERSION 3.10)

# Yet Another Zelda3 Editor
# by scawful
project(yaze VERSION 0.01)

# C++ Standard Specifications -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_LINKER_FLAGS \"-Wl,--no-undefined -Wl,--no-undefined\")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS ON)

# Abseil Standard Specifications ----------------------------------------------
include(cmake/absl.cmake)
include(cmake/openssl.cmake)

set(PROTOBUF_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}
    CACHE INTERNAL "Path to generated protobuf files.")
include_directories(${PROTOBUF_INCLUDE_PATH})

# Video Libraries -------------------------------------------------------------
set(Protobuf_PROTOC_EXECUTABLE "/Users/scawful/code/protobuf/bin/protoc")
set(Protobuf_INCLUDE_DIRS "/Users/scawful/code/protobuf/include/")
find_package(PNG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(gRPC REQUIRED)

# Asar Assembly ---------------------------------------------------------------
add_subdirectory(src/lib/asar/src)
include(cmake/asar.cmake)

include(cmake/imgui.cmake)

# Project Files
add_subdirectory(src/lib/abseil-cpp)
add_subdirectory(src/lib/SDL)
add_subdirectory(src)
add_subdirectory(test)