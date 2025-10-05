# ==============================================================================
# Yaze Net Library
# ==============================================================================
# This library contains networking and collaboration functionality:
# - ROM version management
# - Proposal approval system
# - Collaboration utilities
#
# Dependencies: yaze_util, absl
# ==============================================================================

set(
  YAZE_NET_SRC
  app/net/rom_version_manager.cc
)

add_library(yaze_net STATIC ${YAZE_NET_SRC})

target_include_directories(yaze_net PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_net PUBLIC
  yaze_util
  yaze_common
  ${ABSL_TARGETS}
)

# Add JSON support if enabled
if(YAZE_WITH_JSON)
  target_include_directories(yaze_net PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_net PUBLIC YAZE_WITH_JSON)
endif()

set_target_properties(yaze_net PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "✓ yaze_net library configured")
