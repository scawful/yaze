# YAZE Simplified Build Options with Profiles
# This file is designed to be included from options.cmake to provide
# profile-based defaults while keeping all existing options working.
#
# Design principles:
# 1. Three build profiles: MINIMAL, EDITOR (default), FULL
# 2. Profiles set DEFAULTS only - never use FORCE
# 3. Individual options can always override profile defaults
# 4. Invalid combinations produce errors, not silent mutations

#===============================================================================
# Build Profile (sets defaults only)
#===============================================================================
set(YAZE_BUILD_PROFILE "EDITOR" CACHE STRING
  "Build profile: MINIMAL (libraries only), EDITOR (GUI app), FULL (everything)")
set_property(CACHE YAZE_BUILD_PROFILE PROPERTY STRINGS MINIMAL EDITOR FULL)

#===============================================================================
# Compute profile defaults (not CACHE - just internal defaults)
#===============================================================================
if(YAZE_BUILD_PROFILE STREQUAL "MINIMAL")
  # Libraries only - no GUI, no tests, no AI
  set(_PROFILE_GUI OFF)
  set(_PROFILE_CLI ON)
  set(_PROFILE_Z3ED OFF)
  set(_PROFILE_EMU OFF)
  set(_PROFILE_LIB ON)
  set(_PROFILE_TESTS OFF)
  set(_PROFILE_TOOLS OFF)
  set(_PROFILE_LAB OFF)
  set(_PROFILE_GRPC OFF)
  set(_PROFILE_AI OFF)
  set(_PROFILE_AI_RUNTIME OFF)
  set(_PROFILE_REMOTE_AUTOMATION OFF)
  set(_PROFILE_AGENT_UI OFF)
  set(_PROFILE_AGENT_CLI OFF)
  set(_PROFILE_HTTP_API OFF)
  set(_PROFILE_ROM_TESTS OFF)

elseif(YAZE_BUILD_PROFILE STREQUAL "FULL")
  # Everything enabled
  set(_PROFILE_GUI ON)
  set(_PROFILE_CLI ON)
  set(_PROFILE_Z3ED ON)
  set(_PROFILE_EMU ON)
  set(_PROFILE_LIB ON)
  set(_PROFILE_TESTS ON)
  set(_PROFILE_TOOLS ON)
  set(_PROFILE_LAB OFF)  # Still off by default, opt-in
  set(_PROFILE_GRPC ON)
  set(_PROFILE_AI ON)
  set(_PROFILE_AI_RUNTIME ON)
  set(_PROFILE_REMOTE_AUTOMATION ON)
  set(_PROFILE_AGENT_UI ON)
  set(_PROFILE_AGENT_CLI ON)
  set(_PROFILE_HTTP_API ON)
  set(_PROFILE_ROM_TESTS OFF)  # Still off - requires ROM files

else() # EDITOR (default)
  # GUI application with basic features
  set(_PROFILE_GUI ON)
  set(_PROFILE_CLI ON)
  set(_PROFILE_Z3ED ON)
  set(_PROFILE_EMU ON)
  set(_PROFILE_LIB ON)
  set(_PROFILE_TESTS OFF)
  set(_PROFILE_TOOLS OFF)
  set(_PROFILE_LAB OFF)
  set(_PROFILE_GRPC OFF)
  set(_PROFILE_AI OFF)
  set(_PROFILE_AI_RUNTIME OFF)
  set(_PROFILE_REMOTE_AUTOMATION OFF)
  set(_PROFILE_AGENT_UI ON)  # UI panels enabled, but no AI backend
  set(_PROFILE_AGENT_CLI OFF)
  set(_PROFILE_HTTP_API OFF)
  set(_PROFILE_ROM_TESTS OFF)
endif()

#===============================================================================
# Core Build Targets (use profile default if not explicitly set)
#===============================================================================
if(NOT DEFINED YAZE_BUILD_GUI)
  option(YAZE_BUILD_GUI "Build GUI application" ${_PROFILE_GUI})
