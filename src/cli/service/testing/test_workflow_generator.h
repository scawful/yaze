// test_workflow_generator.h
// Converts natural language prompts into GUI automation workflows

#ifndef YAZE_CLI_SERVICE_TEST_WORKFLOW_GENERATOR_H
#define YAZE_CLI_SERVICE_TEST_WORKFLOW_GENERATOR_H

#include "absl/status/statusor.h"

#include <string>
#include <vector>

namespace yaze {
namespace cli {

/**
 * @brief Type of test step to execute
 */
enum class TestStepType {
  kClick,      // Click a button or element
  kType,       // Type text into an input
  kWait,       // Wait for a condition
  kAssert,     // Assert a condition is true
  kScreenshot  // Capture a screenshot
};

/**
 * @brief A single step in a GUI test workflow
 */
struct TestStep {
  TestStepType type;
  std::string target;        // Widget/element target (e.g., "button:Overworld")
  std::string text;          // Text to type (for kType steps)
  std::string condition;     // Condition to wait for or assert
  int timeout_ms = 5000;     // Timeout for wait operations
  bool clear_first = false;  // Clear text before typing

  std::string ToString() const;
};

/**
 * @brief A complete GUI test workflow
 */
struct TestWorkflow {
  std::string description;
  std::vector<TestStep> steps;

  std::string ToString() const;
};

/**
 * @brief Generates GUI test workflows from natural language prompts
 * 
 * This class uses pattern matching to convert user prompts into
 * structured test workflows that can be executed by GuiAutomationClient.
 * 
 * Example prompts:
 * - "Open Overworld editor" → Click button, Wait for window
 * - "Open Dungeon editor and verify it loads" → Click, Wait, Assert
 * - "Type 'zelda3.sfc' in filename input" → Click input, Type text
 * 
 * Usage:
 * @code
 *   TestWorkflowGenerator generator;
 *   auto workflow = generator.GenerateWorkflow("Open Overworld editor");
 *   if (!workflow.ok()) return workflow.status();
 *   
 *   for (const auto& step : workflow->steps) {
 *     std::cout << step.ToString() << "\n";
 *   }
 * @endcode
 */
class TestWorkflowGenerator {
 public:
  TestWorkflowGenerator() = default;

  /**
   * @brief Generate a test workflow from a natural language prompt
   * @param prompt Natural language description of desired GUI actions
   * @return TestWorkflow or error if prompt is unsupported
   */
  absl::StatusOr<TestWorkflow> GenerateWorkflow(const std::string& prompt);

 private:
  // Pattern matchers for different prompt types
  bool MatchesOpenEditor(const std::string& prompt, std::string* editor_name);
  bool MatchesOpenAndVerify(const std::string& prompt,
                            std::string* editor_name);
  bool MatchesTypeInput(const std::string& prompt, std::string* input_name,
                        std::string* text);
  bool MatchesClickButton(const std::string& prompt, std::string* button_name);
  bool MatchesMultiStep(const std::string& prompt);

  // Workflow builders
  TestWorkflow BuildOpenEditorWorkflow(const std::string& editor_name);
  TestWorkflow BuildOpenAndVerifyWorkflow(const std::string& editor_name);
  TestWorkflow BuildTypeInputWorkflow(const std::string& input_name,
                                      const std::string& text);
  TestWorkflow BuildClickButtonWorkflow(const std::string& button_name);

  // Helper to normalize editor names (e.g., "overworld" → "Overworld Editor")
  std::string NormalizeEditorName(const std::string& name);
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_TEST_WORKFLOW_GENERATOR_H
