// Coverage for the Tile16 proposal pipeline used by `z3ed agent plan`.
//
// This file was unregistered (listed only in the orphaned test/test.cmake) and
// was deleted in #241 as API-stale. That was true of its ParseSetTileCommand,
// ParseSetAreaCommand and ParseReplaceTileCommand cases, which reach private
// methods through a friend declaration that names a differently-scoped class.
// The cases below use only public API and are restored and registered here, so
// the command-list path and proposal serialization are pinned by a running
// test.

#include "cli/service/planning/tile16_proposal_generator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace yaze {
namespace cli {
namespace {

class Tile16ProposalGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    generator_ = std::make_unique<Tile16ProposalGenerator>();
  }

  std::unique_ptr<Tile16ProposalGenerator> generator_;
};

TEST_F(Tile16ProposalGeneratorTest, GenerateFromCommands_MultipleCommands) {
  std::vector<std::string> commands = {
      "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E",
      "overworld set-area --map 0 --x 5 --y 5 --width 2 --height 2 --tile "
      "0x030"};

  auto result = generator_->GenerateFromCommands("Test prompt", commands,
                                                 "test_ai", nullptr);

  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(result->changes.size(), 5);  // 1 from set-tile + 4 from set-area
  EXPECT_EQ(result->prompt, "Test prompt");
  EXPECT_EQ(result->ai_service, "test_ai");
  EXPECT_EQ(result->status, Tile16Proposal::Status::PENDING);
}

TEST_F(Tile16ProposalGeneratorTest, GenerateFromCommands_EmptyCommands) {
  std::vector<std::string> commands = {};

  auto result = generator_->GenerateFromCommands("Test prompt", commands,
                                                 "test_ai", nullptr);

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

  auto result = generator_->GenerateFromCommands("Test prompt", commands,
                                                 "test_ai", nullptr);

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->changes.size(), 1);  // Only the valid command
}

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
