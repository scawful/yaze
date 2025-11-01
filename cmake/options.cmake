# YAZE Build Options
# Centralized feature flags and build configuration

# Core build options
option(YAZE_BUILD_GUI "Build GUI application" ON)
option(YAZE_BUILD_CLI "Build CLI tools (z3ed)" ON)
option(YAZE_BUILD_EMU "Build emulator components" ON)
option(YAZE_BUILD_LIB "Build static library" ON)
option(YAZE_BUILD_TESTS "Build test suite" ON)

# Feature flags
option(YAZE_ENABLE_GRPC "Enable gRPC agent support" ON)
option(YAZE_ENABLE_JSON "Enable JSON support" ON)
option(YAZE_ENABLE_AI "Enable AI agent features" ON)

# Build optimizations
option(YAZE_ENABLE_LTO "Enable link-time optimization" OFF)
option(YAZE_ENABLE_SANITIZERS "Enable AddressSanitizer/UBSanitizer" OFF)
option(YAZE_ENABLE_COVERAGE "Enable code coverage" OFF)
option(YAZE_UNITY_BUILD "Enable Unity (Jumbo) builds" OFF)

# Platform-specific options
option(YAZE_USE_VCPKG "Use vcpkg for Windows dependencies" OFF)
option(YAZE_USE_SYSTEM_DEPS "Use system package manager for dependencies" OFF)

# Development options
option(YAZE_ENABLE_ROM_TESTS "Enable tests that require ROM files" OFF)
option(YAZE_MINIMAL_BUILD "Minimal build for CI (disable optional features)" OFF)
option(YAZE_VERBOSE_BUILD "Verbose build output" OFF)

# Install options
option(YAZE_INSTALL_LIB "Install static library" OFF)
option(YAZE_INSTALL_HEADERS "Install public headers" ON)

# Set preprocessor definitions based on options
if(YAZE_ENABLE_GRPC)
  add_compile_definitions(YAZE_WITH_GRPC)
endif()

if(YAZE_ENABLE_JSON)
  add_compile_definitions(YAZE_WITH_JSON)
endif()

if(YAZE_ENABLE_AI)
  add_compile_definitions(Z3ED_AI)
endif()

# Print configuration summary
message(STATUS "=== YAZE Build Configuration ===")
message(STATUS "GUI Application: ${YAZE_BUILD_GUI}")
message(STATUS "CLI Tools: ${YAZE_BUILD_CLI}")
message(STATUS "Emulator: ${YAZE_BUILD_EMU}")
message(STATUS "Static Library: ${YAZE_BUILD_LIB}")
message(STATUS "Tests: ${YAZE_BUILD_TESTS}")
message(STATUS "gRPC Support: ${YAZE_ENABLE_GRPC}")
message(STATUS "JSON Support: ${YAZE_ENABLE_JSON}")
message(STATUS "AI Features: ${YAZE_ENABLE_AI}")
message(STATUS "LTO: ${YAZE_ENABLE_LTO}")
message(STATUS "Sanitizers: ${YAZE_ENABLE_SANITIZERS}")
message(STATUS "Coverage: ${YAZE_ENABLE_COVERAGE}")
message(STATUS "=================================")

