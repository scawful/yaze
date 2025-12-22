/**
 * @file visual_analysis_tool.h
 * @brief Visual analysis tools for AI agents
 *
 * Provides tools for:
 * - Tile pattern recognition and similarity matching
 * - Spritesheet analysis for unused regions
 * - Palette usage statistics
 * - Tile frequency histograms
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_VISUAL_ANALYSIS_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_VISUAL_ANALYSIS_TOOL_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief Result of a tile similarity comparison
 */
struct TileSimilarityMatch {
  int tile_id;
  double similarity_score;  // 0.0 - 100.0
  int sheet_index;
  int x_position;  // X position in sheet (pixels)
  int y_position;  // Y position in sheet (pixels)
};

/**
 * @brief Represents a contiguous unused region in a spritesheet
 */
struct UnusedRegion {
  int sheet_index;
  int x;
  int y;
  int width;
  int height;
  int tile_count;  // Number of 8x8 tiles in region
};

/**
 * @brief Palette usage statistics
 */
struct PaletteUsageStats {
  int palette_index;
  int usage_count;
  double usage_percentage;
  std::vector<int> used_by_maps;  // Map IDs using this palette
};

/**
 * @brief Tile usage frequency entry
 */
struct TileUsageEntry {
  int tile_id;
  int usage_count;
  double usage_percentage;
  std::vector<int> locations;  // Map/room IDs where used
};

/**
 * @brief Base class for visual analysis tools
 *
 * Provides common functionality for tile and graphics analysis:
 * - Pixel difference computation
 * - Structural similarity metrics
 * - Tile extraction from bitmaps
 * - Empty region detection
 */
class VisualAnalysisBase : public resources::CommandHandler {
 public:
  // Constants for SNES graphics (public for testing)
  static constexpr int kTileWidth = 8;
  static constexpr int kTileHeight = 8;
  static constexpr int kTilePixels = kTileWidth * kTileHeight;  // 64
  static constexpr int kSheetWidth = 128;   // pixels
  static constexpr int kSheetHeight = 32;   // pixels per sheet row
  static constexpr int kTilesPerRow = kSheetWidth / kTileWidth;  // 16
  static constexpr int kMaxSheets = 223;

 protected:
  /**
   * @brief Compute simple pixel difference (Mean Absolute Error)
   * @param tile_a First tile data (64 bytes for 8x8)
   * @param tile_b Second tile data (64 bytes for 8x8)
   * @return Similarity score 0.0-100.0 (100 = identical)
   */
  double ComputePixelDifference(const std::vector<uint8_t>& tile_a,
                                const std::vector<uint8_t>& tile_b) const;

  /**
   * @brief Compute structural similarity index (SSIM-like)
   * @param tile_a First tile data
   * @param tile_b Second tile data
   * @return Similarity score 0.0-100.0 (100 = identical)
   *
   * More robust than MAE for tiles with similar structure but different colors.
   */
  double ComputeStructuralSimilarity(const std::vector<uint8_t>& tile_a,
                                     const std::vector<uint8_t>& tile_b) const;

  /**
   * @brief Extract 8x8 tile data from ROM graphics
   * @param rom ROM to extract from
   * @param sheet_index Graphics sheet index (0-222)
   * @param tile_index Tile index within sheet
   * @return Tile pixel data (64 bytes)
   */
  absl::StatusOr<std::vector<uint8_t>> ExtractTile(Rom* rom, int sheet_index,
                                                   int tile_index) const;

  /**
   * @brief Extract 8x8 tile at specific pixel coordinates
   * @param rom ROM to extract from
   * @param sheet_index Graphics sheet index
   * @param x X coordinate in pixels
   * @param y Y coordinate in pixels
   * @return Tile pixel data (64 bytes)
   */
  absl::StatusOr<std::vector<uint8_t>> ExtractTileAtPosition(Rom* rom,
                                                              int sheet_index,
                                                              int x,
                                                              int y) const;

  /**
   * @brief Check if a tile region is empty (all 0x00 or 0xFF)
   * @param data Tile data to check
   * @return true if the region appears unused
   */
  bool IsRegionEmpty(const std::vector<uint8_t>& data) const;

  /**
   * @brief Get the number of tiles in a graphics sheet
   * @param sheet_index Graphics sheet index
   * @return Number of 8x8 tiles
   */
  int GetTileCountForSheet(int sheet_index) const;

  /**
   * @brief Format similarity matches as JSON
   */
  std::string FormatMatchesAsJson(
      const std::vector<TileSimilarityMatch>& matches) const;

  /**
   * @brief Format unused regions as JSON
   */
  std::string FormatRegionsAsJson(
      const std::vector<UnusedRegion>& regions) const;

  /**
   * @brief Format palette usage as JSON
   */
  std::string FormatPaletteUsageAsJson(
      const std::vector<PaletteUsageStats>& stats) const;

  /**
   * @brief Format tile histogram as JSON
   */
  std::string FormatHistogramAsJson(
      const std::vector<TileUsageEntry>& entries) const;
};

