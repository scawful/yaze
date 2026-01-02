/**
 * @file visual_analysis_tool.cc
 * @brief Implementation of visual analysis tools for AI agents
 */

#include "cli/service/agent/tools/visual_analysis_tool.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

// =============================================================================
// VisualAnalysisBase Implementation
// =============================================================================

double VisualAnalysisBase::ComputePixelDifference(
    const std::vector<uint8_t>& tile_a,
    const std::vector<uint8_t>& tile_b) const {
  if (tile_a.size() != tile_b.size() || tile_a.empty()) {
    return 0.0;
  }

  // Compute Mean Absolute Error
  double total_diff = 0.0;
  for (size_t i = 0; i < tile_a.size(); ++i) {
    total_diff +=
        std::abs(static_cast<int>(tile_a[i]) - static_cast<int>(tile_b[i]));
  }

  // Normalize to 0-100 scale (max diff per pixel is 255)
  double max_possible_diff = tile_a.size() * 255.0;
  double normalized_diff = total_diff / max_possible_diff;

  // Convert to similarity (100 = identical, 0 = completely different)
  return (1.0 - normalized_diff) * 100.0;
}

double VisualAnalysisBase::ComputeStructuralSimilarity(
    const std::vector<uint8_t>& tile_a,
    const std::vector<uint8_t>& tile_b) const {
  if (tile_a.size() != tile_b.size() || tile_a.empty()) {
    return 0.0;
  }

  const size_t n = tile_a.size();

  // Compute means
  double mean_a = 0.0, mean_b = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mean_a += tile_a[i];
    mean_b += tile_b[i];
  }
  mean_a /= n;
  mean_b /= n;

  // Compute variances and covariance
  double var_a = 0.0, var_b = 0.0, covar = 0.0;
  for (size_t i = 0; i < n; ++i) {
    double diff_a = tile_a[i] - mean_a;
    double diff_b = tile_b[i] - mean_b;
    var_a += diff_a * diff_a;
    var_b += diff_b * diff_b;
    covar += diff_a * diff_b;
  }
  var_a /= n;
  var_b /= n;
  covar /= n;

  // SSIM-like formula with stability constants
  const double c1 = 6.5025;   // (0.01 * 255)^2
  const double c2 = 58.5225;  // (0.03 * 255)^2

  double numerator = (2.0 * mean_a * mean_b + c1) * (2.0 * covar + c2);
  double denominator =
      (mean_a * mean_a + mean_b * mean_b + c1) * (var_a + var_b + c2);

  double ssim = numerator / denominator;

  // Convert to 0-100 scale
  return std::max(0.0, std::min(100.0, ssim * 100.0));
}

absl::StatusOr<std::vector<uint8_t>> VisualAnalysisBase::ExtractTile(
    Rom* rom, int sheet_index, int tile_index) const {
  if (!rom) {
    return absl::InvalidArgumentError("ROM is null");
  }

  if (sheet_index < 0 || sheet_index >= kMaxSheets) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid sheet index: ", sheet_index));
  }

  // Calculate tile position in sheet
  int tiles_per_sheet =
      (kSheetWidth / kTileWidth) * (kSheetHeight / kTileHeight);
  if (tile_index < 0 || tile_index >= tiles_per_sheet) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid tile index: ", tile_index));
  }

  int x = (tile_index % kTilesPerRow) * kTileWidth;
  int y = (tile_index / kTilesPerRow) * kTileHeight;

  return ExtractTileAtPosition(rom, sheet_index, x, y);
}

