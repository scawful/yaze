#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  // Support the ability to launch an integration test window.
  SDL_SetMainReady();

  // Check if the argument says `integration`
  if (argc > 1 && std::string(argv[1]) == "integration") {
    // TODO: Create the debugging window with the test engine.
    return EXIT_SUCCESS;
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}