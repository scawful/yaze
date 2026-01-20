# YAZE Build Options
# Centralized feature flags and build configuration
#
# Usage:
# - Set YAZE_BUILD_PROFILE to MINIMAL, EDITOR (default), or FULL for preset defaults
# - Override individual options as needed
# - Invalid combinations will produce clear errors (not silent mutations)

# Include profile-based defaults first
include(${CMAKE_CURRENT_LIST_DIR}/options-simple.cmake)

#===============================================================================
# Set preprocessor definitions based on options
#===============================================================================
if(YAZE_ENABLE_GRPC)
  add_compile_definitions(YAZE_WITH_GRPC)
endif()

if(YAZE_ENABLE_JSON)
  add_compile_definitions(YAZE_WITH_JSON)
endif()

if(YAZE_ENABLE_AI)
  add_compile_definitions(Z3ED_AI)
endif()

if(YAZE_ENABLE_AI_RUNTIME)
  add_compile_definitions(YAZE_AI_RUNTIME_AVAILABLE)
endif()

if(YAZE_ENABLE_HTTP_API)
  add_compile_definitions(YAZE_HTTP_API_ENABLED)
endif()

if(YAZE_WASM_TERMINAL)
  add_compile_definitions(YAZE_WASM_TERMINAL_MODE)
endif()

if(YAZE_USE_SDL3)
  add_compile_definitions(YAZE_USE_SDL3)
endif()

if(YAZE_BUILD_AGENT_UI)
  add_compile_definitions(YAZE_BUILD_AGENT_UI)
endif()

if(YAZE_ENABLE_OPENCV)
  find_package(OpenCV QUIET)
  if(OpenCV_FOUND)
    add_compile_definitions(YAZE_WITH_OPENCV)
    message(STATUS "✓ OpenCV found: ${OpenCV_VERSION}")
  else()
    message(WARNING "OpenCV requested but not found - visual analysis will use fallback")
    set(YAZE_ENABLE_OPENCV OFF CACHE BOOL "Enable OpenCV for advanced visual analysis" FORCE)
  endif()
endif()

#===============================================================================
# Print configuration summary
#===============================================================================
message(STATUS "")
message(STATUS "=== YAZE Build Configuration ===")
message(STATUS "Profile: ${YAZE_BUILD_PROFILE}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  GUI Application: ${YAZE_BUILD_GUI}")
message(STATUS "  CLI Tools: ${YAZE_BUILD_CLI}")
message(STATUS "  z3ed CLI: ${YAZE_BUILD_Z3ED}")
message(STATUS "  Emulator: ${YAZE_BUILD_EMU}")
message(STATUS "  Static Library: ${YAZE_BUILD_LIB}")
message(STATUS "  Tests: ${YAZE_BUILD_TESTS}")
message(STATUS "  Lab Sandbox: ${YAZE_BUILD_LAB}")
message(STATUS "")
message(STATUS "Features:")
if(YAZE_USE_SDL3)
  message(STATUS "  SDL Version: SDL3 (experimental)")
else()
  message(STATUS "  SDL Version: SDL2 (stable)")
endif()
message(STATUS "  gRPC Support: ${YAZE_ENABLE_GRPC}")
message(STATUS "  Remote Automation: ${YAZE_ENABLE_REMOTE_AUTOMATION}")
message(STATUS "  JSON Support: ${YAZE_ENABLE_JSON}")
message(STATUS "  AI Features: ${YAZE_ENABLE_AI}")
message(STATUS "  AI Runtime: ${YAZE_ENABLE_AI_RUNTIME}")
message(STATUS "  Agent UI Panels: ${YAZE_BUILD_AGENT_UI}")
message(STATUS "  Agent CLI Stack: ${YAZE_ENABLE_AGENT_CLI}")
message(STATUS "  HTTP API Server: ${YAZE_ENABLE_HTTP_API}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  LTO: ${YAZE_ENABLE_LTO}")
message(STATUS "  Sanitizers: ${YAZE_ENABLE_SANITIZERS}")
message(STATUS "  Coverage: ${YAZE_ENABLE_COVERAGE}")
message(STATUS "  OpenCV: ${YAZE_ENABLE_OPENCV}")
message(STATUS "  ASAR Assembler: ${YAZE_ENABLE_ASAR}")
message(STATUS "=================================")
message(STATUS "")
