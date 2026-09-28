/**
 * @file visual_analysis_tool_test.cc
 * @brief Unit tests for visual analysis tools
 */

#include "cli/service/agent/tools/visual_analysis_tool.h"

#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

// Test fixture for VisualAnalysisBase helper functions
class VisualAnalysisBaseTest : public ::testing::Test {
 protected:
  // Create a test subclass to access protected methods
  class TestableVisualAnalysis : public VisualAnalysisBase {
   public:
    std::string GetName() const override { return "test-visual-analysis"; }
    std::string GetUsage() const override { return "test usage"; }

    // Expose protected methods for testing
    using VisualAnalysisBase::ComputePixelDifference;
    using VisualAnalysisBase::ComputeStructuralSimilarity;
    using VisualAnalysisBase::FormatMatchesAsJson;
    using VisualAnalysisBase::FormatRegionsAsJson;
    using VisualAnalysisBase::IsRegionEmpty;

   protected:
    absl::Status ValidateArgs(
        const resources::ArgumentParser& /*parser*/) override {
      return absl::OkStatus();
    }
    absl::Status Execute(Rom* /*rom*/,
                         const resources::ArgumentParser& /*parser*/,
                         resources::OutputFormatter& /*formatter*/) override {
      return absl::OkStatus();
    }
  };

  TestableVisualAnalysis tool_;
};

// =============================================================================
// ComputePixelDifference Tests
// =============================================================================

TEST_F(VisualAnalysisBaseTest, PixelDifference_IdenticalTiles_Returns100) {
  std::vector<uint8_t> tile(64, 0x42);  // All pixels same value
  double similarity = tool_.ComputePixelDifference(tile, tile);
  EXPECT_DOUBLE_EQ(similarity, 100.0);
}

TEST_F(VisualAnalysisBaseTest, PixelDifference_CompletelyDifferent_Returns0) {
  std::vector<uint8_t> tile_a(64, 0x00);  // All black
  std::vector<uint8_t> tile_b(64, 0xFF);  // All white
  double similarity = tool_.ComputePixelDifference(tile_a, tile_b);
  EXPECT_DOUBLE_EQ(similarity, 0.0);
}

TEST_F(VisualAnalysisBaseTest, PixelDifference_HalfDifferent_Returns50) {
  std::vector<uint8_t> tile_a(64, 0x00);
  std::vector<uint8_t> tile_b(64, 0x00);
  // Make half the pixels maximally different
  for (int i = 0; i < 32; ++i) {
    tile_b[i] = 0xFF;
  }
  double similarity = tool_.ComputePixelDifference(tile_a, tile_b);
  EXPECT_NEAR(similarity, 50.0, 0.01);
}

TEST_F(VisualAnalysisBaseTest, PixelDifference_EmptyTiles_Returns0) {
  std::vector<uint8_t> empty;
  double similarity = tool_.ComputePixelDifference(empty, empty);
  EXPECT_DOUBLE_EQ(similarity, 0.0);
}

TEST_F(VisualAnalysisBaseTest, PixelDifference_DifferentSizes_Returns0) {
  std::vector<uint8_t> tile_a(64, 0x42);
  std::vector<uint8_t> tile_b(32, 0x42);
  double similarity = tool_.ComputePixelDifference(tile_a, tile_b);
  EXPECT_DOUBLE_EQ(similarity, 0.0);
}

// =============================================================================
// ComputeStructuralSimilarity Tests
// =============================================================================

TEST_F(VisualAnalysisBaseTest, StructuralSimilarity_IdenticalTiles_Returns100) {
  std::vector<uint8_t> tile(64, 0x42);
  double similarity = tool_.ComputeStructuralSimilarity(tile, tile);
  EXPECT_GE(similarity, 99.0);  // Should be very close to 100
}

TEST_F(VisualAnalysisBaseTest, StructuralSimilarity_DifferentTiles_ReturnsLow) {
  std::vector<uint8_t> tile_a(64, 0x00);
  std::vector<uint8_t> tile_b(64, 0xFF);
  double similarity = tool_.ComputeStructuralSimilarity(tile_a, tile_b);
  EXPECT_LT(similarity, 50.0);
}

TEST_F(VisualAnalysisBaseTest,
       StructuralSimilarity_SimilarPattern_ReturnsHigh) {
  // Create two tiles with similar structure but slightly different values
  std::vector<uint8_t> tile_a(64);
  std::vector<uint8_t> tile_b(64);

  for (int i = 0; i < 64; ++i) {
    tile_a[i] = i % 16;        // Pattern 0-15 repeated
    tile_b[i] = (i % 16) + 1;  // Same pattern, shifted by 1
  }

  double similarity = tool_.ComputeStructuralSimilarity(tile_a, tile_b);
  EXPECT_GT(similarity, 80.0);  // Should be high due to similar structure
}

