# TestInfrastructure.cmake
# Advanced test infrastructure configuration for yaze project
# Provides optimized test builds, parallel execution, and smart test selection

include(GoogleTest)
include(CTest)

# =============================================================================
# Test Configuration Options
# =============================================================================

option(YAZE_TEST_PARALLEL "Enable parallel test execution" ON)
option(YAZE_TEST_COVERAGE "Enable code coverage collection" OFF)
option(YAZE_TEST_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(YAZE_TEST_PROFILE "Enable test profiling" OFF)
option(YAZE_TEST_MINIMAL_DEPS "Use minimal dependencies for faster test builds" OFF)
option(YAZE_TEST_PCH "Use precompiled headers for tests" ON)

# Test categories
option(YAZE_TEST_SMOKE "Build smoke tests (critical path)" ON)
option(YAZE_TEST_UNIT "Build unit tests" ON)
option(YAZE_TEST_INTEGRATION "Build integration tests" ON)
option(YAZE_TEST_E2E "Build end-to-end GUI tests" OFF)
option(YAZE_TEST_BENCHMARK "Build performance benchmarks" OFF)
option(YAZE_TEST_FUZZ "Build fuzzing tests" OFF)

# Test execution settings
set(YAZE_TEST_TIMEOUT_SMOKE 30 CACHE STRING "Timeout for smoke tests (seconds)")
set(YAZE_TEST_TIMEOUT_UNIT 60 CACHE STRING "Timeout for unit tests (seconds)")
set(YAZE_TEST_TIMEOUT_INTEGRATION 300 CACHE STRING "Timeout for integration tests (seconds)")
set(YAZE_TEST_TIMEOUT_E2E 600 CACHE STRING "Timeout for E2E tests (seconds)")

# =============================================================================
# Precompiled Headers Configuration
# =============================================================================

if(YAZE_TEST_PCH)
  set(YAZE_TEST_PCH_HEADERS
    <gtest/gtest.h>
    <gmock/gmock.h>
    <absl/status/status.h>
    <absl/status/statusor.h>
    <absl/strings/string_view.h>
    <memory>
    <vector>
    <string>
    <unordered_map>
  )

  # Create PCH target
  add_library(yaze_test_pch INTERFACE)
  target_precompile_headers(yaze_test_pch INTERFACE ${YAZE_TEST_PCH_HEADERS})

  message(STATUS "Test Infrastructure: Precompiled headers enabled")
endif()

# =============================================================================
# Test Interface Library
# =============================================================================

add_library(yaze_test_interface INTERFACE)

# Common compile options
target_compile_options(yaze_test_interface INTERFACE
  $<$<CONFIG:Debug>:-O0 -g3>
  $<$<CONFIG:Release>:-O3 -DNDEBUG>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g>
  $<$<BOOL:${YAZE_TEST_COVERAGE}>:--coverage -fprofile-arcs -ftest-coverage>
  $<$<BOOL:${YAZE_TEST_PROFILE}>:-pg>
)

# Common link options
target_link_options(yaze_test_interface INTERFACE
  $<$<BOOL:${YAZE_TEST_COVERAGE}>:--coverage>
  $<$<BOOL:${YAZE_TEST_PROFILE}>:-pg>
)

# Sanitizers
if(YAZE_TEST_SANITIZERS)
  target_compile_options(yaze_test_interface INTERFACE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
  )
  target_link_options(yaze_test_interface INTERFACE
    -fsanitize=address,undefined
  )

  message(STATUS "Test Infrastructure: Sanitizers enabled (ASan + UBSan)")
endif()

# Coverage
if(YAZE_TEST_COVERAGE)
  find_program(LCOV lcov)
  find_program(GENHTML genhtml)

  if(LCOV AND GENHTML)
    add_custom_target(coverage
      COMMAND ${LCOV} --capture --directory ${CMAKE_BINARY_DIR}
              --output-file ${CMAKE_BINARY_DIR}/coverage.info
              --ignore-errors source
      COMMAND ${LCOV} --remove ${CMAKE_BINARY_DIR}/coverage.info
              '*/test/*' '*/ext/*' '/usr/*' '*/build/*'
              --output-file ${CMAKE_BINARY_DIR}/coverage_filtered.info
      COMMAND ${GENHTML} ${CMAKE_BINARY_DIR}/coverage_filtered.info
              --output-directory ${CMAKE_BINARY_DIR}/coverage_html
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating code coverage report"
    )

    message(STATUS "Test Infrastructure: Coverage target available")
  else()
    message(WARNING "lcov/genhtml not found, coverage target disabled")
  endif()
endif()

# =============================================================================
# Test Creation Function
# =============================================================================

function(yaze_create_test_suite)
  set(options GUI_TEST REQUIRES_ROM USE_PCH)
  set(oneValueArgs NAME CATEGORY TIMEOUT PARALLEL_JOBS OUTPUT_DIR)
  set(multiValueArgs SOURCES DEPENDENCIES LABELS COMPILE_DEFINITIONS)
  cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Validate required arguments
  if(NOT TEST_NAME)
    message(FATAL_ERROR "yaze_create_test_suite: NAME is required")
  endif()
  if(NOT TEST_SOURCES)
    message(FATAL_ERROR "yaze_create_test_suite: SOURCES is required")
  endif()

  # Set defaults
  if(NOT TEST_CATEGORY)
    set(TEST_CATEGORY "general")
  endif()
  if(NOT TEST_TIMEOUT)
    set(TEST_TIMEOUT 60)
  endif()
  if(NOT TEST_OUTPUT_DIR)
    set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/test")
  endif()

  # Create test executable
  set(target_name yaze_test_${TEST_NAME})
  add_executable(${target_name} ${TEST_SOURCES})

  # Set output directory
  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TEST_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TEST_OUTPUT_DIR}"
  )

  # Link libraries
  target_link_libraries(${target_name} PRIVATE
    yaze_test_interface
    yaze_test_support
    GTest::gtest_main
    GTest::gmock
    ${TEST_DEPENDENCIES}
  )

  # Apply PCH if requested
  if(TEST_USE_PCH AND TARGET yaze_test_pch)
    target_link_libraries(${target_name} PRIVATE yaze_test_pch)
    target_precompile_headers(${target_name} REUSE_FROM yaze_test_pch)
  endif()

  # Add compile definitions
  if(TEST_COMPILE_DEFINITIONS)
    target_compile_definitions(${target_name} PRIVATE ${TEST_COMPILE_DEFINITIONS})
  endif()

  # GUI test configuration
  if(TEST_GUI_TEST)
    target_compile_definitions(${target_name} PRIVATE
      IMGUI_ENABLE_TEST_ENGINE=1
      YAZE_GUI_TEST_TARGET=1
    )
    if(TARGET ImGuiTestEngine)
      target_link_libraries(${target_name} PRIVATE ImGuiTestEngine)
    endif()
  endif()

  # ROM test configuration
  if(TEST_REQUIRES_ROM)
    target_compile_definitions(${target_name} PRIVATE
      YAZE_ROM_TEST=1
    )
  endif()

  # Discover tests with CTest
  set(test_labels "${TEST_CATEGORY}")
  if(TEST_LABELS)
    list(APPEND test_labels ${TEST_LABELS})
  endif()

  gtest_discover_tests(${target_name}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
      LABELS "${test_labels}"
      TIMEOUT ${TEST_TIMEOUT}
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 30
  )

  # Set parallel execution if specified
  if(TEST_PARALLEL_JOBS)
    set_property(TEST ${target_name} PROPERTY PROCESSORS ${TEST_PARALLEL_JOBS})
  endif()

  # Create category-specific test target
  add_custom_target(test-${TEST_CATEGORY}-${TEST_NAME}
    COMMAND ${CMAKE_CTEST_COMMAND} -R "^${target_name}" --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running ${TEST_CATEGORY} tests: ${TEST_NAME}"
  )

  message(STATUS "Test Suite: ${target_name} (${TEST_CATEGORY})")
