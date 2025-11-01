# Testing dependencies (GTest, Benchmark)
# Uses CPM.cmake for consistent cross-platform builds

if(NOT YAZE_BUILD_TESTS)
  return()
endif()

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up testing dependencies with CPM.cmake")

# Google Test
CPMAddPackage(
  NAME googletest
  VERSION ${GTEST_VERSION}
  GITHUB_REPOSITORY google/googletest
  GIT_TAG v${GTEST_VERSION}
  OPTIONS
    "BUILD_GMOCK OFF"
    "INSTALL_GTEST OFF"
    "gtest_force_shared_crt ON"
)

# Verify GTest targets are available
if(NOT TARGET gtest)
  message(FATAL_ERROR "GTest target not found after CPM fetch")
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
  
  set(YAZE_BENCHMARK_TARGETS benchmark::benchmark PARENT_SCOPE)
endif()

# Create convenience targets for the rest of the project
add_library(yaze_testing INTERFACE)
target_link_libraries(yaze_testing INTERFACE gtest)

if(TARGET benchmark::benchmark)
  target_link_libraries(yaze_testing INTERFACE benchmark::benchmark)
endif()

# Export testing targets for use in other CMake files
set(YAZE_TESTING_TARGETS yaze_testing)

message(STATUS "Testing dependencies setup complete")
