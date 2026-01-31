# Yet Another Zelda3 Editor
# by scawful
cmake_minimum_required(VERSION 3.16)

# Set minimum policy version for subdirectories to allow older dependencies like yaml-cpp
# This allows cmake_minimum_required in subdirectories to use versions < 3.5
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for subdirectories")

# Set policies for compatibility
cmake_policy(SET CMP0091 NEW)
# CMP0091 allows CMAKE_MSVC_RUNTIME_LIBRARY to be set by presets
# Windows presets specify dynamic CRT (/MD) to avoid linking issues
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)

# Enable Objective-C only on macOS where it's actually used
if(DEFINED ENV{YAZE_VERSION_OVERRIDE})
    set(YAZE_VERSION $ENV{YAZE_VERSION_OVERRIDE})
elseif(DEFINED YAZE_VERSION_OVERRIDE)
     set(YAZE_VERSION ${YAZE_VERSION_OVERRIDE})
else()
    set(_yaze_version_file "${CMAKE_SOURCE_DIR}/VERSION")
    if(EXISTS "${_yaze_version_file}")
        file(READ "${_yaze_version_file}" YAZE_VERSION)
        string(STRIP "${YAZE_VERSION}" YAZE_VERSION)
    else()
        set(YAZE_VERSION "0.5.5")
    endif()
endif()
if(NOT DEFINED YAZE_VERSION_SUFFIX)
    set(YAZE_VERSION_SUFFIX "")
endif()

if(APPLE)
    project(yaze VERSION ${YAZE_VERSION}
                 DESCRIPTION "Yet Another Zelda3 Editor"
                 LANGUAGES CXX C OBJC OBJCXX)
else()
    project(yaze VERSION ${YAZE_VERSION}
                 DESCRIPTION "Yet Another Zelda3 Editor"
                 LANGUAGES CXX C)
endif()

# Set language standards early so dependencies (e.g., Abseil) build with them.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include build options first
include(cmake/options.cmake)

# Enable sccache/ccache for faster rebuilds if available
find_program(SCCACHE_FOUND sccache)
if(SCCACHE_FOUND)
    message(STATUS "✓ sccache found, enabling for faster builds")
    set(CMAKE_CXX_COMPILER_LAUNCHER sccache)
    set(CMAKE_C_COMPILER_LAUNCHER sccache)
else()
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        message(STATUS "✓ ccache found, enabling for faster builds")
        set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
        set(CMAKE_C_COMPILER_LAUNCHER ccache)
    endif()
endif()

# Version is defined in project() above - use those variables
# CMake automatically sets: yaze_VERSION, yaze_VERSION_MAJOR, yaze_VERSION_MINOR, yaze_VERSION_PATCH
# These YAZE_VERSION_* aliases are for compatibility with existing code
set(YAZE_VERSION_MAJOR ${yaze_VERSION_MAJOR})
set(YAZE_VERSION_MINOR ${yaze_VERSION_MINOR})
set(YAZE_VERSION_PATCH ${yaze_VERSION_PATCH})

# Suppress deprecation warnings from submodules
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress deprecation warnings")

