// Integration tests for AIGUIController
// Tests the gRPC GUI automation with vision feedback

#include "cli/service/ai/ai_gui_controller.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/gui/gui_automation_client.h"

namespace yaze {
namespace cli {
namespace ai {
namespace {

using ::testing::_;
using ::testing::Return;

// Mock GuiAutomationClient for testing without actual GUI
class MockGuiAutomationClient : public GuiAutomationClient {
 public:
  MockGuiAutomationClient() : GuiAutomationClient("localhost:50052") {}

  MOCK_METHOD(absl::Status, Connect, ());
  MOCK_METHOD(absl::StatusOr<AutomationResult>, Ping, (const std::string&));
  MOCK_METHOD(absl::StatusOr<AutomationResult>, Click,
              (const std::string&, ClickType));
  MOCK_METHOD(absl::StatusOr<AutomationResult>, Type,
              (const std::string&, const std::string&, bool));
  MOCK_METHOD(absl::StatusOr<AutomationResult>, Wait,
              (const std::string&, int, int));
  MOCK_METHOD(absl::StatusOr<AutomationResult>, Assert, (const std::string&));
};

class AIGUIControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create mock services
    GeminiConfig config;
    config.api_key = "test_key";
    config.model = "gemini-2.5-flash";
    gemini_service_ = std::make_unique<GeminiAIService>(config);

    gui_client_ = std::make_unique<MockGuiAutomationClient>();

    controller_ = std::make_unique<AIGUIController>(gemini_service_.get(),
                                                    gui_client_.get());

    ControlLoopConfig loop_config;
    loop_config.max_iterations = 5;
    loop_config.enable_vision_verification = false;  // Disable for unit tests
    loop_config.enable_iterative_refinement = false;
    controller_->Initialize(loop_config);
  }

  std::unique_ptr<GeminiAIService> gemini_service_;
  std::unique_ptr<MockGuiAutomationClient> gui_client_;
  std::unique_ptr<AIGUIController> controller_;
};

// ============================================================================
// Basic Action Execution Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecuteClickAction_Success) {
  AIAction action(AIActionType::kClickButton);
  action.parameters["target"] = "button:Test";
  action.parameters["click_type"] = "left";

  AutomationResult result;
  result.success = true;
  result.message = "Click successful";

  EXPECT_CALL(*gui_client_, Click("button:Test", ClickType::kLeft))
      .WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  ASSERT_TRUE(status.ok()) << status.status().message();
  EXPECT_TRUE(status->action_successful);
}

TEST_F(AIGUIControllerTest, ExecuteClickAction_Failure) {
  AIAction action(AIActionType::kClickButton);
  action.parameters["target"] = "button:NonExistent";

  AutomationResult result;
  result.success = false;
  result.message = "Button not found";

  EXPECT_CALL(*gui_client_, Click("button:NonExistent", ClickType::kLeft))
      .WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Click action failed"));
}

// ============================================================================
// Type Action Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecuteTypeAction_Success) {
  AIAction action(
      AIActionType::kSelectTile);  // Using SelectTile as a type action
  action.parameters["target"] = "input:TileID";
  action.parameters["text"] = "0x42";
  action.parameters["clear_first"] = "true";

  AutomationResult result;
  result.success = true;
  result.message = "Text entered";

  EXPECT_CALL(*gui_client_, Type("input:TileID", "0x42", true))
      .WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(status->action_successful);
}

// ============================================================================
// Wait Action Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecuteWaitAction_Success) {
  AIAction action(AIActionType::kWait);
  action.parameters["condition"] = "window:OverworldEditor";
  action.parameters["timeout_ms"] = "2000";

  AutomationResult result;
  result.success = true;
  result.message = "Condition met";

  EXPECT_CALL(*gui_client_, Wait("window:OverworldEditor", 2000, 100))
      .WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(status->action_successful);
}

TEST_F(AIGUIControllerTest, ExecuteWaitAction_Timeout) {
  AIAction action(AIActionType::kWait);
  action.parameters["condition"] = "window:NonExistentWindow";
  action.parameters["timeout_ms"] = "100";

  AutomationResult result;
  result.success = false;
  result.message = "Timeout waiting for condition";

  EXPECT_CALL(*gui_client_, Wait("window:NonExistentWindow", 100, 100))
      .WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  EXPECT_FALSE(status.ok());
}

