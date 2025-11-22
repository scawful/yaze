# CMake Configuration Validator
# Validates CMake configuration and catches dependency issues early
#
# Usage:
#   cmake -P scripts/validate-cmake-config.cmake [build_directory]
#
# Exit codes:
#   0 - All checks passed
#   1 - Validation failed (errors detected)

cmake_minimum_required(VERSION 3.16)

# Parse command-line arguments
if(CMAKE_ARGC GREATER 3)
  set(BUILD_DIR "${CMAKE_ARGV3}")
else()
  set(BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")
endif()

# Color output helpers
if(WIN32)
  set(COLOR_RESET "")
  set(COLOR_RED "")
  set(COLOR_GREEN "")
  set(COLOR_YELLOW "")
  set(COLOR_BLUE "")
else()
  string(ASCII 27 ESC)
  set(COLOR_RESET "${ESC}[0m")
  set(COLOR_RED "${ESC}[31m")
  set(COLOR_GREEN "${ESC}[32m")
  set(COLOR_YELLOW "${ESC}[33m")
  set(COLOR_BLUE "${ESC}[34m")
endif()

set(VALIDATION_ERRORS 0)
set(VALIDATION_WARNINGS 0)

macro(log_header msg)
  message(STATUS "${COLOR_BLUE}=== ${msg} ===${COLOR_RESET}")
endmacro()

macro(log_success msg)
  message(STATUS "${COLOR_GREEN}✓${COLOR_RESET} ${msg}")
endmacro()

macro(log_warning msg)
  message(STATUS "${COLOR_YELLOW}⚠${COLOR_RESET} ${msg}")
  math(EXPR VALIDATION_WARNINGS "${VALIDATION_WARNINGS} + 1")
endmacro()

macro(log_error msg)
  message(STATUS "${COLOR_RED}✗${COLOR_RESET} ${msg}")
  math(EXPR VALIDATION_ERRORS "${VALIDATION_ERRORS} + 1")
endmacro()

# ============================================================================
# Check build directory exists
# ============================================================================
log_header("Checking build directory")

if(NOT EXISTS "${BUILD_DIR}")
  log_error("Build directory not found: ${BUILD_DIR}")
  log_error("Run cmake configure first: cmake --preset <preset-name>")
  message(FATAL_ERROR "Build directory does not exist")
endif()

if(NOT EXISTS "${BUILD_DIR}/CMakeCache.txt")
  log_error("CMakeCache.txt not found in ${BUILD_DIR}")
  log_error("Configuration incomplete - run cmake configure first")
  message(FATAL_ERROR "CMake configuration not found")
endif()

log_success("Build directory: ${BUILD_DIR}")

# ============================================================================
# Load CMake cache variables
# ============================================================================
log_header("Loading CMake configuration")

file(STRINGS "${BUILD_DIR}/CMakeCache.txt" CACHE_LINES)
set(CACHE_VARS "")

foreach(line ${CACHE_LINES})
  if(line MATCHES "^([^:]+):([^=]+)=(.*)$")
    set(var_name "${CMAKE_MATCH_1}")
    set(var_type "${CMAKE_MATCH_2}")
    set(var_value "${CMAKE_MATCH_3}")
    set(CACHE_${var_name} "${var_value}")
    list(APPEND CACHE_VARS "${var_name}")
  endif()
endforeach()

log_success("Loaded ${CMAKE_ARGC} cache variables")

# ============================================================================
# Validate required targets exist
# ============================================================================
log_header("Validating required targets")

set(REQUIRED_TARGETS
  "yaze_common"
)

set(OPTIONAL_TARGETS_GRPC
  "grpc::grpc++"
  "grpc::grpc++_reflection"
  "protobuf::libprotobuf"
  "protoc"
  "grpc_cpp_plugin"
)

set(OPTIONAL_TARGETS_ABSL
  "absl::base"
  "absl::status"
  "absl::statusor"
  "absl::strings"
  "absl::flags"
  "absl::flags_parse"
)

# Check for targets by looking for their CMake files
foreach(target ${REQUIRED_TARGETS})
  string(REPLACE "::" "_" target_safe "${target}")
  if(EXISTS "${BUILD_DIR}/CMakeFiles/${target_safe}.dir")
    log_success("Required target exists: ${target}")
  else()
    log_error("Required target missing: ${target}")
  endif()
endforeach()

# ============================================================================
# Validate feature flags and dependencies
# ============================================================================
log_header("Validating feature flags")

if(DEFINED CACHE_YAZE_ENABLE_GRPC)
  if(CACHE_YAZE_ENABLE_GRPC)
    log_success("gRPC enabled: ${CACHE_YAZE_ENABLE_GRPC}")

    # Validate gRPC-related cache variables
    if(NOT DEFINED CACHE_GRPC_VERSION)
      log_warning("GRPC_VERSION not set in cache")
    else()
      log_success("gRPC version: ${CACHE_GRPC_VERSION}")
    endif()
  else()
    log_success("gRPC disabled")
  endif()
endif()

if(DEFINED CACHE_YAZE_BUILD_TESTS)
  if(CACHE_YAZE_BUILD_TESTS)
    log_success("Tests enabled")
  else()
    log_success("Tests disabled")
  endif()
endif()

if(DEFINED CACHE_YAZE_ENABLE_AI)
  if(CACHE_YAZE_ENABLE_AI)
    log_success("AI features enabled")

    # When AI is enabled, gRPC should also be enabled
    if(NOT CACHE_YAZE_ENABLE_GRPC)
      log_error("AI enabled but gRPC disabled - AI requires gRPC")
    endif()
  else()
    log_success("AI features disabled")
  endif()
endif()

# ============================================================================
# Validate compiler flags
# ============================================================================
log_header("Validating compiler flags")

if(DEFINED CACHE_CMAKE_CXX_STANDARD)
  if(CACHE_CMAKE_CXX_STANDARD EQUAL 23)
    log_success("C++ standard: ${CACHE_CMAKE_CXX_STANDARD}")
  else()
    log_warning("C++ standard is ${CACHE_CMAKE_CXX_STANDARD}, expected 23")
  endif()
else()
  log_error("CMAKE_CXX_STANDARD not set")
endif()

if(DEFINED CACHE_CMAKE_CXX_FLAGS)
  log_success("CXX flags set: ${CACHE_CMAKE_CXX_FLAGS}")
endif()

# ============================================================================
# Validate Abseil configuration (Windows-specific)
# ============================================================================
if(WIN32 OR CACHE_CMAKE_SYSTEM_NAME STREQUAL "Windows")
  log_header("Validating Windows/Abseil configuration")

  # Check for MSVC runtime library setting
  if(DEFINED CACHE_CMAKE_MSVC_RUNTIME_LIBRARY)
    if(CACHE_CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded")
      log_success("MSVC runtime: ${CACHE_CMAKE_MSVC_RUNTIME_LIBRARY}")
    else()
      log_warning("MSVC runtime: ${CACHE_CMAKE_MSVC_RUNTIME_LIBRARY} (expected MultiThreaded)")
    endif()
  else()
    log_warning("CMAKE_MSVC_RUNTIME_LIBRARY not set")
  endif()

  # Check for Abseil propagation flag
  if(DEFINED CACHE_ABSL_PROPAGATE_CXX_STD)
    if(CACHE_ABSL_PROPAGATE_CXX_STD)
      log_success("Abseil CXX standard propagation enabled")
    else()
      log_warning("ABSL_PROPAGATE_CXX_STD is OFF - may cause issues")
    endif()
  endif()
endif()

# ============================================================================
# Check for circular dependencies
# ============================================================================
log_header("Checking for circular dependencies")

if(EXISTS "${BUILD_DIR}/CMakeFiles/TargetDirectories.txt")
  file(READ "${BUILD_DIR}/CMakeFiles/TargetDirectories.txt" TARGET_DIRS)
  string(REGEX MATCHALL "[^\n]+" TARGET_LIST "${TARGET_DIRS}")
  list(LENGTH TARGET_LIST NUM_TARGETS)
  log_success("Found ${NUM_TARGETS} build targets")
else()
  log_warning("TargetDirectories.txt not found - cannot validate targets")
endif()

# ============================================================================
# Validate compile_commands.json
# ============================================================================
log_header("Validating compile commands")

if(NOT DEFINED CACHE_CMAKE_EXPORT_COMPILE_COMMANDS)
  log_warning("CMAKE_EXPORT_COMPILE_COMMANDS not set")
elseif(NOT CACHE_CMAKE_EXPORT_COMPILE_COMMANDS)
  log_warning("Compile commands export disabled")
else()
  if(EXISTS "${BUILD_DIR}/compile_commands.json")
    file(READ "${BUILD_DIR}/compile_commands.json" COMPILE_COMMANDS)
    string(LENGTH "${COMPILE_COMMANDS}" COMPILE_COMMANDS_SIZE)

    if(COMPILE_COMMANDS_SIZE GREATER 100)
      log_success("compile_commands.json generated (${COMPILE_COMMANDS_SIZE} bytes)")
    else()
      log_warning("compile_commands.json is very small or empty")
    endif()
  else()
    log_warning("compile_commands.json not found")
  endif()
endif()

# ============================================================================
# Platform-specific validation
# ============================================================================
log_header("Platform-specific checks")

if(DEFINED CACHE_CMAKE_SYSTEM_NAME)
  log_success("Target system: ${CACHE_CMAKE_SYSTEM_NAME}")

  if(CACHE_CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS-specific checks
    if(DEFINED CACHE_CMAKE_OSX_ARCHITECTURES)
      log_success("macOS architectures: ${CACHE_CMAKE_OSX_ARCHITECTURES}")
    endif()
  elseif(CACHE_CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows-specific checks
    if(DEFINED CACHE_CMAKE_GENERATOR)
      log_success("Generator: ${CACHE_CMAKE_GENERATOR}")

      if(CACHE_CMAKE_GENERATOR MATCHES "Visual Studio")
        log_success("Using Visual Studio generator")
      elseif(CACHE_CMAKE_GENERATOR MATCHES "Ninja")
        log_success("Using Ninja generator")
      endif()
    endif()
  elseif(CACHE_CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux-specific checks
    if(DEFINED CACHE_CMAKE_CXX_COMPILER_ID)
      log_success("Compiler: ${CACHE_CMAKE_CXX_COMPILER_ID}")
    endif()
  endif()
endif()

# ============================================================================
# Validate output directories
# ============================================================================
log_header("Validating output directories")

set(OUTPUT_DIRS
  "${BUILD_DIR}/bin"
  "${BUILD_DIR}/lib"
)

foreach(dir ${OUTPUT_DIRS})
  if(EXISTS "${dir}")
    log_success("Output directory exists: ${dir}")
  else()
    log_warning("Output directory missing: ${dir}")
  endif()
endforeach()

# ============================================================================
# Check for common configuration issues
# ============================================================================
log_header("Checking for common issues")

# Issue 1: Missing Abseil include paths on Windows
if(CACHE_YAZE_ENABLE_GRPC AND (WIN32 OR CACHE_CMAKE_SYSTEM_NAME STREQUAL "Windows"))
  if(EXISTS "${BUILD_DIR}/_deps/grpc-build/third_party/abseil-cpp")
    log_success("Abseil include directory exists in gRPC build")
  else()
    log_warning("Abseil directory not found in expected location")
  endif()
endif()

# Issue 2: Flag propagation
if(DEFINED CACHE_YAZE_SUPPRESS_WARNINGS)
  if(CACHE_YAZE_SUPPRESS_WARNINGS)
    log_success("Warnings suppressed (use -v preset for verbose)")
  else()
    log_success("Verbose warnings enabled")
  endif()
endif()

# Issue 3: LTO enabled in debug builds
if(DEFINED CACHE_CMAKE_BUILD_TYPE)
  if(CACHE_CMAKE_BUILD_TYPE STREQUAL "Debug" AND DEFINED CACHE_YAZE_ENABLE_LTO)
    if(CACHE_YAZE_ENABLE_LTO)
      log_warning("LTO enabled in Debug build - may slow compilation")
    endif()
  endif()
endif()

# ============================================================================
# Summary
# ============================================================================
log_header("Validation Summary")

if(VALIDATION_ERRORS GREATER 0)
  message(STATUS "${COLOR_RED}${VALIDATION_ERRORS} error(s) found${COLOR_RESET}")
endif()

if(VALIDATION_WARNINGS GREATER 0)
  message(STATUS "${COLOR_YELLOW}${VALIDATION_WARNINGS} warning(s) found${COLOR_RESET}")
endif()

if(VALIDATION_ERRORS EQUAL 0 AND VALIDATION_WARNINGS EQUAL 0)
  message(STATUS "${COLOR_GREEN}✓ All validation checks passed!${COLOR_RESET}")
  message(STATUS "Configuration is ready for build")
elseif(VALIDATION_ERRORS EQUAL 0)
  message(STATUS "${COLOR_YELLOW}Configuration has warnings but is buildable${COLOR_RESET}")
else()
  message(STATUS "${COLOR_RED}Configuration has errors - fix before building${COLOR_RESET}")
  message(FATAL_ERROR "CMake configuration validation failed")
endif()
