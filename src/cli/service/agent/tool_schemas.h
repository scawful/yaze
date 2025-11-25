/**
 * @file tool_schemas.h
 * @brief Tool schema definitions for AI agent tool documentation
 *
 * Provides structured schema definitions that can be auto-generated
 * into LLM system prompts for better tool calling accuracy.
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_SCHEMAS_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_SCHEMAS_H_

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace cli {
namespace agent {

/**
 * @brief Argument schema for a tool parameter
 */
struct ArgumentSchema {
  std::string name;
  std::string type;         // "string", "number", "boolean", "enum"
  std::string description;
  bool required = false;
  std::string default_value;
  std::vector<std::string> enum_values;  // For enum type

  std::string ToJson() const {
    std::string json = "{";
    json += "\"name\": \"" + name + "\", ";
    json += "\"type\": \"" + type + "\", ";
    json += "\"description\": \"" + description + "\", ";
    json += "\"required\": " + std::string(required ? "true" : "false");
    if (!default_value.empty()) {
      json += ", \"default\": \"" + default_value + "\"";
    }
    if (!enum_values.empty()) {
      json += ", \"enum\": [";
      for (size_t i = 0; i < enum_values.size(); ++i) {
        json += "\"" + enum_values[i] + "\"";
        if (i < enum_values.size() - 1) json += ", ";
      }
      json += "]";
    }
    json += "}";
    return json;
  }
};

/**
 * @brief Complete tool schema for LLM documentation
 */
struct ToolSchema {
  std::string name;
  std::string category;
  std::string description;
  std::string detailed_help;
  std::vector<ArgumentSchema> arguments;
  std::vector<std::string> examples;
  std::vector<std::string> related_tools;
  bool requires_rom = true;
  bool requires_grpc = false;

  /**
   * @brief Generate JSON schema for function calling APIs
   */
  std::string ToJson() const {
    std::string json = "{\n";
    json += "  \"name\": \"" + name + "\",\n";
    json += "  \"description\": \"" + description + "\",\n";
    json += "  \"category\": \"" + category + "\",\n";

    // Parameters
    json += "  \"parameters\": {\n";
    json += "    \"type\": \"object\",\n";
    json += "    \"properties\": {\n";

    for (size_t i = 0; i < arguments.size(); ++i) {
      const auto& arg = arguments[i];
      json += "      \"" + arg.name + "\": {\n";
      json += "        \"type\": \"" + arg.type + "\",\n";
      json += "        \"description\": \"" + arg.description + "\"";
      if (!arg.enum_values.empty()) {
        json += ",\n        \"enum\": [";
        for (size_t j = 0; j < arg.enum_values.size(); ++j) {
          json += "\"" + arg.enum_values[j] + "\"";
          if (j < arg.enum_values.size() - 1) json += ", ";
        }
        json += "]";
      }
      json += "\n      }";
      if (i < arguments.size() - 1) json += ",";
      json += "\n";
    }

    json += "    },\n";

    // Required arguments
    json += "    \"required\": [";
    bool first = true;
    for (const auto& arg : arguments) {
      if (arg.required) {
        if (!first) json += ", ";
        json += "\"" + arg.name + "\"";
        first = false;
      }
    }
    json += "]\n";
    json += "  },\n";

    // Additional metadata
    json += "  \"requires_rom\": " + std::string(requires_rom ? "true" : "false")
         + ",\n";
    json += "  \"requires_grpc\": " +
            std::string(requires_grpc ? "true" : "false");

    if (!examples.empty()) {
      json += ",\n  \"examples\": [\n";
      for (size_t i = 0; i < examples.size(); ++i) {
        json += "    \"" + examples[i] + "\"";
        if (i < examples.size() - 1) json += ",";
        json += "\n";
      }
      json += "  ]";
    }

    if (!related_tools.empty()) {
      json += ",\n  \"related_tools\": [";
      for (size_t i = 0; i < related_tools.size(); ++i) {
        json += "\"" + related_tools[i] + "\"";
        if (i < related_tools.size() - 1) json += ", ";
      }
      json += "]";
    }

    json += "\n}";
    return json;
  }

