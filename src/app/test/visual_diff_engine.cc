#include "app/test/visual_diff_engine.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

// Include libpng for PNG encoding/decoding
extern "C" {
#include <png.h>
}

namespace yaze {
namespace test {

namespace {

// PNG read/write helpers
struct PngReadContext {
  const uint8_t* data;
  size_t offset;
  size_t size;
};

void PngReadCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
  PngReadContext* ctx =
      static_cast<PngReadContext*>(png_get_io_ptr(png_ptr));
  if (ctx->offset + length > ctx->size) {
    png_error(png_ptr, "Read past end of PNG data");
    return;
  }
  std::memcpy(data, ctx->data + ctx->offset, length);
  ctx->offset += length;
}

struct PngWriteContext {
  std::vector<uint8_t>* output;
};

void PngWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length) {
  PngWriteContext* ctx =
      static_cast<PngWriteContext*>(png_get_io_ptr(png_ptr));
  ctx->output->insert(ctx->output->end(), data, data + length);
}

void PngFlushCallback(png_structp /*png_ptr*/) {
  // No-op for memory writes
}

}  // namespace

// ============================================================================
// VisualDiffResult
// ============================================================================

std::string VisualDiffResult::Format() const {
  std::ostringstream ss;

  if (!error_message.empty()) {
    ss << "Error: " << error_message << "\n";
    return ss.str();
  }

  ss << "Visual Comparison Result:\n";
  ss << "  Identical: " << (identical ? "Yes" : "No") << "\n";
  ss << "  Passed: " << (passed ? "Yes" : "No") << "\n";
  ss << "  Similarity: " << absl::StrFormat("%.2f%%", similarity * 100) << "\n";
  ss << "  Difference: " << absl::StrFormat("%.2f%%", difference_pct * 100)
     << "\n";
  ss << "  Dimensions: " << width << "x" << height << " (" << total_pixels
     << " pixels)\n";
  ss << "  Differing Pixels: " << differing_pixels << "\n";

  if (!significant_regions.empty()) {
    ss << "  Significant Regions:\n";
    for (const auto& region : significant_regions) {
      ss << "    - (" << region.x << ", " << region.y << ") " << region.width
         << "x" << region.height << " ("
         << absl::StrFormat("%.1f%%", region.local_diff_pct * 100) << " diff)\n";
    }
  }

  if (!diff_description.empty()) {
    ss << "  Description: " << diff_description << "\n";
  }

  if (!diff_image_png.empty()) {
    ss << "  Diff Image: " << diff_image_png.size() << " bytes (PNG)\n";
  }

  return ss.str();
}

std::string VisualDiffResult::ToJson() const {
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"identical\": " << (identical ? "true" : "false") << ",\n";
  ss << "  \"passed\": " << (passed ? "true" : "false") << ",\n";
  ss << "  \"similarity\": " << similarity << ",\n";
  ss << "  \"difference_pct\": " << difference_pct << ",\n";
  ss << "  \"width\": " << width << ",\n";
  ss << "  \"height\": " << height << ",\n";
  ss << "  \"total_pixels\": " << total_pixels << ",\n";
  ss << "  \"differing_pixels\": " << differing_pixels << ",\n";
  ss << "  \"has_diff_image\": " << (!diff_image_png.empty() ? "true" : "false")
     << ",\n";
  ss << "  \"diff_image_size\": " << diff_image_png.size() << ",\n";
  ss << "  \"significant_regions\": [";

  for (size_t i = 0; i < significant_regions.size(); ++i) {
    if (i > 0) ss << ", ";
    const auto& r = significant_regions[i];
    ss << "{\"x\": " << r.x << ", \"y\": " << r.y << ", \"width\": " << r.width
       << ", \"height\": " << r.height << ", \"diff_pct\": " << r.local_diff_pct
       << "}";
  }
  ss << "]";

  if (!error_message.empty()) {
    ss << ",\n  \"error\": \"" << error_message << "\"";
  }

  ss << "\n}";
  return ss.str();
}

// ============================================================================
// VisualDiffEngine
// ============================================================================

VisualDiffEngine::VisualDiffEngine() = default;

VisualDiffEngine::VisualDiffEngine(const VisualDiffConfig& config)
    : config_(config) {}

