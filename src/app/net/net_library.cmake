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
  app/net/websocket_client.cc
  app/net/collaboration_service.cc
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

# Add JSON and httplib support if enabled
if(YAZE_WITH_JSON)
  target_include_directories(yaze_net PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/third_party/httplib)
  target_compile_definitions(yaze_net PUBLIC YAZE_WITH_JSON)
  
  # Add threading support (cross-platform)
  find_package(Threads REQUIRED)
  target_link_libraries(yaze_net PUBLIC Threads::Threads)
  
  # Add OpenSSL for HTTPS/WSS support (optional but recommended)
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    target_link_libraries(yaze_net PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(yaze_net PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
    message(STATUS "  - WebSocket with SSL/TLS support enabled")
  else()
    message(STATUS "  - WebSocket without SSL/TLS (OpenSSL not found)")
  endif()
  
  # Windows-specific socket library
  if(WIN32)
    target_link_libraries(yaze_net PUBLIC ws2_32)
    message(STATUS "  - Windows socket support (ws2_32) linked")
  endif()
endif()

set_target_properties(yaze_net PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "✓ yaze_net library configured")
