/**
 * @file memory_inspector_tool_test.cc
 * @brief Unit tests for the MemoryInspectorTool AI agent tools
 *
 * Tests the memory inspection functionality including analyzing,
 * searching, comparing, checking, and region listing tools.
 */

#include "cli/service/agent/tools/memory_inspector_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

using ::testing::Contains;
using ::testing::Ge;
using ::testing::HasSubstr;
using ::testing::Le;
using ::testing::Not;
using ::testing::SizeIs;

// =============================================================================
// ALTTPMemoryMap Tests
// =============================================================================

TEST(ALTTPMemoryMapTest, WRAMBoundsAreCorrect) {
  EXPECT_EQ(ALTTPMemoryMap::kWRAMStart, 0x7E0000u);
  EXPECT_EQ(ALTTPMemoryMap::kWRAMEnd, 0x7FFFFFu);
}

TEST(ALTTPMemoryMapTest, IsWRAMDetectsValidAddresses) {
  // Valid WRAM addresses
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(0x7E0000));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(0x7E8000));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(0x7FFFFF));

  // Invalid addresses (outside WRAM)
  EXPECT_FALSE(ALTTPMemoryMap::IsWRAM(0x000000));
  EXPECT_FALSE(ALTTPMemoryMap::IsWRAM(0x7DFFFF));
  EXPECT_FALSE(ALTTPMemoryMap::IsWRAM(0x800000));
}

TEST(ALTTPMemoryMapTest, IsSpriteTableDetectsValidAddresses) {
  // Valid sprite table addresses
  EXPECT_TRUE(ALTTPMemoryMap::IsSpriteTable(0x7E0D00));
  EXPECT_TRUE(ALTTPMemoryMap::IsSpriteTable(0x7E0D50));
  EXPECT_TRUE(ALTTPMemoryMap::IsSpriteTable(0x7E0FFF));

  // Invalid addresses (outside sprite table)
  EXPECT_FALSE(ALTTPMemoryMap::IsSpriteTable(0x7E0CFF));
  EXPECT_FALSE(ALTTPMemoryMap::IsSpriteTable(0x7E1000));
}

TEST(ALTTPMemoryMapTest, IsSaveDataDetectsValidAddresses) {
  // Valid save data addresses
  EXPECT_TRUE(ALTTPMemoryMap::IsSaveData(0x7EF000));
  EXPECT_TRUE(ALTTPMemoryMap::IsSaveData(0x7EF360));  // Rupees
  EXPECT_TRUE(ALTTPMemoryMap::IsSaveData(0x7EF4FF));

  // Invalid addresses (outside save data)
  EXPECT_FALSE(ALTTPMemoryMap::IsSaveData(0x7EEFFF));
  EXPECT_FALSE(ALTTPMemoryMap::IsSaveData(0x7EF500));
}

TEST(ALTTPMemoryMapTest, KnownAddressesAreInWRAM) {
  // All known addresses should be in WRAM range
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(ALTTPMemoryMap::kGameMode));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(ALTTPMemoryMap::kSubmodule));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(ALTTPMemoryMap::kLinkXLow));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(ALTTPMemoryMap::kLinkYLow));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(ALTTPMemoryMap::kPlayerHealth));
  EXPECT_TRUE(ALTTPMemoryMap::IsWRAM(ALTTPMemoryMap::kSpriteType));
}

TEST(ALTTPMemoryMapTest, MaxSpritesIsCorrect) {
  EXPECT_EQ(ALTTPMemoryMap::kMaxSprites, 16);
}

TEST(ALTTPMemoryMapTest, PlayerAddressesAreConsistent) {
  // X and Y coordinates should be adjacent
  EXPECT_EQ(ALTTPMemoryMap::kLinkXHigh - ALTTPMemoryMap::kLinkXLow, 1u);
  EXPECT_EQ(ALTTPMemoryMap::kLinkYHigh - ALTTPMemoryMap::kLinkYLow, 1u);
}

// =============================================================================
// MemoryRegionInfo Structure Tests
// =============================================================================

TEST(MemoryRegionInfoTest, StructureHasExpectedFields) {
  MemoryRegionInfo info;
  info.name = "Test Region";
  info.description = "Test description";
  info.start_address = 0x7E0000;
  info.end_address = 0x7EFFFF;
  info.data_type = "byte";

  EXPECT_EQ(info.name, "Test Region");
  EXPECT_EQ(info.description, "Test description");
  EXPECT_EQ(info.start_address, 0x7E0000u);
  EXPECT_EQ(info.end_address, 0x7EFFFFu);
  EXPECT_EQ(info.data_type, "byte");
}

