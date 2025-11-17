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

target_link_libraries(yaze_util PUBLIC
  yaze_common
)

# Add Abseil dependencies if gRPC is enabled
# We link to grpc++ which transitively provides Abseil and ensures correct build order
if(YAZE_ENABLE_GRPC)
  target_link_libraries(yaze_util PUBLIC
    grpc++
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

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_util PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_util PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_util PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_util library configured")
