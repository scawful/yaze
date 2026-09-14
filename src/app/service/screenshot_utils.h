#ifndef YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_
#define YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_

namespace yaze::test {
enum class ScreenshotFormat { kAuto, kPng, kBmp };
}  // namespace yaze::test

#ifdef YAZE_WITH_GRPC

#include <cstdint>
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

// Validates format/path without creating files. Auto infers PNG/BMP from a
// case-insensitive extension; an empty path/extension defaults to BMP.
// Explicit formats reject mismatched extensions. Missing PNG support returns
// Unimplemented, never a BMP file masquerading as PNG.
absl::StatusOr<ScreenshotFormat> ResolveScreenshotFormat(
    const std::string& path, ScreenshotFormat format = ScreenshotFormat::kAuto);

// Captures the main renderer output before Present. Empty preferred_path uses
// the application's screenshots directory. A missing extension is appended
// for the resolved format. Legacy no-argument calls continue to produce BMP.
absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshot(
    const std::string& preferred_path = "", bool reveal_to_user = true,
    ScreenshotFormat format = ScreenshotFormat::kAuto);

// Captures a specific region of the renderer output.
// Coordinates are framebuffer pixels, clipped by intersection with its bounds.
// If region is nullopt, captures the full renderer.
absl::StatusOr<ScreenshotArtifact> CaptureHarnessScreenshotRegion(
    const std::optional<CaptureRegion>& region,
    const std::string& preferred_path = "", bool reveal_to_user = true,
    ScreenshotFormat format = ScreenshotFormat::kAuto);

// Captures the currently active ImGui window.
absl::StatusOr<ScreenshotArtifact> CaptureActiveWindow(
    const std::string& preferred_path = "", bool reveal_to_user = true,
    ScreenshotFormat format = ScreenshotFormat::kAuto);

// Captures a visible main-viewport ImGui window by name. Missing, hidden,
// collapsed, or detached windows fail rather than returning a full screenshot.
absl::StatusOr<ScreenshotArtifact> CaptureWindowByName(
    const std::string& window_name, const std::string& preferred_path = "",
    bool reveal_to_user = true,
    ScreenshotFormat format = ScreenshotFormat::kAuto);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_SCREENSHOT_UTILS_H_
