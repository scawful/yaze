/**
 * @file tool_schemas.h
 * @brief Tool schema definitions for AI agent tool documentation
 *
 * Provides structured schema definitions that can be auto-generated
 * into LLM system prompts for better tool calling accuracy.
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_SCHEMAS_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_SCHEMAS_H_

#include <map>
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
    Register({.name = "dungeon-list-sprites",
              .category = "dungeon",
              .description = "List sprites in a dungeon room",
              .detailed_help =
                  "Lists all sprites in the room, including ID, resolved name, "
                  "X/Y, subtype, and layer. Use --sprite-registry to load "
                  "Oracle-of-Secrets custom sprite names.",
              .arguments = {{.name = "room",
                             .type = "number",
                             .description = "Room ID (0-319)",
                             .required = true},
                            {.name = "sprite-registry",
                             .type = "string",
                             .description =
                                 "Optional path to an Oracle sprite registry "
                                 "JSON to resolve custom sprite names",
                             .required = false}},
              .examples = {"z3ed dungeon-list-sprites --room=0x77",
                           "z3ed dungeon-list-sprites --room=0x77 "
                           "--sprite-registry=oracle_sprite_registry.json"},
              .related_tools = {"dungeon-describe-room", "dungeon-list-objects"}});

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

    Register({.name = "dungeon-list-objects",
              .category = "dungeon",
              .description = "List tile objects in a dungeon room",
              .detailed_help =
                  "Lists all tile objects for a room (ID, X/Y, size, layer).",
              .arguments = {{.name = "room",
                             .type = "number",
                             .description = "Room ID (0-319)",
                             .required = true}},
              .examples = {"z3ed dungeon-list-objects --room=0x77"},
              .related_tools = {"dungeon-describe-room", "dungeon-list-sprites"}});

    Register(
        {.name = "dungeon-list-custom-collision",
         .category = "dungeon",
         .description = "List custom collision tiles for a dungeon room",
         .detailed_help =
             "Loads the ZScream-style custom collision map (64x64) for a room. "
             "This is where Oracle-of-Secrets stores minecart tracks/stop tiles "
             "(B0-BE, B7-BA) and switch tiles (D0-D3). By default, returns only "
             "nonzero entries unless you pass --all or --tiles.",
         .arguments =
             {{.name = "room",
               .type = "number",
               .description = "Room ID (0-319)",
               .required = true},
              {.name = "tiles",
               .type = "string",
               .description =
                   "Comma-separated list of tile values to filter (hex), e.g. "
                   "0xB7,0xB8,0xB9,0xBA",
               .required = false},
              {.name = "nonzero",
               .type = "boolean",
               .description = "If true, return only nonzero collision tiles",
               .required = false},
              {.name = "all",
               .type = "boolean",
               .description = "If true, return all 4096 tiles (including 0x00)",
               .required = false}},
         .examples = {"z3ed dungeon-list-custom-collision --room=0x77",
                      "z3ed dungeon-list-custom-collision --room=0x77 "
                      "--tiles=0xB7,0xB8,0xB9,0xBA"},
         .related_tools = {"dungeon-map", "dungeon-minecart-audit"}});

    Register(
        {.name = "dungeon-export-custom-collision-json",
         .category = "dungeon",
         .description = "Export custom collision maps to JSON",
         .detailed_help =
             "Exports per-room custom collision tiles (64x64 map) to a JSON "
             "authoring format. Supports filtering by --room/--rooms/--all. "
             "Use --report to emit machine-readable diagnostics to disk.",
         .arguments =
             {{.name = "out",
               .type = "string",
               .description = "Output JSON path",
               .required = true},
              {.name = "room",
               .type = "number",
               .description = "Single room ID (hex)",
               .required = false},
              {.name = "rooms",
              .type = "string",
              .description = "Comma-separated room IDs (hex)",
              .required = false},
              {.name = "all",
               .type = "boolean",
               .description = "Export all dungeon rooms (0-319)",
               .required = false},
              {.name = "report",
               .type = "string",
               .description = "Optional JSON report path for automation",
               .required = false}},
         .examples = {
             "z3ed dungeon-export-custom-collision-json --all "
             "--out=custom_collision.json",
             "z3ed dungeon-export-custom-collision-json --rooms=0x25,0x27 "
             "--out=water_rooms_collision.json"},
         .related_tools = {"dungeon-import-custom-collision-json",
                           "dungeon-list-custom-collision",
                           "dungeon-map"}});

    Register(
        {.name = "dungeon-import-custom-collision-json",
         .category = "dungeon",
         .description = "Import custom collision maps from JSON",
         .detailed_help =
             "Imports custom collision room entries from JSON. Writes are "
             "ROM-safe/fail-closed and reject ROMs without expanded collision "
             "write support. Use --dry-run for validation-only and --report "
             "for machine-readable diagnostics. --replace-all requires "
             "--force unless running --dry-run.",
         .arguments = {
             {.name = "in",
              .type = "string",
              .description = "Input JSON path",
              .required = true},
             {.name = "replace-all",
              .type = "boolean",
              .description =
                  "Clear custom collision for rooms not in the JSON file",
              .required = false},
             {.name = "force",
              .type = "boolean",
              .description =
                  "Required with --replace-all in write mode (safety gate)",
              .required = false},
             {.name = "dry-run",
              .type = "boolean",
              .description = "Validate and summarize without writing ROM data",
              .required = false},
             {.name = "report",
              .type = "string",
              .description = "Optional JSON report path for automation",
              .required = false}},
         .examples = {
             "z3ed dungeon-import-custom-collision-json "
             "--in=custom_collision.json --dry-run "
             "--report=custom_collision.report.json",
             "z3ed dungeon-import-custom-collision-json "
             "--in=custom_collision.json --replace-all --force"},
         .related_tools = {"dungeon-export-custom-collision-json",
                           "dungeon-list-custom-collision",
                           "dungeon-minecart-audit"}});

    Register(
        {.name = "dungeon-export-water-fill-json",
         .category = "dungeon",
         .description = "Export water fill zones to JSON",
         .detailed_help =
             "Exports WaterFill zone data from the reserved ROM table to JSON. "
             "Supports filtering by --room/--rooms/--all. Use --report to "
             "emit machine-readable diagnostics to disk.",
         .arguments =
             {{.name = "out",
               .type = "string",
               .description = "Output JSON path",
               .required = true},
              {.name = "room",
               .type = "number",
               .description = "Single room ID (hex)",
               .required = false},
              {.name = "rooms",
              .type = "string",
              .description = "Comma-separated room IDs (hex)",
              .required = false},
              {.name = "all",
               .type = "boolean",
               .description = "Export all dungeon rooms (0-319)",
               .required = false},
              {.name = "report",
               .type = "string",
               .description = "Optional JSON report path for automation",
               .required = false}},
         .examples = {
             "z3ed dungeon-export-water-fill-json --all "
             "--out=water_fill_zones.json",
             "z3ed dungeon-export-water-fill-json --rooms=0x25,0x27 "
             "--out=d4_water_fill.json"},
         .related_tools = {"dungeon-import-water-fill-json",
                           "rom-doctor"}});

    Register(
        {.name = "dungeon-import-water-fill-json",
         .category = "dungeon",
         .description = "Import water fill zones from JSON",
         .detailed_help =
             "Imports WaterFill zones from JSON, normalizes SRAM bit masks, "
             "and writes to the reserved WaterFill ROM table. Fails closed "
             "when the reserved region is missing. Use --dry-run for "
             "validation-only and --strict-masks to fail when normalization "
             "would be required.",
         .arguments = {
             {.name = "in",
              .type = "string",
              .description = "Input JSON path",
              .required = true},
             {.name = "dry-run",
              .type = "boolean",
              .description = "Validate and summarize without writing ROM data",
              .required = false},
             {.name = "strict-masks",
              .type = "boolean",
              .description =
                  "Fail if SRAM masks require normalization (fail-closed mode)",
              .required = false},
             {.name = "report",
              .type = "string",
              .description = "Optional JSON report path for automation",
              .required = false}},
         .examples = {
             "z3ed dungeon-import-water-fill-json --in=water_fill_zones.json "
             "--dry-run --report=water_fill.report.json",
             "z3ed dungeon-import-water-fill-json --in=water_fill_zones.json "
             "--strict-masks"},
         .related_tools = {"dungeon-export-water-fill-json", "rom-doctor"}});

    Register(
        {.name = "dungeon-minecart-audit",
         .category = "dungeon",
         .description = "Audit minecart-related room data",
         .detailed_help =
             "Checks for minecart track objects (default: 0x31), minecart "
             "sprites (default: 0xA3), and minecart collision tiles in the "
             "custom collision map. Emits heuristics for common mismatches.",
         .arguments =
             {{.name = "room",
               .type = "number",
               .description = "Room ID (0-319). Mutually exclusive with rooms/all",
               .required = false},
              {.name = "rooms",
               .type = "string",
               .description =
                   "Comma-separated room list (hex), e.g. 0x77,0xA8,0xB8",
               .required = false},
              {.name = "all",
               .type = "boolean",
               .description = "If true, audit all rooms (0-319)",
               .required = false},
              {.name = "only-issues",
               .type = "boolean",
               .description = "If true, emit only rooms with issues",
               .required = false},
              {.name = "only-matches",
               .type = "boolean",
               .description =
                   "If true, emit only rooms with minecart-related data",
               .required = false},
              {.name = "include-track-objects",
               .type = "boolean",
               .description =
                   "If true, treat track objects as a match even if collision "
                   "data is absent (useful when collision work is unfinished)",
               .required = false},
              {.name = "track-object-id",
               .type = "number",
               .description = "Track object ID (default: 0x31)",
               .required = false},
              {.name = "minecart-sprite-id",
               .type = "number",
               .description = "Minecart sprite ID (default: 0xA3)",
               .required = false}},
         .examples = {"z3ed dungeon-minecart-audit --room=0x77 --only-issues",
                      "z3ed dungeon-minecart-audit --rooms=0x77,0xA8,0xB8"},
         .related_tools = {"dungeon-list-sprites",
                           "dungeon-list-objects",
                           "dungeon-list-custom-collision",
                           "dungeon-map"}});

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

    // Visual analysis tools
    Register({.name = "visual-find-similar-tiles",
              .category = "visual",
              .description = "Find tiles with similar patterns to a reference",
              .detailed_help =
                  "Compares a reference tile against all tiles in graphics "
                  "sheets and returns matches above the similarity threshold. "
                  "Useful for finding duplicate or near-duplicate tiles for "
                  "ROM optimization.",
              .arguments = {{.name = "tile_id",
                             .type = "number",
                             .description = "Reference tile ID to compare",
                             .required = true},
                            {.name = "sheet",
                             .type = "number",
                             .description = "Graphics sheet index (0-222)",
                             .required = false,
                             .default_value = "0"},
                            {.name = "threshold",
                             .type = "number",
                             .description = "Minimum similarity score (0-100)",
                             .required = false,
                             .default_value = "80"},
                            {.name = "method",
                             .type = "string",
                             .description = "Comparison method",
                             .required = false,
                             .default_value = "structural",
                             .enum_values = {"pixel", "structural"}}},
              .examples = {"z3ed visual-find-similar-tiles --tile_id=42",
                           "z3ed visual-find-similar-tiles --tile_id=10 "
                           "--sheet=5 --threshold=90"},
              .related_tools = {"visual-analyze-spritesheet",
                               "visual-tile-histogram"}});

    Register({.name = "visual-analyze-spritesheet",
              .category = "visual",
              .description = "Identify unused regions in graphics sheets",
              .detailed_help =
                  "Scans graphics sheets for contiguous empty regions that "
                  "can be used for custom graphics in ROM hacking. Reports "
                  "the location and size of each free region.",
              .arguments = {{.name = "sheet",
                             .type = "number",
                             .description = "Specific sheet to analyze (all if omitted)",
                             .required = false},
                            {.name = "tile_size",
                             .type = "number",
                             .description = "Tile size to check (8 or 16)",
                             .required = false,
                             .default_value = "8",
                             .enum_values = {"8", "16"}}},
              .examples = {"z3ed visual-analyze-spritesheet",
                           "z3ed visual-analyze-spritesheet --sheet=10 "
                           "--tile_size=16"},
              .related_tools = {"visual-find-similar-tiles"}});

    Register({.name = "visual-palette-usage",
              .category = "visual",
              .description = "Analyze palette usage statistics across maps",
              .detailed_help =
                  "Analyzes which palette indices are used across overworld "
                  "maps and dungeon rooms. Helps identify under-utilized "
                  "palettes and optimization opportunities.",
              .arguments = {{.name = "type",
                             .type = "string",
                             .description = "Map type to analyze",
                             .required = false,
                             .default_value = "all",
                             .enum_values = {"overworld", "dungeon", "all"}}},
              .examples = {"z3ed visual-palette-usage",
                           "z3ed visual-palette-usage --type=dungeon"},
              .related_tools = {"visual-tile-histogram"}});

    Register({.name = "visual-tile-histogram",
              .category = "visual",
              .description = "Generate frequency histogram of tile usage",
              .detailed_help =
                  "Counts the frequency of each tile ID used across tilemaps "
                  "to identify commonly and rarely used tiles. Useful for "
                  "understanding tile distribution and finding candidates "
                  "for replacement.",
              .arguments = {{.name = "type",
                             .type = "string",
                             .description = "Map type to analyze",
                             .required = false,
                             .default_value = "overworld",
                             .enum_values = {"overworld", "dungeon"}},
                            {.name = "top",
                             .type = "number",
                             .description = "Number of top entries to return",
                             .required = false,
                             .default_value = "20"}},
              .examples = {"z3ed visual-tile-histogram",
                           "z3ed visual-tile-histogram --type=dungeon --top=50"},
              .related_tools = {"visual-palette-usage",
                               "visual-find-similar-tiles"}});

    // =========================================================================
    // Code Generation Tools
    // =========================================================================

    Register({.name = "codegen-asm-hook",
              .category = "codegen",
              .description = "Generate ASM hook at ROM address",
              .detailed_help =
                  "Generates Asar-compatible ASM code to hook into the ROM at "
                  "a specified address using JSL. Validates the address is safe "
                  "and not already hooked. Includes known safe hook locations.",
              .arguments = {{.name = "address",
                             .type = "hex",
                             .description = "ROM address to hook (hex)",
                             .required = true},
                            {.name = "label",
                             .type = "string",
                             .description = "Label name for the hook",
                             .required = true},
                            {.name = "nop-fill",
                             .type = "number",
                             .description = "Number of NOP bytes to add",
                             .required = false,
                             .default_value = "1"}},
              .examples = {"z3ed codegen-asm-hook --address=0x02AB08 --label=MyHook",
                           "z3ed codegen-asm-hook --address=0x00893D --label=ForceBlankHook --nop-fill=2"},
              .requires_rom = true,
              .related_tools = {"codegen-freespace-patch", "memory-analyze"}});

    Register({.name = "codegen-freespace-patch",
              .category = "codegen",
              .description = "Generate patch using detected free regions",
              .detailed_help =
                  "Detects available freespace in the ROM (regions with >80% "
                  "0x00/0xFF bytes) and generates a patch to allocate space "
                  "for custom code. Returns available regions and generated ASM.",
              .arguments = {{.name = "label",
                             .type = "string",
                             .description = "Label for the code block",
                             .required = true},
                            {.name = "size",
                             .type = "hex",
                             .description = "Size in bytes needed (hex)",
                             .required = true},
                            {.name = "prefer-bank",
                             .type = "hex",
                             .description = "Preferred bank number (hex)",
                             .required = false}},
              .examples = {"z3ed codegen-freespace-patch --label=MyCode --size=0x100",
                           "z3ed codegen-freespace-patch --label=CustomRoutine --size=0x200 --prefer-bank=0x3F"},
              .requires_rom = true,
              .related_tools = {"codegen-asm-hook", "memory-regions"}});

    Register({.name = "codegen-sprite-template",
              .category = "codegen",
              .description = "Generate sprite ASM from template",
              .detailed_help =
                  "Generates a complete sprite ASM template with init and main "
                  "state machine. Includes SNES sprite variable documentation "
                  "and proper PHB/PLB register preservation.",
              .arguments = {{.name = "name",
                             .type = "string",
                             .description = "Sprite name/label",
                             .required = true},
                            {.name = "init-code",
                             .type = "string",
                             .description = "Initialization ASM code",
                             .required = false},
                            {.name = "main-code",
                             .type = "string",
                             .description = "Main loop ASM code",
                             .required = false}},
              .examples = {"z3ed codegen-sprite-template --name=MySprite",
                           "z3ed codegen-sprite-template --name=CustomChest --init-code=\"LDA #$42 : STA $0DC0,X\""},
              .requires_rom = false,
              .related_tools = {"codegen-event-handler"}});

    Register({.name = "codegen-event-handler",
              .category = "codegen",
              .description = "Generate event handler code",
              .detailed_help =
                  "Generates ASM event handler code for NMI, IRQ, or Reset "
                  "handlers. Includes proper state preservation and known "
                  "hook addresses for each event type.",
              .arguments = {{.name = "type",
                             .type = "string",
                             .description = "Event type",
                             .required = true,
                             .enum_values = {"nmi", "irq", "reset"}},
                            {.name = "label",
                             .type = "string",
                             .description = "Handler label name",
                             .required = true},
                            {.name = "custom-code",
                             .type = "string",
                             .description = "Custom ASM code",
                             .required = false}},
              .examples = {"z3ed codegen-event-handler --type=nmi --label=MyVBlank",
                           "z3ed codegen-event-handler --type=nmi --label=MyHandler --custom-code=\"LDA #$80 : STA $2100\""},
              .requires_rom = false,
              .related_tools = {"codegen-asm-hook", "codegen-sprite-template"}});

    // =========================================================================
    // Project Management Tools
    // =========================================================================

    Register({.name = "project-status",
              .category = "project",
              .description = "Show current project state and pending edits",
              .detailed_help =
                  "Displays current project state including loaded ROM info, "
                  "pending uncommitted edits, available snapshots, and ROM "
                  "checksum for validation.",
              .arguments = {},
              .examples = {"z3ed project-status"},
              .requires_rom = true,
              .related_tools = {"project-snapshot", "project-restore"}});

    Register({.name = "project-snapshot",
              .category = "project",
              .description = "Create named checkpoint with edit deltas",
              .detailed_help =
                  "Creates a named snapshot storing all pending edits as "
                  "deltas (not full ROM copy). Includes ROM checksum for "
                  "validation when restoring.",
              .arguments = {{.name = "name",
                             .type = "string",
                             .description = "Snapshot name",
                             .required = true},
                            {.name = "description",
                             .type = "string",
                             .description = "Optional description",
                             .required = false}},
              .examples = {"z3ed project-snapshot --name=before-edit",
                           "z3ed project-snapshot --name=dungeon-complete --description=\"Finished dungeon 1\""},
              .requires_rom = true,
              .related_tools = {"project-status", "project-restore", "project-diff"}});

    Register({.name = "project-restore",
              .category = "project",
              .description = "Restore ROM to named checkpoint",
              .detailed_help =
                  "Restores the ROM to a previously saved snapshot state by "
                  "replaying the stored edit deltas. Validates ROM checksum "
                  "to ensure correct base ROM.",
              .arguments = {{.name = "name",
                             .type = "string",
                             .description = "Snapshot name to restore",
                             .required = true}},
              .examples = {"z3ed project-restore --name=before-edit"},
              .requires_rom = true,
              .related_tools = {"project-snapshot", "project-status"}});

    Register({.name = "project-export",
              .category = "project",
              .description = "Export project as portable archive",
              .detailed_help =
                  "Exports the project metadata and all snapshots as a "
                  "portable archive file. Optionally includes the base ROM.",
              .arguments = {{.name = "path",
                             .type = "string",
                             .description = "Output file path",
                             .required = true},
                            {.name = "include-rom",
                             .type = "flag",
                             .description = "Include base ROM in export",
                             .required = false}},
              .examples = {"z3ed project-export --path=myproject.tar.gz",
                           "z3ed project-export --path=backup.tar.gz --include-rom"},
              .requires_rom = true,
              .related_tools = {"project-import"}});

    Register({.name = "project-import",
              .category = "project",
              .description = "Import project archive",
              .detailed_help =
                  "Imports a project archive and loads its metadata and "
                  "snapshots. Validates project structure and checksums.",
              .arguments = {{.name = "path",
                             .type = "string",
                             .description = "Archive file path",
                             .required = true}},
              .examples = {"z3ed project-import --path=myproject.tar.gz"},
              .requires_rom = false,
              .related_tools = {"project-export"}});

    Register({.name = "project-diff",
              .category = "project",
              .description = "Compare two project states",
              .detailed_help =
                  "Compares two snapshots and shows the differences in edits "
                  "between them. Useful for reviewing changes between versions.",
              .arguments = {{.name = "snapshot1",
                             .type = "string",
                             .description = "First snapshot name",
                             .required = true},
                            {.name = "snapshot2",
                             .type = "string",
                             .description = "Second snapshot name",
                             .required = true}},
              .examples = {"z3ed project-diff --snapshot1=v1 --snapshot2=v2"},
              .requires_rom = true,
              .related_tools = {"project-snapshot", "rom-diff"}});

    // =========================================================================
    // Mesen2 Debugging Tools (Live Emulator Integration)
    // =========================================================================

    Register({.name = "mesen-gamestate",
              .category = "mesen2",
              .description = "Get ALTTP game state from running Mesen2",
              .detailed_help =
                  "Queries the Mesen2 socket API for comprehensive ALTTP game "
                  "state including Link's position, direction, health, items, "
                  "and current game mode. Requires Mesen2-OoS running.",
              .arguments = {},
              .examples = {"z3ed mesen-gamestate"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-sprites", "mesen-cpu"}});

    Register({.name = "mesen-sprites",
              .category = "mesen2",
              .description = "Get active sprites from running Mesen2",
              .detailed_help =
                  "Queries sprite table from Mesen2 to show all active sprites "
                  "with their type, position, health, and state. Useful for "
                  "debugging sprite behavior and interactions.",
              .arguments = {{.name = "all",
                             .type = "boolean",
                             .description = "Include inactive sprite slots",
                             .required = false,
                             .default_value = "false"}},
              .examples = {"z3ed mesen-sprites", "z3ed mesen-sprites --all"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-gamestate", "mesen-memory-read"}});

    Register({.name = "mesen-cpu",
              .category = "mesen2",
              .description = "Get CPU register state from Mesen2",
              .detailed_help =
                  "Returns current 65816 CPU register state: A, X, Y, SP, D, "
                  "PC, K (program bank), DBR (data bank), and P (processor "
                  "status). Useful for debugging at breakpoints.",
              .arguments = {},
              .examples = {"z3ed mesen-cpu"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-gamestate", "mesen-disasm"}});

    Register({.name = "mesen-memory-read",
              .category = "mesen2",
              .description = "Read memory from Mesen2 emulator",
              .detailed_help =
                  "Reads a block of memory from the running Mesen2 instance. "
                  "Returns hex dump of the specified region. Useful for "
                  "inspecting live game state.",
              .arguments = {{.name = "address",
                             .type = "hex",
                             .description = "Start address (hex)",
                             .required = true},
                            {.name = "length",
                             .type = "number",
                             .description = "Number of bytes to read",
                             .required = false,
                             .default_value = "16"}},
              .examples = {"z3ed mesen-memory-read --address=0x7E0020 --length=16"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-memory-write", "memory-analyze"}});

    Register({.name = "mesen-memory-write",
              .category = "mesen2",
              .description = "Write memory in Mesen2 emulator",
              .detailed_help =
                  "Writes bytes to memory in the running Mesen2 instance. "
                  "Useful for testing ROM patches or modifying game state.",
              .arguments = {{.name = "address",
                             .type = "hex",
                             .description = "Target address (hex)",
                             .required = true},
                            {.name = "data",
                             .type = "string",
                             .description = "Hex bytes to write",
                             .required = true}},
              .examples = {"z3ed mesen-memory-write --address=0x7EF36D --data=A0"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-memory-read"}});

    Register({.name = "mesen-disasm",
              .category = "mesen2",
              .description = "Disassemble code at address in Mesen2",
              .detailed_help =
                  "Disassembles instructions at the specified address using "
                  "Mesen2's built-in disassembler, which uses loaded symbols "
                  "for labels.",
              .arguments = {{.name = "address",
                             .type = "hex",
                             .description = "Start address (hex)",
                             .required = true},
                            {.name = "count",
                             .type = "number",
                             .description = "Number of instructions",
                             .required = false,
                             .default_value = "10"}},
              .examples = {"z3ed mesen-disasm --address=0x008000 --count=20"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-cpu", "mesen-trace"}});

    Register({.name = "mesen-trace",
              .category = "mesen2",
              .description = "Get execution trace from Mesen2",
              .detailed_help =
                  "Returns the last N executed instructions from Mesen2's "
                  "trace log. Useful for understanding control flow leading "
                  "to a crash or unexpected behavior.",
              .arguments = {{.name = "count",
                             .type = "number",
                             .description = "Number of trace entries",
                             .required = false,
                             .default_value = "20"}},
              .examples = {"z3ed mesen-trace", "z3ed mesen-trace --count=50"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-disasm", "mesen-cpu"}});

    Register({.name = "mesen-breakpoint",
              .category = "mesen2",
              .description = "Manage breakpoints in Mesen2",
              .detailed_help =
                  "Add, remove, or list breakpoints in the running Mesen2 "
                  "instance. Supports execution, read, and write breakpoints.",
              .arguments = {{.name = "action",
                             .type = "string",
                             .description = "Breakpoint action",
                             .required = true,
                             .enum_values = {"add", "remove", "clear", "list"}},
                            {.name = "address",
                             .type = "hex",
                             .description = "Address for add/remove",
                             .required = false},
                            {.name = "type",
                             .type = "string",
                             .description = "Breakpoint type",
                             .required = false,
                             .default_value = "exec",
                             .enum_values = {"exec", "read", "write", "rw"}}},
              .examples = {"z3ed mesen-breakpoint --action=add --address=0x008000",
                           "z3ed mesen-breakpoint --action=clear"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-cpu", "mesen-trace"}});

    Register({.name = "mesen-control",
              .category = "mesen2",
              .description = "Control Mesen2 emulation state",
              .detailed_help =
                  "Pause, resume, step, or frame advance the running Mesen2 "
                  "instance. For automated testing and debugging workflows.",
              .arguments = {{.name = "action",
                             .type = "string",
                             .description = "Control action",
                             .required = true,
                             .enum_values = {"pause", "resume", "step", "frame", "reset"}}},
              .examples = {"z3ed mesen-control --action=pause",
                           "z3ed mesen-control --action=frame"},
              .requires_rom = false,
              .requires_grpc = true,
              .related_tools = {"mesen-cpu", "mesen-gamestate"}});
  }

  std::map<std::string, ToolSchema> schemas_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOL_SCHEMAS_H_