endif()
if(NOT DEFINED YAZE_BUILD_CLI)
  option(YAZE_BUILD_CLI "Build CLI tools (shared libraries)" ${_PROFILE_CLI})
endif()
if(NOT DEFINED YAZE_BUILD_Z3ED)
  option(YAZE_BUILD_Z3ED "Build z3ed CLI executable" ${_PROFILE_Z3ED})
endif()
if(NOT DEFINED YAZE_BUILD_EMU)
  option(YAZE_BUILD_EMU "Build emulator components" ${_PROFILE_EMU})
endif()
if(NOT DEFINED YAZE_BUILD_LIB)
  option(YAZE_BUILD_LIB "Build static library" ${_PROFILE_LIB})
endif()
if(NOT DEFINED YAZE_BUILD_TESTS)
  option(YAZE_BUILD_TESTS "Build test suite" ${_PROFILE_TESTS})
endif()
if(NOT DEFINED YAZE_BUILD_TOOLS)
  option(YAZE_BUILD_TOOLS "Build development utility tools" ${_PROFILE_TOOLS})
endif()
if(NOT DEFINED YAZE_BUILD_LAB)
  option(YAZE_BUILD_LAB "Build lab sandbox executable" ${_PROFILE_LAB})
endif()

# Compatibility alias for YAZE_BUILD_APP (deprecated, use YAZE_BUILD_GUI)
if(DEFINED YAZE_BUILD_APP AND NOT DEFINED YAZE_BUILD_GUI)
  set(YAZE_BUILD_GUI ${YAZE_BUILD_APP} CACHE BOOL "Build GUI application" FORCE)
  message(WARNING "YAZE_BUILD_APP is deprecated, use YAZE_BUILD_GUI instead")
endif()

#===============================================================================
# Feature Toggles (use profile default if not explicitly set)
#===============================================================================
if(NOT DEFINED YAZE_ENABLE_GRPC)
  option(YAZE_ENABLE_GRPC "Enable gRPC agent support" ${_PROFILE_GRPC})
endif()
if(NOT DEFINED YAZE_ENABLE_AI)
  option(YAZE_ENABLE_AI "Enable AI agent features" ${_PROFILE_AI})
endif()
if(NOT DEFINED YAZE_ENABLE_AI_RUNTIME)
  option(YAZE_ENABLE_AI_RUNTIME "Enable AI runtime integrations" ${_PROFILE_AI_RUNTIME})
endif()
if(NOT DEFINED YAZE_ENABLE_REMOTE_AUTOMATION)
  option(YAZE_ENABLE_REMOTE_AUTOMATION "Enable remote automation services" ${_PROFILE_REMOTE_AUTOMATION})
endif()
if(NOT DEFINED YAZE_BUILD_AGENT_UI)
  option(YAZE_BUILD_AGENT_UI "Build ImGui-based agent/chat panels" ${_PROFILE_AGENT_UI})
endif()
if(NOT DEFINED YAZE_ENABLE_AGENT_CLI)
  option(YAZE_ENABLE_AGENT_CLI "Build conversational agent CLI stack" ${_PROFILE_AGENT_CLI})
endif()
if(NOT DEFINED YAZE_ENABLE_HTTP_API)
  option(YAZE_ENABLE_HTTP_API "Enable HTTP REST API server" ${_PROFILE_HTTP_API})
endif()

# Always-on features
if(NOT DEFINED YAZE_ENABLE_JSON)
  option(YAZE_ENABLE_JSON "Enable JSON support" ON)
endif()
if(NOT DEFINED YAZE_ENABLE_ASAR)
  option(YAZE_ENABLE_ASAR "Enable ASAR 65816 assembler integration" ON)
endif()

#===============================================================================
# Validate Option Combinations (errors instead of silent mutations)
#===============================================================================
if(YAZE_ENABLE_REMOTE_AUTOMATION AND NOT YAZE_ENABLE_GRPC)
  message(FATAL_ERROR "YAZE_ENABLE_REMOTE_AUTOMATION requires YAZE_ENABLE_GRPC=ON")
endif()