absl::StatusOr<VisualDiffResult> VisualDiffEngine::ComparePngFiles(
    const std::string& path_a, const std::string& path_b) {
  auto screenshot_a = LoadPng(path_a);
  if (!screenshot_a.ok()) {
    return screenshot_a.status();
  }

  auto screenshot_b = LoadPng(path_b);
  if (!screenshot_b.ok()) {
    return screenshot_b.status();
  }

  return CompareScreenshots(*screenshot_a, *screenshot_b);
}

absl::StatusOr<VisualDiffResult> VisualDiffEngine::ComparePngData(
    const std::vector<uint8_t>& png_a, const std::vector<uint8_t>& png_b) {
  auto screenshot_a = DecodePng(png_a);
  if (!screenshot_a.ok()) {
    return screenshot_a.status();
  }

  auto screenshot_b = DecodePng(png_b);
  if (!screenshot_b.ok()) {
    return screenshot_b.status();
  }

  return CompareScreenshots(*screenshot_a, *screenshot_b);
}

absl::StatusOr<VisualDiffResult> VisualDiffEngine::CompareWithReference(
    const std::vector<uint8_t>& png_data, const std::string& reference_path) {
  auto screenshot = DecodePng(png_data);
  if (!screenshot.ok()) {
    return screenshot.status();
  }

  auto reference = LoadPng(reference_path);
  if (!reference.ok()) {
    return reference.status();
  }

  return CompareScreenshots(*screenshot, *reference);
}

VisualDiffResult VisualDiffEngine::CompareScreenshots(const Screenshot& a,
                                                      const Screenshot& b) {
  return CompareImpl(a, b, ScreenRegion::FullScreen());
}

absl::StatusOr<VisualDiffResult> VisualDiffEngine::CompareRegion(
    const std::vector<uint8_t>& png_a, const std::vector<uint8_t>& png_b,
    const ScreenRegion& region) {
  auto screenshot_a = DecodePng(png_a);
  if (!screenshot_a.ok()) {
    return screenshot_a.status();
  }

  auto screenshot_b = DecodePng(png_b);
  if (!screenshot_b.ok()) {
    return screenshot_b.status();
  }

  return CompareImpl(*screenshot_a, *screenshot_b, region);
}

VisualDiffResult VisualDiffEngine::CompareImpl(const Screenshot& a,
                                               const Screenshot& b,
                                               const ScreenRegion& region) {
  VisualDiffResult result;

  // Validate inputs
  if (!a.IsValid()) {
    result.error_message = "First image is invalid";
    return result;
  }
  if (!b.IsValid()) {
    result.error_message = "Second image is invalid";
    return result;
  }
  if (a.width != b.width || a.height != b.height) {
    result.error_message = absl::StrFormat(
        "Image dimensions don't match: %dx%d vs %dx%d", a.width, a.height,
        b.width, b.height);
    return result;
  }

  result.width = a.width;
  result.height = a.height;

  // Run comparison based on configured algorithm
  switch (config_.algorithm) {
    case VisualDiffConfig::Algorithm::kPixelExact:
      result = ComparePixelExact(a, b, region);
      break;
    case VisualDiffConfig::Algorithm::kSSIM:
      result = CompareSSIM(a, b, region);
      break;
    case VisualDiffConfig::Algorithm::kPerceptualHash:
      // Fall back to pixel exact for now
      result = ComparePixelExact(a, b, region);
      break;
  }

  // Determine pass/fail
  result.passed = result.similarity >= config_.tolerance;

  // Generate diff image if requested and there are differences
  if (config_.generate_diff_image && !result.identical) {
    switch (config_.diff_style) {
      case VisualDiffConfig::DiffStyle::kRedHighlight:
        result.diff_image_png = GenerateRedHighlightDiff(a, b, result);
        break;
      case VisualDiffConfig::DiffStyle::kHeatmap:
        result.diff_image_png = GenerateHeatmapDiff(a, b);
        break;
      case VisualDiffConfig::DiffStyle::kSideBySide:
        result.diff_image_png = GenerateSideBySideDiff(a, b, result);
        break;
      default:
        result.diff_image_png = GenerateRedHighlightDiff(a, b, result);
        break;
    }
  }

  // Find significant regions if there are differences
  if (!result.identical) {
    result.significant_regions =
        FindSignificantRegions(a, b, config_.color_threshold);
    MergeNearbyRegions(result.significant_regions);
  }

  // Generate description
  if (result.identical) {
    result.diff_description = "Images are pixel-perfect identical";
  } else if (result.passed) {
    result.diff_description = absl::StrFormat(
        "Images match within tolerance (%.1f%% similar, %.1f%% threshold)",
        result.similarity * 100, config_.tolerance * 100);
  } else {
    result.diff_description = absl::StrFormat(
        "Images differ significantly (%.1f%% similar, %.1f%% threshold)",
        result.similarity * 100, config_.tolerance * 100);
  }

  return result;
}