TEST_F(VisualAnalysisBaseTest, StructuralSimilarity_EmptyTiles_Returns0) {
  std::vector<uint8_t> empty;
  double similarity = tool_.ComputeStructuralSimilarity(empty, empty);
  EXPECT_DOUBLE_EQ(similarity, 0.0);
}

// =============================================================================
// IsRegionEmpty Tests
// =============================================================================

TEST_F(VisualAnalysisBaseTest, IsRegionEmpty_AllZeros_ReturnsTrue) {
  std::vector<uint8_t> data(64, 0x00);
  EXPECT_TRUE(tool_.IsRegionEmpty(data));
}

TEST_F(VisualAnalysisBaseTest, IsRegionEmpty_AllFF_ReturnsFalse) {
  // Palette index 0xFF pixels are drawn art, not free space.
  std::vector<uint8_t> data(64, 0xFF);
  EXPECT_FALSE(tool_.IsRegionEmpty(data));
}

TEST_F(VisualAnalysisBaseTest, IsRegionEmpty_MostlyZeros_ReturnsFalse) {
  // One drawn pixel means the tile is in use.
  std::vector<uint8_t> data(100, 0x00);
  data[0] = 0x01;
  EXPECT_FALSE(tool_.IsRegionEmpty(data));
}

TEST_F(VisualAnalysisBaseTest, IsRegionEmpty_HalfFilled_ReturnsFalse) {
  std::vector<uint8_t> data(64, 0x00);
  for (int i = 0; i < 32; ++i) {
    data[i] = 0x42;
  }
  EXPECT_FALSE(tool_.IsRegionEmpty(data));
}

TEST_F(VisualAnalysisBaseTest, IsRegionEmpty_CompletelyFilled_ReturnsFalse) {
  std::vector<uint8_t> data(64, 0x42);
  EXPECT_FALSE(tool_.IsRegionEmpty(data));
}

TEST_F(VisualAnalysisBaseTest, IsRegionEmpty_EmptyVector_ReturnsFalse) {
  // Missing pixel data is not evidence of free space.
  std::vector<uint8_t> data;
  EXPECT_FALSE(tool_.IsRegionEmpty(data));
}

// =============================================================================
// SpritesheetAnalysisTool (visual-analyze-spritesheet), headless
// =============================================================================

// Sheet 0x90 holds art everywhere except 16x16 blocks 5 and 9.
Rom SpritesheetTestRom() {
  auto fixture = ::yaze::test::BuildGfxSheetTestRom();
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  std::vector<uint8_t> sheet(zelda3::kGfxSheet3bppBytes, 0x11);
  for (int block : {5, 9}) {
    for (int tile : zelda3::GfxSheetBlockTiles(block)) {
      std::fill_n(sheet.begin() + tile * 24, 24, 0x00);
    }
  }
  zelda3::GfxSheetWritePolicy policy;
  policy.allocation_regions.push_back(
      {::yaze::test::GfxSheetTestRom::kFreeStart,
       ::yaze::test::GfxSheetTestRom::kFreeEnd});
  EXPECT_TRUE(zelda3::WriteGfxSheet(rom, 0x90, sheet, policy).ok());
  return rom;
}

nlohmann::json RunSpritesheet(Rom& rom, std::vector<std::string> args) {
  SpritesheetAnalysisTool tool;
  args.push_back("--format=json");
  std::string out;
  EXPECT_TRUE(tool.Run(args, &rom, &out).ok()) << out;
  const auto brace = out.find('{');
  EXPECT_NE(brace, std::string::npos) << out;
  return nlohmann::json::parse(out.substr(brace));
}

TEST(SpritesheetAnalysisToolTest, FindsOnlyFully16x16EmptyBlocksHeadless) {
  Rom rom = SpritesheetTestRom();
  const auto report = RunSpritesheet(rom, {"--sheet=144", "--tile_size=16"});
  ASSERT_EQ(report["total_regions"], 2) << report.dump(2);
  std::set<std::pair<int, int>> positions;
  for (const auto& region : report["unused_regions"]) {
    EXPECT_EQ(region["sheet_index"], 0x90);
    EXPECT_EQ(region["width"], 16);
    positions.insert({region["x"].get<int>(), region["y"].get<int>()});
  }
  EXPECT_EQ(positions, (std::set<std::pair<int, int>>{{80, 0}, {16, 16}}));
}

TEST(SpritesheetAnalysisToolTest, CountsEmpty8x8TilesInsideTheBlocks) {
  Rom rom = SpritesheetTestRom();
  const auto report = RunSpritesheet(rom, {"--sheet=144", "--tile_size=8"});
  EXPECT_EQ(report["total_free_tiles"], 8) << report.dump(2);
}

TEST(SpritesheetAnalysisToolTest, EngineLoadedSheetsAreNeverFree) {
  Rom rom = SpritesheetTestRom();
  for (const char* sheet : {"--sheet=113", "--sheet=116", "--sheet=220"}) {
    const auto report = RunSpritesheet(rom, {sheet, "--tile_size=16"});
    EXPECT_EQ(report["total_regions"], 0) << sheet << report.dump(2);
  }
}

