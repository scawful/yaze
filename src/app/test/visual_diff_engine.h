#ifndef YAZE_APP_TEST_VISUAL_DIFF_ENGINE_H_
#define YAZE_APP_TEST_VISUAL_DIFF_ENGINE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/test/screenshot_assertion.h"

namespace yaze {
namespace test {

/**
 * @brief Result of visual comparison with diff image.
 *
 * Designed for gRPC serialization and MCP tool output.
 */
struct VisualDiffResult {
  bool identical = false;       // True if images are pixel-perfect identical
  bool passed = false;          // True if similarity >= tolerance
  float similarity = 0.0f;      // 0.0 to 1.0 (1.0 = identical)
  float difference_pct = 0.0f;  // Percentage of differing pixels

  // Image dimensions
  int width = 0;
  int height = 0;
  int total_pixels = 0;
  int differing_pixels = 0;

  // Diff visualization
  std::vector<uint8_t> diff_image_png;  // PNG-encoded diff image
  std::string diff_description;         // Human-readable description

  // Regions of interest
  struct DiffRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float local_diff_pct = 0.0f;  // Diff percentage in this region
  };
  std::vector<DiffRegion> significant_regions;

  // Error info
  std::string error_message;

  /**
   * @brief Format result for human-readable output.
   */
  std::string Format() const;

  /**
   * @brief Serialize to JSON for MCP tool output.
   */
  std::string ToJson() const;
};

/**
 * @brief Configuration for visual diff engine.
 */
struct VisualDiffConfig {
  // Similarity threshold (0.0 to 1.0)
  float tolerance = 0.95f;  // 95% similarity = pass

  // Per-pixel color tolerance (RGB channels)
  int color_threshold = 10;  // Max RGB difference per channel (0-255)

  // Diff image generation
  bool generate_diff_image = true;
  enum class DiffStyle {
    kRedHighlight,    // Red overlay on different pixels
    kSideBySide,      // A | diff | B side by side
    kHeatmap,         // Gradient showing degree of difference
    kBlinkComposite,  // Alternating frames (for animated output)
  };
  DiffStyle diff_style = DiffStyle::kRedHighlight;

  // Comparison algorithm
  enum class Algorithm {
    kPixelExact,       // Exact pixel comparison
    kSSIM,             // Structural Similarity Index
    kPerceptualHash,   // pHash for similarity
  };
  Algorithm algorithm = Algorithm::kPixelExact;

  // Region detection
  int region_merge_distance = 16;  // Merge nearby diff regions (pixels)
  int min_region_size = 4;         // Minimum region size to report (pixels)

  // Ignore regions (for dynamic content)
  std::vector<ScreenRegion> ignore_regions;
};

/**
 * @class VisualDiffEngine
 * @brief High-level visual comparison engine for gRPC/MCP integration.
 *
 * Provides PNG-to-PNG comparison with structured results suitable for:
 * - gRPC VisualService
 * - MCP visual_compare tool
 * - Visual regression testing
 * - AI-assisted debugging workflows
 *
 * Usage:
 * @code
 *   VisualDiffEngine engine;
 *
 *   // Compare two PNG files
 *   auto result = engine.ComparePngFiles("before.png", "after.png");
 *   if (result.ok() && result->passed) {
 *     LOG(INFO) << "Images match with " << result->similarity * 100 << "% similarity";
 *   }
 *
 *   // Compare raw PNG data
 *   auto result = engine.ComparePngData(png_a, png_b);
 *
 *   // Compare with reference (regression test)
 *   auto result = engine.CompareWithReference(current_png, "reference/expected.png");
 * @endcode
 */
class VisualDiffEngine {
 public:
  VisualDiffEngine();
  explicit VisualDiffEngine(const VisualDiffConfig& config);
  ~VisualDiffEngine() = default;

  // Configuration
  void SetConfig(const VisualDiffConfig& config) { config_ = config; }
  const VisualDiffConfig& GetConfig() const { return config_; }
  void SetTolerance(float tolerance) { config_.tolerance = tolerance; }

  // --- Core Comparison Methods ---

  /**
   * @brief Compare two PNG files.
   *
   * @param path_a Path to first PNG image
   * @param path_b Path to second PNG image
   * @return VisualDiffResult with comparison results
   */
  absl::StatusOr<VisualDiffResult> ComparePngFiles(const std::string& path_a,
                                                   const std::string& path_b);

  /**
   * @brief Compare two PNG images from raw data.
   *
   * @param png_a First PNG image data (raw bytes)
   * @param png_b Second PNG image data (raw bytes)
   * @return VisualDiffResult with comparison results
   */
  absl::StatusOr<VisualDiffResult> ComparePngData(
      const std::vector<uint8_t>& png_a, const std::vector<uint8_t>& png_b);

