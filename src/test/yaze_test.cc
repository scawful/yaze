#define SDL_MAIN_HANDLED

#include "yaze.h"

#include <gtest/gtest.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "test/integration/test_editor.h"

namespace yaze {
namespace test {

TEST(YazeTest, LoadAndUnloadRom) {
  yaze_flags flags;
  flags.rom_filename = "zelda3.sfc";
  const int init = yaze_init(&flags);
  ASSERT_EQ(init, 0);
  yaze_cleanup(&flags);
}

TEST(YazeTest, NoFilename) {
  yaze_flags flags;
  const int init = yaze_init(&flags);
  ASSERT_EQ(init, -1);
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