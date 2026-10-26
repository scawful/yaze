#include "cli/handlers/game/overworld_render_commands.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/service/render_service.h"
#include "cli/util/hex_util.h"
#include "core/lyra_terrain_document.h"
#include "core/overworld_graphics_preview.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

namespace ow = app::service::OverworldOverlay;

class ScopedLyraRomAddresses {
 public:
  explicit ScopedLyraRomAddresses(const core::RomAddressOverrides& project)
      : saved_(core::RomSettings::Get().address_overrides()) {
    core::RomSettings::Get().SetAddressOverrides(project);
  }
  ~ScopedLyraRomAddresses() {
    core::RomSettings::Get().SetAddressOverrides(saved_);
  }

 private:
  core::RomAddressOverrides saved_;
};

absl::StatusOr<uint32_t> ParseOverworldOverlays(const std::string& s) {
  uint32_t flags = ow::kNone;
  std::istringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    tok.erase(0, tok.find_first_not_of(" \t"));
    if (!tok.empty())
      tok.erase(tok.find_last_not_of(" \t") + 1);
    if (tok.empty())
      continue;
    if (tok == "sprites")
      flags |= ow::kSprites;
    else if (tok == "entrances")
      flags |= ow::kEntrances;
    else if (tok == "exits")
      flags |= ow::kExits;
    else if (tok == "holes")
      flags |= ow::kHoles;
    else if (tok == "items")
      flags |= ow::kItems;
    else if (tok == "grid")
      flags |= ow::kGrid;
    else if (tok == "all")
      flags = ow::kAll & ~ow::kGrid;  // grid stays opt-in
    else
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown overlay '%s' (expected sprites, entrances, "
                          "exits, holes, items, grid, all)",
                          tok));
  }
  return flags;
}

absl::StatusOr<bool> ParseAreaOverlay(const std::string& value) {
  if (value == "on" || value == "true" || value == "1")
    return true;
  if (value == "off" || value == "false" || value == "0")
    return false;
  return absl::InvalidArgumentError("--area-overlay must be on or off");
}

std::optional<std::string> OutputArg(const resources::ArgumentParser& parser) {
  if (auto out = parser.GetString("out"); out.has_value())
    return out;
  return parser.GetString("output");
}

}  // namespace

