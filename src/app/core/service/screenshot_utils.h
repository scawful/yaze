#ifndef YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_
#define YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_

#ifdef YAZE_WITH_GRPC

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

// Captures the current renderer output into a BMP file.
// If preferred_path is empty, an appropriate path under the system temp
// directory is generated automatically. Returns the resolved artifact metadata
// on success.
absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshot(
    const std::string& preferred_path = "");

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_
