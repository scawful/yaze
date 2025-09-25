#define SDL_MAIN_HANDLED

#include <gtest/gtest.h>
#include <SDL.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "test_editor.h"

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  // Configure failure signal handler to be less aggressive for testing
  // This prevents false positives during SDL/graphics cleanup in tests
  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack = false;  // Avoid conflicts with normal stack during cleanup
  options.alarm_on_failure_secs = false; // Don't set alarms that can trigger on natural leaks
  options.call_previous_handler = true;  // Allow system handlers to also run
  options.writerfn = nullptr;  // Use default writer to avoid custom handling issues
  absl::InstallFailureSignalHandler(options);

  // Initialize SDL to prevent crashes in graphics components
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    // Continue anyway for tests that don't need graphics
  }

  if (argc > 1 && std::string(argv[1]) == "integration") {
    return yaze::test::RunIntegrationTest();
  } else if (argc > 1 && std::string(argv[1]) == "room_object") {
    ::testing::InitGoogleTest(&argc, argv);
    if (!RUN_ALL_TESTS()) {
      return yaze::test::RunIntegrationTest();
    }
  }

  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();
  
  // Cleanup SDL
  SDL_Quit();
  
  return result;
}
