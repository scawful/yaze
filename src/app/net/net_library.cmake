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

if(YAZE_WITH_GRPC)
  # Add ROM service implementation (disabled - proto field mismatch)
  # list(APPEND YAZE_NET_SRC app/net/rom_service_impl.cc)
endif()

add_library(yaze_net STATIC ${YAZE_NET_SRC})

target_precompile_headers(yaze_net PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

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
  
  # Only link OpenSSL if gRPC is NOT enabled (to avoid duplicate symbol errors)
  # When gRPC is enabled, it brings its own OpenSSL which we'll use instead
  if(NOT YAZE_WITH_GRPC)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
      target_link_libraries(yaze_net PUBLIC OpenSSL::SSL OpenSSL::Crypto)
      target_compile_definitions(yaze_net PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
      message(STATUS "  - WebSocket with SSL/TLS support enabled")
    else()
      message(STATUS "  - WebSocket without SSL/TLS (OpenSSL not found)")
    endif()
  else()
    # When gRPC is enabled, still enable OpenSSL features but use gRPC's OpenSSL
    target_compile_definitions(yaze_net PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
    message(STATUS "  - WebSocket with SSL/TLS support enabled via gRPC's OpenSSL")
  endif()
  
  # Windows-specific socket library
  if(WIN32)
    target_link_libraries(yaze_net PUBLIC ws2_32)
    message(STATUS "  - Windows socket support (ws2_32) linked")
  endif()
endif()

# Add gRPC support for ROM service
if(YAZE_WITH_GRPC)
  target_add_protobuf(yaze_net ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto)
  
  target_link_libraries(yaze_net PUBLIC
    grpc++
    grpc++_reflection
  )
  # NOTE: Do NOT link protobuf at library level on Windows - causes LNK1241
  # Executables will link it with /WHOLEARCHIVE to include internal symbols
  if(NOT WIN32 AND YAZE_PROTOBUF_TARGETS)
    target_link_libraries(yaze_net PUBLIC ${YAZE_PROTOBUF_TARGETS})
  endif()
  
  message(STATUS "  - gRPC ROM service enabled")
endif()

set_target_properties(yaze_net PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "✓ yaze_net library configured")
