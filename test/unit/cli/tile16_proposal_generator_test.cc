// Test suite for Tile16ProposalGenerator
// Tests the new ParseSetAreaCommand and ParseReplaceTileCommand functionality

#include "cli/service/planning/tile16_proposal_generator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/rom.h"
#include "test/mocks/mock_rom.h"

namespace yaze {
namespace cli {
namespace {

using ::testing::_;
using ::testing::Return;

class Tile16ProposalGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    generator_ = std::make_unique<Tile16ProposalGenerator>();
  }

  std::unique_ptr<Tile16ProposalGenerator> generator_;
};

// ============================================================================
// ParseSetTileCommand Tests
// ============================================================================

TEST_F(Tile16ProposalGeneratorTest, ParseSetTileCommand_ValidCommand) {
  std::string command = "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E";
  
  auto result = generator_->ParseSetTileCommand(command, nullptr);
  
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(result->map_id, 0);
  EXPECT_EQ(result->x, 10);
  EXPECT_EQ(result->y, 20);
  EXPECT_EQ(result->new_tile, 0x02E);
}

TEST_F(Tile16ProposalGeneratorTest, ParseSetTileCommand_InvalidFormat) {
  std::string command = "overworld set-tile --map 0";  // Missing required args
  
  auto result = generator_->ParseSetTileCommand(command, nullptr);
  
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(), 
              ::testing::HasSubstr("Invalid command format"));
}

TEST_F(Tile16ProposalGeneratorTest, ParseSetTileCommand_WrongCommandType) {
  std::string command = "overworld get-tile --map 0 --x 10 --y 20";
  
  auto result = generator_->ParseSetTileCommand(command, nullptr);
  
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(),
              ::testing::HasSubstr("Not a set-tile command"));
}

// ============================================================================
// ParseSetAreaCommand Tests
// ============================================================================

TEST_F(Tile16ProposalGeneratorTest, ParseSetAreaCommand_ValidCommand) {
  std::string command = 
      "overworld set-area --map 0 --x 10 --y 20 --width 5 --height 3 --tile 0x02E";
  
  auto result = generator_->ParseSetAreaCommand(command, nullptr);
  
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(result->size(), 15);  // 5 width * 3 height = 15 tiles
  
  // Check first tile
  EXPECT_EQ((*result)[0].map_id, 0);
  EXPECT_EQ((*result)[0].x, 10);
  EXPECT_EQ((*result)[0].y, 20);
  EXPECT_EQ((*result)[0].new_tile, 0x02E);
  
  // Check last tile
  EXPECT_EQ((*result)[14].x, 14);  // 10 + 4
  EXPECT_EQ((*result)[14].y, 22);  // 20 + 2
}

TEST_F(Tile16ProposalGeneratorTest, ParseSetAreaCommand_SingleTile) {
  std::string command = 
      "overworld set-area --map 0 --x 10 --y 20 --width 1 --height 1 --tile 0x02E";
  
  auto result = generator_->ParseSetAreaCommand(command, nullptr);
  
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 1);
}

TEST_F(Tile16ProposalGeneratorTest, ParseSetAreaCommand_LargeArea) {
  std::string command = 
      "overworld set-area --map 0 --x 0 --y 0 --width 32 --height 32 --tile 0x000";
  
  auto result = generator_->ParseSetAreaCommand(command, nullptr);
  
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 1024);  // 32 * 32
}

TEST_F(Tile16ProposalGeneratorTest, ParseSetAreaCommand_InvalidFormat) {
  std::string command = "overworld set-area --map 0 --x 10";  // Missing args
  
  auto result = generator_->ParseSetAreaCommand(command, nullptr);
  
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(),
              ::testing::HasSubstr("Invalid set-area command format"));
}

// ============================================================================
// ParseReplaceTileCommand Tests  
// ============================================================================