VisualDiffResult VisualDiffEngine::ComparePixelExact(const Screenshot& a,
                                                     const Screenshot& b,
                                                     const ScreenRegion& region) {
  VisualDiffResult result;
  result.width = a.width;
  result.height = a.height;

  // Calculate comparison region
  int x1 = region.x;
  int y1 = region.y;
  int x2 = region.IsFullScreen() ? a.width : std::min(region.x + region.width, a.width);
  int y2 = region.IsFullScreen() ? a.height : std::min(region.y + region.height, a.height);

  int total = 0;
  int differing = 0;

  for (int y = y1; y < y2; ++y) {
    for (int x = x1; x < x2; ++x) {
      // Skip ignored regions
      bool ignore = false;
      for (const auto& ir : config_.ignore_regions) {
        if (x >= ir.x && x < ir.x + ir.width && y >= ir.y &&
            y < ir.y + ir.height) {
          ignore = true;
          break;
        }
      }
      if (ignore) continue;

      total++;
      size_t idx = a.GetPixelIndex(x, y);
      const uint8_t* pa = &a.data[idx];
      const uint8_t* pb = &b.data[idx];

      if (!ColorsMatch(pa, pb)) {
        differing++;
      }
    }
  }

  result.total_pixels = total;
  result.differing_pixels = differing;
  result.identical = (differing == 0);
  result.similarity = total > 0 ? 1.0f - (static_cast<float>(differing) / total) : 1.0f;
  result.difference_pct = total > 0 ? (static_cast<float>(differing) / total) : 0.0f;

  return result;
}

VisualDiffResult VisualDiffEngine::CompareSSIM(const Screenshot& a,
                                               const Screenshot& b,
                                               const ScreenRegion& region) {
  VisualDiffResult result = ComparePixelExact(a, b, region);

  // Calculate SSIM for more perceptual comparison
  float ssim = region.IsFullScreen() ? CalculateSSIM(a, b)
                                     : CalculateRegionSSIM(a, b, region);

  // Use SSIM as the similarity metric
  result.similarity = ssim;

  return result;
}

float VisualDiffEngine::CalculateSSIM(const Screenshot& a, const Screenshot& b) {
  return CalculateRegionSSIM(a, b, ScreenRegion::FullScreen());
}

float VisualDiffEngine::CalculateRegionSSIM(const Screenshot& a,
                                            const Screenshot& b,
                                            const ScreenRegion& region) {
  // Simplified SSIM calculation
  // Full SSIM uses sliding windows, but this gives a reasonable approximation

  int x1 = region.x;
  int y1 = region.y;
  int x2 = region.IsFullScreen() ? a.width : std::min(region.x + region.width, a.width);
  int y2 = region.IsFullScreen() ? a.height : std::min(region.y + region.height, a.height);

  double sum_a = 0, sum_b = 0;
  double sum_aa = 0, sum_bb = 0, sum_ab = 0;
  int count = 0;

  for (int y = y1; y < y2; ++y) {
    for (int x = x1; x < x2; ++x) {
      size_t idx = a.GetPixelIndex(x, y);

      // Convert to grayscale luminance for SSIM
      double la = 0.299 * a.data[idx] + 0.587 * a.data[idx + 1] +
                  0.114 * a.data[idx + 2];
      double lb = 0.299 * b.data[idx] + 0.587 * b.data[idx + 1] +
                  0.114 * b.data[idx + 2];

      sum_a += la;
      sum_b += lb;
      sum_aa += la * la;
      sum_bb += lb * lb;
      sum_ab += la * lb;
      count++;
    }
  }

  if (count == 0) return 1.0f;

  double mean_a = sum_a / count;
  double mean_b = sum_b / count;
  double var_a = (sum_aa / count) - (mean_a * mean_a);
  double var_b = (sum_bb / count) - (mean_b * mean_b);
  double cov_ab = (sum_ab / count) - (mean_a * mean_b);

  // SSIM constants
  const double C1 = 6.5025;   // (0.01 * 255)^2
  const double C2 = 58.5225;  // (0.03 * 255)^2

  double numerator = (2 * mean_a * mean_b + C1) * (2 * cov_ab + C2);
  double denominator =
      (mean_a * mean_a + mean_b * mean_b + C1) * (var_a + var_b + C2);

  return static_cast<float>(numerator / denominator);
}