  /**
   * @brief Compare PNG data against a reference file.
   *
   * @param png_data Current PNG image data
   * @param reference_path Path to reference PNG file
   * @return VisualDiffResult with comparison results
   */
  absl::StatusOr<VisualDiffResult> CompareWithReference(
      const std::vector<uint8_t>& png_data, const std::string& reference_path);

  /**
   * @brief Compare two Screenshot objects directly.
   *
   * @param a First screenshot
   * @param b Second screenshot
   * @return VisualDiffResult with comparison results
   */
  VisualDiffResult CompareScreenshots(const Screenshot& a, const Screenshot& b);

  // --- Region Comparison ---

  /**
   * @brief Compare a specific region of two images.
   *
   * @param png_a First PNG image data
   * @param png_b Second PNG image data
   * @param region Region to compare
   * @return VisualDiffResult for the specified region
   */
  absl::StatusOr<VisualDiffResult> CompareRegion(
      const std::vector<uint8_t>& png_a, const std::vector<uint8_t>& png_b,
      const ScreenRegion& region);

  // --- Diff Image Generation ---

  /**
   * @brief Generate a diff image highlighting differences.
   *
   * @param a First screenshot
   * @param b Second screenshot
   * @return PNG-encoded diff image
   */
  absl::StatusOr<std::vector<uint8_t>> GenerateDiffPng(const Screenshot& a,
                                                       const Screenshot& b);

  /**
   * @brief Save diff image to file.
   *
   * @param result Comparison result containing diff image
   * @param output_path Path to save the diff PNG
   * @return Status indicating success or failure
   */
  absl::Status SaveDiffImage(const VisualDiffResult& result,
                             const std::string& output_path);

  // --- Utility Methods ---

  /**
   * @brief Decode PNG data to Screenshot.
   *
   * @param png_data Raw PNG bytes
   * @return Screenshot with decoded RGBA data
   */
  static absl::StatusOr<Screenshot> DecodePng(
      const std::vector<uint8_t>& png_data);

  /**
   * @brief Encode Screenshot to PNG.
   *
   * @param screenshot Screenshot to encode
   * @return PNG-encoded bytes
   */
  static absl::StatusOr<std::vector<uint8_t>> EncodePng(
      const Screenshot& screenshot);

  /**
   * @brief Load PNG from file.
   *
   * @param path Path to PNG file
   * @return Screenshot with loaded data
   */
  static absl::StatusOr<Screenshot> LoadPng(const std::string& path);

  /**
   * @brief Save screenshot to PNG file.
   *
   * @param screenshot Screenshot to save
   * @param path Output path
   * @return Status indicating success or failure
   */
  static absl::Status SavePng(const Screenshot& screenshot,
                              const std::string& path);

  // --- SSIM Calculation ---

  /**
   * @brief Calculate Structural Similarity Index.
   *
   * @param a First screenshot
   * @param b Second screenshot
   * @return SSIM value (0.0 to 1.0)
   */
  static float CalculateSSIM(const Screenshot& a, const Screenshot& b);

  /**
   * @brief Calculate SSIM for a specific region.
   *
   * @param a First screenshot
   * @param b Second screenshot
   * @param region Region to analyze
   * @return SSIM value for the region
   */
  static float CalculateRegionSSIM(const Screenshot& a, const Screenshot& b,
                                   const ScreenRegion& region);

 private:
  VisualDiffConfig config_;

  // Core comparison implementation
  VisualDiffResult CompareImpl(const Screenshot& a, const Screenshot& b,
                               const ScreenRegion& region);

  // Pixel-exact comparison
  VisualDiffResult ComparePixelExact(const Screenshot& a, const Screenshot& b,
                                     const ScreenRegion& region);

  // SSIM-based comparison
  VisualDiffResult CompareSSIM(const Screenshot& a, const Screenshot& b,
                               const ScreenRegion& region);

  // Diff image generation
  std::vector<uint8_t> GenerateRedHighlightDiff(const Screenshot& a,
                                                const Screenshot& b,
                                                const VisualDiffResult& result);
  std::vector<uint8_t> GenerateHeatmapDiff(const Screenshot& a,
                                           const Screenshot& b);
  std::vector<uint8_t> GenerateSideBySideDiff(const Screenshot& a,
                                              const Screenshot& b,
                                              const VisualDiffResult& result);

  // Region detection
  std::vector<VisualDiffResult::DiffRegion> FindSignificantRegions(
      const Screenshot& a, const Screenshot& b, int threshold);
  void MergeNearbyRegions(std::vector<VisualDiffResult::DiffRegion>& regions);

  // Helpers
  bool ColorsMatch(const uint8_t* pixel_a, const uint8_t* pixel_b) const;
  float PixelDifference(const uint8_t* pixel_a, const uint8_t* pixel_b) const;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_VISUAL_DIFF_ENGINE_H_
