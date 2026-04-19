#include "app/editor/system/session/background_command_task.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

// BackgroundCommandTask::Start() returns UnimplementedError on Windows,
// Emscripten, and iOS (see background_command_task.cc:SupportsNative*).
// Gate the whole suite on POSIX-desktop targets so CI on those platforms
// does not spuriously fail.
#if defined(_WIN32) || defined(__EMSCRIPTEN__) || \
    (defined(__APPLE__) && TARGET_OS_IPHONE)
#define YAZE_BACKGROUND_COMMANDS_SUPPORTED 0
#else
#define YAZE_BACKGROUND_COMMANDS_SUPPORTED 1
#endif

#if YAZE_BACKGROUND_COMMANDS_SUPPORTED

namespace yaze::editor {
namespace {

TEST(BackgroundCommandTaskTest, StreamsOutputWhileRunning) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start("printf 'first\\n'; sleep 0.2; printf 'second\\n'", ".")
                  .ok());

  bool saw_first_line = false;
  for (int i = 0; i < 20; ++i) {
    const auto snapshot = task.GetSnapshot();
    if (snapshot.output.find("first") != std::string::npos) {
      saw_first_line = true;
      EXPECT_TRUE(snapshot.running || snapshot.finished);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  EXPECT_TRUE(saw_first_line);
  EXPECT_TRUE(task.Wait().ok());
  const auto final_snapshot = task.GetSnapshot();
  EXPECT_TRUE(final_snapshot.finished);
  EXPECT_FALSE(final_snapshot.running);
  EXPECT_NE(final_snapshot.output.find("second"), std::string::npos);
}

TEST(BackgroundCommandTaskTest, CancelStopsRunningCommand) {
  BackgroundCommandTask task;
  ASSERT_TRUE(task.Start(
                  "printf 'start\\n'; sleep 5; printf 'done\\n'", ".")
                  .ok());

  bool saw_start = false;
  for (int i = 0; i < 20; ++i) {
    const auto snapshot = task.GetSnapshot();
    if (snapshot.output.find("start") != std::string::npos) {
      saw_start = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ASSERT_TRUE(saw_start);

  task.Cancel();
  const auto status = task.Wait();
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled);

  const auto snapshot = task.GetSnapshot();
  EXPECT_TRUE(snapshot.finished);
  EXPECT_FALSE(snapshot.running);
  EXPECT_TRUE(snapshot.cancel_requested);
  EXPECT_EQ(snapshot.output.find("done"), std::string::npos);
}

}  // namespace
}  // namespace yaze::editor

#else  // !YAZE_BACKGROUND_COMMANDS_SUPPORTED

#include <absl/status/status.h>
#include "app/editor/system/session/background_command_task.h"

namespace yaze::editor {
namespace {

// Sanity check: on unsupported platforms Start() must refuse cleanly so
// higher-level code can fall back. Skipping the full streaming suite.
TEST(BackgroundCommandTaskTest, UnsupportedPlatformReturnsUnimplemented) {
  BackgroundCommandTask task;
  auto status = task.Start("true", ".");
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
}

}  // namespace
}  // namespace yaze::editor

#endif  // YAZE_BACKGROUND_COMMANDS_SUPPORTED
