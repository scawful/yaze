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
  # Windows-specific: Some clang-cl versions need help finding filesystem
  # Ensure we're using the right C++ standard library headers
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Verify clang-cl can find MSVC STL headers with filesystem support
    include(CheckIncludeFileCXX)
    check_include_file_cxx("filesystem" HAVE_FILESYSTEM)
    if(NOT HAVE_FILESYSTEM)
      message(WARNING "std::filesystem not found - this may cause build failures on Windows")
    endif()
  endif()
endif()

message(STATUS "✓ yaze_util library configured")
