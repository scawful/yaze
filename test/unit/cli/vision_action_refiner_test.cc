#include "cli/service/ai/vision_action_refiner.h"

#include "cli/service/ai/gemini_ai_service.h"
#include "gtest/gtest.h"

namespace yaze {
namespace cli {
namespace ai {

class VisionActionRefinerTest : public ::testing::Test {
 protected:
  VisionActionRefinerTest()
      : gemini_service_(GeminiConfig{}), refiner_(&gemini_service_) {}

  VisionAnalysisResult ParseAnalysis(const std::string& response) {
    return refiner_.ParseAnalysisResponse(response);
  }

  VisionAnalysisResult ParseVerification(const std::string& response) {
    AIAction action(AIActionType::kClickButton, {{"button", "save"}});
    return refiner_.ParseVerificationResponse(response, action);
  }

  GeminiAIService gemini_service_;
  VisionActionRefiner refiner_;
};

TEST_F(VisionActionRefinerTest, ParsesAnalysisWidgetsAndSuggestions) {
  const std::string response =
      "Window: Overworld Editor\n"
      "  - Button: Save\n"
      "Suggestion: try clicking Save\n";

  VisionAnalysisResult result = ParseAnalysis(response);

  EXPECT_EQ(result.description, response);
  ASSERT_EQ(result.widgets.size(), 2u);
  EXPECT_EQ(result.widgets[0], "Window: Overworld Editor");
  EXPECT_EQ(result.widgets[1], "- Button: Save");
  ASSERT_EQ(result.suggestions.size(), 1u);
  EXPECT_EQ(result.suggestions[0], "Suggestion: try clicking Save");
}

TEST_F(VisionActionRefinerTest, ParsesVerificationSuccess) {
  VisionAnalysisResult result = ParseVerification("SUCCESS: Tile placed");

  EXPECT_TRUE(result.action_successful);
  EXPECT_EQ(result.description, "Tile placed");
  EXPECT_TRUE(result.error_message.empty());
}

TEST_F(VisionActionRefinerTest, ParsesVerificationFailureWithReason) {
  VisionAnalysisResult result = ParseVerification("FAILURE: Element not found");

  EXPECT_FALSE(result.action_successful);
  EXPECT_EQ(result.error_message, "Element not found");
}

TEST_F(VisionActionRefinerTest, ParsesVerificationAmbiguousResponse) {
  VisionAnalysisResult result = ParseVerification("No visible change");

  EXPECT_FALSE(result.action_successful);
  EXPECT_EQ(result.error_message,
            "Could not determine action success from vision analysis");
}

}  // namespace ai
}  // namespace cli
}  // namespace yaze
