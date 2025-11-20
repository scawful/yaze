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

  # CRITICAL FIX FOR WINDOWS: Explicitly add Abseil include directory
  # When gRPC bundles Abseil, the include paths don't always propagate correctly
  # on Windows with Ninja + clang-cl. We need to explicitly add the Abseil source
  # directory where headers are located.
  if(WIN32)
    # Get Abseil source directory from CPM or gRPC fetch
    if(DEFINED absl_SOURCE_DIR)
      target_include_directories(yaze_util PUBLIC ${absl_SOURCE_DIR})
      message(STATUS "Windows: Added explicit Abseil include path: ${absl_SOURCE_DIR}")
    elseif(DEFINED grpc_SOURCE_DIR AND EXISTS "${grpc_SOURCE_DIR}/third_party/abseil-cpp")
      target_include_directories(yaze_util PUBLIC "${grpc_SOURCE_DIR}/third_party/abseil-cpp")
      message(STATUS "Windows: Added explicit Abseil include path from gRPC: ${grpc_SOURCE_DIR}/third_party/abseil-cpp")
    else()
      message(WARNING "Windows: Could not find Abseil source directory - build may fail")
    endif()
  endif()
else()
  # Link standalone Abseil targets (configured in cmake/absl.cmake)
  target_link_libraries(yaze_util PUBLIC
    absl::status
    absl::statusor
    absl::strings
    absl::str_format
  )
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

  # CRITICAL FIX: clang-cl on Windows needs explicit /std:c++latest for std::filesystem
  # clang-cl uses Clang's compiler but must interface with MSVC STL. Without this flag,
  # it only finds std::experimental::filesystem (pre-C++17 version).
  #
  # The CMAKE_CXX_COMPILER_FRONTEND_VARIANT check distinguishes:
  # - "MSVC": clang-cl (Clang with MSVC-compatible command line)
  # - "GNU": regular Clang on Windows
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # Use MSVC-style flag for clang-cl
    target_compile_options(yaze_util PUBLIC /std:c++latest)
    message(STATUS "Windows clang-cl detected: Added /std:c++latest for std::filesystem support")
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Regular Clang on Windows - already has proper flags from parent
    message(STATUS "Windows Clang (non-cl) detected: Using inherited C++23 flags")
  endif()
endif()

message(STATUS "✓ yaze_util library configured")
