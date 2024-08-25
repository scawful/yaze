#define SDL_MAIN_HANDLED

#include "yaze.h"

#include <SDL.h>
#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "test/integration/test_editor.h"

namespace yaze {
namespace test {

TEST(YazeCLibTest, InitializeAndCleanup) {
  yaze_flags flags;
  yaze_init(&flags);
  yaze_cleanup(&flags);
}

}  // namespace test
}  // namespace yaze

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);

  if (argc > 1 && std::string(argv[1]) == "integration") {
    return yaze::test::integration::RunIntegrationTest();
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}