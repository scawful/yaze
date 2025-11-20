// test_workflow_generator.cc
// Implementation of natural language to test workflow conversion

#include "cli/service/testing/test_workflow_generator.h"

#include <regex>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"

namespace yaze {
namespace cli {

std::string TestStep::ToString() const {
  switch (type) {
    case TestStepType::kClick:
      return absl::StrFormat("Click(%s)", target);
    case TestStepType::kType:
      return absl::StrFormat("Type(%s, \"%s\"%s)", target, text,
                             clear_first ? ", clear_first" : "");
    case TestStepType::kWait:
      return absl::StrFormat("Wait(%s, %dms)", condition, timeout_ms);
    case TestStepType::kAssert:
      return absl::StrFormat("Assert(%s)", condition);
    case TestStepType::kScreenshot:
      return "Screenshot()";
  }
  return "Unknown";
}

std::string TestWorkflow::ToString() const {
  std::string result = absl::StrCat("Workflow: ", description, "\n");
  for (size_t i = 0; i < steps.size(); ++i) {
    absl::StrAppend(&result, "  ", i + 1, ". ", steps[i].ToString(), "\n");
  }
  return result;
}

absl::StatusOr<TestWorkflow> TestWorkflowGenerator::GenerateWorkflow(
    const std::string& prompt) {
  std::string normalized_prompt = absl::AsciiStrToLower(prompt);

  // Try pattern matching in order of specificity
  std::string editor_name, input_name, text, button_name;

  // Pattern 1: "Open <Editor> and verify it loads"
  if (MatchesOpenAndVerify(normalized_prompt, &editor_name)) {
    return BuildOpenAndVerifyWorkflow(editor_name);
  }

  // Pattern 2: "Open <Editor> editor"
  if (MatchesOpenEditor(normalized_prompt, &editor_name)) {
    return BuildOpenEditorWorkflow(editor_name);
  }

  // Pattern 3: "Type '<text>' in <input>"
  if (MatchesTypeInput(normalized_prompt, &input_name, &text)) {
    return BuildTypeInputWorkflow(input_name, text);
  }

  // Pattern 4: "Click <button>"
  if (MatchesClickButton(normalized_prompt, &button_name)) {
    return BuildClickButtonWorkflow(button_name);
  }

  // If no patterns match, return helpful error
  return absl::InvalidArgumentError(
      absl::StrFormat("Unable to parse prompt: \"%s\"\n\n"
                      "Supported patterns:\n"
                      "  - Open <Editor> editor\n"
                      "  - Open <Editor> and verify it loads\n"
                      "  - Type '<text>' in <input>\n"
                      "  - Click <button>\n\n"
                      "Examples:\n"
                      "  - Open Overworld editor\n"
                      "  - Open Dungeon editor and verify it loads\n"
                      "  - Type 'zelda3.sfc' in filename input\n"
                      "  - Click Open ROM button",
                      prompt));
}

bool TestWorkflowGenerator::MatchesOpenEditor(const std::string& prompt,
                                              std::string* editor_name) {
  // Match: "open <name> editor" or "open <name>"
  std::regex pattern(R"(open\s+(\w+)(?:\s+editor)?)");
  std::smatch match;
  if (std::regex_search(prompt, match, pattern) && match.size() > 1) {
    *editor_name = match[1].str();
    return true;
  }
  return false;
}

bool TestWorkflowGenerator::MatchesOpenAndVerify(const std::string& prompt,
                                                 std::string* editor_name) {
  // Match: "open <name> and verify" or "open <name> editor and verify it loads"
  std::regex pattern(R"(open\s+(\w+)(?:\s+editor)?\s+and\s+verify)");
  std::smatch match;
  if (std::regex_search(prompt, match, pattern) && match.size() > 1) {
    *editor_name = match[1].str();
    return true;
  }
  return false;
}

bool TestWorkflowGenerator::MatchesTypeInput(const std::string& prompt,
                                             std::string* input_name,
                                             std::string* text) {
  // Match: "type 'text' in <input>" or "type \"text\" in <input>"
  std::regex pattern(R"(type\s+['"]([^'"]+)['"]\s+in(?:to)?\s+(\w+))");
  std::smatch match;
  if (std::regex_search(prompt, match, pattern) && match.size() > 2) {
    *text = match[1].str();
    *input_name = match[2].str();
    return true;
  }
  return false;
}

bool TestWorkflowGenerator::MatchesClickButton(const std::string& prompt,
                                               std::string* button_name) {
  // Match: "click <button>" or "click <button> button"
  std::regex pattern(R"(click\s+([\w\s]+?)(?:\s+button)?\s*$)");
  std::smatch match;
  if (std::regex_search(prompt, match, pattern) && match.size() > 1) {
    *button_name = match[1].str();
    return true;
  }
  return false;
}

std::string TestWorkflowGenerator::NormalizeEditorName(
    const std::string& name) {
  std::string normalized = name;
  // Capitalize first letter
  if (!normalized.empty()) {
    normalized[0] = std::toupper(normalized[0]);
  }
  // Add " Editor" suffix if not present
  if (!absl::StrContains(absl::AsciiStrToLower(normalized), "editor")) {
    absl::StrAppend(&normalized, " Editor");
  }
  return normalized;
}

TestWorkflow TestWorkflowGenerator::BuildOpenEditorWorkflow(
    const std::string& editor_name) {
  std::string normalized_name = NormalizeEditorName(editor_name);

  TestWorkflow workflow;
  workflow.description = absl::StrFormat("Open %s", normalized_name);

  // Step 1: Click the editor button
  TestStep click_step;
  click_step.type = TestStepType::kClick;
  click_step.target = absl::StrFormat(
      "button:%s", absl::StrReplaceAll(normalized_name, {{" Editor", ""}}));
  workflow.steps.push_back(click_step);

  // Step 2: Wait for editor window to appear
  TestStep wait_step;
  wait_step.type = TestStepType::kWait;
  wait_step.condition = absl::StrFormat("window_visible:%s", normalized_name);
  wait_step.timeout_ms = 5000;
  workflow.steps.push_back(wait_step);

  return workflow;
}

TestWorkflow TestWorkflowGenerator::BuildOpenAndVerifyWorkflow(
    const std::string& editor_name) {
  // Start with basic open workflow
  TestWorkflow workflow = BuildOpenEditorWorkflow(editor_name);
  workflow.description =
      absl::StrFormat("Open and verify %s", NormalizeEditorName(editor_name));

  // Add assertion step
  TestStep assert_step;
  assert_step.type = TestStepType::kAssert;
  assert_step.condition =
      absl::StrFormat("visible:%s", NormalizeEditorName(editor_name));
  workflow.steps.push_back(assert_step);

  return workflow;
}

TestWorkflow TestWorkflowGenerator::BuildTypeInputWorkflow(
    const std::string& input_name, const std::string& text) {
  TestWorkflow workflow;
  workflow.description = absl::StrFormat("Type '%s' into %s", text, input_name);

  // Step 1: Click input to focus
  TestStep click_step;
  click_step.type = TestStepType::kClick;
  click_step.target = absl::StrFormat("input:%s", input_name);
  workflow.steps.push_back(click_step);

  // Step 2: Type the text
  TestStep type_step;
  type_step.type = TestStepType::kType;
  type_step.target = absl::StrFormat("input:%s", input_name);
  type_step.text = text;
  type_step.clear_first = true;
  workflow.steps.push_back(type_step);

  return workflow;
}

TestWorkflow TestWorkflowGenerator::BuildClickButtonWorkflow(
    const std::string& button_name) {
  TestWorkflow workflow;
  workflow.description = absl::StrFormat("Click '%s' button", button_name);

  TestStep click_step;
  click_step.type = TestStepType::kClick;
  click_step.target = absl::StrFormat("button:%s", button_name);
  workflow.steps.push_back(click_step);

  return workflow;
}

}  // namespace cli
}  // namespace yaze
