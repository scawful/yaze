# Yet Another Zelda3 Editor
# by scawful
cmake_minimum_required(VERSION 3.16)

# Set policy version to handle compatibility issues
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# Set additional policies to handle submodule compatibility
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# Suppress deprecation warnings from submodules
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress deprecation warnings")

# Handle pthread issues on Windows
if(WIN32)
    set(THREADS_PREFER_PTHREAD_FLAG OFF)
endif()

project(yaze VERSION 0.3.1
             DESCRIPTION "Yet Another Zelda3 Editor"
             LANGUAGES CXX C)

# Set project metadata
set(YAZE_VERSION_MAJOR 0)
set(YAZE_VERSION_MINOR 3)
set(YAZE_VERSION_PATCH 1)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/yaze_config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h
  @ONLY
)

# Build Flags
set(YAZE_BUILD_APP ON)
set(YAZE_BUILD_LIB ON)
set(YAZE_BUILD_EMU ON)
set(YAZE_BUILD_Z3ED ON)
set(YAZE_BUILD_TESTS ON CACHE BOOL "Build test suite")
set(YAZE_INSTALL_LIB OFF)

# Testing and CI Configuration
option(YAZE_ENABLE_ROM_TESTS "Enable tests that require ROM files" OFF)
option(YAZE_ENABLE_EXPERIMENTAL_TESTS "Enable experimental/unstable tests" ON)
option(YAZE_ENABLE_UI_TESTS "Enable ImGui Test Engine UI testing" ON)
option(YAZE_MINIMAL_BUILD "Minimal build for CI (disable optional features)" OFF)

# Configure minimal builds for CI/CD
if(YAZE_MINIMAL_BUILD)
  set(YAZE_ENABLE_UI_TESTS OFF CACHE BOOL "Disabled for minimal build" FORCE)
  set(YAZE_BUILD_Z3ED OFF CACHE BOOL "Disabled for minimal build" FORCE)
  # Keep EMU and LIB enabled for comprehensive testing
  set(YAZE_BUILD_EMU ON CACHE BOOL "Required for test suite" FORCE)
  set(YAZE_BUILD_LIB ON CACHE BOOL "Required for test suite" FORCE)
  set(YAZE_INSTALL_LIB OFF CACHE BOOL "Disabled for minimal build" FORCE)
endif()
set(YAZE_TEST_ROM_PATH "${CMAKE_BINARY_DIR}/bin/zelda3.sfc" CACHE STRING "Path to test ROM file")

# PNG support removed - no longer needed

# Modern CMake standards
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_FIND_FRAMEWORK LAST)

# Platform detection
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(YAZE_PLATFORM_MACOS ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(YAZE_PLATFORM_LINUX ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(YAZE_PLATFORM_WINDOWS ON)
    # Enable vcpkg integration for Windows builds
    if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
    # Setup yaze_config include directories

endif()

# Create a common interface target for shared settings
add_library(yaze_common INTERFACE)
target_compile_features(yaze_common INTERFACE cxx_std_23)

# Platform-specific configurations
if(YAZE_PLATFORM_LINUX)
    target_compile_definitions(yaze_common INTERFACE linux stricmp=strcasecmp)
elseif(YAZE_PLATFORM_MACOS)
    set(CMAKE_INSTALL_PREFIX /usr/local)
    target_compile_definitions(yaze_common INTERFACE MACOS)
elseif(YAZE_PLATFORM_WINDOWS)
    include(cmake/vcpkg.cmake)
    target_compile_definitions(yaze_common INTERFACE WINDOWS)
    
    # Windows-specific architecture detection and configuration
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        target_compile_definitions(yaze_common INTERFACE YAZE_ARCH_ARM64)
        message(STATUS "Building for Windows ARM64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        target_compile_definitions(yaze_common INTERFACE YAZE_ARCH_X64)
        message(STATUS "Building for Windows x64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
        target_compile_definitions(yaze_common INTERFACE YAZE_ARCH_X86)
        message(STATUS "Building for Windows x86")
    else()
        message(WARNING "Unknown Windows architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

# Compiler-specific settings
if(MSVC)
    target_compile_options(yaze_common INTERFACE 
        /W4 /permissive-
        /bigobj  # Support large object files
        /utf-8   # Use UTF-8 encoding
    )
    target_compile_definitions(yaze_common INTERFACE 
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        SILENCE_CXX23_DEPRECATIONS
        _SILENCE_CXX23_DEPRECATION_WARNING
        _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
        NOMINMAX  # Disable min/max macros
        WIN32_LEAN_AND_MEAN  # Reduce Windows header bloat
        strncasecmp=_strnicmp
        strcasecmp=_stricmp
    )
else()
    target_compile_options(yaze_common INTERFACE 
        -Wall -Wextra -Wpedantic
        -Wno-deprecated-declarations  # Silence deprecation warnings
        -Wno-c++23-compat             # Silence C++23 compatibility warnings
    )
    # Add C++23 deprecation silencing for GCC/Clang
    target_compile_definitions(yaze_common INTERFACE 
        _SILENCE_CXX23_DEPRECATION_WARNING
        _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
        ABSL_HAVE_INTRINSIC_INT128=1  # Enable intrinsic int128 support
    )
endif()

# Abseil Standard Specifications
include(cmake/absl.cmake)

# SDL2 and PNG
include(cmake/sdl2.cmake)

# Asar
include(cmake/asar.cmake)

# Google Test (if needed for main app integration)
if (YAZE_BUILD_TESTS)
include(cmake/gtest.cmake)
endif()

# ImGui (after minimal build flags are set)
include(cmake/imgui.cmake)

# Project Files  
# Copy theme files to build directory (for development)
file(GLOB THEME_FILES "${CMAKE_SOURCE_DIR}/assets/themes/*.theme")
file(COPY ${THEME_FILES} DESTINATION "${CMAKE_BINARY_DIR}/assets/themes/")

# IMPORTANT: Also ensure themes are included in macOS bundles
# This is handled in src/CMakeLists.txt via YAZE_RESOURCE_FILES

add_subdirectory(src)

# Tests
if (YAZE_BUILD_TESTS)
add_subdirectory(test)
endif()

# Code quality targets
find_program(CLANG_FORMAT NAMES clang-format clang-format-14 clang-format-15 clang-format-16 clang-format-17 clang-format-18)
if(CLANG_FORMAT)
  file(GLOB_RECURSE ALL_SOURCE_FILES 
       "${CMAKE_SOURCE_DIR}/src/*.cc" 
       "${CMAKE_SOURCE_DIR}/src/*.h"
       "${CMAKE_SOURCE_DIR}/test/*.cc"
       "${CMAKE_SOURCE_DIR}/test/*.h")
  
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i --style=Google ${ALL_SOURCE_FILES}
    COMMENT "Running clang-format on source files"
  )
  
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT} --dry-run --Werror --style=Google ${ALL_SOURCE_FILES}
    COMMENT "Checking code format"
  )
endif()

# Packaging configuration
include(cmake/packaging.cmake)

