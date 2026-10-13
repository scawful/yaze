#include "cli/handlers/game/atlas_commands.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "yaze_config.h"
#include "zelda3/atlas/atlas_annotations.h"
#include "zelda3/atlas/atlas_json.h"
#include "zelda3/atlas/dungeon_atlas.h"
#include "zelda3/resource_labels.h"

namespace yaze::cli::handlers {

namespace {

namespace fs = std::filesystem;
namespace atlas = zelda3::atlas;

// YazeProject::Open installs its manifest in the global label provider;
// restore the previous binding when done.
class ScopedLabelManifestRestore {
 public:
  explicit ScopedLabelManifestRestore(zelda3::ResourceLabelProvider& provider)
      : provider_(provider), previous_(provider.hack_manifest()) {}
  ~ScopedLabelManifestRestore() { provider_.SetHackManifest(previous_); }
  ScopedLabelManifestRestore(const ScopedLabelManifestRestore&) = delete;
  ScopedLabelManifestRestore& operator=(const ScopedLabelManifestRestore&) =
      delete;

 private:
  zelda3::ResourceLabelProvider& provider_;
  const core::HackManifest* previous_ = nullptr;
};

std::vector<std::string> SplitList(const std::string& value) {
  std::vector<std::string> out;
  for (absl::string_view part : absl::StrSplit(value, ',')) {
    std::string item(part);
    item.erase(0, item.find_first_not_of(" \t"));
    item.erase(item.find_last_not_of(" \t") + 1);
    if (!item.empty()) {
      out.push_back(item);
    }
  }
  return out;
}

bool Exists(const std::string& path) {
  std::error_code ec;
  return !path.empty() && fs::is_regular_file(path, ec);
}

}  // namespace

absl::Status AtlasRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  const std::string view = parser.GetString("view").value_or("dungeon");
  if (view == "overworld") {
    return absl::UnimplementedError(
        "--view overworld is not implemented yet (Atlas P2); use --view "
        "dungeon");
  }
  if (view != "dungeon") {
    return absl::InvalidArgumentError("--view must be dungeon or overworld");
  }
  if (parser.GetString("out").has_value()) {
    return absl::UnimplementedError(
        "PNG output (--out) is not implemented yet (Atlas P3); use --json "
        "<path>");
  }
  const auto json = parser.GetString("json");
  if (!json.has_value() || json->empty()) {
    return absl::InvalidArgumentError("--json <path> is required");
  }
  if (auto defaults = parser.GetString("defaults");
      defaults.has_value() && *defaults != "vanilla" && *defaults != "none") {
    return absl::InvalidArgumentError("--defaults must be vanilla or none");
  }
  return absl::OkStatus();
}

