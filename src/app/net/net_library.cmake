# ==============================================================================
# Yaze Net Library
# ==============================================================================
# This library contains networking and collaboration functionality:
# - Network abstraction layer (HTTP/WebSocket interfaces)
# - Platform-specific implementations (native/WASM)
# - ROM version management
# - Proposal approval system
# - Collaboration utilities
#
# Dependencies: yaze_util, absl
# ==============================================================================

# Base network sources (always included)
set(
  YAZE_NET_BASE_SRC
  app/net/rom_version_manager.cc
  app/net/websocket_client.cc
  app/net/collaboration_service.cc
  app/net/network_factory.cc
)

# Platform-specific network implementation
if(EMSCRIPTEN)
  # WASM/Emscripten implementation
  set(
    YAZE_NET_PLATFORM_SRC
    app/net/wasm/emscripten_http_client.cc
    app/net/wasm/emscripten_websocket.cc
  )
  message(STATUS "  - Using Emscripten network implementation (Fetch API & WebSocket)")
else()
  # Native implementation
  set(
    YAZE_NET_PLATFORM_SRC
    app/net/native/httplib_client.cc
    app/net/native/httplib_websocket.cc
  )
  message(STATUS "  - Using native network implementation (cpp-httplib)")
endif()

# Combine all sources
set(YAZE_NET_SRC ${YAZE_NET_BASE_SRC} ${YAZE_NET_PLATFORM_SRC})

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
  ${CMAKE_SOURCE_DIR}/ext
  ${CMAKE_SOURCE_DIR}/ext/imgui
  ${CMAKE_SOURCE_DIR}/ext/json/include
  ${CMAKE_SOURCE_DIR}/ext/httplib
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(yaze_net PUBLIC
  yaze_util
  yaze_common
  ${ABSL_TARGETS}
  ${YAZE_SDL2_TARGETS}
)

# Add Emscripten-specific flags for WASM builds
if(EMSCRIPTEN)
  # Enable Fetch API for HTTP requests
  target_compile_options(yaze_net PUBLIC "-sFETCH=1")
  target_link_options(yaze_net PUBLIC "-sFETCH=1")

  # WebSocket support requires linking websocket.js library
  # The <emscripten/websocket.h> header provides the API, but the
  # implementation is in the websocket.js library
  target_link_libraries(yaze_net PUBLIC websocket.js)

  message(STATUS "  - Emscripten Fetch API and WebSocket enabled")
endif()

# Add JSON and httplib support if enabled
if(YAZE_WITH_JSON)
  # Link nlohmann_json which provides the include directories automatically
  target_link_libraries(yaze_net PUBLIC nlohmann_json::nlohmann_json)
  target_include_directories(yaze_net PUBLIC ${CMAKE_SOURCE_DIR}/ext/httplib)
  target_compile_definitions(yaze_net PUBLIC YAZE_WITH_JSON)
  
  # Add threading support (cross-platform)
  find_package(Threads REQUIRED)
  target_link_libraries(yaze_net PUBLIC Threads::Threads)
  
  # Only link OpenSSL if gRPC is NOT enabled (to avoid duplicate symbol errors)
  # When gRPC is enabled, it brings its own OpenSSL which we'll use instead
  if(NOT YAZE_WITH_GRPC)
    # CRITICAL FIX: Disable OpenSSL on Windows to avoid missing header errors
    # Windows CI doesn't have OpenSSL headers properly configured
    # WebSocket will work with plain HTTP (no SSL/TLS) on Windows
    if(NOT WIN32)
      find_package(OpenSSL QUIET)
      if(OpenSSL_FOUND)
        target_link_libraries(yaze_net PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(yaze_net PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
        message(STATUS "  - WebSocket with SSL/TLS support enabled")
      else()
        message(STATUS "  - WebSocket without SSL/TLS (OpenSSL not found)")
      endif()
    else()
      message(STATUS "  - Windows: WebSocket using plain HTTP (no SSL) - OpenSSL headers not available in CI")
    endif()
  else()
    # When gRPC is enabled, still enable OpenSSL features but use gRPC's OpenSSL
    # CRITICAL: Skip on Windows - gRPC's OpenSSL headers aren't accessible in Windows CI
    if(NOT WIN32)
      target_compile_definitions(yaze_net PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
      message(STATUS "  - WebSocket with SSL/TLS support enabled via gRPC's OpenSSL")
    else()
      message(STATUS "  - Windows + gRPC: WebSocket using plain HTTP (no SSL) - OpenSSL headers not available")
    endif()
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
