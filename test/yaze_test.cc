#define SDL_MAIN_HANDLED
#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}