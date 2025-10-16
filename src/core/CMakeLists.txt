# ==============================================================================
# YAZE Core Library
# ==============================================================================
# Foundation library for project management and ROM patching logic.
# This library has minimal dependencies and provides application-agnostic
# functionality for managing YAZE projects and applying assembly patches.
#
# Dependencies: yaze_util, yaze_zelda3, asar, absl
# ==============================================================================

# Core library sources
set(YAZE_CORE_LIB_SOURCES
  asar_wrapper.cc
  project.cc
)

# Create the static library
add_library(yaze_core_lib STATIC ${YAZE_CORE_LIB_SOURCES})

# Set C++ standard
target_compile_features(yaze_core_lib PUBLIC cxx_std_20)

# Include directories
target_include_directories(yaze_core_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app  # Needed for app/gui/core/icons.h temporarily
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
  ${CMAKE_SOURCE_DIR}/src/lib/imgui  # Needed for ImGui in project.cc temporarily
  ${CMAKE_SOURCE_DIR}/incl
  ${PROJECT_BINARY_DIR}
)

# Link dependencies - minimal set
target_link_libraries(yaze_core_lib PUBLIC
  yaze_util          # Logging, file I/O, platform paths
  yaze_zelda3        # Zelda3 labels for embedded project labels
  asar-static        # Assembly patching
  ${ABSL_TARGETS}    # Abseil for Status/StatusOr
  ${CMAKE_DL_LIBS}   # Dynamic library loading
)

# Precompiled headers for faster builds
target_precompile_headers(yaze_core_lib PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

# Output directory
set_target_properties(yaze_core_lib PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_core_lib PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_core_lib PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_core_lib PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_core_lib configured (foundational project management)")