absl::Status LyraVerifyProjectCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser&,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("ROM not loaded");
  if (!project_ || !project_->project_opened())
    return absl::FailedPreconditionError(
        "Lyra build verification requires --project-context");
  if (project_->lyra_terrain_file.empty())
    return absl::FailedPreconditionError(
        "Save the project's Lyra terrain before building");

  const auto source_hash = core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(rom->data()), rom->size()});
  ScopedRomReadOnly read_only(*rom);
  ScopedLyraRomAddresses addresses(project_->rom_address_overrides);

  const auto terrain_path =
      project_->GetAbsolutePath(project_->lyra_terrain_file);
  std::error_code ec;
  const auto size = std::filesystem::file_size(terrain_path, ec);
  if (ec || size > 4 * 1024 * 1024)
    return absl::FailedPreconditionError(
        "Lyra terrain is missing or exceeds 4 MiB");
  std::ifstream file(terrain_path, std::ios::binary);
  if (!file)
    return absl::PermissionDeniedError("Cannot read Lyra terrain");
  const std::string text{std::istreambuf_iterator<char>(file), {}};
  if (file.bad())
    return absl::DataLossError("Cannot finish reading Lyra terrain");

  zelda3::GameData game_data;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, game_data, options));
  zelda3::Overworld overworld(rom, &game_data);
  RETURN_IF_ERROR(overworld.Load(rom));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(zelda3::BindLyraTerrainSource(overworld, &profile));
  ASSIGN_OR_RETURN(auto document,
                   core::LyraTerrainDocument::Parse(text, profile));
  std::unordered_map<std::string, std::string> native_bindings;
  std::unordered_map<std::string, std::array<uint16_t, 256>> resolved_palettes;
  auto inspected = nlohmann::json::array();
  int mismatches = 0;
  std::array<std::array<std::string, 3>, core::LyraTerrainDocument::kMapCount>
      verified_bindings;
  for (int i = 0; i < core::LyraTerrainDocument::kMapCount; ++i) {
    const auto& map = document.authoring().maps[i];
    for (int state = 0; state < 3; ++state) {
      const auto key = zelda3::LyraMapAssetKey(map.properties, state);
      auto found = native_bindings.find(key);
      if (found == native_bindings.end()) {
        ASSIGN_OR_RETURN(auto assets, zelda3::ResolveLyraMapAssets(
                                          overworld, map.properties, state));
        if (InspectAssets()) {
          std::array<uint16_t, 256> palette;
          for (size_t color = 0; color < palette.size(); ++color)
            palette[color] = assets.source.current_palette()[color].snes();
          resolved_palettes.emplace(key, palette);
        }
        found = native_bindings.emplace(key, std::move(assets.binding)).first;
      }
      const auto& expected = map.asset_bindings[state];
      const bool changed = !expected.empty() && expected != found->second;
      if (changed) {
        ++mismatches;
        if (!InspectAssets())
          return absl::FailedPreconditionError(absl::StrFormat(
              "Lyra map $%02X state %d native asset binding changed",
              core::LyraTerrainDocument::kFirstMap + i, state));
      }
      if (InspectAssets()) {
        inspected.push_back({
            {"map_id", core::LyraTerrainDocument::kFirstMap + i},
            {"state", state},
            {"stored_binding", expected},
            {"current_binding", found->second},
            {"binding_changed", changed},
            {"palette_words", resolved_palettes.at(key)},
        });
      }
      // Default properties bind through the verified shared profile. Preserve
      // their empty representation for the compiler's existing input contract.
      verified_bindings[i][state] = expected.empty() ? "" : found->second;
    }
  }

  if (source_hash !=
      core::ComputeSourceArtifactSha256(
          {reinterpret_cast<const char*>(rom->data()), rom->size()}))
    return absl::DataLossError("Lyra verification changed the source ROM");
  formatter.AddField("source_rom_sha256", source_hash);
  formatter.AddField("terrain_sha256", core::ComputeSourceArtifactSha256(text));
  formatter.AddField("terrain_path", terrain_path);
  formatter.AddField("map_count", core::LyraTerrainDocument::kMapCount);
  formatter.AddField("donor_map", 0x3A);
  formatter.BeginObject("profile");
  std::ostringstream sheets;
  sheets << '[';
  for (size_t i = 0; i < profile.graphics_sheets.size(); ++i) {
    if (i)
      sheets << ',';
    sheets << static_cast<int>(profile.graphics_sheets[i]);
  }
  sheets << ']';
  formatter.AddRawJsonField("graphics_sheets", sheets.str());
  formatter.AddField("main_palette", profile.main_palette);
  formatter.AddField("palette_set", profile.palette_set);
  formatter.AddField("animated_sheet", profile.animated_sheet);
  formatter.AddField("tile_count", static_cast<int>(profile.tile_count));
  formatter.AddField("tile16_definition_fingerprint",
                     profile.tile16_definition_fingerprint);
  formatter.AddField("graphics_fingerprint", profile.graphics_fingerprint);
  formatter.AddField("palette_fingerprint", profile.palette_fingerprint);
  formatter.AddField("animation_fingerprint", profile.animation_fingerprint);
  formatter.EndObject();

  if (InspectAssets()) {
    formatter.AddField("inspection_only", true);
    formatter.AddField("asset_binding_mismatches", mismatches);
    formatter.AddRawJsonField("map_assets", inspected.dump());
    return absl::OkStatus();
  }

  auto native_99 = *overworld.mutable_overworld_map(0x99);
  native_99.set_game_state(0);
  native_99.LoadAreaGraphics();
  formatter.BeginObject("native_99");
  std::ostringstream native_sheets;
  native_sheets << '[';
  for (int i = 0; i < 8; ++i) {
    if (i)
      native_sheets << ',';
    native_sheets << static_cast<int>(native_99.static_graphics(i));
  }
  native_sheets << ']';
  formatter.AddRawJsonField("graphics_sheets", native_sheets.str());
  formatter.AddField("main_palette", native_99.render_main_palette());
  formatter.AddField("palette_set", native_99.area_palette());
  formatter.AddField("animated_sheet", native_99.animated_sheet());
  formatter.AddField("backdrop_color", native_99.area_specific_bg_color());
  formatter.EndObject();
  std::string bindings = "[";
  for (int i = 0; i < core::LyraTerrainDocument::kMapCount; ++i) {
    if (i)
      bindings += ',';
    bindings += '[';
    for (int state = 0; state < 3; ++state) {
      if (state)
        bindings += ',';
      bindings += '"' + verified_bindings[i][state] + '"';
    }
    bindings += ']';
  }
  formatter.AddRawJsonField("verified_asset_bindings", bindings + "]");
  std::ostringstream definitions;
  definitions << '[';
  const auto native = overworld.tiles32_unique();
  for (size_t i = 0; i < native.size(); ++i) {
    if (i)
      definitions << ',';
    const auto& tile = native[i];
    definitions << '[' << tile.tile0_ << ',' << tile.tile1_ << ','
                << tile.tile2_ << ',' << tile.tile3_ << ']';
  }
  definitions << ']';
  formatter.AddRawJsonField("native_definitions", definitions.str());
  formatter.AddField("native_tile32_capacity", static_cast<int>(native.size()));
  return absl::OkStatus();
}