// =============================================================================
// MemoryAnomaly Structure Tests
// =============================================================================

TEST(MemoryAnomalyTest, StructureHasExpectedFields) {
  MemoryAnomaly anomaly;
  anomaly.address = 0x7E0D00;
  anomaly.type = "out_of_bounds";
  anomaly.description = "Sprite X position out of bounds";
  anomaly.severity = 3;

  EXPECT_EQ(anomaly.address, 0x7E0D00u);
  EXPECT_EQ(anomaly.type, "out_of_bounds");
  EXPECT_THAT(anomaly.description, HasSubstr("Sprite"));
  EXPECT_THAT(anomaly.severity, Ge(1));
  EXPECT_THAT(anomaly.severity, Le(5));
}

// =============================================================================
// PatternMatch Structure Tests
// =============================================================================

TEST(PatternMatchTest, StructureHasExpectedFields) {
  PatternMatch match;
  match.address = 0x7E0D00;
  match.matched_bytes = {0x12, 0x34, 0x56};
  match.context = "Sprite Table";

  EXPECT_EQ(match.address, 0x7E0D00u);
  EXPECT_THAT(match.matched_bytes, SizeIs(3));
  EXPECT_EQ(match.context, "Sprite Table");
}

// =============================================================================
// MemoryAnalyzeTool Tests
// =============================================================================

TEST(MemoryAnalyzeToolTest, GetNameReturnsCorrectName) {
  MemoryAnalyzeTool tool;
  EXPECT_EQ(tool.GetName(), "memory-analyze");
}

TEST(MemoryAnalyzeToolTest, GetUsageContainsAddress) {
  MemoryAnalyzeTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--address"));
}

TEST(MemoryAnalyzeToolTest, GetUsageContainsLength) {
  MemoryAnalyzeTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--length"));
}

TEST(MemoryAnalyzeToolTest, GetDescriptionIsNotEmpty) {
  MemoryAnalyzeTool tool;
  EXPECT_FALSE(tool.GetDescription().empty());
}

TEST(MemoryAnalyzeToolTest, DoesNotRequireLabels) {
  MemoryAnalyzeTool tool;
  EXPECT_FALSE(tool.RequiresLabels());
}

// =============================================================================
// MemorySearchTool Tests
// =============================================================================

TEST(MemorySearchToolTest, GetNameReturnsCorrectName) {
  MemorySearchTool tool;
  EXPECT_EQ(tool.GetName(), "memory-search");
}

TEST(MemorySearchToolTest, GetUsageContainsPattern) {
  MemorySearchTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--pattern"));
}

TEST(MemorySearchToolTest, GetUsageContainsStartEnd) {
  MemorySearchTool tool;
  std::string usage = tool.GetUsage();
  EXPECT_THAT(usage, HasSubstr("--start"));
  EXPECT_THAT(usage, HasSubstr("--end"));
}

TEST(MemorySearchToolTest, GetDescriptionIsNotEmpty) {
  MemorySearchTool tool;
  EXPECT_FALSE(tool.GetDescription().empty());
}

TEST(MemorySearchToolTest, DoesNotRequireLabels) {
  MemorySearchTool tool;
  EXPECT_FALSE(tool.RequiresLabels());
}

// =============================================================================
// MemoryCompareTool Tests
// =============================================================================

TEST(MemoryCompareToolTest, GetNameReturnsCorrectName) {
  MemoryCompareTool tool;
  EXPECT_EQ(tool.GetName(), "memory-compare");
}

TEST(MemoryCompareToolTest, GetUsageContainsAddress) {
  MemoryCompareTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--address"));
}

TEST(MemoryCompareToolTest, GetUsageContainsExpected) {
  MemoryCompareTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--expected"));
}

TEST(MemoryCompareToolTest, GetDescriptionIsNotEmpty) {
  MemoryCompareTool tool;
  EXPECT_FALSE(tool.GetDescription().empty());
}

TEST(MemoryCompareToolTest, DoesNotRequireLabels) {
  MemoryCompareTool tool;
  EXPECT_FALSE(tool.RequiresLabels());
}

// =============================================================================
// MemoryCheckTool Tests
// =============================================================================

TEST(MemoryCheckToolTest, GetNameReturnsCorrectName) {
  MemoryCheckTool tool;
  EXPECT_EQ(tool.GetName(), "memory-check");
}