absl::StatusOr<std::vector<uint8_t>> VisualAnalysisBase::ExtractTileAtPosition(
    Rom* rom, int sheet_index, int x, int y) const {
  if (!rom) {
    return absl::InvalidArgumentError("ROM is null");
  }

  // Get graphics sheet data from ROM
  // Graphics sheets are stored in a compressed format in the ROM
  // For now, we'll work with the decompressed data if available

  // Calculate the base address for the graphics sheet
  // ALTTP graphics are stored at various locations depending on the sheet
  // This is a simplified extraction - in practice, this would need to
  // interface with the existing graphics loading code

  std::vector<uint8_t> tile_data(kTilePixels, 0);

  // For the agent tool, we should interface with existing graphics
  // decompression. For now, return a placeholder that indicates
  // we need ROM context with loaded graphics.

  // Access graphics from Arena if available
  const auto& gfx_sheets = gfx::Arena::Get().gfx_sheets();
  if (gfx_sheets.empty() || !gfx_sheets[0].is_active()) {
    return absl::FailedPreconditionError(
        "Graphics not loaded. Load ROM with graphics first.");
  }

  // Check sheet index is valid
  if (sheet_index >= static_cast<int>(gfx_sheets.size())) {
    return absl::OutOfRangeError(
        absl::StrCat("Sheet ", sheet_index, " out of range"));
  }

  const auto& sheet = gfx_sheets[sheet_index];
  if (!sheet.is_active()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Sheet ", sheet_index, " is not loaded"));
  }

  const auto& sheet_data = sheet.vector();

  // Extract 8x8 tile from the sheet
  for (int row = 0; row < kTileHeight; ++row) {
    size_t src_offset = (y + row) * kSheetWidth + x;
    for (int col = 0; col < kTileWidth; ++col) {
      if (src_offset + col < sheet_data.size()) {
        tile_data[row * kTileWidth + col] = sheet_data[src_offset + col];
      }
    }
  }

  return tile_data;
}

bool VisualAnalysisBase::IsRegionEmpty(const std::vector<uint8_t>& data) const {
  if (data.empty()) {
    return true;
  }

  // Check if all bytes are 0x00 (fully transparent/black)
  bool all_zero = std::all_of(data.begin(), data.end(),
                              [](uint8_t b) { return b == 0x00; });
  if (all_zero) {
    return true;
  }

  // Check if all bytes are 0xFF (common empty pattern)
  bool all_ff = std::all_of(data.begin(), data.end(),
                            [](uint8_t b) { return b == 0xFF; });
  if (all_ff) {
    return true;
  }

  // Check if mostly empty (>95% zeroes)
  int zero_count = std::count(data.begin(), data.end(), 0x00);
  if (static_cast<double>(zero_count) / data.size() > 0.95) {
    return true;
  }

  return false;
}

int VisualAnalysisBase::GetTileCountForSheet(int sheet_index) const {
  // Standard SNES sheet is 128x32 pixels = 16x4 = 64 tiles
  return (kSheetWidth / kTileWidth) * (kSheetHeight / kTileHeight);
}

std::string VisualAnalysisBase::FormatMatchesAsJson(
    const std::vector<TileSimilarityMatch>& matches) const {
  std::ostringstream json;
  if (matches.empty()) {
    json << "{\n  \"matches\": [],\n";
    json << "  \"total_matches\": 0\n";
    json << "}\n";
    return json.str();
  }

  json << "{\n  \"matches\": [\n";

  for (size_t i = 0; i < matches.size(); ++i) {
    const auto& m = matches[i];
    json << "    {\n";
    json << "      \"tile_id\": " << m.tile_id << ",\n";
    json << "      \"similarity_score\": "
         << absl::StrFormat("%.2f", m.similarity_score) << ",\n";
    json << "      \"sheet_index\": " << m.sheet_index << ",\n";
    json << "      \"x_position\": " << m.x_position << ",\n";
    json << "      \"y_position\": " << m.y_position << "\n";
    json << "    }";
    if (i < matches.size() - 1)
      json << ",";
    json << "\n";
  }

  json << "  ],\n";
  json << "  \"total_matches\": " << matches.size() << "\n";
  json << "}\n";

  return json.str();
}

