#include "cli/service/ai/prompt_builder.h"
#include "cli/service/agent/conversational_agent_service.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "nlohmann/json.hpp"
#include "yaml-cpp/yaml.h"

namespace yaze {
namespace cli {

namespace {

namespace fs = std::filesystem;

nlohmann::json YamlToJson(const YAML::Node& node) {
  if (!node) {
    return nlohmann::json();
  }

  switch (node.Type()) {
    case YAML::NodeType::Scalar:
      return node.as<std::string>("");
    case YAML::NodeType::Sequence: {
      nlohmann::json array = nlohmann::json::array();
      for (const auto& item : node) {
        array.push_back(YamlToJson(item));
      }
      return array;
    }
    case YAML::NodeType::Map: {
      nlohmann::json object = nlohmann::json::object();
      for (const auto& kv : node) {
        object[kv.first.as<std::string>()] = YamlToJson(kv.second);
      }
      return object;
    }
    default:
      return nlohmann::json();
  }
}

std::vector<fs::path> BuildCatalogueSearchPaths(const std::string& explicit_path) {
  std::vector<fs::path> paths;
  if (!explicit_path.empty()) {
    paths.emplace_back(explicit_path);
  }

  if (const char* env_path = std::getenv("YAZE_AGENT_CATALOGUE")) {
    if (*env_path != '\0') {
      paths.emplace_back(env_path);
    }
  }

  const std::vector<std::string> defaults = {
      "assets/agent/prompt_catalogue.yaml",
      "../assets/agent/prompt_catalogue.yaml",
      "../../assets/agent/prompt_catalogue.yaml",
      "assets/z3ed/prompt_catalogue.yaml",
      "../assets/z3ed/prompt_catalogue.yaml",
  };

  for (const auto& candidate : defaults) {
    paths.emplace_back(candidate);
  }

  return paths;
}

}  // namespace

PromptBuilder::PromptBuilder() = default;

void PromptBuilder::ClearCatalogData() {
  command_docs_.clear();
  examples_.clear();
  tool_specs_.clear();
  tile_reference_.clear();
  catalogue_loaded_ = false;
}

absl::StatusOr<std::string> PromptBuilder::ResolveCataloguePath(
    const std::string& yaml_path) const {
  const auto search_paths = BuildCatalogueSearchPaths(yaml_path);
  for (const auto& candidate : search_paths) {
    fs::path resolved = candidate;
    if (resolved.is_relative()) {
      resolved = fs::absolute(resolved);
    }
    if (fs::exists(resolved)) {
      return resolved.string();
    }
  }

  return absl::NotFoundError(
      absl::StrCat("Prompt catalogue not found. Checked paths: ",
                   absl::StrJoin(search_paths, ", ",
                                 [](std::string* out, const fs::path& path) {
                                   absl::StrAppend(out, path.string());
                                 })));
}

absl::Status PromptBuilder::LoadResourceCatalogue(const std::string& yaml_path) {
  auto resolved_or = ResolveCataloguePath(yaml_path);
  if (!resolved_or.ok()) {
    ClearCatalogData();
    return resolved_or.status();
  }

  const std::string& resolved_path = resolved_or.value();

  YAML::Node root;
  try {
    root = YAML::LoadFile(resolved_path);
  } catch (const YAML::BadFile& e) {
    ClearCatalogData();
    return absl::NotFoundError(
        absl::StrCat("Unable to open prompt catalogue at ", resolved_path,
                     ": ", e.what()));
  } catch (const YAML::ParserException& e) {
    ClearCatalogData();
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse prompt catalogue at ", resolved_path,
                     ": ", e.what()));
  }

  nlohmann::json catalogue = YamlToJson(root);
  ClearCatalogData();

  if (catalogue.contains("commands")) {
    if (auto status = ParseCommands(catalogue["commands"]); !status.ok()) {
      return status;
    }
  }

  if (catalogue.contains("tools")) {
    if (auto status = ParseTools(catalogue["tools"]); !status.ok()) {
      return status;
    }
  }

  if (catalogue.contains("examples")) {
    if (auto status = ParseExamples(catalogue["examples"]); !status.ok()) {
      return status;
    }
  }

  if (catalogue.contains("tile16_reference")) {
    ParseTileReference(catalogue["tile16_reference"]);
  }

  catalogue_loaded_ = true;
  return absl::OkStatus();
}

absl::Status PromptBuilder::ParseCommands(const nlohmann::json& commands) {
  if (!commands.is_object()) {
    return absl::InvalidArgumentError(
        "commands section must be an object mapping command names to docs");
  }

  for (const auto& [name, value] : commands.items()) {
    if (!value.is_string()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Command entry for ", name, " must be a string"));
    }
    command_docs_[name] = value.get<std::string>();
  }

  return absl::OkStatus();
}

