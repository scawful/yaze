// Simplified test executable for CI/CD builds
// This version removes complex argument parsing and SDL initialization
// to ensure reliable test discovery and execution in automated environments

#include <gtest/gtest.h>

#include <iostream>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"

int main(int argc, char* argv[]) {
  // Initialize symbolizer for better error reporting
  absl::InitializeSymbolizer(argv[0]);

  // Configure failure signal handler to be less aggressive for CI
  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack = false;
  options.alarm_on_failure_secs = false;
  options.call_previous_handler = true;
  options.writerfn = nullptr;
  absl::InstallFailureSignalHandler(options);

  // Initialize Google Test with minimal configuration
  ::testing::InitGoogleTest(&argc, argv);

  // Set up basic test environment
  ::testing::FLAGS_gtest_color = "yes";
  ::testing::FLAGS_gtest_print_time = true;

  // For CI builds, skip ROM-dependent tests by default
  // These tests require actual ROM files which aren't available in CI
  std::string filter = ::testing::GTEST_FLAG(filter);
  if (filter.empty()) {
    // Default filter for CI: exclude ROM-dependent and E2E tests
    ::testing::GTEST_FLAG(filter) = "-*RomTest*:-*E2E*:-*ZSCustomOverworld*";
  }

  std::cout << "Running YAZE tests in CI mode..." << std::endl;
  std::cout << "Test filter: " << ::testing::GTEST_FLAG(filter) << std::endl;

  // Run tests
  int result = RUN_ALL_TESTS();

  return result;
}
