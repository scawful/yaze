#include "cli/service/resource_catalog.h"

#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {

namespace {

ResourceSchema MakePaletteSchema() {
  ResourceSchema schema;
  schema.resource = "palette";
  schema.description =
    "Palette manipulation commands covering export, import, and color editing.";

  ResourceAction export_action;
  export_action.name = "export";
  export_action.synopsis = "z3ed palette export --group <group> --id <id> --to <file>";
  export_action.stability = "experimental";
  export_action.arguments = {
    ResourceArgument{"--group", "integer", true, "Palette group id (0-31)."},
    ResourceArgument{"--id", "integer", true, "Palette index inside the group."},
    ResourceArgument{"--to", "path", true, "Destination file path for binary export."},
  };
  export_action.effects = {
      "Reads ROM palette buffer and writes binary palette data to disk."};

  ResourceAction import_action;
  import_action.name = "import";
  import_action.synopsis =
    "z3ed palette import --group <group> --id <id> --from <file>";
  import_action.stability = "experimental";
  import_action.arguments = {
    ResourceArgument{"--group", "integer", true, "Palette group id (0-31)."},
    ResourceArgument{"--id", "integer", true, "Palette index inside the group."},
    ResourceArgument{"--from", "path", true, "Source binary palette file."},
  };
  import_action.effects = {
      "Writes imported palette bytes into ROM buffer and marks project dirty."};

  schema.actions = {export_action, import_action};
  return schema;
}

ResourceSchema MakeRomSchema() {
  ResourceSchema schema;
  schema.resource = "rom";
  schema.description = "ROM validation, diffing, and snapshot helpers.";

  ResourceAction info_action;
  info_action.name = "info";
  info_action.synopsis = "z3ed rom info --rom <file>";
  info_action.stability = "stable";
  info_action.arguments = {
    ResourceArgument{"--rom", "path", true, "Path to ROM file configured via global flag."},
  };
  info_action.effects = {
    "Reads ROM from disk and displays basic information (title, size, filename)."};
  info_action.returns = {
    {"title", "string", "ROM internal title from header."},
    {"size", "integer", "ROM file size in bytes."},
    {"filename", "string", "Full path to the ROM file."}};

  ResourceAction validate_action;
  validate_action.name = "validate";
  validate_action.synopsis = "z3ed rom validate --rom <file>";
  validate_action.stability = "stable";
  validate_action.arguments = {
    ResourceArgument{"--rom", "path", true, "Path to ROM file configured via global flag."},
  };
  validate_action.effects = {
    "Reads ROM from disk, verifies checksum, and reports header status."};
  validate_action.returns = {
    {"report", "object",
     "Structured validation summary with checksum and header results."}};

  ResourceAction diff_action;
  diff_action.name = "diff";
  diff_action.synopsis = "z3ed rom diff <rom_a> <rom_b>";
  diff_action.stability = "beta";
  diff_action.arguments = {
    ResourceArgument{"rom_a", "path", true, "Reference ROM path."},
    ResourceArgument{"rom_b", "path", true, "Candidate ROM path."},
  };
  diff_action.effects = {
    "Reads two ROM images, compares bytes, and streams differences to stdout."};
  diff_action.returns = {
    {"differences", "integer", "Count of mismatched bytes between ROMs."}};

  ResourceAction generate_action;
  generate_action.name = "generate-golden";
  generate_action.synopsis =
    "z3ed rom generate-golden <rom_file> <golden_file>";
  generate_action.stability = "experimental";
  generate_action.arguments = {
    ResourceArgument{"rom_file", "path", true, "Source ROM to snapshot."},
    ResourceArgument{"golden_file", "path", true, "Output path for golden image."},
  };
  generate_action.effects = {
    "Writes out exact ROM image for tooling baselines and diff workflows."};
  generate_action.returns = {
    {"artifact", "path", "Absolute path to the generated golden image."}};

  schema.actions = {info_action, validate_action, diff_action, generate_action};
  return schema;
}

ResourceSchema MakePatchSchema() {
  ResourceSchema schema;
  schema.resource = "patch";
  schema.description =
    "Patch authoring and application commands covering BPS and Asar flows.";

  ResourceAction apply_action;
  apply_action.name = "apply";
  apply_action.synopsis = "z3ed patch apply <rom_file> <bps_patch>";
  apply_action.stability = "beta";
  apply_action.arguments = {
    ResourceArgument{"rom_file", "path", true,
             "Source ROM image that will receive the patch."},
    ResourceArgument{"bps_patch", "path", true,
             "BPS patch to apply to the ROM."},
  };
  apply_action.effects = {
    "Loads ROM from disk, applies a BPS patch, and writes `patched.sfc`."};
  apply_action.returns = {
    {"artifact", "path",
     "Absolute path to the patched ROM image produced on success."}};

  ResourceAction asar_action;
  asar_action.name = "apply-asar";
  asar_action.synopsis = "z3ed patch apply-asar <patch.asm>";
  asar_action.stability = "prototype";
  asar_action.arguments = {
    ResourceArgument{"patch.asm", "path", true,
             "Assembly patch consumed by the bundled Asar runtime."},
    ResourceArgument{"--rom", "path", false,
             "ROM path supplied via global --rom flag."},
  };
  asar_action.effects = {
    "Invokes Asar against the active ROM buffer and applies assembled changes."};
  asar_action.returns = {
    {"log", "string", "Assembler diagnostics emitted during application."}};

  ResourceAction create_action;
  create_action.name = "create";
  create_action.synopsis =
    "z3ed patch create --source <rom> --target <rom> --out <patch.bps>";
  create_action.stability = "experimental";
  create_action.arguments = {
    ResourceArgument{"--source", "path", true,
             "Baseline ROM used when computing the patch."},
    ResourceArgument{"--target", "path", true,
             "Modified ROM to diff against the baseline."},
    ResourceArgument{"--out", "path", true,
             "Output path for the generated BPS patch."},
  };
  create_action.effects = {
    "Compares source and target images to synthesize a distributable BPS patch."};
  create_action.returns = {
    {"artifact", "path", "File system path to the generated patch."}};

  schema.actions = {apply_action, asar_action, create_action};
  return schema;
}

ResourceSchema MakeOverworldSchema() {
  ResourceSchema schema;
  schema.resource = "overworld";
  schema.description = "Overworld tile inspection and manipulation commands.";

  ResourceAction get_tile;
  get_tile.name = "get-tile";
  get_tile.synopsis = "z3ed overworld get-tile --map <map_id> --x <x> --y <y>";
  get_tile.stability = "stable";
  get_tile.arguments = {
    ResourceArgument{"--map", "integer", true, "Overworld map identifier (0-63)."},
    ResourceArgument{"--x", "integer", true, "Tile x coordinate."},
    ResourceArgument{"--y", "integer", true, "Tile y coordinate."},
  };
  get_tile.returns = {
      {"tile", "integer",
       "Tile id located at the supplied coordinates."}};

  ResourceAction set_tile;
  set_tile.name = "set-tile";
  set_tile.synopsis =
    "z3ed overworld set-tile --map <map_id> --x <x> --y <y> --tile <tile_id>";
  set_tile.stability = "experimental";
  set_tile.arguments = {
    ResourceArgument{"--map", "integer", true, "Overworld map identifier (0-63)."},
    ResourceArgument{"--x", "integer", true, "Tile x coordinate."},
    ResourceArgument{"--y", "integer", true, "Tile y coordinate."},
    ResourceArgument{"--tile", "integer", true, "Tile id to write."},
  };
  set_tile.effects = {
      "Mutates overworld tile map and enqueues render invalidation."};

  schema.actions = {get_tile, set_tile};
  return schema;
}

ResourceSchema MakeDungeonSchema() {
  ResourceSchema schema;
  schema.resource = "dungeon";
  schema.description = "Dungeon room export and inspection utilities.";

  ResourceAction export_action;
  export_action.name = "export";
  export_action.synopsis = "z3ed dungeon export <room_id>";
  export_action.stability = "prototype";
  export_action.arguments = {
      ResourceArgument{"room_id", "integer", true,
                       "Dungeon room identifier to inspect."},
  };
  export_action.effects = {
      "Loads the active ROM via --rom and prints metadata for the requested room."};
  export_action.returns = {
      {"metadata", "object",
       "Structured room summary including blockset, spriteset, palette, and layout."}};

  ResourceAction list_objects_action;
  list_objects_action.name = "list-objects";
  list_objects_action.synopsis = "z3ed dungeon list-objects <room_id>";
  list_objects_action.stability = "prototype";
  list_objects_action.arguments = {
      ResourceArgument{"room_id", "integer", true,
                       "Dungeon room identifier whose objects should be listed."},
  };
  list_objects_action.effects = {
      "Streams parsed dungeon object records for the requested room to stdout."};
  list_objects_action.returns = {
      {"objects", "array",
       "Collection of tile object records with ids, coordinates, and layers."}};

  schema.actions = {export_action, list_objects_action};
  return schema;
}

ResourceSchema MakeAgentSchema() {
  ResourceSchema schema;
  schema.resource = "agent";
  schema.description =
    "Agent workflow helpers including planning, diffing, listing, and schema discovery.";

  ResourceAction describe_action;
  describe_action.name = "describe";
  describe_action.synopsis = "z3ed agent describe --resource <name>";
  describe_action.stability = "prototype";
  describe_action.arguments = {
    ResourceArgument{"--resource", "string", false,
             "Optional resource name to filter results."},
  };
  describe_action.returns = {
      {"schema", "object",
       "JSON schema describing resource arguments and semantics."}};

  ResourceAction list_action;
  list_action.name = "list";
  list_action.synopsis = "z3ed agent list";
  list_action.stability = "prototype";
  list_action.arguments = {};
  list_action.effects = {{"reads", "proposal_registry"}};
  list_action.returns = {
      {"proposals", "array",
       "List of all proposals with ID, status, prompt, and metadata."}};

  ResourceAction diff_action;
  diff_action.name = "diff";
  diff_action.synopsis = "z3ed agent diff [--proposal-id <id>]";
  diff_action.stability = "prototype";
  diff_action.arguments = {
    ResourceArgument{"--proposal-id", "string", false,
             "Optional proposal ID to view specific proposal. Defaults to latest pending."},
  };
  diff_action.effects = {{"reads", "proposal_registry"}, {"reads", "sandbox"}};
  diff_action.returns = {
      {"diff", "string", "Unified diff showing changes to ROM."},
      {"log", "string", "Execution log of commands run."},
      {"metadata", "object", "Proposal metadata including status and timestamps."}};

  schema.actions = {describe_action, list_action, diff_action};
  return schema;
}

}  // namespace

