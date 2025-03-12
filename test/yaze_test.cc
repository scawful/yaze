#define SDL_MAIN_HANDLED

#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "test/integration/test_editor.h"

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  if (argc > 1 && std::string(argv[1]) == "integration") {
    return yaze::test::RunIntegrationTest();
  } else if (argc > 1 && std::string(argv[1]) == "room_object") {
    ::testing::InitGoogleTest(&argc, argv);
    if (!RUN_ALL_TESTS()) {
      return yaze::test::RunIntegrationTest();
    }
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