absl::Status AtlasRenderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = fs::path(rom->filename());
    invocation_context.active_rom_path = fs::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status AtlasRenderCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const std::string json_arg = *parser.GetString("json");
  ASSIGN_OR_RETURN(const auto json_path,
                   resources::ResolveStableArtifactPath(json_arg));
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--json", json_path,
                                                      invocation_context));

  // ---- Project: registry, warp tags, [atlas] --------------------------
  std::vector<std::string> annotation_files;
  std::string defaults = "vanilla";
  std::vector<std::string> symbol_candidates;
  core::HackManifest manifest;
  const std::string project_arg = parser.GetString("project").value_or("");
  if (!project_arg.empty()) {
    std::error_code ec;
    if (!fs::exists(project_arg, ec)) {
      return absl::NotFoundError(
          absl::StrFormat("--project path does not exist: %s", project_arg));
    }
    if (fs::is_directory(project_arg, ec) &&
        fs::path(project_arg).extension() != ".yazeproj") {
      return absl::InvalidArgumentError(absl::StrFormat(
          "--project %s is a folder; pass the .yaze project file",
          project_arg));
    }
    project::YazeProject project;
    absl::Status status;
    {
      ScopedLabelManifestRestore restore(zelda3::GetResourceLabels());
      try {
        status = project.Open(project_arg);
      } catch (const std::exception& error) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Cannot load project '%s': %s", project_arg, error.what()));
      }
    }
    RETURN_IF_ERROR(status);
    manifest = project.hack_manifest;
    annotation_files = project.AtlasAnnotationPaths();
    defaults = project.atlas.defaults;
    for (const auto& file : annotation_files) {
      if (!Exists(file)) {
        return absl::NotFoundError(absl::StrFormat(
            "--project %s: [atlas] annotations file %s does not exist",
            project_arg, file));
      }
    }
    if (manifest.project_registry().dungeons.empty() &&
        annotation_files.empty()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "--project %s: no dungeon registry (Docs/Dev/Planning/"
          "dungeons.json beside the code folder, hack manifest or project "
          "file) and no [atlas] annotations; nothing project-specific would "
          "load. Drop --project for a ROM-only Atlas",
          project_arg));
    }
    if (!project.atlas.symbols.empty()) {
      symbol_candidates.push_back(project.AtlasSymbolsPath());
    }
    if (!project.symbols_filename.empty()) {
      symbol_candidates.push_back(
          project.GetAbsolutePath(project.symbols_filename));
    }
  }
  if (auto extra = parser.GetString("annotations"); extra.has_value()) {
    for (const auto& file : SplitList(*extra)) {
      annotation_files.push_back(file);
    }
  }
  if (auto value = parser.GetString("defaults"); value.has_value()) {
    defaults = *value;
  }
  ASSIGN_OR_RETURN(auto annotations,
                   atlas::LoadAtlasAnnotations(annotation_files, defaults));

  // ---- Symbols: --symbols, <rom>.sym, [atlas] symbols, [files] symbols ---
  std::string symbols_path;
  if (auto symbols = parser.GetString("symbols"); symbols.has_value()) {
    if (!Exists(*symbols)) {
      return absl::NotFoundError(
          absl::StrFormat("--symbols file does not exist: %s", *symbols));
    }
    symbols_path = *symbols;
  } else if (!annotations.symbol_links.empty()) {
    std::vector<std::string> candidates;
    if (!rom->filename().empty()) {
      candidates.push_back(
          fs::path(rom->filename()).replace_extension(".sym").string());
    }
    candidates.insert(candidates.end(), symbol_candidates.begin(),
                      symbol_candidates.end());
    for (const auto& candidate : candidates) {
      if (Exists(candidate)) {
        symbols_path = candidate;
        break;
      }
    }
  }

  // ---- Model ------------------------------------------------------------
  atlas::DungeonAtlasCollectOptions options;
  if (!manifest.project_registry().dungeons.empty()) {
    options.registry = &manifest.project_registry();
  }
  options.manifest = &manifest;
  options.symbol_links = annotations.symbol_links;
  options.symbols_path = symbols_path;
  Rom vanilla_rom;
  if (auto vanilla = parser.GetString("vanilla"); vanilla.has_value()) {
    RETURN_IF_ERROR(vanilla_rom.LoadFromFile(*vanilla));
    options.vanilla_rom = &vanilla_rom;
  }
  ASSIGN_OR_RETURN(auto input, atlas::CollectDungeonAtlasInput(rom, options));
  const atlas::DungeonAtlas model =
      atlas::BuildDungeonAtlas(input, annotations);

  atlas::DungeonAtlasJsonContext context;
  context.project = project_arg;
  context.generator = std::string("z3ed ") + YAZE_VERSION_STRING;
  const std::string json = atlas::DungeonAtlasToJson(model, context);
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--json", json_path,
                                                      invocation_context));
  {
    std::ofstream out(json_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      return absl::PermissionDeniedError(
          absl::StrFormat("Cannot write %s", json_arg));
    }
    out << json << "\n";
    if (!out) {
      return absl::InternalError(absl::StrFormat("Write failed: %s", json_arg));
    }
  }

  // ---- Summary ----------------------------------------------------------
  formatter.AddField("schema", atlas::kDungeonAtlasJsonSchema);
  formatter.AddField("view", "dungeon");
  formatter.AddField("json", json_arg);
  formatter.AddField("rom", model.rom.name);
  formatter.AddField("rom_sha1", model.rom.sha1);
  formatter.AddField("project", project_arg);
  formatter.BeginArray("annotations");
  for (const auto& file : model.annotation_files) {
    formatter.AddArrayItem(file);
  }
  formatter.EndArray();
  formatter.AddField("defaults", defaults);
  formatter.AddField("symbols",
                     symbols_path.empty()
                         ? std::string()
                         : fs::path(symbols_path).filename().string());
  formatter.AddField("owners_source", model.owners_from_project
                                          ? "project"
                                          : "derived_from_entrances");
  formatter.BeginObject("counts");
  formatter.AddField("free", model.counts.free);
  formatter.AddField("reclaimable", model.counts.reclaimable);
  formatter.AddField("in_use", model.counts.in_use);
  formatter.AddField("orphan", model.counts.orphan);
  formatter.AddField("reserved", model.counts.reserved);
  formatter.AddField("interior", model.counts.interior);
  formatter.AddField("areas", static_cast<int>(model.areas.size()));
  formatter.AddField("door_edges", static_cast<int>(model.edges.size()));
  formatter.EndObject();
  formatter.BeginArray("warnings");
  for (const auto& warning : model.warnings) {
    formatter.AddArrayItem(warning);
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