bool VisualDiffEngine::ColorsMatch(const uint8_t* pixel_a,
                                   const uint8_t* pixel_b) const {
  int threshold = config_.color_threshold;
  return std::abs(pixel_a[0] - pixel_b[0]) <= threshold &&
         std::abs(pixel_a[1] - pixel_b[1]) <= threshold &&
         std::abs(pixel_a[2] - pixel_b[2]) <= threshold;
}

float VisualDiffEngine::PixelDifference(const uint8_t* pixel_a,
                                        const uint8_t* pixel_b) const {
  int diff_r = std::abs(pixel_a[0] - pixel_b[0]);
  int diff_g = std::abs(pixel_a[1] - pixel_b[1]);
  int diff_b = std::abs(pixel_a[2] - pixel_b[2]);
  return (diff_r + diff_g + diff_b) / (255.0f * 3.0f);
}

std::vector<uint8_t> VisualDiffEngine::GenerateRedHighlightDiff(
    const Screenshot& a, const Screenshot& b, const VisualDiffResult& result) {
  // Create diff image: base image A with red overlay on differences
  Screenshot diff;
  diff.width = a.width;
  diff.height = a.height;
  diff.data.resize(a.data.size());

  for (int y = 0; y < a.height; ++y) {
    for (int x = 0; x < a.width; ++x) {
      size_t idx = a.GetPixelIndex(x, y);
      const uint8_t* pa = &a.data[idx];
      const uint8_t* pb = &b.data[idx];

      if (ColorsMatch(pa, pb)) {
        // Same: show original (slightly dimmed)
        diff.data[idx + 0] = pa[0] / 2;
        diff.data[idx + 1] = pa[1] / 2;
        diff.data[idx + 2] = pa[2] / 2;
        diff.data[idx + 3] = 255;
      } else {
        // Different: red highlight
        diff.data[idx + 0] = 255;
        diff.data[idx + 1] = 0;
        diff.data[idx + 2] = 0;
        diff.data[idx + 3] = 255;
      }
    }
  }

  auto encoded = EncodePng(diff);
  return encoded.ok() ? *encoded : std::vector<uint8_t>();
}

std::vector<uint8_t> VisualDiffEngine::GenerateHeatmapDiff(const Screenshot& a,
                                                          const Screenshot& b) {
  Screenshot diff;
  diff.width = a.width;
  diff.height = a.height;
  diff.data.resize(a.data.size());

  for (int y = 0; y < a.height; ++y) {
    for (int x = 0; x < a.width; ++x) {
      size_t idx = a.GetPixelIndex(x, y);
      const uint8_t* pa = &a.data[idx];
      const uint8_t* pb = &b.data[idx];

      float diff_amount = PixelDifference(pa, pb);

      // Map to heatmap color (green -> yellow -> red)
      uint8_t r, g, b_val;
      if (diff_amount < 0.5f) {
        // Green to yellow
        float t = diff_amount * 2;
        r = static_cast<uint8_t>(255 * t);
        g = 255;
        b_val = 0;
      } else {
        // Yellow to red
        float t = (diff_amount - 0.5f) * 2;
        r = 255;
        g = static_cast<uint8_t>(255 * (1 - t));
        b_val = 0;
      }

      diff.data[idx + 0] = r;
      diff.data[idx + 1] = g;
      diff.data[idx + 2] = b_val;
      diff.data[idx + 3] = 255;
    }
  }

  auto encoded = EncodePng(diff);
  return encoded.ok() ? *encoded : std::vector<uint8_t>();
}

