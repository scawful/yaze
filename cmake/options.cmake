# YAZE Build Options
# Centralized feature flags and build configuration

# Core build options
option(YAZE_BUILD_GUI "Build GUI application" ON)
option(YAZE_BUILD_CLI "Build CLI tools (shared libraries)" ON)
option(YAZE_BUILD_Z3ED "Build z3ed CLI executable" ON)
option(YAZE_BUILD_EMU "Build emulator components" ON)
option(YAZE_BUILD_LIB "Build static library" ON)
option(YAZE_BUILD_TESTS "Build test suite" ON)
option(YAZE_BUILD_TOOLS "Build development utility tools" ${YAZE_BUILD_TESTS})
option(YAZE_BUILD_LAB "Build lab sandbox executable" OFF)

# Feature flags
option(YAZE_ENABLE_GRPC "Enable gRPC agent support" ON)
option(YAZE_ENABLE_JSON "Enable JSON support" ON)
option(YAZE_ENABLE_AI "Enable AI agent features" OFF)
option(YAZE_ENABLE_ASAR "Enable ASAR 65816 assembler integration" ON)
option(YAZE_ENABLE_CLANG_TIDY "Enable clang-tidy linting during build" ON)
option(YAZE_ENABLE_OPENCV "Enable OpenCV for advanced visual analysis" OFF)

# Advanced feature toggles
option(YAZE_ENABLE_REMOTE_AUTOMATION
  "Enable remote automation services (gRPC/protobuf servers + GUI automation clients)"
  ${YAZE_ENABLE_GRPC})
option(YAZE_ENABLE_AI_RUNTIME
  "Enable AI runtime integrations (Gemini/Ollama, advanced routing, proposal planning)"
  ${YAZE_ENABLE_AI})
option(YAZE_BUILD_AGENT_UI
  "Build ImGui-based agent/chat panels inside the GUI"
  ${YAZE_BUILD_GUI})
option(YAZE_ENABLE_AGENT_CLI
  "Build the conversational agent CLI stack (z3ed agent commands)"
  ${YAZE_BUILD_CLI})
option(YAZE_ENABLE_HTTP_API
  "Enable HTTP REST API server for external agent access"
  ${YAZE_ENABLE_AGENT_CLI})

if((YAZE_BUILD_CLI OR YAZE_BUILD_Z3ED) AND NOT YAZE_ENABLE_AGENT_CLI)
  set(YAZE_ENABLE_AGENT_CLI ON CACHE BOOL "Build the conversational agent CLI stack (z3ed agent commands)" FORCE)
endif()

if(YAZE_ENABLE_HTTP_API AND NOT YAZE_ENABLE_AGENT_CLI)
  set(YAZE_ENABLE_AGENT_CLI ON CACHE BOOL "Build the conversational agent CLI stack (z3ed agent commands)" FORCE)
endif()

# Build optimizations
option(YAZE_ENABLE_LTO "Enable link-time optimization" OFF)
option(YAZE_ENABLE_SANITIZERS "Enable AddressSanitizer/UBSanitizer" OFF)
option(YAZE_ENABLE_COVERAGE "Enable code coverage" OFF)
option(YAZE_UNITY_BUILD "Enable Unity (Jumbo) builds" OFF)

# Platform-specific options
option(YAZE_USE_VCPKG "Use vcpkg for Windows dependencies" OFF)
option(YAZE_USE_SYSTEM_DEPS "Use system package manager for dependencies" OFF)
option(YAZE_USE_SDL3 "Use SDL3 instead of SDL2 (experimental)" OFF)
option(YAZE_WASM_TERMINAL "Build z3ed for WASM terminal mode (no TUI)" OFF)

# Development options
option(YAZE_ENABLE_ROM_TESTS "Enable tests that require ROM files" OFF)
option(YAZE_ENABLE_BENCHMARK_TESTS "Enable benchmark/performance tests" OFF)
option(YAZE_MINIMAL_BUILD "Minimal build for CI (disable optional features)" OFF)
option(YAZE_VERBOSE_BUILD "Verbose build output" OFF)

# Install options
option(YAZE_INSTALL_LIB "Install static library" OFF)
option(YAZE_INSTALL_HEADERS "Install public headers" ON)
option(YAZE_INSTALL_TEST_HELPERS "Install development test helper tools" OFF)

# Set preprocessor definitions based on options
if(YAZE_ENABLE_REMOTE_AUTOMATION AND NOT YAZE_ENABLE_GRPC)
  set(YAZE_ENABLE_GRPC ON CACHE BOOL "Enable gRPC agent support" FORCE)
endif()

if(NOT YAZE_ENABLE_REMOTE_AUTOMATION)
  set(YAZE_ENABLE_GRPC OFF CACHE BOOL "Enable gRPC agent support" FORCE)
endif()

if(YAZE_ENABLE_GRPC)
  add_compile_definitions(YAZE_WITH_GRPC)
endif()

if(YAZE_ENABLE_JSON)
  add_compile_definitions(YAZE_WITH_JSON)
endif()

if(YAZE_ENABLE_AI_RUNTIME AND NOT YAZE_ENABLE_AI)
  set(YAZE_ENABLE_AI ON CACHE BOOL "Enable AI agent features" FORCE)
endif()

if(NOT YAZE_ENABLE_AI_RUNTIME)
  set(YAZE_ENABLE_AI OFF CACHE BOOL "Enable AI agent features" FORCE)
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

# Print configuration summary
message(STATUS "=== YAZE Build Configuration ===")
message(STATUS "GUI Application: ${YAZE_BUILD_GUI}")
message(STATUS "CLI Tools: ${YAZE_BUILD_CLI}")
message(STATUS "z3ed CLI: ${YAZE_BUILD_Z3ED}")
message(STATUS "Emulator: ${YAZE_BUILD_EMU}")
message(STATUS "Static Library: ${YAZE_BUILD_LIB}")
message(STATUS "Tests: ${YAZE_BUILD_TESTS}")
message(STATUS "Lab Sandbox: ${YAZE_BUILD_LAB}")
if(YAZE_USE_SDL3)
  message(STATUS "SDL Version: SDL3 (experimental)")
else()
  message(STATUS "SDL Version: SDL2 (stable)")
endif()
message(STATUS "gRPC Support: ${YAZE_ENABLE_GRPC}")
message(STATUS "Remote Automation: ${YAZE_ENABLE_REMOTE_AUTOMATION}")
message(STATUS "JSON Support: ${YAZE_ENABLE_JSON}")
message(STATUS "AI Runtime: ${YAZE_ENABLE_AI_RUNTIME}")
message(STATUS "AI Features (legacy): ${YAZE_ENABLE_AI}")
message(STATUS "Agent UI Panels: ${YAZE_BUILD_AGENT_UI}")
message(STATUS "Agent CLI Stack: ${YAZE_ENABLE_AGENT_CLI}")
message(STATUS "HTTP API Server: ${YAZE_ENABLE_HTTP_API}")
message(STATUS "LTO: ${YAZE_ENABLE_LTO}")
message(STATUS "Sanitizers: ${YAZE_ENABLE_SANITIZERS}")
message(STATUS "Coverage: ${YAZE_ENABLE_COVERAGE}")
message(STATUS "OpenCV Visual Analysis: ${YAZE_ENABLE_OPENCV}")
message(STATUS "ASAR Assembler: ${YAZE_ENABLE_ASAR}")
message(STATUS "=================================")