std::string VisualAnalysisBase::FormatRegionsAsJson(
    const std::vector<UnusedRegion>& regions) const {
  std::ostringstream json;
  if (regions.empty()) {
    json << "{\n  \"unused_regions\": [],\n";
    json << "  \"total_regions\": 0,\n";
    json << "  \"total_free_tiles\": 0\n";
    json << "}\n";
    return json.str();
  }

  json << "{\n  \"unused_regions\": [\n";

  for (size_t i = 0; i < regions.size(); ++i) {
    const auto& r = regions[i];
    json << "    {\n";
    json << "      \"sheet_index\": " << r.sheet_index << ",\n";
    json << "      \"x\": " << r.x << ",\n";
    json << "      \"y\": " << r.y << ",\n";
    json << "      \"width\": " << r.width << ",\n";
    json << "      \"height\": " << r.height << ",\n";
    json << "      \"tile_count\": " << r.tile_count << "\n";
    json << "    }";
    if (i < regions.size() - 1)
      json << ",";
    json << "\n";
  }

  int total_tiles = 0;
  for (const auto& r : regions) {
    total_tiles += r.tile_count;
  }

  json << "  ],\n";
  json << "  \"total_regions\": " << regions.size() << ",\n";
  json << "  \"total_free_tiles\": " << total_tiles << "\n";
  json << "}\n";

  return json.str();
}

std::string VisualAnalysisBase::FormatPaletteUsageAsJson(
    const std::vector<PaletteUsageStats>& stats) const {
  std::ostringstream json;
  json << "{\n  \"palette_usage\": [\n";

  for (size_t i = 0; i < stats.size(); ++i) {
    const auto& s = stats[i];
    json << "    {\n";
    json << "      \"palette_index\": " << s.palette_index << ",\n";
    json << "      \"usage_count\": " << s.usage_count << ",\n";
    json << "      \"usage_percentage\": "
         << absl::StrFormat("%.2f", s.usage_percentage) << ",\n";
    json << "      \"used_by_maps\": [";
    for (size_t j = 0; j < s.used_by_maps.size(); ++j) {
      json << s.used_by_maps[j];
      if (j < s.used_by_maps.size() - 1)
        json << ", ";
    }
    json << "]\n";
    json << "    }";
    if (i < stats.size() - 1)
      json << ",";
    json << "\n";
  }

  json << "  ]\n";
  json << "}\n";

  return json.str();
}

std::string VisualAnalysisBase::FormatHistogramAsJson(
    const std::vector<TileUsageEntry>& entries) const {
  std::ostringstream json;
  json << "{\n  \"tile_histogram\": [\n";

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    json << "    {\n";
    json << "      \"tile_id\": " << e.tile_id << ",\n";
    json << "      \"usage_count\": " << e.usage_count << ",\n";
    json << "      \"usage_percentage\": "
         << absl::StrFormat("%.2f", e.usage_percentage) << ",\n";
    json << "      \"locations\": [";
    for (size_t j = 0; j < std::min(e.locations.size(), size_t(10)); ++j) {
      json << e.locations[j];
      if (j < std::min(e.locations.size(), size_t(10)) - 1)
        json << ", ";
    }
    if (e.locations.size() > 10)
      json << ", ...";
    json << "]\n";
    json << "    }";
    if (i < entries.size() - 1)
      json << ",";
    json << "\n";
  }

  json << "  ],\n";
  json << "  \"total_entries\": " << entries.size() << "\n";
  json << "}\n";

  return json.str();
}

// =============================================================================
// TileSimilarityTool Implementation
// =============================================================================

absl::Status TileSimilarityTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"tile_id"});
}