std::vector<uint8_t> VisualDiffEngine::GenerateSideBySideDiff(
    const Screenshot& a, const Screenshot& b, const VisualDiffResult& result) {
  // Create side-by-side: A | Diff | B
  Screenshot combined;
  combined.width = a.width * 3;
  combined.height = a.height;
  combined.data.resize(combined.width * combined.height * 4);

  for (int y = 0; y < a.height; ++y) {
    for (int x = 0; x < a.width; ++x) {
      size_t src_idx = a.GetPixelIndex(x, y);
      const uint8_t* pa = &a.data[src_idx];
      const uint8_t* pb = &b.data[src_idx];

      // Image A (left)
      size_t dst_a = (y * combined.width + x) * 4;
      combined.data[dst_a + 0] = pa[0];
      combined.data[dst_a + 1] = pa[1];
      combined.data[dst_a + 2] = pa[2];
      combined.data[dst_a + 3] = 255;

      // Diff (center)
      size_t dst_diff = (y * combined.width + x + a.width) * 4;
      if (ColorsMatch(pa, pb)) {
        combined.data[dst_diff + 0] = pa[0] / 2;
        combined.data[dst_diff + 1] = pa[1] / 2;
        combined.data[dst_diff + 2] = pa[2] / 2;
      } else {
        combined.data[dst_diff + 0] = 255;
        combined.data[dst_diff + 1] = 0;
        combined.data[dst_diff + 2] = 0;
      }
      combined.data[dst_diff + 3] = 255;

      // Image B (right)
      size_t dst_b = (y * combined.width + x + a.width * 2) * 4;
      combined.data[dst_b + 0] = pb[0];
      combined.data[dst_b + 1] = pb[1];
      combined.data[dst_b + 2] = pb[2];
      combined.data[dst_b + 3] = 255;
    }
  }

  auto encoded = EncodePng(combined);
  return encoded.ok() ? *encoded : std::vector<uint8_t>();
}

std::vector<VisualDiffResult::DiffRegion> VisualDiffEngine::FindSignificantRegions(
    const Screenshot& a, const Screenshot& b, int threshold) {
  std::vector<VisualDiffResult::DiffRegion> regions;

  // Simple approach: find bounding boxes of contiguous diff regions
  // More sophisticated: use connected component analysis

  // For now, use a grid-based approach
  const int grid_size = 32;  // 32x32 pixel cells

  for (int gy = 0; gy < (a.height + grid_size - 1) / grid_size; ++gy) {
    for (int gx = 0; gx < (a.width + grid_size - 1) / grid_size; ++gx) {
      int x1 = gx * grid_size;
      int y1 = gy * grid_size;
      int x2 = std::min(x1 + grid_size, a.width);
      int y2 = std::min(y1 + grid_size, a.height);

      int diff_count = 0;
      int total = 0;

      for (int y = y1; y < y2; ++y) {
        for (int x = x1; x < x2; ++x) {
          total++;
          size_t idx = a.GetPixelIndex(x, y);
          if (!ColorsMatch(&a.data[idx], &b.data[idx])) {
            diff_count++;
          }
        }
      }

      // If more than 10% of cell is different, report as region
      float local_diff_pct = static_cast<float>(diff_count) / total;
      if (local_diff_pct > 0.1f) {
        VisualDiffResult::DiffRegion region;
        region.x = x1;
        region.y = y1;
        region.width = x2 - x1;
        region.height = y2 - y1;
        region.local_diff_pct = local_diff_pct;
        regions.push_back(region);
      }
    }
  }

  return regions;
}

void VisualDiffEngine::MergeNearbyRegions(
    std::vector<VisualDiffResult::DiffRegion>& regions) {
  if (regions.size() < 2) return;

  // Simple merge: combine overlapping or adjacent regions
  bool merged = true;
  while (merged) {
    merged = false;
    for (size_t i = 0; i < regions.size(); ++i) {
      for (size_t j = i + 1; j < regions.size(); ++j) {
        auto& ri = regions[i];
        auto& rj = regions[j];

        // Check if regions are close enough to merge
        int gap = config_.region_merge_distance;
        bool adjacent =
            (ri.x - gap <= rj.x + rj.width && ri.x + ri.width + gap >= rj.x &&
             ri.y - gap <= rj.y + rj.height && ri.y + ri.height + gap >= rj.y);

        if (adjacent) {
          // Merge into ri
          int new_x = std::min(ri.x, rj.x);
          int new_y = std::min(ri.y, rj.y);
          int new_x2 = std::max(ri.x + ri.width, rj.x + rj.width);
          int new_y2 = std::max(ri.y + ri.height, rj.y + rj.height);

          ri.x = new_x;
          ri.y = new_y;
          ri.width = new_x2 - new_x;
          ri.height = new_y2 - new_y;
          ri.local_diff_pct = std::max(ri.local_diff_pct, rj.local_diff_pct);

          regions.erase(regions.begin() + j);
          merged = true;
          break;
        }
      }
      if (merged) break;
    }
  }
}

