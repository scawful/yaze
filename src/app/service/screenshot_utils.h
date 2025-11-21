#ifndef YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_
#define YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_

#ifdef YAZE_WITH_GRPC

#include <optional>
#include <string>

#include "absl/status/statusor.h"

namespace yaze {
namespace test {

struct ScreenshotArtifact {
  std::string file_path;
  int width = 0;
  int height = 0;
  int64_t file_size_bytes = 0;
};

struct CaptureRegion {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Captures the current renderer output into a BMP file.
// If preferred_path is empty, an appropriate path under the system temp
// directory is generated automatically. Returns the resolved artifact metadata
// on success.
absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshot(
    const std::string& preferred_path = "");

// Captures a specific region of the renderer output.
// If region is nullopt, captures the full renderer (same as
// CaptureHarnessScreenshot).
absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshotRegion(
    const std::optional<CaptureRegion>& region,
    const std::string& preferred_path = "");

// Captures the currently active ImGui window.
absl::StatusOr<ScreenshotArtifact> CaptureActiveWindow(
    const std::string& preferred_path = "");

// Captures a specific ImGui window by name.
absl::StatusOr<ScreenshotArtifact> CaptureWindowByName(
    const std::string& window_name, const std::string& preferred_path = "");

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_