# Add cmake directory to module path
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Platform detection
if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(YAZE_PLATFORM_IOS ON)
    else()
        set(YAZE_PLATFORM_MACOS ON)
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(YAZE_PLATFORM_LINUX ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(YAZE_PLATFORM_WINDOWS ON)
endif()

# macOS: ensure C++ standard library headers are searched before /usr/local/include
# so that <cmath> gets libc++'s <math.h> and not the C library's (avoids isinf/signbit conflicts)
if(YAZE_PLATFORM_MACOS AND NOT CMAKE_TOOLCHAIN_FILE MATCHES "llvm-brew")
    set(_yaze_sdk_path "${CMAKE_OSX_SYSROOT}")
    if(NOT _yaze_sdk_path)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _yaze_sdk_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(_yaze_sdk_path)
        set(_yaze_cxx_headers "${_yaze_sdk_path}/usr/include/c++/v1")
        if(EXISTS "${_yaze_cxx_headers}")
            set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
            include_directories(BEFORE SYSTEM "${_yaze_cxx_headers}")
            message(STATUS "✓ C++ stdlib headers first: ${_yaze_cxx_headers}")
        endif()
    endif()
    unset(_yaze_sdk_path)
    unset(_yaze_cxx_headers)
endif()

# Include utility functions
include(cmake/utils.cmake)

# Set up dependencies using CPM.cmake
include(cmake/dependencies.cmake)

# Additional configuration options
option(YAZE_SUPPRESS_WARNINGS "Suppress compiler warnings (use -v preset suffix for verbose)" ON)
set(YAZE_TEST_ROM_VANILLA_PATH "" CACHE STRING "Path to vanilla test ROM file")
set(YAZE_TEST_ROM_US_PATH "" CACHE STRING "Path to US test ROM file")
set(YAZE_TEST_ROM_JP_PATH "" CACHE STRING "Path to JP test ROM file")
set(YAZE_TEST_ROM_EU_PATH "" CACHE STRING "Path to EU test ROM file")
set(YAZE_TEST_ROM_EXPANDED_PATH "" CACHE STRING "Path to expanded test ROM file")
set(YAZE_TEST_ROM_PATH "" CACHE STRING "Legacy path to test ROM file (vanilla)")

# Export compile commands for clangd/LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Setup compiler flags and common interface target
yaze_add_compiler_flags()

# Configure yaze_config.h
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/yaze_config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h
  @ONLY
)

# Output directories
include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF)

# Handle dependencies

# Project Files
add_subdirectory(src)

# Tools
if(YAZE_BUILD_TOOLS)
  message(STATUS "Building development tools")
  add_subdirectory(tools)
endif()

# Tests
if(YAZE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# Code quality targets
if(YAZE_ENABLE_CLANG_TIDY)
  if(NOT YAZE_CLANG_TIDY_EXE)
    find_program(YAZE_CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16)
  endif()

  if(YAZE_CLANG_TIDY_EXE)
    message(STATUS "✓ clang-tidy enabled: ${YAZE_CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${YAZE_CLANG_TIDY_EXE}")
  else()
    message(WARNING "clang-tidy requested but not found")
  endif()
endif()

find_program(CLANG_FORMAT
  NAMES clang-format-18 clang-format
  HINTS "${HOMEBREW_LLVM_PREFIX}/bin" # Prefer clang-format from Homebrew LLVM
  NO_DEFAULT_PATH
)
if(NOT CLANG_FORMAT) # Fallback to generic search if not found in Homebrew prefix
  find_program(CLANG_FORMAT NAMES clang-format clang-format-17 clang-format-16 clang-format-15 clang-format-14)
endif()
if(CLANG_FORMAT)
  file(GLOB_RECURSE ALL_SOURCE_FILES
       "${CMAKE_SOURCE_DIR}/src/*.cc"
       "${CMAKE_SOURCE_DIR}/src/*.h"
       "${CMAKE_SOURCE_DIR}/test/*.cc"
       "${CMAKE_SOURCE_DIR}/test/*.h")

  # Exclude third-party library directories from formatting
  list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX "src/lib/.*")

  add_custom_target(yaze-format
    COMMAND ${CLANG_FORMAT} -i --style=file ${ALL_SOURCE_FILES}
    COMMENT "Running clang-format on source files"
  )

  add_custom_target(yaze-format-check
    COMMAND ${CLANG_FORMAT} --dry-run --Werror --style=file ${ALL_SOURCE_FILES}
    COMMENT "Checking code format"
  )
endif()

# Packaging configuration
if(NOT YAZE_PLATFORM_IOS)
  include(cmake/packaging/cpack.cmake)
endif()

add_custom_target(build_cleaner
  COMMAND ${CMAKE_COMMAND} -E echo "Running scripts/build_cleaner.py --dry-run"
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} ${Python3_EXECUTABLE} scripts/build_cleaner.py --dry-run
  COMMENT "Validate CMake source lists and includes"
)
