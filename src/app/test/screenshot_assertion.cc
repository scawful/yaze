#include "app/test/screenshot_assertion.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_cat.h"
#include "util/log.h"

namespace yaze {
namespace test {

ScreenshotAssertion::ScreenshotAssertion() = default;

absl::StatusOr<ComparisonResult> ScreenshotAssertion::AssertMatchesReference(
    const std::string& reference_path) {
  auto current = CaptureScreen();
  if (!current.ok()) {
    return current.status();
  }

  auto expected = LoadReference(reference_path);
  if (!expected.ok()) {
    return expected.status();
  }

  auto result = Compare(*current, *expected);
  result.passed = result.similarity >= config_.tolerance;

  return result;
}

absl::StatusOr<ComparisonResult> ScreenshotAssertion::AssertMatchesScreenshot(
    const Screenshot& expected) {
  auto current = CaptureScreen();
  if (!current.ok()) {
    return current.status();
  }

  auto result = Compare(*current, expected);
  result.passed = result.similarity >= config_.tolerance;

  return result;
}

absl::StatusOr<ComparisonResult> ScreenshotAssertion::AssertRegionMatches(
    const std::string& reference_path, const ScreenRegion& region) {
  auto current = CaptureScreen();
  if (!current.ok()) {
    return current.status();
  }

  auto expected = LoadReference(reference_path);
  if (!expected.ok()) {
    return expected.status();
  }

  auto result = CompareRegion(*current, *expected, region);
  result.passed = result.similarity >= config_.tolerance;

  return result;
}

absl::StatusOr<ComparisonResult> ScreenshotAssertion::AssertChanged(
    const std::string& baseline_name) {
  auto current = CaptureScreen();
  if (!current.ok()) {
    return current.status();
  }

  auto baseline = GetBaseline(baseline_name);
  if (!baseline.ok()) {
    return baseline.status();
  }

  auto result = Compare(*current, *baseline);
  // For "changed" assertion, we want LOW similarity
  result.passed = result.similarity < config_.tolerance;

  return result;
}

absl::StatusOr<ComparisonResult> ScreenshotAssertion::AssertUnchanged(
    const std::string& baseline_name) {
  auto current = CaptureScreen();
  if (!current.ok()) {
    return current.status();
  }

  auto baseline = GetBaseline(baseline_name);
  if (!baseline.ok()) {
    return baseline.status();
  }

  auto result = Compare(*current, *baseline);
  result.passed = result.similarity >= config_.tolerance;

  return result;
}

absl::Status ScreenshotAssertion::CaptureBaseline(const std::string& name) {
  auto screenshot = CaptureScreen();
  if (!screenshot.ok()) {
    return screenshot.status();
  }

  baselines_[name] = std::move(*screenshot);
  LOG_DEBUG("ScreenshotAssertion", "Baseline '%s' captured (%dx%d)",
            name.c_str(), baselines_[name].width, baselines_[name].height);

  return absl::OkStatus();
}

absl::Status ScreenshotAssertion::SaveAsReference(const std::string& path) {
  auto screenshot = CaptureScreen();
  if (!screenshot.ok()) {
    return screenshot.status();
  }

  return SaveScreenshot(*screenshot, path);
}

absl::StatusOr<Screenshot> ScreenshotAssertion::LoadReference(
    const std::string& path) {
  return LoadScreenshot(path);
}

absl::StatusOr<Screenshot> ScreenshotAssertion::GetBaseline(
    const std::string& name) const {
  auto it = baselines_.find(name);
  if (it == baselines_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Baseline not found: ", name));
  }
  return it->second;
}

ComparisonResult ScreenshotAssertion::Compare(const Screenshot& actual,
                                               const Screenshot& expected) {
  return CompareRegion(actual, expected, ScreenRegion::FullScreen());
}

ComparisonResult ScreenshotAssertion::CompareRegion(
    const Screenshot& actual, const Screenshot& expected,
    const ScreenRegion& region) {
  auto start = std::chrono::steady_clock::now();

  ComparisonResult result;

  // Validate screenshots
  if (!actual.IsValid() || !expected.IsValid()) {
    result.error_message = "Invalid screenshot data";
    return result;
  }

  // Use appropriate algorithm
  switch (config_.algorithm) {
    case ComparisonConfig::Algorithm::kPixelExact:
      result = ComparePixelExact(actual, expected, region);
      break;
    case ComparisonConfig::Algorithm::kPerceptualHash:
      result = ComparePerceptualHash(actual, expected);
      break;
    case ComparisonConfig::Algorithm::kStructuralSim:
      result = CompareStructural(actual, expected);
      break;
  }

  auto end = std::chrono::steady_clock::now();
  result.comparison_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  // Generate diff image if requested and there are differences
  if (config_.generate_diff_image && result.differing_pixels > 0) {
    std::string diff_path = absl::StrCat(
        config_.diff_output_dir, "/diff_",
        std::chrono::system_clock::now().time_since_epoch().count(), ".png");
    auto gen_result = GenerateDiffImage(actual, expected, diff_path);
    if (gen_result.ok()) {
      result.diff_image_path = *gen_result;
    }
  }

  return result;
}

ComparisonResult ScreenshotAssertion::ComparePixelExact(
    const Screenshot& actual, const Screenshot& expected,
    const ScreenRegion& region) {
  ComparisonResult result;

  // Determine comparison bounds
  int start_x = region.x;
  int start_y = region.y;
  int end_x = region.width > 0 ? region.x + region.width
                               : std::min(actual.width, expected.width);
  int end_y = region.height > 0 ? region.y + region.height
                                : std::min(actual.height, expected.height);

  // Clamp to valid range
  start_x = std::max(0, start_x);
  start_y = std::max(0, start_y);
  end_x = std::min(end_x, std::min(actual.width, expected.width));
  end_y = std::min(end_y, std::min(actual.height, expected.height));

  int total_pixels = 0;
  int matching_pixels = 0;

  for (int y = start_y; y < end_y; ++y) {
    for (int x = start_x; x < end_x; ++x) {
      // Check if in ignore region
      bool ignored = false;
      for (const auto& ignore : config_.ignore_regions) {
        if (x >= ignore.x && x < ignore.x + ignore.width &&
            y >= ignore.y && y < ignore.y + ignore.height) {
          ignored = true;
          break;
        }
      }

      if (ignored) continue;

      total_pixels++;

      size_t actual_idx = actual.GetPixelIndex(x, y);
      size_t expected_idx = expected.GetPixelIndex(x, y);

      if (actual_idx + 3 < actual.data.size() &&
          expected_idx + 3 < expected.data.size()) {
        bool match = ColorsMatch(
            actual.data[actual_idx], actual.data[actual_idx + 1],
            actual.data[actual_idx + 2], expected.data[expected_idx],
            expected.data[expected_idx + 1], expected.data[expected_idx + 2],
            config_.color_threshold);

        if (match) {
          matching_pixels++;
        }
      }
    }
  }

  result.total_pixels = total_pixels;
  result.differing_pixels = total_pixels - matching_pixels;
  result.similarity = total_pixels > 0
                          ? static_cast<float>(matching_pixels) / total_pixels
                          : 0.0f;
  result.difference_percentage =
      total_pixels > 0
          ? (static_cast<float>(result.differing_pixels) / total_pixels) * 100
          : 0.0f;

  return result;
}

ComparisonResult ScreenshotAssertion::ComparePerceptualHash(
    const Screenshot& actual, const Screenshot& expected) {
  // Simplified perceptual hash comparison
  // TODO: Implement proper pHash algorithm
  ComparisonResult result;
  result.error_message = "Perceptual hash not yet implemented";
  return result;
}

ComparisonResult ScreenshotAssertion::CompareStructural(
    const Screenshot& actual, const Screenshot& expected) {
  // Simplified SSIM-like comparison
  // TODO: Implement proper SSIM algorithm
  ComparisonResult result;
  result.error_message = "Structural similarity not yet implemented";
  return result;
}

absl::StatusOr<std::string> ScreenshotAssertion::GenerateDiffImage(
    const Screenshot& actual, const Screenshot& expected,
    const std::string& output_path) {
  // Create a diff image highlighting differences
  Screenshot diff;
  diff.width = std::min(actual.width, expected.width);
  diff.height = std::min(actual.height, expected.height);
  diff.data.resize(diff.width * diff.height * 4);

  for (int y = 0; y < diff.height; ++y) {
    for (int x = 0; x < diff.width; ++x) {
      size_t actual_idx = actual.GetPixelIndex(x, y);
      size_t expected_idx = expected.GetPixelIndex(x, y);
      size_t diff_idx = diff.GetPixelIndex(x, y);

      if (actual_idx + 3 < actual.data.size() &&
          expected_idx + 3 < expected.data.size()) {
        bool match = ColorsMatch(
            actual.data[actual_idx], actual.data[actual_idx + 1],
            actual.data[actual_idx + 2], expected.data[expected_idx],
            expected.data[expected_idx + 1], expected.data[expected_idx + 2],
            config_.color_threshold);

        if (match) {
          // Matching pixels: dimmed grayscale
          uint8_t gray = static_cast<uint8_t>(
              (actual.data[actual_idx] + actual.data[actual_idx + 1] +
               actual.data[actual_idx + 2]) / 3 * 0.3);
          diff.data[diff_idx] = gray;
          diff.data[diff_idx + 1] = gray;
          diff.data[diff_idx + 2] = gray;
          diff.data[diff_idx + 3] = 255;
        } else {
          // Different pixels: bright red
          diff.data[diff_idx] = 255;
          diff.data[diff_idx + 1] = 0;
          diff.data[diff_idx + 2] = 0;
          diff.data[diff_idx + 3] = 255;
        }
      }
    }
  }

  auto status = SaveScreenshot(diff, output_path);
  if (!status.ok()) {
    return status;
  }

  return output_path;
}

absl::StatusOr<bool> ScreenshotAssertion::AssertPixelColor(
    int x, int y, uint8_t r, uint8_t g, uint8_t b, int tolerance) {
  auto screenshot = CaptureScreen();
  if (!screenshot.ok()) {
    return screenshot.status();
  }

  if (x < 0 || x >= screenshot->width || y < 0 || y >= screenshot->height) {
    return absl::OutOfRangeError("Pixel coordinates out of bounds");
  }

  size_t idx = screenshot->GetPixelIndex(x, y);
  return ColorsMatch(screenshot->data[idx], screenshot->data[idx + 1],
                     screenshot->data[idx + 2], r, g, b, tolerance);
}

absl::StatusOr<bool> ScreenshotAssertion::AssertRegionContainsColor(
    const ScreenRegion& region, uint8_t r, uint8_t g, uint8_t b,
    float min_coverage) {
  auto screenshot = CaptureScreen();
  if (!screenshot.ok()) {
    return screenshot.status();
  }

  int matching = 0;
  int total = 0;

  int end_x = region.width > 0 ? region.x + region.width : screenshot->width;
  int end_y = region.height > 0 ? region.y + region.height : screenshot->height;

  for (int y = region.y; y < end_y && y < screenshot->height; ++y) {
    for (int x = region.x; x < end_x && x < screenshot->width; ++x) {
      total++;
      size_t idx = screenshot->GetPixelIndex(x, y);
      if (ColorsMatch(screenshot->data[idx], screenshot->data[idx + 1],
                      screenshot->data[idx + 2], r, g, b,
                      config_.color_threshold)) {
        matching++;
      }
    }
  }

  float coverage = total > 0 ? static_cast<float>(matching) / total : 0.0f;
  return coverage >= min_coverage;
}

absl::StatusOr<bool> ScreenshotAssertion::AssertRegionExcludesColor(
    const ScreenRegion& region, uint8_t r, uint8_t g, uint8_t b,
    int tolerance) {
  auto result = AssertRegionContainsColor(region, r, g, b, 0.001f);
  if (!result.ok()) {
    return result.status();
  }
  return !*result;  // Invert: true if color NOT found
}

absl::StatusOr<Screenshot> ScreenshotAssertion::CaptureScreen() {
  if (!capture_callback_) {
    return absl::FailedPreconditionError("Capture callback not set");
  }
  return capture_callback_();
}

absl::Status ScreenshotAssertion::SaveScreenshot(const Screenshot& screenshot,
                                                  const std::string& path) {
  // Create directory if needed
  std::filesystem::path filepath(path);
  std::filesystem::create_directories(filepath.parent_path());

  // TODO: Implement proper PNG encoding
  // For now, save as raw RGBA
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return absl::UnavailableError(
        absl::StrCat("Cannot open file for writing: ", path));
  }

