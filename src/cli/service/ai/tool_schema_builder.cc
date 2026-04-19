#include "cli/service/ai/tool_schema_builder.h"

#include <fstream>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"
#include "util/platform_paths.h"

namespace yaze::cli {

namespace {

nlohmann::json BuildFunctionDeclaration(const ToolSpecification& spec) {
  nlohmann::json function;
  function["name"] = spec.name;
  function["description"] = spec.description;
  if (!spec.usage_notes.empty()) {
    function["description"] = spec.description + " " + spec.usage_notes;
  }

  nlohmann::json parameters;
  parameters["type"] = "object";

  nlohmann::json properties = nlohmann::json::object();
  nlohmann::json required = nlohmann::json::array();

  for (const auto& arg : spec.arguments) {
    nlohmann::json arg_schema;
    arg_schema["type"] = "string";
    arg_schema["description"] = arg.description;
    if (!arg.example.empty()) {
      arg_schema["example"] = arg.example;
    }
    properties[arg.name] = std::move(arg_schema);

    if (arg.required) {
      required.push_back(arg.name);
    }
  }

  parameters["properties"] = std::move(properties);
  if (!required.empty()) {
    parameters["required"] = std::move(required);
  }

  function["parameters"] = std::move(parameters);
  return function;
}

absl::StatusOr<nlohmann::json> ExtractFunctionDeclaration(
    const nlohmann::json& schema_json) {
  if (!schema_json.is_object()) {
    return absl::InvalidArgumentError(
        "Tool schema entries must be JSON objects");
  }

  if (schema_json.contains("function")) {
    const auto& function = schema_json["function"];
    if (!function.is_object()) {
      return absl::InvalidArgumentError(
          "OpenAI tool schema must contain an object-valued function field");
    }
    return function;
  }

  return schema_json;
}

}  // namespace

nlohmann::json ToolSchemaBuilder::BuildFunctionDeclarations(
    const std::vector<ToolSpecification>& tool_specs) {
  nlohmann::json functions = nlohmann::json::array();
  for (const auto& spec : tool_specs) {
    functions.push_back(BuildFunctionDeclaration(spec));
  }
  return functions;
}

absl::StatusOr<nlohmann::json> ToolSchemaBuilder::ResolveFunctionDeclarations(
    const PromptBuilder& prompt_builder) {
  nlohmann::json function_declarations =
      BuildFunctionDeclarations(prompt_builder.tool_specs());
  if (!function_declarations.empty()) {
    return function_declarations;
  }

  return LoadFunctionDeclarationsFromAsset();
}

absl::StatusOr<nlohmann::json> ToolSchemaBuilder::NormalizeFunctionDeclarations(
    const nlohmann::json& schema_json) {
  if (schema_json.is_object() &&
      schema_json.contains("function_declarations")) {
    const auto& functions = schema_json["function_declarations"];
    if (!functions.is_array()) {
      return absl::InvalidArgumentError(
          "function_declarations must be a JSON array");
    }
    return functions;
  }

  if (schema_json.is_array()) {
    nlohmann::json functions = nlohmann::json::array();
    for (const auto& entry : schema_json) {
      auto function_or = ExtractFunctionDeclaration(entry);
      if (!function_or.ok()) {
        return function_or.status();
      }
      functions.push_back(std::move(function_or).value());
    }
    return functions;
  }

  auto function_or = ExtractFunctionDeclaration(schema_json);
  if (!function_or.ok()) {
    return function_or.status();
  }

  nlohmann::json functions = nlohmann::json::array();
  functions.push_back(std::move(function_or).value());
  return functions;
}

absl::StatusOr<nlohmann::json> ToolSchemaBuilder::ParseFunctionDeclarations(
    std::string_view schema_json) {
  if (schema_json.empty()) {
    return nlohmann::json::array();
  }

  try {
    return NormalizeFunctionDeclarations(nlohmann::json::parse(schema_json));
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse tool schema JSON: ", e.what()));
  }
}

absl::StatusOr<nlohmann::json>
ToolSchemaBuilder::LoadFunctionDeclarationsFromAsset(
    const std::string& relative_path) {
  auto schema_path_or = util::PlatformPaths::FindAsset(relative_path);
  if (!schema_path_or.ok()) {
    return schema_path_or.status();
  }

  std::ifstream file(schema_path_or->string());
  if (!file.is_open()) {
    return absl::NotFoundError(absl::StrCat(
        "Failed to open tool schema asset: ", schema_path_or->string()));
  }

  try {
    nlohmann::json schema_json;
    file >> schema_json;
    return NormalizeFunctionDeclarations(schema_json);
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse tool schema asset: ", e.what()));
  }
}

nlohmann::json ToolSchemaBuilder::BuildOpenAITools(
    const nlohmann::json& function_declarations) {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& function : function_declarations) {
    tools.push_back({{"type", "function"}, {"function", function}});
  }
  return tools;
}

nlohmann::json ToolSchemaBuilder::BuildGeminiTools(
    const nlohmann::json& function_declarations) {
  if (function_declarations.empty()) {
    return nlohmann::json::array();
  }

  return nlohmann::json::array(
      {{{"function_declarations", function_declarations}}});
}

nlohmann::json ToolSchemaBuilder::BuildAnthropicTools(
    const nlohmann::json& function_declarations) {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& function : function_declarations) {
    tools.push_back({{"name", function.value("name", "")},
                     {"description", function.value("description", "")},
                     {"input_schema",
                      function.value("parameters", nlohmann::json::object())}});
  }
  return tools;
}

}  // namespace yaze::cli