if(YAZE_ENABLE_AI_RUNTIME AND NOT YAZE_ENABLE_AI)
  message(FATAL_ERROR "YAZE_ENABLE_AI_RUNTIME requires YAZE_ENABLE_AI=ON")
endif()

if(YAZE_ENABLE_HTTP_API AND NOT YAZE_ENABLE_AGENT_CLI)
  message(FATAL_ERROR "YAZE_ENABLE_HTTP_API requires YAZE_ENABLE_AGENT_CLI=ON")
endif()

#===============================================================================
# Build Optimizations
#===============================================================================
if(NOT DEFINED YAZE_ENABLE_LTO)
  option(YAZE_ENABLE_LTO "Enable link-time optimization" OFF)
endif()
if(NOT DEFINED YAZE_ENABLE_SANITIZERS)
  option(YAZE_ENABLE_SANITIZERS "Enable AddressSanitizer/UBSanitizer" OFF)
endif()
if(NOT DEFINED YAZE_ENABLE_COVERAGE)
  option(YAZE_ENABLE_COVERAGE "Enable code coverage" OFF)
endif()
if(NOT DEFINED YAZE_UNITY_BUILD)
  option(YAZE_UNITY_BUILD "Enable Unity (Jumbo) builds" OFF)
endif()

#===============================================================================
# Platform-specific options
#===============================================================================
if(NOT DEFINED YAZE_USE_VCPKG)
  option(YAZE_USE_VCPKG "Use vcpkg for Windows dependencies" OFF)
endif()
if(NOT DEFINED YAZE_USE_SYSTEM_DEPS)
  option(YAZE_USE_SYSTEM_DEPS "Use system package manager for dependencies" OFF)
endif()
if(NOT DEFINED YAZE_USE_SDL3)
  option(YAZE_USE_SDL3 "Use SDL3 instead of SDL2 (experimental)" OFF)
endif()
if(NOT DEFINED YAZE_WASM_TERMINAL)
  option(YAZE_WASM_TERMINAL "Build z3ed for WASM terminal mode (no TUI)" OFF)
endif()

#===============================================================================
# Development options
#===============================================================================
if(NOT DEFINED YAZE_ENABLE_ROM_TESTS)
  option(YAZE_ENABLE_ROM_TESTS "Enable tests that require ROM files" ${_PROFILE_ROM_TESTS})
endif()
if(NOT DEFINED YAZE_ENABLE_EXPERIMENTAL_APP_TEST_SUITES)
  option(YAZE_ENABLE_EXPERIMENTAL_APP_TEST_SUITES
    "Enable experimental app test suites in the yaze_test runner"
    OFF)
endif()
if(NOT DEFINED YAZE_ENABLE_BENCHMARK_TESTS)
  option(YAZE_ENABLE_BENCHMARK_TESTS "Enable benchmark/performance tests" OFF)
endif()
if(NOT DEFINED YAZE_MINIMAL_BUILD)
  option(YAZE_MINIMAL_BUILD "Minimal build for CI (disable optional features)" OFF)
endif()
if(NOT DEFINED YAZE_VERBOSE_BUILD)
  option(YAZE_VERBOSE_BUILD "Verbose build output" OFF)
endif()
if(NOT DEFINED YAZE_ENABLE_CLANG_TIDY)
  option(YAZE_ENABLE_CLANG_TIDY "Enable clang-tidy linting during build" ON)
endif()
if(NOT DEFINED YAZE_ENABLE_OPENCV)
  option(YAZE_ENABLE_OPENCV "Enable OpenCV for advanced visual analysis" OFF)
endif()

#===============================================================================
# Install options
#===============================================================================
if(NOT DEFINED YAZE_INSTALL_LIB)
  option(YAZE_INSTALL_LIB "Install static library" OFF)
endif()
if(NOT DEFINED YAZE_INSTALL_HEADERS)
  option(YAZE_INSTALL_HEADERS "Install public headers" ON)
endif()
if(NOT DEFINED YAZE_INSTALL_TEST_HELPERS)
  option(YAZE_INSTALL_TEST_HELPERS "Install development test helper tools" OFF)
endif()