absl::Status PromptBuilder::ParseTools(const nlohmann::json& tools) {
  if (!tools.is_array()) {
    return absl::InvalidArgumentError("tools section must be an array");
  }

  for (const auto& tool_json : tools) {
    if (!tool_json.is_object()) {
      return absl::InvalidArgumentError(
          "Each tool entry must be an object with name/description");
    }

    ToolSpecification spec;
    if (tool_json.contains("name") && tool_json["name"].is_string()) {
      spec.name = tool_json["name"].get<std::string>();
    } else {
      return absl::InvalidArgumentError("Tool entry missing name");
    }

    if (tool_json.contains("description") && tool_json["description"].is_string()) {
      spec.description = tool_json["description"].get<std::string>();
    }

    if (tool_json.contains("usage_notes") && tool_json["usage_notes"].is_string()) {
      spec.usage_notes = tool_json["usage_notes"].get<std::string>();
    }

    if (tool_json.contains("arguments")) {
      const auto& args = tool_json["arguments"];
      if (!args.is_array()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Tool arguments for ", spec.name, " must be an array"));
      }
      for (const auto& arg_json : args) {
        if (!arg_json.is_object()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Argument entries for ", spec.name,
                           " must be objects"));
        }
        ToolArgument arg;
        if (arg_json.contains("name") && arg_json["name"].is_string()) {
          arg.name = arg_json["name"].get<std::string>();
        } else {
          return absl::InvalidArgumentError(
              absl::StrCat("Argument entry for ", spec.name,
                           " is missing a name"));
        }
        if (arg_json.contains("description") && arg_json["description"].is_string()) {
          arg.description = arg_json["description"].get<std::string>();
        }
        if (arg_json.contains("required")) {
          if (!arg_json["required"].is_boolean()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Argument 'required' flag for ", spec.name,
                             "::", arg.name, " must be boolean"));
          }
          arg.required = arg_json["required"].get<bool>();
        }
        if (arg_json.contains("example") && arg_json["example"].is_string()) {
          arg.example = arg_json["example"].get<std::string>();
        }
        spec.arguments.push_back(std::move(arg));
      }
    }

    tool_specs_.push_back(std::move(spec));
  }

  return absl::OkStatus();
}

absl::Status PromptBuilder::ParseExamples(const nlohmann::json& examples) {
  if (!examples.is_array()) {
    return absl::InvalidArgumentError("examples section must be an array");
  }

  for (const auto& example_json : examples) {
    if (!example_json.is_object()) {
      return absl::InvalidArgumentError("Each example entry must be an object");
    }

    FewShotExample example;
    if (example_json.contains("user_prompt") &&
        example_json["user_prompt"].is_string()) {
      example.user_prompt = example_json["user_prompt"].get<std::string>();
    } else {
      return absl::InvalidArgumentError("Example missing user_prompt");
    }

    if (example_json.contains("text_response") &&
        example_json["text_response"].is_string()) {
      example.text_response = example_json["text_response"].get<std::string>();
    }

    if (example_json.contains("reasoning") &&
        example_json["reasoning"].is_string()) {
      example.explanation = example_json["reasoning"].get<std::string>();
    }

    if (example_json.contains("commands")) {
      const auto& commands = example_json["commands"];
      if (!commands.is_array()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Example commands for ", example.user_prompt,
                         " must be an array"));
      }
      for (const auto& cmd : commands) {
        if (!cmd.is_string()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Command entries for ", example.user_prompt,
                           " must be strings"));
        }
        example.expected_commands.push_back(cmd.get<std::string>());
      }
    }

    if (example_json.contains("tool_calls")) {
      const auto& calls = example_json["tool_calls"];
      if (!calls.is_array()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Tool calls for ", example.user_prompt,
                         " must be an array"));
      }
      for (const auto& call_json : calls) {
        if (!call_json.is_object()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Tool call entries for ", example.user_prompt,
                           " must be objects"));
        }
        ToolCall call;
        if (call_json.contains("tool_name") && call_json["tool_name"].is_string()) {
          call.tool_name = call_json["tool_name"].get<std::string>();
        } else {
          return absl::InvalidArgumentError(
              absl::StrCat("Tool call missing tool_name in example: ",
                           example.user_prompt));
        }
        if (call_json.contains("args")) {
          const auto& args = call_json["args"];
          if (!args.is_object()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Tool call args for ", example.user_prompt,
                             " must be an object"));
          }
          for (const auto& [key, value] : args.items()) {
            if (!value.is_string()) {
              return absl::InvalidArgumentError(
                  absl::StrCat("Tool call arg value for ", example.user_prompt,
                               " must be a string"));
            }
            call.args[key] = value.get<std::string>();
          }
        }
        example.tool_calls.push_back(std::move(call));
      }
    }

    example.explanation = example_json.value("explanation", example.explanation);
    examples_.push_back(std::move(example));
  }

  return absl::OkStatus();
}

