# Testing dependencies (GTest, Benchmark)
# Uses CPM.cmake for consistent cross-platform builds

if(NOT YAZE_BUILD_TESTS)
  return()
endif()

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up testing dependencies with CPM.cmake")

set(_YAZE_USE_SYSTEM_GTEST ${YAZE_USE_SYSTEM_DEPS})

# Detect Homebrew installation automatically (helps offline builds)
if(APPLE AND NOT _YAZE_USE_SYSTEM_GTEST)
  set(_YAZE_GTEST_PREFIX_CANDIDATES
    /opt/homebrew/opt/googletest
    /usr/local/opt/googletest)

  foreach(_prefix IN LISTS _YAZE_GTEST_PREFIX_CANDIDATES)
    if(EXISTS "${_prefix}")
      list(APPEND CMAKE_PREFIX_PATH "${_prefix}")
      message(STATUS "Added Homebrew googletest prefix: ${_prefix}")
      set(_YAZE_USE_SYSTEM_GTEST ON)
      break()
    endif()
  endforeach()

  if(NOT _YAZE_USE_SYSTEM_GTEST)
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
      execute_process(
        COMMAND "${HOMEBREW_EXECUTABLE}" --prefix googletest
        OUTPUT_VARIABLE HOMEBREW_GTEST_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE HOMEBREW_GTEST_RESULT
        ERROR_QUIET)
      if(HOMEBREW_GTEST_RESULT EQUAL 0 AND EXISTS "${HOMEBREW_GTEST_PREFIX}")
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_GTEST_PREFIX}")
        message(STATUS "Added Homebrew googletest prefix: ${HOMEBREW_GTEST_PREFIX}")
        set(_YAZE_USE_SYSTEM_GTEST ON)
      endif()
    endif()
  endif()
endif()

# Try to use system packages first
if(_YAZE_USE_SYSTEM_GTEST)
  find_package(GTest QUIET)
  if(GTest_FOUND)
    message(STATUS "Using system googletest")
    # GTest found, targets should already be available
    # Verify targets exist
    if(NOT TARGET GTest::gtest)
      message(WARNING "GTest::gtest target not found despite GTest_FOUND=TRUE; falling back to CPM download")
      set(_YAZE_USE_SYSTEM_GTEST OFF)
    else()
      # Create aliases to match CPM target names
      if(NOT TARGET gtest)
        add_library(gtest ALIAS GTest::gtest)
      endif()
      if(NOT TARGET gtest_main)
        add_library(gtest_main ALIAS GTest::gtest_main)
      endif()
      if(TARGET GTest::gmock AND NOT TARGET gmock)
        add_library(gmock ALIAS GTest::gmock)
      endif()
      if(TARGET GTest::gmock_main AND NOT TARGET gmock_main)
        add_library(gmock_main ALIAS GTest::gmock_main)
      endif()
      # Skip CPM fetch
      set(_YAZE_GTEST_SYSTEM_USED ON)
    endif()
  elseif(YAZE_USE_SYSTEM_DEPS)
    message(WARNING "System googletest not found despite YAZE_USE_SYSTEM_DEPS=ON; falling back to CPM download")
  endif()
endif()

# Use CPM to fetch googletest if not using system version
if(NOT _YAZE_GTEST_SYSTEM_USED)
  CPMAddPackage(
    NAME googletest
    VERSION ${GTEST_VERSION}
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v${GTEST_VERSION}
    OPTIONS
      "BUILD_GMOCK ON"
      "INSTALL_GTEST OFF"
      "gtest_force_shared_crt ON"
  )
endif()

# Verify GTest and GMock targets are available
if(NOT TARGET gtest)
  message(FATAL_ERROR "GTest target not found after CPM fetch")
endif()

if(NOT TARGET gmock)
  message(FATAL_ERROR "GMock target not found after CPM fetch")
endif()

# Google Benchmark (optional, for performance tests)
if(YAZE_ENABLE_COVERAGE OR DEFINED ENV{YAZE_ENABLE_BENCHMARKS})
  CPMAddPackage(
    NAME benchmark
    VERSION ${BENCHMARK_VERSION}
    GITHUB_REPOSITORY google/benchmark
    GIT_TAG v${BENCHMARK_VERSION}
    OPTIONS
      "BENCHMARK_ENABLE_TESTING OFF"
      "BENCHMARK_ENABLE_INSTALL OFF"
  )
  
  if(NOT TARGET benchmark::benchmark)
    message(FATAL_ERROR "Benchmark target not found after CPM fetch")
  endif()
  
  set(YAZE_BENCHMARK_TARGETS benchmark::benchmark)
endif()

# Create convenience targets for the rest of the project
add_library(yaze_testing INTERFACE)
target_link_libraries(yaze_testing INTERFACE 
  gtest 
  gtest_main
  gmock
  gmock_main
)

if(TARGET benchmark::benchmark)
  target_link_libraries(yaze_testing INTERFACE benchmark::benchmark)
endif()

# Export testing targets for use in other CMake files
set(YAZE_TESTING_TARGETS yaze_testing)

message(STATUS "Testing dependencies setup complete - GTest + GMock available")
