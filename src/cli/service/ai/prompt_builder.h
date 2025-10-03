#ifndef YAZE_CLI_SERVICE_PROMPT_BUILDER_H_
#define YAZE_CLI_SERVICE_PROMPT_BUILDER_H_

#include <string>
#include <vector>
#include <map>

#include "absl/status/statusor.h"
#include "cli/service/resources/resource_context_builder.h"
#include "app/rom.h"

namespace yaze {
namespace cli {

// Few-shot example for prompt engineering
struct FewShotExample {
  std::string user_prompt;
  std::vector<std::string> expected_commands;
  std::string explanation;  // Why these commands work
};

// ROM context information to inject into prompts
struct RomContext {
  std::string rom_path;
  bool rom_loaded = false;
  std::string current_editor;  // "overworld", "dungeon", "sprite", etc.
  std::map<std::string, std::string> editor_state;  // Context-specific state
};

// Builds sophisticated prompts for LLM services
class PromptBuilder {
 public:
  PromptBuilder();
  
  void SetRom(Rom* rom) { rom_ = rom; }

  // Load z3ed command documentation from resources
  absl::Status LoadResourceCatalogue(const std::string& yaml_path);
  
  // Build system instruction with full command reference
  std::string BuildSystemInstruction();
  
  // Build system instruction with few-shot examples
  std::string BuildSystemInstructionWithExamples();
  
  // Build user prompt with ROM context
  std::string BuildContextualPrompt(
      const std::string& user_prompt,
      const RomContext& context);
  
  // Add custom few-shot examples
  void AddFewShotExample(const FewShotExample& example);
  
  // Get few-shot examples for specific category
  std::vector<FewShotExample> GetExamplesForCategory(
      const std::string& category);
  
  // Set verbosity level (0=minimal, 1=standard, 2=verbose)
  void SetVerbosity(int level) { verbosity_ = level; }
  
 private:
  std::string BuildCommandReference();
  std::string BuildFewShotExamplesSection();
  std::string BuildContextSection(const RomContext& context);
  std::string BuildConstraintsSection();
  
  void LoadDefaultExamples();
  
  Rom* rom_ = nullptr;
  std::unique_ptr<ResourceContextBuilder> resource_context_builder_;
  std::map<std::string, std::string> command_docs_;  // Command name -> docs
  std::vector<FewShotExample> examples_;
  int verbosity_ = 1;
  bool catalogue_loaded_ = false;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_PROMPT_BUILDER_H_
