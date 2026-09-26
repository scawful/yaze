#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/emu/emulator.h"
#include "app/emu/internal_emulator_adapter.h"
#include "app/service/emulator_service_impl.h"
#include "unique_temp_path.h"

namespace yaze::net {
namespace {

// GetGameState(include_screenshot) runs on gRPC threads. It must delegate the
// capture to the injected render-thread capturer and never read the renderer
// itself: that raced Emulator::Run's texture update and aborted in Metal.
class EmulatorServiceScreenshotTest : public ::testing::Test {
 protected:
  emu::Emulator emulator_;
  emu::InternalEmulatorAdapter adapter_{&emulator_};
  EmulatorServiceImpl service_{&adapter_};

  grpc::Status GetGameStateWithScreenshot(agent::GameStateResponse* response) {
    grpc::ServerContext context;
    agent::GameStateRequest request;
    request.set_include_screenshot(true);
    return service_.GetGameState(&context, &request, response);
  }
};

TEST_F(EmulatorServiceScreenshotTest, UsesInjectedCapturer) {
  const auto png_path = test::UniqueTempPath("game_state_shot", ".png");
  {
    std::ofstream out(png_path, std::ios::binary);
    out << "fake-png-bytes";
  }

  int calls = 0;
  service_.SetScreenshotCapturer(
      [&]() -> absl::StatusOr<test::ScreenshotArtifact> {
        ++calls;
        test::ScreenshotArtifact artifact;
        artifact.file_path = png_path.string();
        return artifact;
      });

  agent::GameStateResponse response;
  const grpc::Status status = GetGameStateWithScreenshot(&response);

  EXPECT_TRUE(status.ok()) << status.error_message();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(response.screenshot_png(), "fake-png-bytes");

  std::filesystem::remove(png_path);
}

TEST_F(EmulatorServiceScreenshotTest, OmitsScreenshotWithoutCapturer) {
  agent::GameStateResponse response;
  const grpc::Status status = GetGameStateWithScreenshot(&response);

  EXPECT_TRUE(status.ok()) << status.error_message();
  EXPECT_TRUE(response.screenshot_png().empty());
}

TEST_F(EmulatorServiceScreenshotTest, OmitsScreenshotWhenCaptureFails) {
  service_.SetScreenshotCapturer(
      []() -> absl::StatusOr<test::ScreenshotArtifact> {
        return absl::DeadlineExceededError("no frame rendered");
      });

  agent::GameStateResponse response;
  const grpc::Status status = GetGameStateWithScreenshot(&response);

  EXPECT_TRUE(status.ok()) << status.error_message();
  EXPECT_TRUE(response.screenshot_png().empty());
}

}  // namespace
}  // namespace yaze::net