endfunction()

# =============================================================================
# Test Suites Configuration
# =============================================================================

# Smoke Tests (Critical Path)
if(YAZE_TEST_SMOKE)
  file(GLOB SMOKE_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/test/smoke/*.cc
    ${CMAKE_SOURCE_DIR}/test/smoke/*.h
  )

  if(SMOKE_TEST_SOURCES)
    yaze_create_test_suite(
      NAME smoke
      CATEGORY smoke
      SOURCES ${SMOKE_TEST_SOURCES}
      LABELS critical fast ci-stage1
      TIMEOUT ${YAZE_TEST_TIMEOUT_SMOKE}
      USE_PCH
    )
  else()
    message(WARNING "No smoke test sources found")
  endif()
endif()

# Unit Tests
if(YAZE_TEST_UNIT)
  file(GLOB_RECURSE UNIT_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/test/unit/*.cc
  )

  if(UNIT_TEST_SOURCES)
    # Split unit tests into multiple binaries for parallel execution
    list(LENGTH UNIT_TEST_SOURCES num_unit_tests)

    if(num_unit_tests GREATER 50)
      # Create sharded unit test executables
      set(shard_size 25)
      math(EXPR num_shards "(${num_unit_tests} + ${shard_size} - 1) / ${shard_size}")

      foreach(shard RANGE 1 ${num_shards})
        math(EXPR start "(${shard} - 1) * ${shard_size}")
        math(EXPR end "${shard} * ${shard_size} - 1")

        if(end GREATER ${num_unit_tests})
          set(end ${num_unit_tests})
        endif()

        list(SUBLIST UNIT_TEST_SOURCES ${start} ${shard_size} shard_sources)

        yaze_create_test_suite(
          NAME unit_shard${shard}
          CATEGORY unit
          SOURCES ${shard_sources}
          LABELS unit fast ci-stage2 shard${shard}
          TIMEOUT ${YAZE_TEST_TIMEOUT_UNIT}
          USE_PCH
        )
      endforeach()
    else()
      # Single unit test executable
      yaze_create_test_suite(
        NAME unit
        CATEGORY unit
        SOURCES ${UNIT_TEST_SOURCES}
        LABELS unit fast ci-stage2
        TIMEOUT ${YAZE_TEST_TIMEOUT_UNIT}
        USE_PCH
      )
    endif()
  endif()
endif()

# Integration Tests
if(YAZE_TEST_INTEGRATION)
  file(GLOB_RECURSE INTEGRATION_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/test/integration/*.cc
  )

  if(INTEGRATION_TEST_SOURCES)
    yaze_create_test_suite(
      NAME integration
      CATEGORY integration
      SOURCES ${INTEGRATION_TEST_SOURCES}
      LABELS integration medium ci-stage3
      TIMEOUT ${YAZE_TEST_TIMEOUT_INTEGRATION}
      PARALLEL_JOBS 2
      USE_PCH
    )
  endif()
endif()

# E2E Tests
if(YAZE_TEST_E2E)
  file(GLOB_RECURSE E2E_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/test/e2e/*.cc
  )

  if(E2E_TEST_SOURCES)
    yaze_create_test_suite(
      NAME e2e
      CATEGORY e2e
      SOURCES ${E2E_TEST_SOURCES}
      LABELS e2e slow gui ci-stage3
      TIMEOUT ${YAZE_TEST_TIMEOUT_E2E}
      GUI_TEST
      USE_PCH
    )
  endif()
endif()

# Benchmark Tests
if(YAZE_TEST_BENCHMARK)
  file(GLOB_RECURSE BENCHMARK_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/test/benchmarks/*.cc
  )

  if(BENCHMARK_TEST_SOURCES)
    yaze_create_test_suite(
      NAME benchmark
      CATEGORY benchmark
      SOURCES ${BENCHMARK_TEST_SOURCES}
      LABELS benchmark performance nightly
      TIMEOUT 1800
      COMPILE_DEFINITIONS BENCHMARK_BUILD=1
    )
  endif()
endif()

# =============================================================================
# Custom Test Commands
# =============================================================================

# Run all fast tests
add_custom_target(test-fast
  COMMAND ${CMAKE_CTEST_COMMAND} -L "fast" --parallel ${CMAKE_NUMBER_OF_CORES} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running fast tests"
)

# Run tests by stage
add_custom_target(test-stage1
  COMMAND ${CMAKE_CTEST_COMMAND} -L "ci-stage1" --parallel 4 --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running Stage 1 (Smoke) tests"
)

add_custom_target(test-stage2
  COMMAND ${CMAKE_CTEST_COMMAND} -L "ci-stage2" --parallel ${CMAKE_NUMBER_OF_CORES} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running Stage 2 (Unit) tests"
)

add_custom_target(test-stage3
  COMMAND ${CMAKE_CTEST_COMMAND} -L "ci-stage3" --parallel 2 --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running Stage 3 (Integration/E2E) tests"
)

# Smart test selection based on changed files
add_custom_target(test-changed
  COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/scripts/smart_test_selector.py
          --output filter | xargs ${CMAKE_CTEST_COMMAND} -R --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running tests for changed files"
)

# Parallel test execution with sharding
add_custom_target(test-parallel
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/test_runner.py
          ${CMAKE_BINARY_DIR}/bin/test/yaze_test_unit
          --shards ${CMAKE_NUMBER_OF_CORES}
          --output-dir ${CMAKE_BINARY_DIR}/test_results
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running tests with parallel sharding"
)

# Test with retries for flaky tests
add_custom_target(test-retry
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/test_runner.py
          ${CMAKE_BINARY_DIR}/bin/test/yaze_test_unit
          --retry 2
          --output-dir ${CMAKE_BINARY_DIR}/test_results
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running tests with retry for failures"
)

# =============================================================================
# CTest Configuration
# =============================================================================

set(CTEST_PARALLEL_LEVEL ${CMAKE_NUMBER_OF_CORES} CACHE STRING "Default parallel level for CTest")
set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "Show output on test failure")
set(CTEST_PROGRESS_OUTPUT ON CACHE BOOL "Show progress during test execution")

# Configure test output
set(CMAKE_CTEST_ARGUMENTS
  --output-on-failure
  --progress
  --parallel ${CTEST_PARALLEL_LEVEL}
)

# Configure test timeout multiplier for slower systems
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CTEST_TEST_TIMEOUT_MULTIPLIER 1.5)
endif()

# =============================================================================
# Test Data Management
# =============================================================================

# Download test data if needed
if(YAZE_TEST_INTEGRATION OR YAZE_TEST_E2E)
  include(FetchContent)

  # Check if test data exists
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/test_data/VERSION")
    message(STATUS "Downloading test data...")

    FetchContent_Declare(
      yaze_test_data
      URL https://github.com/yaze/test-data/archive/refs/tags/v1.0.0.tar.gz
      URL_HASH SHA256=abcdef1234567890  # Replace with actual hash
      DOWNLOAD_EXTRACT_TIMESTAMP ON
    )

    FetchContent_MakeAvailable(yaze_test_data)

    set(YAZE_TEST_DATA_DIR ${yaze_test_data_SOURCE_DIR} CACHE PATH "Test data directory")
  else()
    set(YAZE_TEST_DATA_DIR ${CMAKE_BINARY_DIR}/test_data CACHE PATH "Test data directory")
  endif()
endif()

# =============================================================================
# Test Report Generation
# =============================================================================

add_custom_target(test-report
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/aggregate_test_results.py
          --input-dir ${CMAKE_BINARY_DIR}/Testing
          --output ${CMAKE_BINARY_DIR}/test_report.json
          --generate-html ${CMAKE_BINARY_DIR}/test_report.html
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generating test report"
  DEPENDS test-all
)

# =============================================================================
# Summary Message
# =============================================================================

message(STATUS "========================================")
message(STATUS "Test Infrastructure Configuration:")
message(STATUS "  Parallel Testing: ${YAZE_TEST_PARALLEL}")
message(STATUS "  Precompiled Headers: ${YAZE_TEST_PCH}")
message(STATUS "  Code Coverage: ${YAZE_TEST_COVERAGE}")
message(STATUS "  Sanitizers: ${YAZE_TEST_SANITIZERS}")
message(STATUS "  Test Categories:")
message(STATUS "    Smoke Tests: ${YAZE_TEST_SMOKE}")
message(STATUS "    Unit Tests: ${YAZE_TEST_UNIT}")
message(STATUS "    Integration Tests: ${YAZE_TEST_INTEGRATION}")
message(STATUS "    E2E Tests: ${YAZE_TEST_E2E}")
message(STATUS "    Benchmarks: ${YAZE_TEST_BENCHMARK}")
message(STATUS "  Parallel Level: ${CTEST_PARALLEL_LEVEL}")
message(STATUS "========================================")