void PromptBuilder::ParseTileReference(const nlohmann::json& tile_reference) {
  if (!tile_reference.is_object()) {
    return;
  }

  for (const auto& [alias, value] : tile_reference.items()) {
    if (value.is_string()) {
      tile_reference_[alias] = value.get<std::string>();
    }
  }
}

std::string PromptBuilder::LookupTileId(const std::string& alias) const {
  auto it = tile_reference_.find(alias);
  if (it != tile_reference_.end()) {
    return it->second;
  }
  return "";
}

std::string PromptBuilder::BuildCommandReference() const {
  std::ostringstream oss;
  
  oss << "# Available z3ed Commands\n\n";
  
  for (const auto& [cmd, docs] : command_docs_) {
    oss << "## " << cmd << "\n";
    oss << docs << "\n\n";
  }
  
  return oss.str();
}

std::string PromptBuilder::BuildToolReference() const {
  if (tool_specs_.empty()) {
    return "";
  }

  std::ostringstream oss;
  oss << "# Available Agent Tools\n\n";

  for (const auto& spec : tool_specs_) {
    oss << "## " << spec.name << "\n";
    if (!spec.description.empty()) {
      oss << spec.description << "\n\n";
    }

    if (!spec.arguments.empty()) {
      oss << "| Argument | Required | Description | Example |\n";
      oss << "| --- | --- | --- | --- |\n";
      for (const auto& arg : spec.arguments) {
        oss << "| `" << arg.name << "` | " << (arg.required ? "yes" : "no")
            << " | " << arg.description << " | "
            << (arg.example.empty() ? "" : "`" + arg.example + "`")
            << " |\n";
      }
      oss << "\n";
    }

    if (!spec.usage_notes.empty()) {
      oss << "_Usage:_ " << spec.usage_notes << "\n\n";
    }
  }

  return oss.str();
}

std::string PromptBuilder::BuildFewShotExamplesSection() const {
  std::ostringstream oss;

  oss << "# Example Command Sequences\n\n";
  oss << "Here are proven examples of how to accomplish common tasks:\n\n";

  for (const auto& example : examples_) {
    oss << "**User Request:** \"" << example.user_prompt << "\"\n";
    oss << "**Structured Response:**\n";

    nlohmann::json example_json = nlohmann::json::object();
    if (!example.text_response.empty()) {
      example_json["text_response"] = example.text_response;
    }
    if (!example.expected_commands.empty()) {
      example_json["commands"] = example.expected_commands;
    }
    if (!example.explanation.empty()) {
      example_json["reasoning"] = example.explanation;
    }
    if (!example.tool_calls.empty()) {
      nlohmann::json calls = nlohmann::json::array();
      for (const auto& call : example.tool_calls) {
        nlohmann::json call_json;
        call_json["tool_name"] = call.tool_name;
        nlohmann::json args = nlohmann::json::object();
        for (const auto& [key, value] : call.args) {
          args[key] = value;
        }
        call_json["args"] = std::move(args);
        calls.push_back(std::move(call_json));
      }
      example_json["tool_calls"] = std::move(calls);
    }

    oss << "```json\n" << example_json.dump(2) << "\n```\n\n";
  }

  return oss.str();
}

std::string PromptBuilder::BuildConstraintsSection() const {
  std::ostringstream oss;
  oss << R"(
# Critical Constraints

1. **Output Format:** You MUST respond with ONLY a JSON object with the following structure:
  {
    "text_response": "Your natural language reply to the user.",
    "tool_calls": [{ "tool_name": "tool_name", "args": { "arg1": "value1" } }],
    "commands": ["command1", "command2"],
    "reasoning": "Your thought process."
  }
  - `text_response` is for conversational replies.
  - `tool_calls` is for asking questions about the ROM. Use the available tools.
  - `commands` is for generating commands to modify the ROM.
  - All fields are optional.

2. **Command Syntax:** Follow the exact syntax shown in examples
  - Use correct flag names (--group, --id, --to, --from, etc.)
  - Use hex format for colors (0xRRGGBB) and tile IDs (0xNNN)
  - Coordinates are 0-based indices

3. **Common Patterns:**
  - Palette modifications: export → set-color → import
  - Multiple tile placement: multiple overworld set-tile commands
  - Validation: single rom validate command

4. **Error Prevention:**
  - Always export before modifying palettes
  - Use temporary file names (temp_*.json) for intermediate files
  - Validate coordinates are within bounds
)";

  if (!tile_reference_.empty()) {
   oss << "\n" << BuildTileReferenceSection();
  }

  return oss.str();
}

