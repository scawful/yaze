# ==============================================================================
# Yaze Utility Library
# ==============================================================================
# This library contains low-level utilities used throughout the codebase:
# - BPS patch handling
# - Command-line flag parsing  
# - Hexadecimal utilities
#
# This library has no dependencies on GUI, graphics, or game-specific code,
# making it the foundation of the dependency hierarchy.
# ==============================================================================

set(YAZE_UTIL_SRC
  util/bps.cc
  util/flag.cc
  util/hex.cc
  util/log.cc
  util/platform_paths.cc
  util/file_util.cc
  util/hyrule_magic.cc  # Byte order utilities (moved from zelda3)
)

add_library(yaze_util STATIC ${YAZE_UTIL_SRC})

# Note: PCH disabled for yaze_util to avoid circular dependency with Abseil
# The log.h header requires Abseil, but Abseil is built after yaze_util
# in the dependency chain. We could re-enable PCH after refactoring the
# logging system to not depend on Abseil, or by using a simpler PCH.

target_include_directories(yaze_util PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/incl
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/ext
  ${PROJECT_BINARY_DIR}
)

# Note: Abseil include paths are provided automatically through target_link_libraries
# No manual include_directories needed - linking to absl::* targets provides the paths

target_link_libraries(yaze_util PUBLIC
  yaze_common
)

# Add Abseil dependencies
# When gRPC is enabled, we link to grpc++ which transitively provides Abseil
# When gRPC is disabled, we use the standalone Abseil from absl.cmake
if(YAZE_ENABLE_GRPC)
  # Link to gRPC which provides bundled Abseil
  target_link_libraries(yaze_util PUBLIC
    grpc++
    absl::status
    absl::statusor
    absl::strings
    absl::str_format
  )
else()
  # Link standalone Abseil targets (configured in cmake/absl.cmake)
  target_link_libraries(yaze_util PUBLIC
    absl::status
    absl::statusor
    absl::strings
    absl::str_format
  )
endif()

# CRITICAL FIX FOR WINDOWS: Explicitly add Abseil include directory
# Issue: Windows builds with Ninja + clang-cl fail to find Abseil headers even when
# linking to absl::* targets. This affects BOTH standalone Abseil AND gRPC-bundled Abseil.
#
# Root cause: In ci-windows preset, YAZE_ENABLE_GRPC=ON but YAZE_ENABLE_REMOTE_AUTOMATION=OFF,
# which causes options.cmake to forcibly disable gRPC. This means standalone Abseil is used,
# but the previous fix only ran when YAZE_ENABLE_GRPC=ON, so it never applied.
#
# Solution: Apply include path fix unconditionally on Windows, with multi-source detection:
# 1. Check YAZE_ABSL_SOURCE_DIR (exported from cmake/absl.cmake for standalone Abseil)
# 2. Check absl_SOURCE_DIR (direct FetchContent variable)
# 3. Check grpc_SOURCE_DIR/third_party/abseil-cpp (gRPC-bundled Abseil)
if(WIN32)
  set(_absl_include_found FALSE)

  # Priority 1: Check YAZE_ABSL_SOURCE_DIR (exported by cmake/absl.cmake)
  if(DEFINED YAZE_ABSL_SOURCE_DIR AND EXISTS "${YAZE_ABSL_SOURCE_DIR}")
    target_include_directories(yaze_util PUBLIC ${YAZE_ABSL_SOURCE_DIR})
    message(STATUS "Windows: Added explicit Abseil include path (standalone): ${YAZE_ABSL_SOURCE_DIR}")
    set(_absl_include_found TRUE)
  # Priority 2: Check absl_SOURCE_DIR (direct FetchContent variable)
  elseif(DEFINED absl_SOURCE_DIR AND EXISTS "${absl_SOURCE_DIR}")
    target_include_directories(yaze_util PUBLIC ${absl_SOURCE_DIR})
    message(STATUS "Windows: Added explicit Abseil include path (FetchContent): ${absl_SOURCE_DIR}")
    set(_absl_include_found TRUE)
  # Priority 3: Check gRPC-bundled Abseil
  elseif(DEFINED grpc_SOURCE_DIR AND EXISTS "${grpc_SOURCE_DIR}/third_party/abseil-cpp")
    target_include_directories(yaze_util PUBLIC "${grpc_SOURCE_DIR}/third_party/abseil-cpp")
    message(STATUS "Windows: Added explicit Abseil include path (gRPC-bundled): ${grpc_SOURCE_DIR}/third_party/abseil-cpp")
    set(_absl_include_found TRUE)
  endif()

  if(NOT _absl_include_found)
    message(WARNING "Windows: Could not find Abseil source directory - build may fail with 'absl/status/statusor.h' not found")
  endif()
endif()

set_target_properties(yaze_util PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Ensure C++ standard is set (should be inherited from parent, but be explicit)
set_target_properties(yaze_util PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

# Platform-specific compile definitions and libraries
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_util PRIVATE linux stricmp=strcasecmp)
  # Link filesystem library for older GCC versions (< 9.0)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(yaze_util PUBLIC stdc++fs)
  endif()
elseif(APPLE)
  target_compile_definitions(yaze_util PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_util PRIVATE WINDOWS)

  # CRITICAL FIX: Windows builds need /std:c++latest for std::filesystem support
  # clang-cl requires this flag to access std::filesystem from MSVC STL
  # (without it, only std::experimental::filesystem is available)
  target_compile_options(yaze_util PUBLIC /std:c++latest /EHsc)
  message(STATUS "Applied /std:c++latest to yaze_util for std::filesystem support on Windows")
endif()

message(STATUS "✓ yaze_util library configured")
