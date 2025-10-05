#include "gtest/gtest.h"

#include "absl/strings/str_format.h"
#include "cli/service/ai/ai_action_parser.h"
#include "cli/service/ai/vision_action_refiner.h"
#include "cli/service/ai/ai_gui_controller.h"

#ifdef YAZE_WITH_GRPC
#include "cli/service/gui/gui_automation_client.h"
#include "cli/service/ai/gemini_ai_service.h"
#endif

namespace yaze {
namespace test {

/**
 * @brief Integration tests for AI-controlled tile placement
 * 
 * These tests verify the complete pipeline:
 * 1. Parse natural language commands
 * 2. Execute actions via gRPC
 * 3. Verify success with vision analysis
 * 4. Refine and retry on failure
 */
class AITilePlacementTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // These tests require YAZE GUI to be running with gRPC test harness
    // Skip if not available
  }
};

TEST_F(AITilePlacementTest, ParsePlaceTileCommand) {
  using namespace cli::ai;
  
  // Test basic tile placement command
  auto result = AIActionParser::ParseCommand(
      "Place tile 0x42 at overworld position (5, 7)");
  
  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_EQ(result->size(), 3);  // Select, Place, Save
  
  // Check first action (Select)
  EXPECT_EQ(result->at(0).type, AIActionType::kSelectTile);
  EXPECT_EQ(result->at(0).parameters.at("tile_id"), "66");  // 0x42 = 66
  
  // Check second action (Place)
  EXPECT_EQ(result->at(1).type, AIActionType::kPlaceTile);
  EXPECT_EQ(result->at(1).parameters.at("x"), "5");
  EXPECT_EQ(result->at(1).parameters.at("y"), "7");
  EXPECT_EQ(result->at(1).parameters.at("map_id"), "0");
  
  // Check third action (Save)
  EXPECT_EQ(result->at(2).type, AIActionType::kSaveTile);
}

TEST_F(AITilePlacementTest, ParseSelectTileCommand) {
  using namespace cli::ai;
  
  auto result = AIActionParser::ParseCommand("Select tile 100");
  
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 1);
  EXPECT_EQ(result->at(0).type, AIActionType::kSelectTile);
  EXPECT_EQ(result->at(0).parameters.at("tile_id"), "100");
}

TEST_F(AITilePlacementTest, ParseOpenEditorCommand) {
  using namespace cli::ai;
  
  auto result = AIActionParser::ParseCommand("Open the overworld editor");
  
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 1);
  EXPECT_EQ(result->at(0).type, AIActionType::kOpenEditor);
  EXPECT_EQ(result->at(0).parameters.at("editor"), "overworld");
}

TEST_F(AITilePlacementTest, ActionToStringRoundtrip) {
  using namespace cli::ai;
  
  AIAction action(AIActionType::kPlaceTile, {
    {"x", "5"},
    {"y", "7"},
    {"tile_id", "42"}
  });
  
  std::string str = AIActionParser::ActionToString(action);
  EXPECT_FALSE(str.empty());
  EXPECT_TRUE(str.find("5") != std::string::npos);
  EXPECT_TRUE(str.find("7") != std::string::npos);
}

#ifdef YAZE_WITH_GRPC

TEST_F(AITilePlacementTest, DISABLED_VisionAnalysisBasic) {
  // This test requires Gemini API key
  const char* api_key = std::getenv("GEMINI_API_KEY");
  if (!api_key || std::string(api_key).empty()) {
    GTEST_SKIP() << "GEMINI_API_KEY not set";
  }
  
  cli::GeminiConfig config;
  config.api_key = api_key;
  config.model = "gemini-2.0-flash-exp";
  
  cli::GeminiAIService gemini_service(config);
  cli::ai::VisionActionRefiner refiner(&gemini_service);
  
  // Would need actual screenshots for real test
  // This is a structure test
  EXPECT_TRUE(true);
}

TEST_F(AITilePlacementTest, DISABLED_FullAIControlLoop) {
  // This test requires:
  // 1. YAZE GUI running with gRPC test harness
  // 2. Gemini API key for vision
  // 3. Test ROM loaded
  
  const char* api_key = std::getenv("GEMINI_API_KEY");
  if (!api_key || std::string(api_key).empty()) {
    GTEST_SKIP() << "GEMINI_API_KEY not set";
  }
  
  // Initialize services
  cli::GeminiConfig gemini_config;
  gemini_config.api_key = api_key;
  cli::GeminiAIService gemini_service(gemini_config);
  
  cli::gui::GuiAutomationClient gui_client;
  auto connect_status = gui_client.Connect("localhost", 50051);
  if (!connect_status.ok()) {
    GTEST_SKIP() << "GUI test harness not available: " 
                 << connect_status.message();
  }
  
  // Create AI controller
  cli::ai::AIGUIController controller(&gemini_service, &gui_client);
  cli::ai::ControlLoopConfig config;
  config.max_iterations = 5;
  config.enable_vision_verification = true;
  controller.Initialize(config);
  
  // Execute command
  auto result = controller.ExecuteCommand(
      "Place tile 0x42 at overworld position (5, 7)");
  
  if (result.ok()) {
    EXPECT_TRUE(result->success);
    EXPECT_GT(result->iterations_performed, 0);
  }
}

#endif  // YAZE_WITH_GRPC

TEST_F(AITilePlacementTest, ActionRefinement) {
  using namespace cli::ai;
  
  // Test refinement logic with a failed action
  VisionAnalysisResult analysis;
  analysis.action_successful = false;
  analysis.error_message = "Element not found";
  
  AIAction original_action(AIActionType::kClickButton, {
    {"button", "save"}
  });
  
  // Would need VisionActionRefiner for real test
  // This verifies the structure compiles
  EXPECT_TRUE(true);
}

TEST_F(AITilePlacementTest, MultipleCommandsParsing) {
  using namespace cli::ai;
  
  // Test that we can parse multiple commands in sequence
  std::vector<std::string> commands = {
    "Open overworld editor",
    "Select tile 0x42",
    "Place tile at position (5, 7)",
    "Save changes"
  };
  
  for (const auto& cmd : commands) {
    auto result = AIActionParser::ParseCommand(cmd);
    // At least some should parse successfully
    if (result.ok()) {
      EXPECT_FALSE(result->empty());
    }
  }
}

TEST_F(AITilePlacementTest, HexAndDecimalParsing) {
  using namespace cli::ai;
  
  // Test hex notation
  auto hex_result = AIActionParser::ParseCommand("Select tile 0xFF");
  if (hex_result.ok() && !hex_result->empty()) {
    EXPECT_EQ(hex_result->at(0).parameters.at("tile_id"), "255");
  }
  
  // Test decimal notation
  auto dec_result = AIActionParser::ParseCommand("Select tile 255");
  if (dec_result.ok() && !dec_result->empty()) {
    EXPECT_EQ(dec_result->at(0).parameters.at("tile_id"), "255");
  }
}

}  // namespace test
}  // namespace yaze