/**
 * @brief Find tiles with similar patterns
 *
 * Usage: visual-find-similar-tiles --tile_id <id> [--sheet <index>]
 *        [--threshold <0-100>] [--method <pixel|structural>]
 *
 * Compares a reference tile against all tiles in graphics sheets
 * and returns matches above the similarity threshold.
 */
class TileSimilarityTool : public VisualAnalysisBase {
 public:
  std::string GetName() const override { return "visual-find-similar-tiles"; }

  std::string GetDescription() const {
    return "Find tiles with similar patterns to a reference tile";
  }

  std::string GetUsage() const override {
    return "visual-find-similar-tiles --tile_id <id> [--sheet <index>] "
           "[--threshold <0-100>] [--method <pixel|structural>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }
};

/**
 * @brief Analyze spritesheets for unused graphics regions
 *
 * Usage: visual-analyze-spritesheet [--sheet <index>] [--tile_size <8|16>]
 *        [--format <json|text>]
 *
 * Scans graphics sheets for contiguous empty regions that can be
 * used for custom graphics in ROM hacking.
 */
class SpritesheetAnalysisTool : public VisualAnalysisBase {
 public:
  std::string GetName() const override { return "visual-analyze-spritesheet"; }

  std::string GetDescription() const {
    return "Identify unused regions in graphics sheets for ROM hacking";
  }

  std::string GetUsage() const override {
    return "visual-analyze-spritesheet [--sheet <index>] [--tile_size <8|16>] "
           "[--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  /**
   * @brief Find contiguous empty regions in a sheet
   */
  std::vector<UnusedRegion> FindUnusedRegions(Rom* rom, int sheet_index,
                                               int tile_size) const;

  /**
   * @brief Merge adjacent empty regions
   */
  std::vector<UnusedRegion> MergeAdjacentRegions(
      const std::vector<UnusedRegion>& regions) const;
};

/**
 * @brief Analyze palette usage across maps
 *
 * Usage: visual-palette-usage [--type <overworld|dungeon|all>]
 *        [--format <json|text>]
 *
 * Analyzes which palette indices are used across overworld maps
 * and dungeon rooms to identify optimization opportunities.
 */
class PaletteUsageTool : public VisualAnalysisBase {
 public:
  std::string GetName() const override { return "visual-palette-usage"; }

  std::string GetDescription() const {
    return "Analyze palette usage statistics across maps";
  }

  std::string GetUsage() const override {
    return "visual-palette-usage [--type <overworld|dungeon|all>] "
           "[--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  /**
   * @brief Analyze overworld palette usage
   */
  std::vector<PaletteUsageStats> AnalyzeOverworldPalettes(Rom* rom) const;

  /**
   * @brief Analyze dungeon palette usage
   */
  std::vector<PaletteUsageStats> AnalyzeDungeonPalettes(Rom* rom) const;
};

/**
 * @brief Generate tile usage histogram
 *
 * Usage: visual-tile-histogram [--type <overworld|dungeon>] [--top <n>]
 *        [--format <json|text>]
 *
 * Counts the frequency of tile IDs used across tilemaps to
 * identify commonly and rarely used tiles.
 */
class TileHistogramTool : public VisualAnalysisBase {
 public:
  std::string GetName() const override { return "visual-tile-histogram"; }

  std::string GetDescription() const {
    return "Generate frequency histogram of tile usage across maps";
  }

  std::string GetUsage() const override {
    return "visual-tile-histogram [--type <overworld|dungeon>] [--top <n>] "
           "[--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  /**
   * @brief Count tile usage in overworld
   */
  std::map<int, TileUsageEntry> CountOverworldTiles(Rom* rom) const;

  /**
   * @brief Count tile usage in dungeons
   */
  std::map<int, TileUsageEntry> CountDungeonTiles(Rom* rom) const;
};

#ifdef YAZE_WITH_OPENCV
/**
 * @brief OpenCV-enhanced visual analysis functions
 *
 * When YAZE_WITH_OPENCV is defined, these functions provide
 * advanced pattern matching using OpenCV algorithms.
 */
namespace opencv {

/**
 * @brief Template matching using OpenCV
 * @param reference Reference tile (8x8 grayscale)
 * @param search_region Region to search in
 * @param method OpenCV matching method
 * @return Best match location and score
 */
std::pair<std::pair<int, int>, double> TemplateMatch(
    const std::vector<uint8_t>& reference,
    const std::vector<uint8_t>& search_region, int width, int height,
    int method);

/**
 * @brief Feature-based matching using ORB
 * @param tile_a First tile
 * @param tile_b Second tile
 * @return Match score based on feature correspondences
 */
double FeatureMatch(const std::vector<uint8_t>& tile_a,
                    const std::vector<uint8_t>& tile_b);

/**
 * @brief Compute SSIM using OpenCV
 * @param tile_a First tile
 * @param tile_b Second tile
 * @return SSIM value (0.0-1.0)
 */
double ComputeSSIM(const std::vector<uint8_t>& tile_a,
                   const std::vector<uint8_t>& tile_b);

}  // namespace opencv
#endif  // YAZE_WITH_OPENCV

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_VISUAL_ANALYSIS_TOOL_H_