const ResourceCatalog& ResourceCatalog::Instance() {
  static ResourceCatalog* instance = new ResourceCatalog();
  return *instance;
}

ResourceCatalog::ResourceCatalog()
  : resources_({MakeRomSchema(), MakePatchSchema(), MakePaletteSchema(),
          MakeOverworldSchema(), MakeDungeonSchema(), MakeAgentSchema()}) {}

absl::StatusOr<ResourceSchema> ResourceCatalog::GetResource(absl::string_view name) const {
  for (const auto& resource : resources_) {
    if (resource.resource == name) {
      return resource;
    }
  }
  return absl::NotFoundError(absl::StrCat("Resource not found: ", name));
}

const std::vector<ResourceSchema>& ResourceCatalog::AllResources() const { return resources_; }

std::string ResourceCatalog::SerializeResource(const ResourceSchema& schema) const {
  return SerializeResources({schema});
}

std::string ResourceCatalog::SerializeResources(const std::vector<ResourceSchema>& schemas) const {
  std::vector<std::string> entries;
  entries.reserve(schemas.size());
  for (const auto& resource : schemas) {
    std::vector<std::string> action_entries;
    action_entries.reserve(resource.actions.size());
    for (const auto& action : resource.actions) {
      std::vector<std::string> arg_entries;
      arg_entries.reserve(action.arguments.size());
      for (const auto& arg : action.arguments) {
        arg_entries.push_back(absl::StrCat(
            "{\"flag\":\"", EscapeJson(arg.flag),
            "\",\"type\":\"", EscapeJson(arg.type),
            "\",\"required\":", arg.required ? "true" : "false",
            ",\"description\":\"", EscapeJson(arg.description), "\"}"));
      }
      std::vector<std::string> effect_entries;
      effect_entries.reserve(action.effects.size());
      for (const auto& effect : action.effects) {
        effect_entries.push_back(absl::StrCat("\"", EscapeJson(effect), "\""));
      }
      std::vector<std::string> return_entries;
      return_entries.reserve(action.returns.size());
      for (const auto& ret : action.returns) {
        return_entries.push_back(absl::StrCat(
            "{\"field\":\"", EscapeJson(ret.field),
            "\",\"type\":\"", EscapeJson(ret.type),
            "\",\"description\":\"", EscapeJson(ret.description), "\"}"));
      }
      action_entries.push_back(absl::StrCat(
          "{\"name\":\"", EscapeJson(action.name),
          "\",\"synopsis\":\"", EscapeJson(action.synopsis),
          "\",\"stability\":\"", EscapeJson(action.stability),
          "\",\"arguments\":[", absl::StrJoin(arg_entries, ","), "],",
          "\"effects\":[", absl::StrJoin(effect_entries, ","), "],",
          "\"returns\":[", absl::StrJoin(return_entries, ","), "]}"));
    }
    entries.push_back(absl::StrCat(
        "{\"resource\":\"", EscapeJson(resource.resource),
        "\",\"description\":\"", EscapeJson(resource.description),
        "\",\"actions\":[", absl::StrJoin(action_entries, ","), "]}"));
  }
  return absl::StrCat("{\"resources\":[", absl::StrJoin(entries, ","), "]}");
}