TEST_F(Tile16ProposalGeneratorTest, ParseReplaceTileCommand_NoROM) {
  std::string command = 
      "overworld replace-tile --map 0 --old-tile 0x02E --new-tile 0x030";
  
  auto result = generator_->ParseReplaceTileCommand(command, nullptr);
  
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(),
              ::testing::HasSubstr("ROM must be loaded"));
}

TEST_F(Tile16ProposalGeneratorTest, ParseReplaceTileCommand_InvalidFormat) {
  std::string command = "overworld replace-tile --map 0";  // Missing tiles
  
  auto result = generator_->ParseReplaceTileCommand(command, nullptr);
  
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(),
              ::testing::HasSubstr("Invalid replace-tile command format"));
}

// ============================================================================
// GenerateFromCommands Tests
// ============================================================================

TEST_F(Tile16ProposalGeneratorTest, GenerateFromCommands_MultipleCommands) {
  std::vector<std::string> commands = {
      "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E",
      "overworld set-area --map 0 --x 5 --y 5 --width 2 --height 2 --tile 0x030"
  };
  
  auto result = generator_->GenerateFromCommands(
      "Test prompt", commands, "test_ai", nullptr);
  
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(result->changes.size(), 5);  // 1 from set-tile + 4 from set-area
  EXPECT_EQ(result->prompt, "Test prompt");
  EXPECT_EQ(result->ai_service, "test_ai");
  EXPECT_EQ(result->status, Tile16Proposal::Status::PENDING);
}

TEST_F(Tile16ProposalGeneratorTest, GenerateFromCommands_EmptyCommands) {
  std::vector<std::string> commands = {};
  
  auto result = generator_->GenerateFromCommands(
      "Test prompt", commands, "test_ai", nullptr);
  
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.status().message(),
              ::testing::HasSubstr("No valid tile16 changes found"));
}

TEST_F(Tile16ProposalGeneratorTest, GenerateFromCommands_IgnoresComments) {
  std::vector<std::string> commands = {
      "# This is a comment",
      "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E",
      "# Another comment",
      ""  // Empty line
  };
  
  auto result = generator_->GenerateFromCommands(
      "Test prompt", commands, "test_ai", nullptr);
  
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->changes.size(), 1);  // Only the valid command
}

// ============================================================================
// Tile16Change Tests
// ============================================================================

TEST_F(Tile16ProposalGeneratorTest, Tile16Change_ToString) {
  Tile16Change change;
  change.map_id = 5;
  change.x = 10;
  change.y = 20;
  change.old_tile = 0x02E;
  change.new_tile = 0x030;
  
  std::string result = change.ToString();
  
  EXPECT_THAT(result, ::testing::HasSubstr("Map 5"));
  EXPECT_THAT(result, ::testing::HasSubstr("(10,20)"));
  EXPECT_THAT(result, ::testing::HasSubstr("0x2e"));
  EXPECT_THAT(result, ::testing::HasSubstr("0x30"));
}

// ============================================================================
// Proposal Serialization Tests
// ============================================================================

TEST_F(Tile16ProposalGeneratorTest, Proposal_ToJsonAndFromJson) {
  Tile16Proposal original;
  original.id = "test_id_123";
  original.prompt = "Test prompt";
  original.ai_service = "gemini";
  original.reasoning = "Test reasoning";
  original.status = Tile16Proposal::Status::PENDING;
  
  Tile16Change change;
  change.map_id = 5;
  change.x = 10;
  change.y = 20;
  change.old_tile = 0x02E;
  change.new_tile = 0x030;
  original.changes.push_back(change);
  
  std::string json = original.ToJson();
  auto result = Tile16Proposal::FromJson(json);
  
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(result->id, original.id);
  EXPECT_EQ(result->prompt, original.prompt);
  EXPECT_EQ(result->ai_service, original.ai_service);
  EXPECT_EQ(result->reasoning, original.reasoning);
  EXPECT_EQ(result->status, original.status);
  EXPECT_EQ(result->changes.size(), 1);
  EXPECT_EQ(result->changes[0].map_id, 5);
}

}  // namespace
}  // namespace cli
}  // namespace yaze