// ============================================================================
// Verify/Assert Action Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecuteVerifyAction_Success) {
  AIAction action(AIActionType::kVerifyTile);
  action.parameters["condition"] = "tile_placed";

  AutomationResult result;
  result.success = true;
  result.message = "Assertion passed";
  result.expected_value = "0x42";
  result.actual_value = "0x42";

  EXPECT_CALL(*gui_client_, Assert("tile_placed")).WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(status->action_successful);
}

TEST_F(AIGUIControllerTest, ExecuteVerifyAction_Failure) {
  AIAction action(AIActionType::kVerifyTile);
  action.parameters["condition"] = "tile_placed";

  AutomationResult result;
  result.success = false;
  result.message = "Assertion failed";
  result.expected_value = "0x42";
  result.actual_value = "0x00";

  EXPECT_CALL(*gui_client_, Assert("tile_placed")).WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Assert action failed"));
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("expected: 0x42"));
  EXPECT_THAT(status.status().message(), ::testing::HasSubstr("actual: 0x00"));
}

// ============================================================================
// Complex Tile Placement Action Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecutePlaceTileAction_CompleteFlow) {
  AIAction action(AIActionType::kPlaceTile);
  action.parameters["map_id"] = "5";
  action.parameters["x"] = "10";
  action.parameters["y"] = "20";
  action.parameters["tile"] = "0x42";

  AutomationResult result;
  result.success = true;

  // Expect sequence: open menu, wait for window, set map ID, click position
  testing::InSequence seq;

  EXPECT_CALL(*gui_client_, Click("menu:Overworld", ClickType::kLeft))
      .WillOnce(Return(result));

  EXPECT_CALL(*gui_client_, Wait("window:Overworld Editor", 2000, 100))
      .WillOnce(Return(result));

  EXPECT_CALL(*gui_client_, Type("input:Map ID", "5", true))
      .WillOnce(Return(result));

  EXPECT_CALL(*gui_client_, Click(::testing::_, ClickType::kLeft))
      .WillOnce(Return(result));

  auto status = controller_->ExecuteSingleAction(action, false);

  ASSERT_TRUE(status.ok()) << status.status().message();
  EXPECT_TRUE(status->action_successful);
}

// ============================================================================
// Multiple Actions Execution Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecuteActions_MultipleActionsSuccess) {
  std::vector<AIAction> actions;

  AIAction action1(AIActionType::kClickButton);
  action1.parameters["target"] = "button:Overworld";
  actions.push_back(action1);

  AIAction action2(AIActionType::kWait);
  action2.parameters["condition"] = "window:OverworldEditor";
  actions.push_back(action2);

  AutomationResult success_result;
  success_result.success = true;

  EXPECT_CALL(*gui_client_, Click("button:Overworld", ClickType::kLeft))
      .WillOnce(Return(success_result));

  EXPECT_CALL(*gui_client_, Wait("window:OverworldEditor", 5000, 100))
      .WillOnce(Return(success_result));

  auto result = controller_->ExecuteActions(actions);

  ASSERT_TRUE(result.ok()) << result.status().message();
  EXPECT_TRUE(result->success);
  EXPECT_EQ(result->actions_executed.size(), 2);
}

TEST_F(AIGUIControllerTest, ExecuteActions_StopsOnFirstFailure) {
  std::vector<AIAction> actions;

  AIAction action1(AIActionType::kClickButton);
  action1.parameters["target"] = "button:Test";
  actions.push_back(action1);

  AIAction action2(AIActionType::kClickButton);
  action2.parameters["target"] = "button:NeverReached";
  actions.push_back(action2);

  AutomationResult failure_result;
  failure_result.success = false;
  failure_result.message = "First action failed";

  EXPECT_CALL(*gui_client_, Click("button:Test", ClickType::kLeft))
      .WillOnce(Return(failure_result));

  // Second action should never be called
  EXPECT_CALL(*gui_client_, Click("button:NeverReached", _)).Times(0);

  auto result = controller_->ExecuteActions(actions);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result->actions_executed.size(), 1);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(AIGUIControllerTest, ExecuteAction_InvalidActionType) {
  AIAction action(AIActionType::kInvalidAction);

  auto status = controller_->ExecuteSingleAction(action, false);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Action type not implemented"));
}

TEST_F(AIGUIControllerTest, ExecutePlaceTileAction_MissingParameters) {
  AIAction action(AIActionType::kPlaceTile);
  // Missing required parameters

  auto status = controller_->ExecuteSingleAction(action, false);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("requires map_id, x, y, and tile"));
}

}  // namespace
}  // namespace ai
}  // namespace cli
}  // namespace yaze
