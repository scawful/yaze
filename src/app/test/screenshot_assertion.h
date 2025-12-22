#ifndef YAZE_APP_TEST_SCREENSHOT_ASSERTION_H
#define YAZE_APP_TEST_SCREENSHOT_ASSERTION_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace test {

/**
 * @brief Region of interest for screenshot comparison.
 */
struct ScreenRegion {
  int x = 0;
  int y = 0;
  int width = 0;   // 0 = full width
  int height = 0;  // 0 = full height

  bool IsFullScreen() const { return width == 0 && height == 0; }

  static ScreenRegion FullScreen() { return {0, 0, 0, 0}; }
  static ScreenRegion At(int x, int y, int w, int h) { return {x, y, w, h}; }
};

/**
 * @brief Result of a screenshot comparison.
 */
struct ComparisonResult {
  bool passed = false;
  float similarity = 0.0f;        // 0.0 to 1.0 (1.0 = identical)
  float difference_percentage = 0.0f;  // Percentage of pixels that differ
  int total_pixels = 0;
  int differing_pixels = 0;

  // Difference visualization
  std::string diff_image_path;  // Path to generated diff image
  std::vector<ScreenRegion> differing_regions;  // Clusters of differences

  // Performance
  std::chrono::milliseconds comparison_time{0};
  std::string error_message;
};

/**
 * @brief Configuration for screenshot comparison.
 */
struct ComparisonConfig {
  float tolerance = 0.95f;  // Minimum similarity to pass (0.95 = 95%)

  // Per-pixel tolerance (for anti-aliasing, etc.)
  int color_threshold = 10;  // Max RGB difference per channel (0-255)

  // Ignore regions (for dynamic content like timers)
  std::vector<ScreenRegion> ignore_regions;

  // Output options
  bool generate_diff_image = true;
  std::string diff_output_dir = "/tmp/yaze_test_diffs";

  // Comparison algorithm
  enum class Algorithm {
    kPixelExact,       // Exact pixel match (with color threshold)
    kPerceptualHash,   // Perceptual hashing (more tolerant)
    kStructuralSim,    // SSIM-like structural comparison
  };
  Algorithm algorithm = Algorithm::kPixelExact;
};

/**
 * @brief Screenshot data container.
 */
struct Screenshot {
  std::vector<uint8_t> data;  // RGBA pixel data
  int width = 0;
  int height = 0;
  std::string source;  // Path or description of source

  bool IsValid() const {
    return !data.empty() && width > 0 && height > 0 &&
           data.size() == static_cast<size_t>(width * height * 4);
  }

  size_t GetPixelIndex(int x, int y) const {
    return static_cast<size_t>((y * width + x) * 4);
  }
};

/**
 * @class ScreenshotAssertion
 * @brief Utilities for screenshot-based testing assertions.
 *
 * Provides tools for capturing, comparing, and asserting screenshot content
 * for visual regression testing and AI-assisted verification.
 *
 * Usage:
 * @code
 *   ScreenshotAssertion assertion;
 *   assertion.SetCaptureCallback(my_capture_func);
 *
 *   // Compare to reference
 *   auto result = assertion.AssertMatchesReference("expected_state.png");
 *   EXPECT_TRUE(result.passed) << result.error_message;
 *
 *   // Compare specific regions
 *   auto result = assertion.AssertRegionMatches(
 *       "canvas_expected.png",
 *       ScreenRegion::At(100, 100, 256, 256));
 *
 *   // Check for visual changes
 *   assertion.CaptureBaseline("before_edit");
 *   // ... make changes ...
 *   auto result = assertion.AssertChanged("before_edit");
 * @endcode
 */
class ScreenshotAssertion {
 public:
  ScreenshotAssertion();
  ~ScreenshotAssertion() = default;

  // Configuration
  void SetConfig(const ComparisonConfig& config) { config_ = config; }
  const ComparisonConfig& GetConfig() const { return config_; }

  // Screenshot capture
  using CaptureCallback =
      std::function<absl::StatusOr<Screenshot>()>;
  void SetCaptureCallback(CaptureCallback callback) {
    capture_callback_ = std::move(callback);
  }

  // --- Core Assertions ---

  /**
   * @brief Assert current screen matches a reference image file.
   */
  absl::StatusOr<ComparisonResult> AssertMatchesReference(
      const std::string& reference_path);

  /**
   * @brief Assert current screen matches another Screenshot object.
   */
  absl::StatusOr<ComparisonResult> AssertMatchesScreenshot(
      const Screenshot& expected);

  /**
   * @brief Assert a specific region matches reference.
   */
  absl::StatusOr<ComparisonResult> AssertRegionMatches(
      const std::string& reference_path,
      const ScreenRegion& region);

  /**
   * @brief Assert screen has changed since baseline.
   */
  absl::StatusOr<ComparisonResult> AssertChanged(
      const std::string& baseline_name);

  /**
   * @brief Assert screen has NOT changed since baseline.
   */
  absl::StatusOr<ComparisonResult> AssertUnchanged(
      const std::string& baseline_name);

  // --- Baseline Management ---

  /**
   * @brief Capture and store a baseline screenshot.
   */
  absl::Status CaptureBaseline(const std::string& name);

  /**
   * @brief Save current screen as a new reference image.
   */
  absl::Status SaveAsReference(const std::string& path);