  /**
   * @brief Generate markdown documentation for a tool
   */
  std::string ToMarkdown() const {
    std::string md;
    md += "### " + name + "\n\n";
    md += description + "\n\n";

    if (!detailed_help.empty()) {
      md += detailed_help + "\n\n";
    }

    md += "**Category:** " + category + "\n\n";

    if (!arguments.empty()) {
      md += "**Arguments:**\n\n";
      md += "| Name | Type | Required | Description |\n";
      md += "|------|------|----------|-------------|\n";
      for (const auto& arg : arguments) {
        md += "| `" + arg.name + "` | " + arg.type + " | ";
        md += (arg.required ? "Yes" : "No") + " | ";
        md += arg.description + " |\n";
      }
      md += "\n";
    }

    if (!examples.empty()) {
      md += "**Examples:**\n\n```bash\n";
      for (const auto& ex : examples) {
        md += ex + "\n";
      }
      md += "```\n\n";
    }

    if (!related_tools.empty()) {
      md += "**Related:** " + absl::StrJoin(related_tools, ", ") + "\n\n";
    }

    return md;
  }
};

/**
 * @brief Registry of all tool schemas
 */
class ToolSchemaRegistry {
 public:
  static ToolSchemaRegistry& Instance() {
    static ToolSchemaRegistry instance;
    return instance;
  }

  void Register(const ToolSchema& schema) { schemas_[schema.name] = schema; }

  const ToolSchema* Get(const std::string& name) const {
    auto it = schemas_.find(name);
    return it != schemas_.end() ? &it->second : nullptr;
  }

  std::vector<ToolSchema> GetAll() const {
    std::vector<ToolSchema> result;
    for (const auto& [_, schema] : schemas_) {
      result.push_back(schema);
    }
    return result;
  }

  std::vector<ToolSchema> GetByCategory(const std::string& category) const {
    std::vector<ToolSchema> result;
    for (const auto& [_, schema] : schemas_) {
      if (schema.category == category) {
        result.push_back(schema);
      }
    }
    return result;
  }

  /**
   * @brief Export all schemas as JSON array for function calling APIs
   */
  std::string ExportAllAsJson() const {
    std::string json = "[\n";
    bool first = true;
    for (const auto& [_, schema] : schemas_) {
      if (!first) json += ",\n";
      json += schema.ToJson();
      first = false;
    }
    json += "\n]";
    return json;
  }

  /**
   * @brief Export all schemas as Markdown documentation
   */
  std::string ExportAllAsMarkdown() const {
    std::string md = "# Z3ED Tool Reference\n\n";

    // Group by category
    std::map<std::string, std::vector<const ToolSchema*>> by_category;
    for (const auto& [_, schema] : schemas_) {
      by_category[schema.category].push_back(&schema);
    }

    for (const auto& [category, tools] : by_category) {
      md += "## " + category + "\n\n";
      for (const auto* tool : tools) {
        md += tool->ToMarkdown();
      }
    }

    return md;
  }

  /**
   * @brief Generate LLM system prompt section for tools
   */
  std::string GenerateLLMPrompt() const {
    std::string prompt = "## Available Tools\n\n";
    prompt +=
        "You can call the following tools to interact with the ROM and "
        "editor:\n\n";

    for (const auto& [_, schema] : schemas_) {
      prompt += "- **" + schema.name + "**: " + schema.description + "\n";
      if (!schema.arguments.empty()) {
        prompt += "  Arguments: ";
        for (size_t i = 0; i < schema.arguments.size(); ++i) {
          const auto& arg = schema.arguments[i];
          prompt += arg.name;
          if (arg.required) prompt += "*";
          if (i < schema.arguments.size() - 1) prompt += ", ";
        }
        prompt += "\n";
      }
    }

    return prompt;
  }

 private:
  ToolSchemaRegistry() { RegisterBuiltinSchemas(); }