absl::StatusOr<OverworldRenderGraphicsPreview>
ResolveOverworldRenderGraphicsPreview(const project::YazeProject* project,
                                      const std::optional<std::string>& flag) {
  OverworldRenderGraphicsPreview result;
  const std::string value = flag.value_or("");
  if (value == "off" || value == "false" || value == "0") {
    result.mode = "off";
    return result;
  }
  const bool required = value == "on" || value == "true" || value == "1";
  if (!value.empty() && !required) {
    result.mode = "file";
    result.manifest = value;
  } else if (project != nullptr &&
             !project->overworld_graphics_preview.empty()) {
    result.mode = "project";
    result.manifest =
        project->GetAbsolutePath(project->overworld_graphics_preview);
  } else if (required) {
    return absl::FailedPreconditionError(
        "--graphics-preview=on needs --project-context with [files] "
        "overworld_graphics_preview");
  } else {
    result.mode = "none";
    return result;
  }
  auto preview = core::LoadOverworldGraphicsPreview(result.manifest);
  if (!preview.ok()) {
    return absl::Status(
        preview.status().code(),
        absl::StrFormat("--graphics-preview %s: %s", result.manifest,
                        preview.status().message()));
  }
  result.preview = std::move(preview).value();
  return result;
}

absl::Status OverworldRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"screen"}));
  const auto out = OutputArg(parser);
  if (!out.has_value() || out->empty()) {
    return absl::InvalidArgumentError("--out <path.png> is required");
  }
  if (const auto scale = parser.GetString("scale"); scale.has_value()) {
    RETURN_IF_ERROR(app::service::ParseRenderScale(*scale).status());
  }
  if (const auto ov = parser.GetString("overlays"); ov.has_value()) {
    RETURN_IF_ERROR(ParseOverworldOverlays(*ov).status());
  }
  if (const auto ao = parser.GetString("area-overlay"); ao.has_value()) {
    RETURN_IF_ERROR(ParseAreaOverlay(*ao).status());
  }
  if (const auto gp = parser.GetString("graphics-preview");
      gp.has_value() && gp->empty()) {
    return absl::InvalidArgumentError(
        "--graphics-preview must be on, off, or a manifest path");
  }
  if (const auto phase = parser.GetString("phase"); phase.has_value()) {
    int value = -1;
    if (!absl::SimpleAtoi(*phase, &value) || value < 0 || value > 2) {
      return absl::InvalidArgumentError("--phase must be 0, 1, or 2");
    }
  }
  return absl::OkStatus();
}