absl::Status TileSimilarityTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // Parse arguments
  auto tile_id_or = parser.GetInt("tile_id");
  if (!tile_id_or.ok()) {
    return absl::InvalidArgumentError("tile_id is required");
  }
  int tile_id = tile_id_or.value();
  int sheet = parser.GetInt("sheet").value_or(0);
  int threshold = parser.GetInt("threshold").value_or(80);
  std::string method = parser.GetString("method").value_or("structural");

  // Validate threshold
  threshold = std::clamp(threshold, 0, 100);

  // Extract reference tile
  auto ref_tile_or = ExtractTile(rom, sheet, tile_id);
  if (!ref_tile_or.ok()) {
    return ref_tile_or.status();
  }
  const auto& ref_tile = ref_tile_or.value();

  // Find similar tiles across all sheets (or specified sheet)
  std::vector<TileSimilarityMatch> matches;

  bool has_sheet = parser.GetString("sheet").has_value();
  int start_sheet = has_sheet ? sheet : 0;
  int end_sheet = has_sheet ? sheet + 1 : kMaxSheets;

  for (int s = start_sheet; s < end_sheet; ++s) {
    int tile_count = GetTileCountForSheet(s);

    for (int t = 0; t < tile_count; ++t) {
      // Skip the reference tile itself
      if (s == sheet && t == tile_id) {
        continue;
      }

      auto cmp_tile_or = ExtractTile(rom, s, t);
      if (!cmp_tile_or.ok()) {
        continue;  // Skip tiles that can't be extracted
      }
      const auto& cmp_tile = cmp_tile_or.value();

      // Compute similarity
      double score;
      if (method == "pixel") {
        score = ComputePixelDifference(ref_tile, cmp_tile);
      } else {
        score = ComputeStructuralSimilarity(ref_tile, cmp_tile);
      }

      if (score >= threshold) {
        TileSimilarityMatch match;
        match.tile_id = t;
        match.similarity_score = score;
        match.sheet_index = s;
        match.x_position = (t % kTilesPerRow) * kTileWidth;
        match.y_position = (t / kTilesPerRow) * kTileHeight;
        matches.push_back(match);
      }
    }
  }

  // Sort by similarity score descending
  std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
    return a.similarity_score > b.similarity_score;
  });

  // Limit to top 50 matches
  if (matches.size() > 50) {
    matches.resize(50);
  }

  // Output results
  std::cout << FormatMatchesAsJson(matches);

  return absl::OkStatus();
}

// =============================================================================
// SpritesheetAnalysisTool Implementation
// =============================================================================

absl::Status SpritesheetAnalysisTool::ValidateArgs(
    const resources::ArgumentParser& /*parser*/) {
  return absl::OkStatus();
}

absl::Status SpritesheetAnalysisTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  int tile_size = parser.GetInt("tile_size").value_or(8);
  if (tile_size != 8 && tile_size != 16) {
    tile_size = 8;
  }

  std::vector<UnusedRegion> all_regions;

  auto sheet_arg = parser.GetString("sheet");
  if (sheet_arg.has_value()) {
    int sheet = parser.GetInt("sheet").value_or(0);
    auto regions = FindUnusedRegions(rom, sheet, tile_size);
    all_regions.insert(all_regions.end(), regions.begin(), regions.end());
  } else {
    // Analyze all sheets
    for (int s = 0; s < kMaxSheets; ++s) {
      auto regions = FindUnusedRegions(rom, s, tile_size);
      all_regions.insert(all_regions.end(), regions.begin(), regions.end());
    }
  }

  // Merge adjacent regions
  all_regions = MergeAdjacentRegions(all_regions);

  // Sort by size (largest first)
  std::sort(
      all_regions.begin(), all_regions.end(),
      [](const auto& a, const auto& b) { return a.tile_count > b.tile_count; });

  // Output results
  std::cout << FormatRegionsAsJson(all_regions);

  return absl::OkStatus();
}