  void RegisterBuiltinSchemas() {
    // Meta-tools
    Register({.name = "tools-list",
              .category = "meta",
              .description = "List all available tools",
              .detailed_help = "Returns a JSON array of all tools the AI can "
                               "call, including their names, categories, and "
                               "brief descriptions.",
              .arguments = {},
              .examples = {"z3ed tools-list"},
              .requires_rom = false});

    Register({.name = "tools-describe",
              .category = "meta",
              .description = "Get detailed information about a specific tool",
              .arguments = {{.name = "name",
                             .type = "string",
                             .description = "Name of the tool to describe",
                             .required = true}},
              .examples = {"z3ed tools-describe --name=dungeon-describe-room"},
              .requires_rom = false});

    Register({.name = "tools-search",
              .category = "meta",
              .description = "Search tools by keyword",
              .arguments = {{.name = "query",
                             .type = "string",
                             .description = "Search query",
                             .required = true}},
              .examples = {"z3ed tools-search --query=sprite"},
              .requires_rom = false});

    // Resource tools
    Register({.name = "resource-list",
              .category = "resource",
              .description = "List resource labels for a specific type",
              .arguments = {{.name = "type",
                             .type = "string",
                             .description = "Resource type to list",
                             .required = true,
                             .enum_values = {"dungeon", "overworld", "sprite",
                                             "palette", "message"}}},
              .examples = {"z3ed resource-list --type=dungeon"},
              .related_tools = {"resource-search"}});

    // Dungeon tools
    Register(
        {.name = "dungeon-describe-room",
         .category = "dungeon",
         .description = "Get detailed description of a dungeon room",
         .detailed_help =
             "Returns information about room layout, objects, sprites, "
             "and properties for the specified room ID.",
         .arguments = {{.name = "room",
                        .type = "number",
                        .description = "Room ID (0-295)",
                        .required = true}},
         .examples = {"z3ed dungeon-describe-room --room=5"},
         .related_tools = {"dungeon-list-sprites", "dungeon-list-objects"}});

    // Overworld tools
    Register({.name = "overworld-describe-map",
              .category = "overworld",
              .description = "Get detailed description of an overworld map",
              .arguments = {{.name = "map",
                             .type = "number",
                             .description = "Map ID (0-159)",
                             .required = true}},
              .examples = {"z3ed overworld-describe-map --map=0"},
              .related_tools = {"overworld-list-sprites", "overworld-find-tile"}});

    // Filesystem tools
    Register({.name = "filesystem-list",
              .category = "filesystem",
              .description = "List directory contents",
              .arguments = {{.name = "path",
                             .type = "string",
                             .description = "Directory path to list",
                             .required = true}},
              .examples = {"z3ed filesystem-list --path=src/app"},
              .requires_rom = false,
              .related_tools = {"filesystem-read", "filesystem-exists"}});

    Register({.name = "filesystem-read",
              .category = "filesystem",
              .description = "Read file contents",
              .arguments = {{.name = "path",
                             .type = "string",
                             .description = "File path to read",
                             .required = true},
                            {.name = "lines",
                             .type = "number",
                             .description = "Maximum lines to read",
                             .required = false,
                             .default_value = "100"}},
              .examples = {"z3ed filesystem-read --path=src/app/rom.h"},
              .requires_rom = false});

    // Memory tools
    Register({.name = "memory-regions",
              .category = "memory",
              .description = "List known ALTTP memory regions",
              .detailed_help = "Returns a list of known memory regions in A "
                               "Link to the Past, including player state, "
                               "sprites, save data, and system variables.",
              .arguments = {},
              .examples = {"z3ed memory-regions"},
              .requires_rom = false,
              .related_tools = {"memory-analyze", "memory-search"}});

    // Test helper tools
    Register({.name = "tools-harness-state",
              .category = "tools",
              .description = "Generate WRAM state for test harnesses",
              .detailed_help =
                  "Runs the emulator to the main game loop and dumps the "
                  "complete WRAM state and register values to a C++ header "
                  "file for use in test harnesses.",
              .arguments = {{.name = "rom",
                             .type = "string",
                             .description = "Path to ROM file",
                             .required = true},
                            {.name = "output",
                             .type = "string",
                             .description = "Output header file path",
                             .required = true}},
              .examples = {"z3ed tools-harness-state --rom=zelda3.sfc "
                           "--output=harness_state.h"},
              .requires_rom = false});

    Register({.name = "tools-extract-golden",
              .category = "tools",
              .description = "Extract comprehensive golden data from ROM",
              .arguments = {{.name = "rom",
                             .type = "string",
                             .description = "Path to ROM file",
                             .required = true},
                            {.name = "output",
                             .type = "string",
                             .description = "Output header file path",
                             .required = true}},
              .examples = {"z3ed tools-extract-golden --rom=zelda3.sfc "
                           "--output=golden_data.h"}});
  }

  std::map<std::string, ToolSchema> schemas_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOL_SCHEMAS_H_

