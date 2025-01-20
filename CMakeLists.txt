# Yet Another Zelda3 Editor
# by scawful
cmake_minimum_required(VERSION 3.10)
project(yaze VERSION 0.2.2
             DESCRIPTION "Yet Another Zelda3 Editor"
             LANGUAGES CXX)
configure_file(src/yaze_config.h.in yaze_config.h)

# Build Flags
set(YAZE_BUILD_APP ON)
set(YAZE_BUILD_LIB ON)
set(YAZE_BUILD_EMU ON)
set(YAZE_BUILD_Z3ED ON)
set(YAZE_BUILD_PYTHON OFF)
set(YAZE_BUILD_TESTS ON)
set(YAZE_INSTALL_LIB OFF)

# libpng features in bitmap.cc
add_definitions("-DYAZE_LIB_PNG=1")

# C++ Standard and CMake Specifications
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_FIND_FRAMEWORK LAST)
set(CMAKE_SHARED_MODULE_PREFIX "")

if (UNIX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dlinux -Dstricmp=strcasecmp")
endif()

if (MACOS)
  set(CMAKE_INSTALL_PREFIX /usr/local)
endif()

if (WIN32)
  include(cmake/vcpkg.cmake)
endif()

# Abseil Standard Specifications
include(cmake/absl.cmake)

# SDL2 and PNG
include(cmake/sdl2.cmake)

# Asar
include(cmake/asar.cmake)

# ImGui
include(cmake/imgui.cmake)

# Project Files
add_subdirectory(src)

# Tests
if (YAZE_BUILD_TESTS)
include(cmake/gtest.cmake)
add_subdirectory(test)
endif()

