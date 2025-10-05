#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "cli/service/ai/ai_action_parser.h"
#include "cli/service/gui/gui_action_generator.h"

#ifdef YAZE_WITH_GRPC
#include "cli/service/gui/gui_automation_client.h"
#endif

namespace yaze {
namespace test {

class AITilePlacementTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_ai_tile_test";
    std::filesystem::create_directories(test_dir_);
  }
  
  void TearDown() override {
    if (std::filesystem::exists(test_dir_)) {
      std::filesystem::remove_all(test_dir_);
    }
  }
  
  std::filesystem::path test_dir_;
};

TEST_F(AITilePlacementTest, ParsePlaceTileCommand) {
  std::string command = "Place tile 0x42 at position (5, 7)";
  
  auto actions = cli::ai::AIActionParser::ParseCommand(command);
  ASSERT_TRUE(actions.ok()) << actions.status().message();
  
  // Should generate: SelectTile, PlaceTile, SaveTile
  ASSERT_EQ(actions->size(), 3);
  
  EXPECT_EQ((*actions)[0].type, cli::ai::AIActionType::kSelectTile);
  EXPECT_EQ((*actions)[0].parameters.at("tile_id"), "66");  // 0x42 = 66
  
  EXPECT_EQ((*actions)[1].type, cli::ai::AIActionType::kPlaceTile);
  EXPECT_EQ((*actions)[1].parameters.at("x"), "5");
  EXPECT_EQ((*actions)[1].parameters.at("y"), "7");
  
  EXPECT_EQ((*actions)[2].type, cli::ai::AIActionType::kSaveTile);
}

TEST_F(AITilePlacementTest, GenerateTestScript) {
  std::string command = "Place tile 100 at position (10, 15)";
  
  auto actions = cli::ai::AIActionParser::ParseCommand(command);
  ASSERT_TRUE(actions.ok());
  
  cli::gui::GuiActionGenerator generator;
  auto script = generator.GenerateTestScript(*actions);
  
  ASSERT_TRUE(script.ok()) << script.status().message();
  
  // Verify it's valid JSON
  #ifdef YAZE_WITH_JSON
  nlohmann::json parsed;
  ASSERT_NO_THROW(parsed = nlohmann::json::parse(*script));
  
  ASSERT_TRUE(parsed.contains("steps"));
  ASSERT_TRUE(parsed["steps"].is_array());
  EXPECT_EQ(parsed["steps"].size(), 3);
  
  // Verify first step is select tile
  EXPECT_EQ(parsed["steps"][0]["action"], "click");
  EXPECT_EQ(parsed["steps"][0]["target"], "canvas:tile16_selector");
  
  // Verify second step is place tile  
  EXPECT_EQ(parsed["steps"][1]["action"], "click");
  EXPECT_EQ(parsed["steps"][1]["target"], "canvas:overworld_map");
  EXPECT_EQ(parsed["steps"][1]["position"]["x"], 168);  // 10 * 16 + 8
  EXPECT_EQ(parsed["steps"][1]["position"]["y"], 248);  // 15 * 16 + 8
  
  // Verify third step is save
  EXPECT_EQ(parsed["steps"][2]["action"], "click");
  EXPECT_EQ(parsed["steps"][2]["target"], "button:Save to ROM");
  #endif
}

TEST_F(AITilePlacementTest, ParseMultipleFormats) {
  std::vector<std::string> commands = {
    "Place tile 0x10 at (3, 4)",
    "Put tile 20 at position 3,4",
    "Set tile 30 at x=3 y=4",
    "Place tile 40 at overworld 0 position (3, 4)"
  };
  
  for (const auto& cmd : commands) {
    auto actions = cli::ai::AIActionParser::ParseCommand(cmd);
    EXPECT_TRUE(actions.ok()) << "Failed to parse: " << cmd 
                              << " - " << actions.status().message();
    if (actions.ok()) {
      EXPECT_GE(actions->size(), 2) << "Command: " << cmd;
    }
  }
}

TEST_F(AITilePlacementTest, GenerateActionDescription) {
  cli::ai::AIAction select_action(cli::ai::AIActionType::kSelectTile);
  select_action.parameters["tile_id"] = "42";
  
  std::string desc = cli::ai::AIActionParser::ActionToString(select_action);
  EXPECT_EQ(desc, "Select tile 42");
  
  cli::ai::AIAction place_action(cli::ai::AIActionType::kPlaceTile);
  place_action.parameters["x"] = "5";
  place_action.parameters["y"] = "7";
  
  desc = cli::ai::AIActionParser::ActionToString(place_action);
  EXPECT_EQ(desc, "Place tile at position (5, 7)");
}

#ifdef YAZE_WITH_GRPC
// Integration test with actual gRPC test harness
// This test requires YAZE to be running with test harness enabled
TEST_F(AITilePlacementTest, DISABLED_ExecuteViaGRPC) {
  // This test is disabled by default as it requires YAZE to be running
  // Enable it manually when testing with a running instance
  
  std::string command = "Place tile 50 at position (2, 3)";
  
  // Parse command
  auto actions = cli::ai::AIActionParser::ParseCommand(command);
  ASSERT_TRUE(actions.ok());
  
  // Generate test script
  cli::gui::GuiActionGenerator generator;
  auto script_json = generator.GenerateTestJSON(*actions);
  ASSERT_TRUE(script_json.ok());
  
  // Connect to test harness
  cli::gui::GuiAutomationClient client("localhost:50051");
  
  // Execute each step
  for (const auto& step : (*script_json)["steps"]) {
    if (step["action"] == "click") {
      std::string target = step["target"];
      // Execute click via gRPC
      // (Implementation depends on GuiAutomationClient interface)
    } else if (step["action"] == "wait") {
      int duration_ms = step["duration_ms"];
      std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
  }
  
  // Verify tile was placed
  // (Would require ROM inspection via gRPC)
}
#endif

}  // namespace test
}  // namespace yaze

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  
  std::cout << "\n=== AI Tile Placement Tests ===" << std::endl;
  std::cout << "Testing AI command parsing and GUI action generation.\n" << std::endl;
  
  return RUN_ALL_TESTS();
}
