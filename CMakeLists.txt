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
# AI Agent Build Flags (Consolidated)
# ============================================================================
# Z3ED_AI: Master flag for z3ed AI agent features (Ollama + Gemini support)
# Enables: JSON parsing, YAML config, httplib for API calls, prompt builder
option(Z3ED_AI "Enable z3ed AI agent features (Gemini/Ollama integration)" OFF)

# YAZE_WITH_JSON: JSON support (bundled header-only library, minimal overhead)
# Enabled by default to avoid Windows build issues with agent history codec
option(YAZE_WITH_JSON "Enable JSON support for AI integrations" ON)

# YAZE_WITH_GRPC: gRPC for GUI automation (auto-enables JSON)
option(YAZE_WITH_GRPC "Enable gRPC-based ImGuiTestHarness for automated GUI testing (experimental)" OFF)

# Dependency resolution
if(Z3ED_AI)
    message(STATUS "Z3ED_AI enabled: Activating AI agent dependencies (JSON, YAML, httplib)")
    set(YAZE_WITH_JSON ON CACHE BOOL "Enable JSON support" FORCE)
endif()

if(YAZE_WITH_GRPC AND NOT YAZE_WITH_JSON)
    message(STATUS "Enabling JSON support because gRPC is enabled")
    set(YAZE_WITH_JSON ON CACHE BOOL "Enable JSON support" FORCE)
endif()

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
set(_yaze_default_force_absl OFF)
if(YAZE_PLATFORM_MACOS)
    set(_yaze_default_force_absl ON)
endif()
option(YAZE_FORCE_BUNDLED_ABSL
             "Force building the bundled Abseil FetchContent dependency instead of finding a system package"
             ${_yaze_default_force_absl})

# Create a common interface target for shared settings
add_library(yaze_common INTERFACE)
target_compile_features(yaze_common INTERFACE cxx_std_23)
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
    # Only include vcpkg configuration if vcpkg toolchain is available
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        include(cmake/vcpkg.cmake)
        message(STATUS "Using vcpkg integration")
        message(STATUS "  - Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(STATUS "vcpkg not available - using system packages or bundled dependencies")
        message(STATUS "  Tip: Set VCPKG_ROOT environment variable to enable vcpkg")
    endif()
    target_compile_definitions(yaze_common INTERFACE WINDOWS)
    
    # Windows-specific build guidance
    message(STATUS "===========================================")
    message(STATUS "Windows Build Configuration:")
    message(STATUS "  - Generator: ${CMAKE_GENERATOR}")
    message(STATUS "  - Architecture: ${CMAKE_GENERATOR_PLATFORM}")
    message(STATUS "  - JSON Support: ${YAZE_WITH_JSON}")
    message(STATUS "  - AI Features: ${Z3ED_AI}")
    message(STATUS "  - gRPC Support: ${YAZE_WITH_GRPC}")
    message(STATUS "===========================================")
    
    # Helpful messages about build configuration
    if(NOT Z3ED_AI)
        message(STATUS "Note: AI agent features disabled (JSON is enabled)")
        message(STATUS "      To enable AI: cmake --preset windows-ai-debug")
    endif()
    
    if(NOT YAZE_WITH_JSON)
        message(STATUS "Warning: JSON disabled - some features may not compile")
        message(STATUS "         Recommend keeping JSON enabled (it's header-only)")
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

# Tools
add_subdirectory(tools)

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