  // Write simple header (width, height, then RGBA data)
  file.write(reinterpret_cast<const char*>(&screenshot.width), sizeof(int));
  file.write(reinterpret_cast<const char*>(&screenshot.height), sizeof(int));
  file.write(reinterpret_cast<const char*>(screenshot.data.data()),
             screenshot.data.size());

  LOG_DEBUG("ScreenshotAssertion", "Screenshot saved: %s (%dx%d)",
            path.c_str(), screenshot.width, screenshot.height);

  return absl::OkStatus();
}

absl::StatusOr<Screenshot> ScreenshotAssertion::LoadScreenshot(
    const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::NotFoundError(absl::StrCat("Cannot open file: ", path));
  }

  Screenshot screenshot;
  screenshot.source = path;

  file.read(reinterpret_cast<char*>(&screenshot.width), sizeof(int));
  file.read(reinterpret_cast<char*>(&screenshot.height), sizeof(int));

  size_t data_size = screenshot.width * screenshot.height * 4;
  screenshot.data.resize(data_size);
  file.read(reinterpret_cast<char*>(screenshot.data.data()), data_size);

  return screenshot;
}

bool ScreenshotAssertion::ColorsMatch(uint8_t r1, uint8_t g1, uint8_t b1,
                                       uint8_t r2, uint8_t g2, uint8_t b2,
                                       int threshold) const {
  return std::abs(r1 - r2) <= threshold && std::abs(g1 - g2) <= threshold &&
         std::abs(b1 - b2) <= threshold;
}

std::vector<ScreenRegion> ScreenshotAssertion::FindDifferingRegions(
    const Screenshot& actual, const Screenshot& expected, int threshold) {
  // TODO: Implement region clustering for difference visualization
  return {};
}

}  // namespace test
}  // namespace yaze