TEST(MemoryCheckToolTest, GetUsageContainsRegion) {
  MemoryCheckTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--region"));
}

TEST(MemoryCheckToolTest, GetDescriptionIsNotEmpty) {
  MemoryCheckTool tool;
  EXPECT_FALSE(tool.GetDescription().empty());
}

TEST(MemoryCheckToolTest, DoesNotRequireLabels) {
  MemoryCheckTool tool;
  EXPECT_FALSE(tool.RequiresLabels());
}

// =============================================================================
// MemoryRegionsTool Tests
// =============================================================================

TEST(MemoryRegionsToolTest, GetNameReturnsCorrectName) {
  MemoryRegionsTool tool;
  EXPECT_EQ(tool.GetName(), "memory-regions");
}

TEST(MemoryRegionsToolTest, GetUsageContainsFilter) {
  MemoryRegionsTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--filter"));
}

TEST(MemoryRegionsToolTest, GetUsageContainsFormat) {
  MemoryRegionsTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--format"));
}

TEST(MemoryRegionsToolTest, GetDescriptionIsNotEmpty) {
  MemoryRegionsTool tool;
  EXPECT_FALSE(tool.GetDescription().empty());
}

TEST(MemoryRegionsToolTest, DoesNotRequireLabels) {
  MemoryRegionsTool tool;
  EXPECT_FALSE(tool.RequiresLabels());
}

// =============================================================================
// Tool Name Uniqueness Tests
// =============================================================================

TEST(MemoryToolsTest, AllToolNamesAreUnique) {
  MemoryAnalyzeTool analyze;
  MemorySearchTool search;
  MemoryCompareTool compare;
  MemoryCheckTool check;
  MemoryRegionsTool regions;

  std::vector<std::string> names = {
      analyze.GetName(), search.GetName(), compare.GetName(),
      check.GetName(), regions.GetName()};

  // Check all names are unique
  std::set<std::string> unique_names(names.begin(), names.end());
  EXPECT_EQ(unique_names.size(), names.size())
      << "All memory tool names should be unique";
}

TEST(MemoryToolsTest, AllToolNamesStartWithMemory) {
  MemoryAnalyzeTool analyze;
  MemorySearchTool search;
  MemoryCompareTool compare;
  MemoryCheckTool check;
  MemoryRegionsTool regions;

  // All memory tools should have names starting with "memory-"
  EXPECT_THAT(analyze.GetName(), HasSubstr("memory-"));
  EXPECT_THAT(search.GetName(), HasSubstr("memory-"));
  EXPECT_THAT(compare.GetName(), HasSubstr("memory-"));
  EXPECT_THAT(check.GetName(), HasSubstr("memory-"));
  EXPECT_THAT(regions.GetName(), HasSubstr("memory-"));
}

// =============================================================================
// Memory Address Constants Validation
// =============================================================================

TEST(ALTTPMemoryMapTest, SpriteTableAddressesAreSequential) {
  // Sprite tables should be at sequential offsets
  uint32_t sprite_y_low = ALTTPMemoryMap::kSpriteYLow;
  uint32_t sprite_x_low = ALTTPMemoryMap::kSpriteXLow;
  uint32_t sprite_y_high = ALTTPMemoryMap::kSpriteYHigh;
  uint32_t sprite_x_high = ALTTPMemoryMap::kSpriteXHigh;

  // Each table is 16 bytes (one per sprite)
  EXPECT_EQ(sprite_x_low - sprite_y_low, 0x10u);
  EXPECT_EQ(sprite_y_high - sprite_x_low, 0x10u);
  EXPECT_EQ(sprite_x_high - sprite_y_high, 0x10u);
}

TEST(ALTTPMemoryMapTest, OAMBufferSizeIsCorrect) {
  uint32_t oam_size =
      ALTTPMemoryMap::kOAMBufferEnd - ALTTPMemoryMap::kOAMBuffer + 1;
  // OAM buffer should be 544 bytes (512 for main OAM + 32 for high table)
  EXPECT_EQ(oam_size, 0x220u);  // 544 bytes
}

TEST(ALTTPMemoryMapTest, SRAMRegionSizeIsCorrect) {
  uint32_t sram_size =
      ALTTPMemoryMap::kSRAMEnd - ALTTPMemoryMap::kSRAMStart + 1;
  // SRAM region should be 0x500 bytes (1280 bytes)
  EXPECT_EQ(sram_size, 0x500u);
}

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