absl::StatusOr<std::vector<uint8_t>> VisualDiffEngine::GenerateDiffPng(
    const Screenshot& a, const Screenshot& b) {
  VisualDiffResult result = CompareScreenshots(a, b);
  if (!result.diff_image_png.empty()) {
    return result.diff_image_png;
  }
  return GenerateRedHighlightDiff(a, b, result);
}

absl::Status VisualDiffEngine::SaveDiffImage(const VisualDiffResult& result,
                                             const std::string& output_path) {
  if (result.diff_image_png.empty()) {
    return absl::InvalidArgumentError("No diff image available");
  }

  std::ofstream file(output_path, std::ios::binary);
  if (!file) {
    return absl::InternalError(
        absl::StrCat("Failed to open file for writing: ", output_path));
  }

  file.write(reinterpret_cast<const char*>(result.diff_image_png.data()),
             result.diff_image_png.size());

  return absl::OkStatus();
}

absl::StatusOr<Screenshot> VisualDiffEngine::DecodePng(
    const std::vector<uint8_t>& png_data) {
  if (png_data.size() < 8) {
    return absl::InvalidArgumentError("PNG data too small");
  }

  // Check PNG signature
  if (png_sig_cmp(reinterpret_cast<png_bytep>(
                      const_cast<uint8_t*>(png_data.data())),
                  0, 8) != 0) {
    return absl::InvalidArgumentError("Invalid PNG signature");
  }

  png_structp png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    return absl::InternalError("Failed to create PNG read struct");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    return absl::InternalError("Failed to create PNG info struct");
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    return absl::InternalError("PNG decoding error");
  }

  PngReadContext ctx{png_data.data(), 0, png_data.size()};
  png_set_read_fn(png_ptr, &ctx, PngReadCallback);

  png_read_info(png_ptr, info_ptr);

  png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
  png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
  png_byte color_type = png_get_color_type(png_ptr, info_ptr);
  png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  // Convert to RGBA
  if (bit_depth == 16) png_set_strip_16(png_ptr);
  if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png_ptr);
  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  Screenshot result;
  result.width = static_cast<int>(width);
  result.height = static_cast<int>(height);
  result.data.resize(width * height * 4);

  std::vector<png_bytep> row_pointers(height);
  for (png_uint_32 y = 0; y < height; ++y) {
    row_pointers[y] = result.data.data() + y * width * 4;
  }

  png_read_image(png_ptr, row_pointers.data());
  png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

  return result;
}

absl::StatusOr<std::vector<uint8_t>> VisualDiffEngine::EncodePng(
    const Screenshot& screenshot) {
  if (!screenshot.IsValid()) {
    return absl::InvalidArgumentError("Invalid screenshot");
  }

  png_structp png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    return absl::InternalError("Failed to create PNG write struct");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    return absl::InternalError("Failed to create PNG info struct");
  }

  std::vector<uint8_t> output;
  PngWriteContext ctx{&output};

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return absl::InternalError("PNG encoding error");
  }

  png_set_write_fn(png_ptr, &ctx, PngWriteCallback, PngFlushCallback);

  png_set_IHDR(png_ptr, info_ptr, screenshot.width, screenshot.height, 8,
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr, info_ptr);

  std::vector<png_bytep> row_pointers(screenshot.height);
  for (int y = 0; y < screenshot.height; ++y) {
    row_pointers[y] =
        const_cast<png_bytep>(screenshot.data.data() + y * screenshot.width * 4);
  }

  png_write_image(png_ptr, row_pointers.data());
  png_write_end(png_ptr, nullptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  return output;
}

absl::StatusOr<Screenshot> VisualDiffEngine::LoadPng(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::NotFoundError(absl::StrCat("File not found: ", path));
  }

  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);

  auto result = DecodePng(data);
  if (result.ok()) {
    result->source = path;
  }
  return result;
}

absl::Status VisualDiffEngine::SavePng(const Screenshot& screenshot,
                                       const std::string& path) {
  auto encoded = EncodePng(screenshot);
  if (!encoded.ok()) {
    return encoded.status();
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return absl::InternalError(
        absl::StrCat("Failed to open file for writing: ", path));
  }

  file.write(reinterpret_cast<const char*>(encoded->data()), encoded->size());
  return absl::OkStatus();
}

}  // namespace test
}  // namespace yaze