  /**
   * @brief Load a reference image from file.
   */
  absl::StatusOr<Screenshot> LoadReference(const std::string& path);

  /**
   * @brief Get a previously captured baseline.
   */
  absl::StatusOr<Screenshot> GetBaseline(const std::string& name) const;

  /**
   * @brief Clear all captured baselines.
   */
  void ClearBaselines() { baselines_.clear(); }

  // --- Comparison Utilities ---

  /**
   * @brief Compare two screenshots.
   */
  ComparisonResult Compare(const Screenshot& actual,
                           const Screenshot& expected);

  /**
   * @brief Compare specific regions of two screenshots.
   */
  ComparisonResult CompareRegion(const Screenshot& actual,
                                 const Screenshot& expected,
                                 const ScreenRegion& region);

  /**
   * @brief Generate a visual diff image.
   */
  absl::StatusOr<std::string> GenerateDiffImage(
      const Screenshot& actual,
      const Screenshot& expected,
      const std::string& output_path);

  // --- Pixel-Level Assertions ---

  /**
   * @brief Assert pixel at (x, y) has expected color.
   * @param x X coordinate
   * @param y Y coordinate
   * @param r Expected red (0-255)
   * @param g Expected green (0-255)
   * @param b Expected blue (0-255)
   * @param tolerance Per-channel tolerance
   */
  absl::StatusOr<bool> AssertPixelColor(
      int x, int y, uint8_t r, uint8_t g, uint8_t b, int tolerance = 10);

  /**
   * @brief Assert region contains a specific color.
   */
  absl::StatusOr<bool> AssertRegionContainsColor(
      const ScreenRegion& region,
      uint8_t r, uint8_t g, uint8_t b,
      float min_coverage = 0.1f);  // At least 10% of pixels

  /**
   * @brief Assert region does NOT contain a specific color.
   */
  absl::StatusOr<bool> AssertRegionExcludesColor(
      const ScreenRegion& region,
      uint8_t r, uint8_t g, uint8_t b,
      int tolerance = 10);

  // --- Convenience Methods ---

  /**
   * @brief Capture current screen and return it.
   */
  absl::StatusOr<Screenshot> CaptureScreen();

  /**
   * @brief Save screenshot to file (PNG format).
   */
  static absl::Status SaveScreenshot(const Screenshot& screenshot,
                                     const std::string& path);

  /**
   * @brief Load screenshot from file.
   */
  static absl::StatusOr<Screenshot> LoadScreenshot(const std::string& path);

 private:
  ComparisonConfig config_;
  CaptureCallback capture_callback_;
  std::unordered_map<std::string, Screenshot> baselines_;

  // Comparison algorithms
  ComparisonResult ComparePixelExact(const Screenshot& actual,
                                     const Screenshot& expected,
                                     const ScreenRegion& region);
  ComparisonResult ComparePerceptualHash(const Screenshot& actual,
                                         const Screenshot& expected);
  ComparisonResult CompareStructural(const Screenshot& actual,
                                     const Screenshot& expected);

  // Helper methods
  bool ColorsMatch(uint8_t r1, uint8_t g1, uint8_t b1,
                   uint8_t r2, uint8_t g2, uint8_t b2,
                   int threshold) const;
  std::vector<ScreenRegion> FindDifferingRegions(
      const Screenshot& actual,
      const Screenshot& expected,
      int threshold);
};

// --- Test Macros ---

/**
 * @brief Assert screenshot matches reference (for use in tests).
 *
 * Usage:
 * @code
 *   YAZE_ASSERT_SCREENSHOT_MATCHES(assertion, "expected.png");
 * @endcode
 */
#define YAZE_ASSERT_SCREENSHOT_MATCHES(assertion, reference_path)   \
  do {                                                              \
    auto result = (assertion).AssertMatchesReference(reference_path); \
    ASSERT_TRUE(result.ok()) << result.status().message();          \
    EXPECT_TRUE(result->passed)                                     \
        << "Screenshot mismatch: " << result->difference_percentage \
        << "% different, expected <" << (1.0f - (assertion).GetConfig().tolerance) * 100 \
        << "%\nDiff image: " << result->diff_image_path;            \
  } while (0)

/**
 * @brief Assert screenshot has changed from baseline.
 */
#define YAZE_ASSERT_SCREENSHOT_CHANGED(assertion, baseline_name) \
  do {                                                           \
    auto result = (assertion).AssertChanged(baseline_name);      \
    ASSERT_TRUE(result.ok()) << result.status().message();       \
    EXPECT_TRUE(result->passed)                                  \
        << "Expected screenshot to change but similarity was "   \
        << result->similarity * 100 << "%";                      \
  } while (0)

/**
 * @brief Assert screenshot has NOT changed from baseline.
 */
#define YAZE_ASSERT_SCREENSHOT_UNCHANGED(assertion, baseline_name) \
  do {                                                             \
    auto result = (assertion).AssertUnchanged(baseline_name);      \
    ASSERT_TRUE(result.ok()) << result.status().message();         \
    EXPECT_TRUE(result->passed)                                    \
        << "Expected screenshot unchanged but "                    \
        << result->difference_percentage << "% different\n"        \
        << "Diff image: " << result->diff_image_path;              \
  } while (0)

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_SCREENSHOT_ASSERTION_H