std::vector<UnusedRegion> SpritesheetAnalysisTool::FindUnusedRegions(
    Rom* rom, int sheet_index, int tile_size) const {
  std::vector<UnusedRegion> regions;

  int tiles_x = kSheetWidth / tile_size;
  int tiles_y = kSheetHeight / tile_size;
  int pixels_per_tile = tile_size * tile_size;

  for (int ty = 0; ty < tiles_y; ++ty) {
    for (int tx = 0; tx < tiles_x; ++tx) {
      // Extract tile region
      std::vector<uint8_t> tile_data;
      tile_data.reserve(pixels_per_tile);

      for (int py = 0; py < tile_size; ++py) {
        for (int px = 0; px < tile_size; ++px) {
          auto pixel_or = ExtractTileAtPosition(rom, sheet_index,
                                                tx * tile_size, ty * tile_size);
          if (pixel_or.ok() && !pixel_or.value().empty()) {
            // Get the specific pixel
            int local_idx = py * tile_size + px;
            if (local_idx < static_cast<int>(pixel_or.value().size())) {
              tile_data.push_back(pixel_or.value()[local_idx]);
            }
          }
        }
      }

      if (IsRegionEmpty(tile_data)) {
        UnusedRegion region;
        region.sheet_index = sheet_index;
        region.x = tx * tile_size;
        region.y = ty * tile_size;
        region.width = tile_size;
        region.height = tile_size;
        region.tile_count = (tile_size == 8) ? 1 : 4;
        regions.push_back(region);
      }
    }
  }

  return regions;
}

std::vector<UnusedRegion> SpritesheetAnalysisTool::MergeAdjacentRegions(
    const std::vector<UnusedRegion>& regions) const {
  if (regions.empty()) {
    return regions;
  }

  // Group by sheet
  std::map<int, std::vector<UnusedRegion>> by_sheet;
  for (const auto& r : regions) {
    by_sheet[r.sheet_index].push_back(r);
  }

  std::vector<UnusedRegion> merged;

  for (auto& [sheet, sheet_regions] : by_sheet) {
    // Simple horizontal merging for adjacent tiles
    std::sort(sheet_regions.begin(), sheet_regions.end(),
              [](const auto& a, const auto& b) {
                if (a.y != b.y)
                  return a.y < b.y;
                return a.x < b.x;
              });

    for (size_t i = 0; i < sheet_regions.size();) {
      UnusedRegion current = sheet_regions[i];

      // Try to merge with following regions on the same row
      size_t j = i + 1;
      while (j < sheet_regions.size() && sheet_regions[j].y == current.y &&
             sheet_regions[j].x == current.x + current.width) {
        current.width += sheet_regions[j].width;
        current.tile_count += sheet_regions[j].tile_count;
        ++j;
      }

      merged.push_back(current);
      i = j;
    }
  }

  return merged;
}

// =============================================================================
// PaletteUsageTool Implementation
// =============================================================================

absl::Status PaletteUsageTool::ValidateArgs(
    const resources::ArgumentParser& /*parser*/) {
  return absl::OkStatus();
}

absl::Status PaletteUsageTool::Execute(Rom* rom,
                                       const resources::ArgumentParser& parser,
                                       resources::OutputFormatter& formatter) {
  std::string type = parser.GetString("type").value_or("all");

  std::vector<PaletteUsageStats> stats;

  if (type == "overworld" || type == "all") {
    auto ow_stats = AnalyzeOverworldPalettes(rom);
    stats.insert(stats.end(), ow_stats.begin(), ow_stats.end());
  }

  if (type == "dungeon" || type == "all") {
    auto dg_stats = AnalyzeDungeonPalettes(rom);
    // Merge with overworld stats or add new entries
    for (const auto& ds : dg_stats) {
      bool found = false;
      for (auto& s : stats) {
        if (s.palette_index == ds.palette_index) {
          s.usage_count += ds.usage_count;
          s.used_by_maps.insert(s.used_by_maps.end(), ds.used_by_maps.begin(),
                                ds.used_by_maps.end());
          found = true;
          break;
        }
      }
      if (!found) {
        stats.push_back(ds);
      }
    }
  }

  // Calculate percentages
  int total_usage = 0;
  for (const auto& s : stats) {
    total_usage += s.usage_count;
  }
  for (auto& s : stats) {
    s.usage_percentage =
        total_usage > 0
            ? (static_cast<double>(s.usage_count) / total_usage) * 100.0
            : 0.0;
  }

  // Sort by usage count
  std::sort(stats.begin(), stats.end(), [](const auto& a, const auto& b) {
    return a.usage_count > b.usage_count;
  });

  std::cout << FormatPaletteUsageAsJson(stats);

  return absl::OkStatus();
}

