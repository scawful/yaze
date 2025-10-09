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

project(yaze VERSION 0.3.2
             DESCRIPTION "Yet Another Zelda3 Editor"
             LANGUAGES CXX C)

# Set project metadata
set(YAZE_VERSION_MAJOR 0)
set(YAZE_VERSION_MINOR 3)
set(YAZE_VERSION_PATCH 2)

# Add an option to enable Unity builds for faster compilation
option(YAZE_UNITY_BUILD "Enable Unity (Jumbo) builds" OFF)

if(YAZE_UNITY_BUILD)
  message(STATUS "Unity builds enabled")
  set(CMAKE_UNITY_BUILD ON)
  set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)
endif()

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
option(YAZE_USE_MODULAR_BUILD "Use modularized library build system for faster builds" ON)

# ============================================================================
# Feature Flags - Always Enable All Features
# ============================================================================
# CI and Release now use the same configuration for consistency
# This avoids runtime library mismatches and ensures CI tests what we ship
set(Z3ED_AI ON)
set(YAZE_WITH_JSON ON)
set(YAZE_WITH_GRPC ON)
message(STATUS "✓ All features enabled: JSON, gRPC, AI Agent")

# YAZE_SUPPRESS_WARNINGS: Suppress compiler warnings for cleaner build output
option(YAZE_SUPPRESS_WARNINGS "Suppress compiler warnings (use -v preset suffix for verbose)" ON)
set(YAZE_TEST_ROM_PATH "${CMAKE_BINARY_DIR}/bin/zelda3.sfc" CACHE STRING "Path to test ROM file")

# PNG support removed - no longer needed

# Modern CMake standards
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apply warning suppression if requested
if(YAZE_SUPPRESS_WARNINGS)
  if(MSVC)
    add_compile_options(/w)
  else()
    add_compile_options(-w)
  endif()
  message(STATUS "✓ Warnings suppressed (use -v preset suffix for verbose builds)")
else()
  message(STATUS "○ Verbose warnings enabled")
endif()
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
    # Check if CMAKE_TOOLCHAIN_FILE is set but the file doesn't exist
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        message(WARNING "vcpkg toolchain file specified but not found: ${CMAKE_TOOLCHAIN_FILE}")
        message(WARNING "Disabling vcpkg integration. Install vcpkg or set VCPKG_ROOT environment variable.")
        unset(CMAKE_TOOLCHAIN_FILE CACHE)
    endif()
    
    # Set vcpkg toolchain file if not already set or if the previous one was invalid
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        if(DEFINED ENV{VCPKG_ROOT})
            set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
                CACHE STRING "Vcpkg toolchain file")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
            set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
                CACHE STRING "Vcpkg toolchain file")
        endif()
    endif()
    # Setup yaze_config include directories

endif()

# Abseil provider selection: default to bundled libraries on macOS to avoid
# deployment target mismatches with system packages, but let other platforms
# use their package managers unless overridden.
# CRITICAL: When gRPC is enabled, always use bundled Abseil to avoid version conflicts
set(_yaze_default_force_absl OFF)
if(YAZE_PLATFORM_MACOS OR YAZE_WITH_GRPC)
    set(_yaze_default_force_absl ON)
endif()
option(YAZE_FORCE_BUNDLED_ABSL
             "Force building the bundled Abseil FetchContent dependency instead of finding a system package"
             ${_yaze_default_force_absl})

# Create a common interface target for shared settings
add_library(yaze_common INTERFACE)
target_compile_features(yaze_common INTERFACE cxx_std_23)

# Configure httplib before including it (header-only, minimal dependencies)
# Disable optional compression features to avoid Windows build issues
if (WIN32)
set(HTTPLIB_COMPILE OFF CACHE BOOL "Use httplib as header-only library" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "Disable zlib to avoid Windows issues" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "Disable brotli" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "Disable zstd" FORCE)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "Don't require OpenSSL" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "Don't require zlib" FORCE)
set(HTTPLIB_REQUIRE_BROTLI OFF CACHE BOOL "Don't require brotli" FORCE)
set(HTTPLIB_REQUIRE_ZSTD OFF CACHE BOOL "Don't require zstd" FORCE)
endif()

target_include_directories(yaze_common INTERFACE ${CMAKE_SOURCE_DIR}/third_party/httplib)

if(YAZE_WITH_JSON)
    set(JSON_BuildTests OFF CACHE INTERNAL "Disable nlohmann_json tests")
    
    # Verify JSON library exists
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json/CMakeLists.txt")
        message(FATAL_ERROR 
            "JSON library not found in third_party/json/\n"
            "This is required when YAZE_WITH_JSON=ON.\n"
            "Please ensure the repository is fully cloned with submodules."
        )
    endif()
    
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/json ${CMAKE_BINARY_DIR}/third_party/json EXCLUDE_FROM_ALL)
    
    # Verify the target was created
    if(NOT TARGET nlohmann_json::nlohmann_json)
        message(FATAL_ERROR 
            "Failed to create nlohmann_json::nlohmann_json target.\n"
            "The JSON library subdirectory was added but the target is missing.\n"
            "Please check third_party/json/CMakeLists.txt for errors."
        )
    endif()
    
    message(STATUS "✓ JSON support enabled (nlohmann/json)")
    message(STATUS "  - Include directory: ${CMAKE_SOURCE_DIR}/third_party/json/include")
    message(STATUS "  - Target available: nlohmann_json::nlohmann_json")