std::string ResourceCatalog::EscapeJson(absl::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string ResourceCatalog::SerializeResourcesAsYaml(
    const std::vector<ResourceSchema>& schemas,
    absl::string_view version,
    absl::string_view last_updated) const {
  std::string out;
  absl::StrAppend(&out, "# Auto-generated resource catalogue\n");
  absl::StrAppend(&out, "version: ", EscapeYaml(version), "\n");
  absl::StrAppend(&out, "last_updated: ", EscapeYaml(last_updated), "\n");
  absl::StrAppend(&out, "resources:\n");

  for (const auto& resource : schemas) {
    absl::StrAppend(&out, "  - name: ", EscapeYaml(resource.resource), "\n");
    absl::StrAppend(&out, "    description: ", EscapeYaml(resource.description), "\n");

    if (resource.actions.empty()) {
      absl::StrAppend(&out, "    actions: []\n");
      continue;
    }

    absl::StrAppend(&out, "    actions:\n");
    for (const auto& action : resource.actions) {
      absl::StrAppend(&out, "      - name: ", EscapeYaml(action.name), "\n");
      absl::StrAppend(&out, "        synopsis: ", EscapeYaml(action.synopsis), "\n");
      absl::StrAppend(&out, "        stability: ", EscapeYaml(action.stability), "\n");

      if (action.arguments.empty()) {
        absl::StrAppend(&out, "        args: []\n");
      } else {
        absl::StrAppend(&out, "        args:\n");
        for (const auto& arg : action.arguments) {
          absl::StrAppend(&out, "          - flag: ", EscapeYaml(arg.flag), "\n");
          absl::StrAppend(&out, "            type: ", EscapeYaml(arg.type), "\n");
          absl::StrAppend(&out, "            required: ", arg.required ? "true\n" : "false\n");
          absl::StrAppend(&out, "            description: ", EscapeYaml(arg.description), "\n");
        }
      }

      if (action.effects.empty()) {
        absl::StrAppend(&out, "        effects: []\n");
      } else {
        absl::StrAppend(&out, "        effects:\n");
        for (const auto& effect : action.effects) {
          absl::StrAppend(&out, "          - ", EscapeYaml(effect), "\n");
        }
      }

      if (action.returns.empty()) {
        absl::StrAppend(&out, "        returns: []\n");
      } else {
        absl::StrAppend(&out, "        returns:\n");
        for (const auto& ret : action.returns) {
          absl::StrAppend(&out, "          - field: ", EscapeYaml(ret.field), "\n");
          absl::StrAppend(&out, "            type: ", EscapeYaml(ret.type), "\n");
          absl::StrAppend(&out, "            description: ", EscapeYaml(ret.description), "\n");
        }
      }
    }
  }

  return out;
}

std::string ResourceCatalog::EscapeYaml(absl::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (char c : value) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  out.push_back('"');
  return out;
}

}  // namespace cli
}  // namespace yaze