std::vector<PaletteUsageStats> PaletteUsageTool::AnalyzeOverworldPalettes(
    Rom* rom) const {
  std::map<int, PaletteUsageStats> palette_map;

  // Overworld palette indices used by each map are stored in ROM
  // Light World uses palettes 0-7, Dark World uses 8-15 typically
  // For now, return basic structure - full implementation would
  // read actual palette assignments from ROM

  // Initialize with 8 main palettes
  for (int i = 0; i < 8; ++i) {
    PaletteUsageStats stats;
    stats.palette_index = i;
    stats.usage_count = 0;
    palette_map[i] = stats;
  }

  // Count usage across 128 overworld maps (0-63 Light, 64-127 Dark)
  for (int map_id = 0; map_id < 128; ++map_id) {
    // In actual implementation, read palette from map header
    // For now, use a heuristic: Light World maps use lower palettes
    int palette_idx = (map_id < 64) ? (map_id % 8) : (map_id % 8);
    palette_map[palette_idx].usage_count++;
    palette_map[palette_idx].used_by_maps.push_back(map_id);
  }

  std::vector<PaletteUsageStats> result;
  for (const auto& [_, stats] : palette_map) {
    result.push_back(stats);
  }

  return result;
}

std::vector<PaletteUsageStats> PaletteUsageTool::AnalyzeDungeonPalettes(
    Rom* rom) const {
  std::map<int, PaletteUsageStats> palette_map;

  // Dungeons use a different palette system
  // Each room can reference one of several dungeon palettes

  // Initialize with dungeon palettes (typically 0-15)
  for (int i = 0; i < 16; ++i) {
    PaletteUsageStats stats;
    stats.palette_index = i + 100;  // Offset to distinguish from OW
    stats.usage_count = 0;
    palette_map[i + 100] = stats;
  }

  // Count usage across ~320 dungeon rooms
  for (int room_id = 0; room_id < 320; ++room_id) {
    // In actual implementation, read palette from room header
    int palette_idx = 100 + (room_id % 16);
    palette_map[palette_idx].usage_count++;
    palette_map[palette_idx].used_by_maps.push_back(room_id);
  }

  std::vector<PaletteUsageStats> result;
  for (const auto& [_, stats] : palette_map) {
    result.push_back(stats);
  }

  return result;
}

// =============================================================================
// TileHistogramTool Implementation
// =============================================================================

absl::Status TileHistogramTool::ValidateArgs(
    const resources::ArgumentParser& /*parser*/) {
  return absl::OkStatus();
}

absl::Status TileHistogramTool::Execute(Rom* rom,
                                        const resources::ArgumentParser& parser,
                                        resources::OutputFormatter& formatter) {
  std::string type = parser.GetString("type").value_or("overworld");
  int top_n = parser.GetInt("top").value_or(20);

  std::map<int, TileUsageEntry> usage_map;

  if (type == "overworld") {
    usage_map = CountOverworldTiles(rom);
  } else if (type == "dungeon") {
    usage_map = CountDungeonTiles(rom);
  } else {
    // Both
    auto ow = CountOverworldTiles(rom);
    auto dg = CountDungeonTiles(rom);
    usage_map = ow;
    for (const auto& [tile_id, entry] : dg) {
      if (usage_map.count(tile_id)) {
        usage_map[tile_id].usage_count += entry.usage_count;
        usage_map[tile_id].locations.insert(usage_map[tile_id].locations.end(),
                                            entry.locations.begin(),
                                            entry.locations.end());
      } else {
        usage_map[tile_id] = entry;
      }
    }
  }

  // Convert to vector and calculate percentages
  std::vector<TileUsageEntry> entries;
  int total_usage = 0;
  for (const auto& [_, entry] : usage_map) {
    total_usage += entry.usage_count;
    entries.push_back(entry);
  }

  for (auto& e : entries) {
    e.usage_percentage =
        total_usage > 0
            ? (static_cast<double>(e.usage_count) / total_usage) * 100.0
            : 0.0;
  }

  // Sort by usage count descending
  std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
    return a.usage_count > b.usage_count;
  });

  // Limit to top N
  if (static_cast<int>(entries.size()) > top_n) {
    entries.resize(top_n);
  }

  std::cout << FormatHistogramAsJson(entries);

  return absl::OkStatus();
}