endif()

# Platform-specific configurations
if(YAZE_PLATFORM_LINUX)
    target_compile_definitions(yaze_common INTERFACE linux stricmp=strcasecmp)
elseif(YAZE_PLATFORM_MACOS)
    set(CMAKE_INSTALL_PREFIX /usr/local)
    target_compile_definitions(yaze_common INTERFACE MACOS)
elseif(YAZE_PLATFORM_WINDOWS)
    # vcpkg configuration must be done early (in CMakeLists.txt or via command line)
    # The vcpkg.cmake file contains post-toolchain configuration and reporting
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        # vcpkg toolchain is active
        # Note: VCPKG_TARGET_TRIPLET and other settings should be set via command line
        # or will be auto-detected by our cmake/vcpkg.cmake include
        message(STATUS "✓ Using vcpkg integration")
        message(STATUS "  ├─ Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
        if(DEFINED VCPKG_TARGET_TRIPLET)
            message(STATUS "  └─ Triplet: ${VCPKG_TARGET_TRIPLET}")
        endif()
        
        # Include our vcpkg configuration for additional settings
        include(cmake/vcpkg.cmake)
    else()
        message(STATUS "○ vcpkg not available - using system packages or bundled dependencies")
        message(STATUS "  Tip: Set VCPKG_ROOT environment variable or CMAKE_TOOLCHAIN_FILE to enable vcpkg")
    endif()
    target_compile_definitions(yaze_common INTERFACE WINDOWS)
    
    # Windows-specific build guidance
    message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    message(STATUS "Windows Build Configuration:")
    message(STATUS "  ├─ Generator: ${CMAKE_GENERATOR}")
    message(STATUS "  ├─ Architecture: ${CMAKE_GENERATOR_PLATFORM}")
    message(STATUS "  ├─ Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  ├─ Minimal Build: ${YAZE_MINIMAL_BUILD}")
    message(STATUS "  ├─ JSON Support: ${YAZE_WITH_JSON}")
    message(STATUS "  ├─ AI Features: ${Z3ED_AI}")
    message(STATUS "  └─ gRPC Support: ${YAZE_WITH_GRPC}")
    message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    
    # Helpful messages about build configuration
    if(YAZE_MINIMAL_BUILD)
        message(STATUS "ℹ️  Minimal Build Mode:")
        message(STATUS "   • gRPC disabled for faster CI builds")
        message(STATUS "   • UI tests disabled")
        message(STATUS "   • JSON support enabled (header-only, lightweight)")
    endif()
    
    if(NOT Z3ED_AI AND NOT YAZE_MINIMAL_BUILD)
        message(STATUS "Note: AI agent features disabled")
        message(STATUS "      To enable: cmake -DYAZE_WITH_GRPC=ON -DZ3ED_AI=ON ...")
    endif()
    
    if(NOT YAZE_WITH_JSON)
        message(WARNING "JSON disabled - some features may not compile")
        message(WARNING "Recommend keeping JSON enabled (it's header-only)")
    endif()
    
    # Note about httplib
    message(STATUS "httplib: Header-only mode (compression features disabled)")
    if(Z3ED_AI)
        message(STATUS "         For Gemini API: curl.exe must be in PATH")
        message(STATUS "         (Usually included with Windows 10/11 or Git)")
    endif()
    
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
    )
endif()

# Abseil Standard Specifications
include(cmake/absl.cmake)

# Optional gRPC support
if(YAZE_WITH_GRPC)
  message(STATUS "✓ gRPC support enabled (FetchContent will download and build from source)")
  message(STATUS "  Note: First build takes 15-20 minutes to compile gRPC + Protobuf")
  message(STATUS "  Versions: gRPC v1.62.0, Protobuf (bundled), Abseil (bundled)")
  
  # Include existing gRPC infrastructure
  include(cmake/grpc.cmake)
  
  # Pass to source code
  add_compile_definitions(YAZE_WITH_GRPC)
  
  set(YAZE_HAS_GRPC TRUE)
else()
  message(STATUS "○ gRPC support disabled (set -DYAZE_WITH_GRPC=ON to enable)")
  set(YAZE_HAS_GRPC FALSE)
endif()

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

# Copy agent resource files to build directory (for AI features)
file(GLOB AGENT_FILES "${CMAKE_SOURCE_DIR}/assets/agent/*")
file(COPY ${AGENT_FILES} DESTINATION "${CMAKE_BINARY_DIR}/assets/agent/")

# IMPORTANT: Also ensure themes are included in macOS bundles
# This is handled in src/CMakeLists.txt via YAZE_RESOURCE_FILES

add_subdirectory(src)

# Tools (development utilities - only for local development)
option(YAZE_BUILD_TOOLS "Build development utility tools" OFF)
if(YAZE_BUILD_TOOLS)
  message(STATUS "Building development tools")
  add_subdirectory(tools)
endif()

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

add_custom_target(build_cleaner
  COMMAND ${CMAKE_COMMAND} -E echo "Running scripts/build_cleaner.py --dry-run"
  COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} ${Python3_EXECUTABLE} scripts/build_cleaner.py --dry-run
  COMMENT "Validate CMake source lists and includes"
)

