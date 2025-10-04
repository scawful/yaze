#ifndef YAZE_CLI_SERVICE_PROMPT_BUILDER_H_
#define YAZE_CLI_SERVICE_PROMPT_BUILDER_H_

// PromptBuilder requires JSON and YAML support for catalogue loading
// If you see linker errors, enable Z3ED_AI or YAZE_WITH_JSON in CMake
#if !defined(YAZE_WITH_JSON)
#ifdef _MSC_VER
  #pragma message("PromptBuilder requires JSON support. Build with -DZ3ED_AI=ON or -DYAZE_WITH_JSON=ON")
#else
  #warning "PromptBuilder requires JSON support. Build with -DZ3ED_AI=ON or -DYAZE_WITH_JSON=ON"
#endif
#endif

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json_fwd.hpp"
#include "cli/service/ai/common.h"
#include "cli/service/resources/resource_context_builder.h"
#include "app/rom.h"

namespace yaze {
namespace cli {

namespace agent {
struct ChatMessage;
}

// Few-shot example for prompt engineering
struct FewShotExample {
  std::string user_prompt;
  std::string text_response;
  std::vector<std::string> expected_commands;
  std::string explanation;  // Why these commands work
  std::vector<ToolCall> tool_calls;
};

struct ToolArgument {
  std::string name;
  std::string description;
  bool required = false;
  std::string example;
};

struct ToolSpecification {
  std::string name;
  std::string description;
  std::vector<ToolArgument> arguments;
  std::string usage_notes;
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
      
  // Build a full prompt from a conversation history
  std::string BuildPromptFromHistory(
      const std::vector<agent::ChatMessage>& history);
  
  // Add custom few-shot examples
  void AddFewShotExample(const FewShotExample& example);
  
  // Get few-shot examples for specific category
  std::vector<FewShotExample> GetExamplesForCategory(
      const std::string& category);
  std::string LookupTileId(const std::string& alias) const;
  const std::map<std::string, std::string>& tile_reference() const {
    return tile_reference_;
  }
  
  // Generate OpenAI-compatible function call schemas (JSON format)
  std::string BuildFunctionCallSchemas() const;
  
  // Set verbosity level (0=minimal, 1=standard, 2=verbose)
  void SetVerbosity(int level) { verbosity_ = level; }
  
 private:
  std::string BuildCommandReference() const;
  std::string BuildFewShotExamplesSection() const;
  std::string BuildToolReference() const;
  std::string BuildContextSection(const RomContext& context);
  std::string BuildConstraintsSection() const;
  std::string BuildTileReferenceSection() const;
  absl::StatusOr<std::string> ResolveCataloguePath(const std::string& yaml_path) const;
  void ClearCatalogData();
  absl::Status ParseCommands(const nlohmann::json& commands);
  absl::Status ParseTools(const nlohmann::json& tools);
  absl::Status ParseExamples(const nlohmann::json& examples);
  void ParseTileReference(const nlohmann::json& tile_reference);
  
  Rom* rom_ = nullptr;
  std::unique_ptr<ResourceContextBuilder> resource_context_builder_;
  std::map<std::string, std::string> command_docs_;  // Command name -> docs
  std::vector<FewShotExample> examples_;
  std::vector<ToolSpecification> tool_specs_;
  std::map<std::string, std::string> tile_reference_;
  int verbosity_ = 1;
  bool catalogue_loaded_ = false;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_PROMPT_BUILDER_H_