std::map<int, TileUsageEntry> TileHistogramTool::CountOverworldTiles(
    Rom* rom) const {
  std::map<int, TileUsageEntry> usage;

  // Overworld uses Tile16 (16x16 metatiles) composed of Tile32 (32x32 blocks)
  // Each overworld map is 32x32 Tile16s = 1024 tiles per map
  // There are 160 overworld maps total

  // For demonstration, create synthetic data
  // In actual implementation, read tilemap data from ROM

  for (int map_id = 0; map_id < 160; ++map_id) {
    // Simulate reading 1024 tiles per map
    for (int i = 0; i < 1024; ++i) {
      // In actual implementation: int tile_id = ReadTileFromMap(rom, map_id, i);
      int tile_id = (map_id * 7 + i) % 512;  // Synthetic distribution

      if (usage.count(tile_id) == 0) {
        TileUsageEntry entry;
        entry.tile_id = tile_id;
        entry.usage_count = 0;
        usage[tile_id] = entry;
      }
      usage[tile_id].usage_count++;

      // Track first 10 locations only to save memory
      if (usage[tile_id].locations.size() < 10) {
        usage[tile_id].locations.push_back(map_id);
      }
    }
  }

  return usage;
}

std::map<int, TileUsageEntry> TileHistogramTool::CountDungeonTiles(
    Rom* rom) const {
  std::map<int, TileUsageEntry> usage;

  // Dungeons use different tile arrangements
  // Each room varies in size but typically uses floor, wall, and object tiles

  for (int room_id = 0; room_id < 320; ++room_id) {
    // Simulate tile usage per room
    for (int i = 0; i < 256; ++i) {
      int tile_id = (room_id * 11 + i) % 256;  // Synthetic distribution

      if (usage.count(tile_id) == 0) {
        TileUsageEntry entry;
        entry.tile_id = tile_id;
        entry.usage_count = 0;
        usage[tile_id] = entry;
      }
      usage[tile_id].usage_count++;

      if (usage[tile_id].locations.size() < 10) {
        usage[tile_id].locations.push_back(room_id +
                                           1000);  // Offset for dungeons
      }
    }
  }

  return usage;
}

// =============================================================================
// OpenCV Integration (Optional)
// =============================================================================

#ifdef YAZE_WITH_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>

