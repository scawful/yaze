#ifndef YAZE_SRC_CLI_SERVICE_AI_TOOL_SCHEMA_BUILDER_H_
#define YAZE_SRC_CLI_SERVICE_AI_TOOL_SCHEMA_BUILDER_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/prompt_builder.h"
#include "nlohmann/json_fwd.hpp"

namespace yaze::cli {

// Normalizes prompt-catalogue and legacy asset tool schema shapes into a single
// provider-neutral function declaration array. Provider adapters can then build
// their native payloads from the same source data.
class ToolSchemaBuilder {
 public:
  static nlohmann::json BuildFunctionDeclarations(
      const std::vector<ToolSpecification>& tool_specs);

  static absl::StatusOr<nlohmann::json> ResolveFunctionDeclarations(
      const PromptBuilder& prompt_builder);

  static absl::StatusOr<nlohmann::json> ParseFunctionDeclarations(
      std::string_view schema_json);

  static absl::StatusOr<nlohmann::json> LoadFunctionDeclarationsFromAsset(
      const std::string& relative_path = "agent/function_schemas.json");

  static nlohmann::json BuildOpenAITools(
      const nlohmann::json& function_declarations);

  static nlohmann::json BuildGeminiTools(
      const nlohmann::json& function_declarations);

  static nlohmann::json BuildAnthropicTools(
      const nlohmann::json& function_declarations);

 private:
  static absl::StatusOr<nlohmann::json> NormalizeFunctionDeclarations(
      const nlohmann::json& schema_json);
};

}  // namespace yaze::cli

#endif  // YAZE_SRC_CLI_SERVICE_AI_TOOL_SCHEMA_BUILDER_H_
