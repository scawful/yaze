# CMake Specifications 
cmake_minimum_required(VERSION 3.10)

# Yet Another Zelda3 Editor
# by scawful
project(yaze VERSION 0.2.0)

# Build Flags
set(YAZE_BUILD_APP ON)
set(YAZE_BUILD_Z3ED ON)
set(YAZE_BUILD_PYTHON ON)
set(YAZE_BUILD_CLIB ON)
set(YAZE_BUILD_TESTS OFF)
set(YAZE_BUILD_EXTENSIONS OFF)

# C++ Standard Specifications 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_FIND_FRAMEWORK LAST) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dlinux -Dstricmp=strcasecmp")

# Abseil Standard Specifications
include(cmake/absl.cmake)

# Video Libraries 
find_package(PNG REQUIRED)
include(cmake/sdl2.cmake)

# Asar 
add_subdirectory(src/lib/asar/src)
include(cmake/asar.cmake)

# ImGui
include(cmake/imgui.cmake)

# Project Files
add_subdirectory(src)