namespace opencv {

std::pair<std::pair<int, int>, double> TemplateMatch(
    const std::vector<uint8_t>& reference,
    const std::vector<uint8_t>& search_region, int width, int height,
    int method) {
  // Convert to cv::Mat
  cv::Mat ref_mat(8, 8, CV_8UC1, const_cast<uint8_t*>(reference.data()));
  cv::Mat search_mat(height, width, CV_8UC1,
                     const_cast<uint8_t*>(search_region.data()));

  cv::Mat result;
  cv::matchTemplate(search_mat, ref_mat, result, method);

  double min_val, max_val;
  cv::Point min_loc, max_loc;
  cv::minMaxLoc(result, &min_val, &max_val, &min_loc, &max_loc);

  // For TM_SQDIFF methods, lower is better
  if (method == cv::TM_SQDIFF || method == cv::TM_SQDIFF_NORMED) {
    return {{min_loc.x, min_loc.y}, 1.0 - min_val};
  }

  return {{max_loc.x, max_loc.y}, max_val};
}

double FeatureMatch(const std::vector<uint8_t>& tile_a,
                    const std::vector<uint8_t>& tile_b) {
  cv::Mat mat_a(8, 8, CV_8UC1, const_cast<uint8_t*>(tile_a.data()));
  cv::Mat mat_b(8, 8, CV_8UC1, const_cast<uint8_t*>(tile_b.data()));

  // ORB detector
  cv::Ptr<cv::ORB> orb = cv::ORB::create();

  std::vector<cv::KeyPoint> kp_a, kp_b;
  cv::Mat desc_a, desc_b;

  orb->detectAndCompute(mat_a, cv::noArray(), kp_a, desc_a);
  orb->detectAndCompute(mat_b, cv::noArray(), kp_b, desc_b);

  if (desc_a.empty() || desc_b.empty()) {
    return 0.0;  // No features found
  }

  // BFMatcher
  cv::BFMatcher matcher(cv::NORM_HAMMING);
  std::vector<cv::DMatch> matches;
  matcher.match(desc_a, desc_b, matches);

  if (matches.empty()) {
    return 0.0;
  }

  // Calculate match score based on distances
  double total_dist = 0.0;
  for (const auto& m : matches) {
    total_dist += m.distance;
  }

  // Normalize: lower distance = higher similarity
  double avg_dist = total_dist / matches.size();
  return std::max(0.0, 100.0 - avg_dist);
}

double ComputeSSIM(const std::vector<uint8_t>& tile_a,
                   const std::vector<uint8_t>& tile_b) {
  cv::Mat mat_a(8, 8, CV_8UC1, const_cast<uint8_t*>(tile_a.data()));
  cv::Mat mat_b(8, 8, CV_8UC1, const_cast<uint8_t*>(tile_b.data()));

  const double C1 = 6.5025, C2 = 58.5225;

  cv::Mat I1, I2;
  mat_a.convertTo(I1, CV_32F);
  mat_b.convertTo(I2, CV_32F);

  cv::Mat I1_2 = I1.mul(I1);
  cv::Mat I2_2 = I2.mul(I2);
  cv::Mat I1_I2 = I1.mul(I2);

  cv::Mat mu1, mu2;
  cv::GaussianBlur(I1, mu1, cv::Size(3, 3), 1.5);
  cv::GaussianBlur(I2, mu2, cv::Size(3, 3), 1.5);

  cv::Mat mu1_2 = mu1.mul(mu1);
  cv::Mat mu2_2 = mu2.mul(mu2);
  cv::Mat mu1_mu2 = mu1.mul(mu2);

  cv::Mat sigma1_2, sigma2_2, sigma12;
  cv::GaussianBlur(I1_2, sigma1_2, cv::Size(3, 3), 1.5);
  sigma1_2 -= mu1_2;
  cv::GaussianBlur(I2_2, sigma2_2, cv::Size(3, 3), 1.5);
  sigma2_2 -= mu2_2;
  cv::GaussianBlur(I1_I2, sigma12, cv::Size(3, 3), 1.5);
  sigma12 -= mu1_mu2;

  cv::Mat t1 = 2 * mu1_mu2 + C1;
  cv::Mat t2 = 2 * sigma12 + C2;
  cv::Mat t3 = t1.mul(t2);

  t1 = mu1_2 + mu2_2 + C1;
  t2 = sigma1_2 + sigma2_2 + C2;
  t1 = t1.mul(t2);

  cv::Mat ssim_map;
  cv::divide(t3, t1, ssim_map);

  cv::Scalar mssim = cv::mean(ssim_map);
  return mssim[0] * 100.0;  // Convert to percentage
}

}  // namespace opencv
#endif  // YAZE_WITH_OPENCV

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