absl::Status OverworldRenderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status OverworldRenderCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));

  int screen = 0;
  if (!util::ParseHexString(*parser.GetString("screen"), &screen)) {
    return absl::InvalidArgumentError("--screen must be hex (e.g. 0x40)");
  }

  const std::string output_path = *OutputArg(parser);
  ASSIGN_OR_RETURN(const auto resolved_output,
                   resources::ResolveStableArtifactPath(output_path));
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", resolved_output,
                                                      invocation_context));

  app::service::OverworldRenderRequest req;
  req.screen_id = screen;
  if (auto ov = parser.GetString("overlays"); ov.has_value()) {
    ASSIGN_OR_RETURN(req.overlay_flags, ParseOverworldOverlays(*ov));
  }
  if (auto phase = parser.GetString("phase"); phase.has_value()) {
    absl::SimpleAtoi(*phase, &req.phase);
  }
  if (auto sc = parser.GetString("scale"); sc.has_value()) {
    ASSIGN_OR_RETURN(req.scale, app::service::ParseRenderScale(*sc));
  }
  if (auto ao = parser.GetString("area-overlay"); ao.has_value()) {
    ASSIGN_OR_RETURN(req.area_overlay, ParseAreaOverlay(*ao));
  }
  ASSIGN_OR_RETURN(auto graphics_preview,
                   ResolveOverworldRenderGraphicsPreview(
                       project_, parser.GetString("graphics-preview")));
  req.graphics_preview = std::move(graphics_preview.preview);

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  zelda3::GameData game_data;
  auto gd_status = zelda3::LoadGameData(*rom, game_data);
  if (!gd_status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Failed to load game data: %s", gd_status.message()));
  }

  app::service::RenderService render_service(rom, &game_data);
  ASSIGN_OR_RETURN(auto result, render_service.RenderOverworldArea(req));

  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", resolved_output,
                                                      invocation_context));
  std::ofstream out(resolved_output, std::ios::binary);
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Cannot open output file: %s", output_path));
  }
  out.write(reinterpret_cast<const char*>(result.png_data.data()),
            static_cast<std::streamsize>(result.png_data.size()));
  out.close();
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Write failed for: %s", output_path));
  }

  formatter.AddField("requested_screen",
                     absl::StrFormat("0x%02X", result.requested_screen));
  formatter.AddField("parent_screen",
                     absl::StrFormat("0x%02X", result.parent_screen));
  formatter.AddField("area_size", result.area_size);
  formatter.AddField("subscreen_overlay",
                     result.subscreen_overlay < 0
                         ? std::string("none")
                         : absl::StrFormat("0x%02X", result.subscreen_overlay));
  formatter.AddField("graphics_preview", graphics_preview.mode);
  if (!graphics_preview.manifest.empty()) {
    formatter.AddField("graphics_preview_manifest", graphics_preview.manifest);
  }
  formatter.AddField("graphics_preview_applied", result.graphics_preview);
  formatter.BeginArray("screens");
  for (int s : result.screens)
    formatter.AddArrayItem(absl::StrFormat("0x%02X", s));
  formatter.EndArray();
  formatter.AddField("sprite_phase", req.phase < 0
                                         ? std::string("all")
                                         : absl::StrFormat("%d", req.phase));
  formatter.AddField("output", output_path);
  formatter.AddField("width", result.width);
  formatter.AddField("height", result.height);
  formatter.AddField("scale", absl::StrFormat("%.2f", req.scale));
  formatter.AddField("size_bytes", static_cast<int>(result.png_data.size()));
  formatter.AddField(
      "coordinates",
      "marker x/y are area-local pixels at scale 1; tile = x/16, y/16");
  formatter.BeginArray("markers");
  for (const auto& m : result.markers) {
    formatter.BeginObject();
    formatter.AddField("kind", m.kind);
    formatter.AddField("id", absl::StrFormat("0x%02X", m.id));
    if (m.phase >= 0) {
      formatter.AddField("phase", m.phase);
      formatter.AddField("list_index", m.list_index);
    }
    formatter.AddField("x", m.x);
    formatter.AddField("y", m.y);
    formatter.AddField("tile", absl::StrFormat("(%d,%d)", m.x / 16, m.y / 16));
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