std::string PromptBuilder::BuildTileReferenceSection() const {
  std::ostringstream oss;
  oss << "# Tile16 Reference (ALTTP)\n\n";

  for (const auto& [alias, value] : tile_reference_) {
    oss << "- " << alias << ": " << value << "\n";
  }

  oss << "\n";
  return oss.str();
}

std::string PromptBuilder::BuildContextSection(const RomContext& context) {
  std::ostringstream oss;

  oss << "# Current ROM Context\n\n";

  // Use ResourceContextBuilder if a ROM is available
  if (rom_ && rom_->is_loaded()) {
    if (!resource_context_builder_) {
      resource_context_builder_ = std::make_unique<ResourceContextBuilder>(rom_);
    }
    auto resource_context_or = resource_context_builder_->BuildResourceContext();
    if (resource_context_or.ok()) {
      oss << resource_context_or.value();
    }
  }

  if (context.rom_loaded) {
    oss << "- **ROM Loaded:** Yes (" << context.rom_path << ")\n";
  } else {
    oss << "- **ROM Loaded:** No\n";
  }
  
  if (!context.current_editor.empty()) {
    oss << "- **Active Editor:** " << context.current_editor << "\n";
  }
  
  if (!context.editor_state.empty()) {
    oss << "- **Editor State:**\n";
    for (const auto& [key, value] : context.editor_state) {
      oss << "  - " << key << ": " << value << "\n";
    }
  }
  
  oss << "\n";
  return oss.str();
}

std::string PromptBuilder::BuildSystemInstruction() {
  std::ostringstream oss;
  
  oss << "You are an expert ROM hacking assistant for The Legend of Zelda: "
      << "A Link to the Past (ALTTP).\n\n";
  
  oss << "Your task is to generate a sequence of z3ed CLI commands to achieve "
      << "the user's request.\n\n";
  
  if (catalogue_loaded_) {
    if (!command_docs_.empty()) {
      oss << BuildCommandReference();
    }
    if (!tool_specs_.empty()) {
      oss << BuildToolReference();
    }
  }
  
  oss << BuildConstraintsSection();
  
  oss << "\n**Response Format:**\n";
  oss << "```json\n";
  oss << "[\"command1 --flag value\", \"command2 --flag value\"]\n";
  oss << "```\n";
  
  return oss.str();
}

std::string PromptBuilder::BuildSystemInstructionWithExamples() {
  std::ostringstream oss;
  
  oss << BuildSystemInstruction();
  oss << "\n---\n\n";
  oss << BuildFewShotExamplesSection();
  
  return oss.str();
}

std::string PromptBuilder::BuildContextualPrompt(
    const std::string& user_prompt,
    const RomContext& context) {
  std::ostringstream oss;
  
  if (context.rom_loaded || !context.current_editor.empty()) {
    oss << BuildContextSection(context);
    oss << "---\n\n";
  }
  
  oss << "**User Request:** " << user_prompt << "\n\n";
  oss << "Generate the appropriate z3ed commands as a JSON array.";
  
  return oss.str();
}

std::string PromptBuilder::BuildPromptFromHistory(
    const std::vector<agent::ChatMessage>& history) {
  std::ostringstream oss;
  oss << "This is a conversation between a user and an expert ROM hacking "
         "assistant.\n\n";

  for (const auto& msg : history) {
    if (msg.sender == agent::ChatMessage::Sender::kUser) {
      oss << "User: " << msg.message << "\n";
    } else {
      oss << "Agent: " << msg.message << "\n";
    }
  }
  oss << "\nBased on this conversation, provide a response in the required JSON "
         "format.";
  return oss.str();
}

void PromptBuilder::AddFewShotExample(const FewShotExample& example) {
  examples_.push_back(example);
}

std::vector<FewShotExample> PromptBuilder::GetExamplesForCategory(
    const std::string& category) {
  std::vector<FewShotExample> result;
  
  for (const auto& example : examples_) {
    // Simple category matching based on keywords
    if (category == "palette" && 
        (example.user_prompt.find("palette") != std::string::npos ||
         example.user_prompt.find("color") != std::string::npos)) {
      result.push_back(example);
    } else if (category == "overworld" && 
               (example.user_prompt.find("place") != std::string::npos ||
                example.user_prompt.find("tree") != std::string::npos ||
                example.user_prompt.find("house") != std::string::npos)) {
      result.push_back(example);
    } else if (category == "validation" && 
               example.user_prompt.find("validate") != std::string::npos) {
      result.push_back(example);
    }
  }
  
  return result;
}

}  // namespace cli
}  // namespace yaze