// =============================================================================
// JSON Formatting Tests
// =============================================================================

TEST_F(VisualAnalysisBaseTest, FormatMatchesAsJson_EmptyList_ReturnsValidJson) {
  std::vector<TileSimilarityMatch> matches;
  std::string json = tool_.FormatMatchesAsJson(matches);

  EXPECT_TRUE(json.find("\"matches\": []") != std::string::npos);
  EXPECT_TRUE(json.find("\"total_matches\": 0") != std::string::npos);
}

TEST_F(VisualAnalysisBaseTest,
       FormatMatchesAsJson_SingleMatch_ReturnsValidJson) {
  std::vector<TileSimilarityMatch> matches = {{.tile_id = 42,
                                               .similarity_score = 95.5,
                                               .sheet_index = 1,
                                               .x_position = 16,
                                               .y_position = 8}};
  std::string json = tool_.FormatMatchesAsJson(matches);

  EXPECT_TRUE(json.find("\"tile_id\": 42") != std::string::npos);
  EXPECT_TRUE(json.find("\"similarity_score\": 95.50") != std::string::npos);
  EXPECT_TRUE(json.find("\"sheet_index\": 1") != std::string::npos);
  EXPECT_TRUE(json.find("\"total_matches\": 1") != std::string::npos);
}

TEST_F(VisualAnalysisBaseTest, FormatRegionsAsJson_EmptyList_ReturnsValidJson) {
  std::vector<UnusedRegion> regions;
  std::string json = tool_.FormatRegionsAsJson(regions);

  EXPECT_TRUE(json.find("\"unused_regions\": []") != std::string::npos);
  EXPECT_TRUE(json.find("\"total_regions\": 0") != std::string::npos);
  EXPECT_TRUE(json.find("\"total_free_tiles\": 0") != std::string::npos);
}

TEST_F(VisualAnalysisBaseTest,
       FormatRegionsAsJson_SingleRegion_ReturnsValidJson) {
  std::vector<UnusedRegion> regions = {{.sheet_index = 5,
                                        .x = 0,
                                        .y = 0,
                                        .width = 16,
                                        .height = 8,
                                        .tile_count = 2}};
  std::string json = tool_.FormatRegionsAsJson(regions);

  EXPECT_TRUE(json.find("\"sheet_index\": 5") != std::string::npos);
  EXPECT_TRUE(json.find("\"width\": 16") != std::string::npos);
  EXPECT_TRUE(json.find("\"tile_count\": 2") != std::string::npos);
  EXPECT_TRUE(json.find("\"total_free_tiles\": 2") != std::string::npos);
}

// =============================================================================
// UnusedRegion Struct Tests
// =============================================================================

TEST(UnusedRegionTest, DefaultInitialization) {
  UnusedRegion region = {};
  EXPECT_EQ(region.sheet_index, 0);
  EXPECT_EQ(region.x, 0);
  EXPECT_EQ(region.y, 0);
  EXPECT_EQ(region.width, 0);
  EXPECT_EQ(region.height, 0);
  EXPECT_EQ(region.tile_count, 0);
}

// =============================================================================
// Constants Tests
// =============================================================================

TEST(VisualAnalysisConstantsTest, TileConstants) {
  EXPECT_EQ(VisualAnalysisBase::kTileWidth, 8);
  EXPECT_EQ(VisualAnalysisBase::kTileHeight, 8);
  EXPECT_EQ(VisualAnalysisBase::kTilePixels, 64);
  EXPECT_EQ(VisualAnalysisBase::kSheetWidth, 128);
  EXPECT_EQ(VisualAnalysisBase::kSheetHeight, 32);
  EXPECT_EQ(VisualAnalysisBase::kTilesPerRow, 16);
  EXPECT_EQ(VisualAnalysisBase::kMaxSheets, 223);
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(VisualAnalysisBaseTest, PixelDifference_LargeTile_HandlesCorrectly) {
  // Test with a larger tile (256 pixels = 16x16)
  std::vector<uint8_t> tile_a(256, 0x80);
  std::vector<uint8_t> tile_b(256, 0x80);
  tile_b[0] = 0x00;  // Single pixel difference

  double similarity = tool_.ComputePixelDifference(tile_a, tile_b);
  // (255/256 pixels same) = 99.6% similar after accounting for intensity diff
  EXPECT_GT(similarity, 99.0);
}

TEST_F(VisualAnalysisBaseTest,
       StructuralSimilarity_GradientPattern_MatchesWell) {
  // Create gradient patterns
  std::vector<uint8_t> tile_a(64);
  std::vector<uint8_t> tile_b(64);

  for (int i = 0; i < 64; ++i) {
    tile_a[i] = i * 4;      // Gradient 0-252
    tile_b[i] = i * 4 + 2;  // Same gradient, offset by 2
  }

  double similarity = tool_.ComputeStructuralSimilarity(tile_a, tile_b);
  EXPECT_GT(similarity, 90.0);  // Same structure
}

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
