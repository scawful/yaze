#include "cli/handlers/game/dungeon_render_commands.h"

#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/service/render_service.h"
#include "cli/handlers/game/dungeon_render_object_plan.h"
#include "cli/service/resources/command_context.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/gfx_sheet_source.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/room_default_entrance.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

absl::Status RejectPreviewInputAliases(
    const std::filesystem::path& output,
    const std::vector<std::filesystem::path>& inputs) {
  for (const auto& input : inputs) {
    ASSIGN_OR_RETURN(const bool alias, resources::PathsAlias(output, input));
    if (alias) {
      return absl::InvalidArgumentError(
          "PNG output aliases a read-only object-plan/manifest input");
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::optional<int>> ParseOptionalEntranceId(
    const resources::ArgumentParser& parser, const char* option = "entrance") {
  if (!parser.GetString(option).has_value()) {
    if (parser.HasFlag(option)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "--%s requires a hexadecimal regular entrance ID", option));
    }
    return std::optional<int>();
  }
  ASSIGN_OR_RETURN(const int entrance_id, parser.GetHex(option));
  if (entrance_id < 0 || entrance_id >= zelda3::kNumRegularDungeonEntrances) {
    return absl::OutOfRangeError("Regular dungeon entrance ID is out of range");
  }
  return std::optional<int>(entrance_id);
}

// Parse overlays from a comma-separated string into RenderOverlay flags.
uint32_t ParseOverlays(const std::string& s) {
  if (s.empty())
    return app::service::RenderOverlay::kNone;
  uint32_t flags = app::service::RenderOverlay::kNone;
  std::istringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    tok.erase(0, tok.find_first_not_of(" \t"));
    if (!tok.empty())
      tok.erase(tok.find_last_not_of(" \t") + 1);
    if (tok == "collision")
      flags |= app::service::RenderOverlay::kCollision;
    else if (tok == "sprites")
      flags |= app::service::RenderOverlay::kSprites;
    else if (tok == "objects")
      flags |= app::service::RenderOverlay::kObjects;
    else if (tok == "track")
      flags |= app::service::RenderOverlay::kTrack;
    else if (tok == "camera")
      flags |= app::service::RenderOverlay::kCameraQuads;
    else if (tok == "grid")
      flags |= app::service::RenderOverlay::kGrid;
    else if (tok == "tiletype")
      flags |= app::service::RenderOverlay::kTileTypes;
    else if (tok == "items")
      flags |= app::service::RenderOverlay::kItems;
    else if (tok == "all")
      flags = app::service::RenderOverlay::kAll;
  }
  return flags;
}

// Which entrance the default graphics come from, and the others that could
// have loaded this room. Reported even with an explicit entrance so a reader
// can see what the option replaced.
void AddDefaultEntranceReport(resources::OutputFormatter& formatter,
                              const zelda3::GameData& game_data, int room_id) {
  if (room_id < 0 ||
      room_id >= static_cast<int>(game_data.room_default_entrances.size())) {
    return;
  }
  const auto& entry = game_data.room_default_entrances[room_id];
  formatter.AddField("default_entrance_source",
                     zelda3::RoomEntranceSourceName(entry.source));
  if (entry.entrance_id >= 0) {
    formatter.AddHexField("default_entrance_id", entry.entrance_id, 2);
    formatter.AddHexField("default_main_blockset", entry.main_blockset, 2);
  }
  if (entry.via_room >= 0) {
    formatter.AddHexField("default_entrance_via_room", entry.via_room, 3);
  }
  formatter.BeginArray("entrance_candidates");
  for (const auto& candidate : entry.candidates) {
    formatter.BeginObject();
    formatter.AddHexField("entrance_id", candidate.entrance_id, 2);
    formatter.AddHexField("main_blockset", candidate.main_blockset, 2);
    formatter.EndObject();
  }
  formatter.EndArray();
}

}  // namespace

absl::Status DungeonRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"room", "output"}));
  if (parser.GetString("output")->empty()) {
    return absl::InvalidArgumentError("--output cannot be empty");
  }
  if ((parser.HasFlag("object-plan") &&
       !parser.GetString("object-plan").has_value()) ||
      (parser.HasFlag("manifest") &&
       !parser.GetString("manifest").has_value())) {
    return absl::InvalidArgumentError(
        "--object-plan and --manifest require file paths");
  }
  for (const char* name : {"object-plan", "manifest"}) {
    if (const auto value = parser.GetString(name);
        value.has_value() && value->empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("--%s cannot be empty", name));
    }
  }
  if (parser.GetString("manifest").has_value() &&
      !parser.GetString("object-plan").has_value()) {
    return absl::InvalidArgumentError("--manifest requires --object-plan");
  }
  if (parser.GetString("object-plan").has_value() &&
      (parser.GetString("compiled-build-receipt").has_value() ||
       parser.HasFlag("compiled-build-receipt"))) {
    return absl::InvalidArgumentError(
        "--object-plan previews authoring edits; it cannot be combined with "
        "--compiled-build-receipt");
  }
  if (const auto scale = parser.GetString("scale"); scale.has_value()) {
    RETURN_IF_ERROR(app::service::ParseRenderScale(*scale).status());
  }
  if ((parser.GetString("entrance").has_value() ||
       parser.HasFlag("entrance")) &&
      (parser.GetString("graphics-entrance").has_value() ||
       parser.HasFlag("graphics-entrance"))) {
    return absl::InvalidArgumentError(
        "--entrance and --graphics-entrance are mutually exclusive");
  }
  RETURN_IF_ERROR(ParseOptionalEntranceId(parser).status());
  return ParseOptionalEntranceId(parser, "graphics-entrance").status();
}

absl::Status DungeonRenderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status DungeonRenderCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  // Parse --room (decimal or 0x-hex).
  auto room_str = parser.GetString("room").value();
  int room_id = 0;
  try {
    room_id = std::stoi(room_str, nullptr, 0);
  } catch (...) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid room id: %s", room_str));
  }

  // Parse --output.
  const std::string output_path = parser.GetString("output").value();
  ASSIGN_OR_RETURN(const auto resolved_output,
                   resources::ResolveStableArtifactPath(output_path));
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases(
      "--output", resolved_output, invocation_context));

  std::optional<nlohmann::json> object_plan;
  std::optional<core::HackManifest> manifest_override;
  std::vector<std::filesystem::path> preview_inputs;
  if (const auto path = parser.GetString("object-plan"); path.has_value()) {
    ASSIGN_OR_RETURN(const auto input,
                     resources::ResolveStableArtifactPath(*path));
    preview_inputs.push_back(input);
    if (const auto manifest_path = parser.GetString("manifest");
        manifest_path.has_value()) {
      ASSIGN_OR_RETURN(const auto manifest_input,
                       resources::ResolveStableArtifactPath(*manifest_path));
      preview_inputs.push_back(manifest_input);
    }
    RETURN_IF_ERROR(RejectPreviewInputAliases(resolved_output, preview_inputs));
    ASSIGN_OR_RETURN(object_plan, LoadDungeonObjectPlanFile(input));
    if (preview_inputs.size() > 1) {
      manifest_override.emplace();
      RETURN_IF_ERROR(
          manifest_override->LoadFromFile(preview_inputs[1].string()));
    }
  }

  // Parse --overlays (optional, default none).
  uint32_t overlay_flags = app::service::RenderOverlay::kNone;
  if (auto ov = parser.GetString("overlays"); ov.has_value()) {
    overlay_flags = ParseOverlays(ov.value());
  }

  // Parse --scale (optional, default 1.0, finite 0.25–8.0).
  float scale = 1.0f;
  if (auto sc = parser.GetString("scale"); sc.has_value()) {
    ASSIGN_OR_RETURN(scale, app::service::ParseRenderScale(*sc));
  }

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Load GameData (palette groups, tileset tables). Sheet pixels always come
  // from the loaded ROM; the project's graphics source is never selected here.
  const auto* compiled = invocation_context.compiled_build;
  if (compiled != nullptr && project_ == nullptr) {
    return absl::FailedPreconditionError(
        "A compiled build render requires its project context");
  }
  zelda3::LoadOptions load_options;
  if (compiled != nullptr) {
    load_options.expand_rom = false;
  }
  if (project_ != nullptr) {
    ASSIGN_OR_RETURN(
        auto layout,
        compiled != nullptr
            ? core::ResolveGfxSheetLayout(*rom, *project_, *compiled)
            : core::ResolveGfxSheetLayout(*rom, *project_));
    // Keep ordinary ROM decoding unchanged. Expanded inspection must use the
    // identity-bound project layout rather than the legacy sheet limit.
    if (layout.explicit_layout) {
      load_options.gfx_sheet_layout = std::move(layout);
    }
  }
  auto game_data_owner = std::make_unique<zelda3::GameData>();
  auto& game_data = *game_data_owner;
  auto gd_status = zelda3::LoadGameData(*rom, game_data, load_options);
  if (!gd_status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Failed to load game data: %s", gd_status.message()));
  }

  // Bind graphics against the original image above. The object-save plan and
  // rendering below use only detached RAM; never save or mutate the caller.
  std::optional<DungeonRenderObjectPreview> preview;
  if (object_plan.has_value()) {
    const auto* manifest =
        manifest_override.has_value()
            ? &*manifest_override
            : (project_ ? &project_->hack_manifest : nullptr);
    ASSIGN_OR_RETURN(preview, StageDungeonRenderObjectPlan(
                                  *rom, room_id, *object_plan, manifest));
  }
  app::service::RenderService render_service(
      preview.has_value() ? preview->rom.get() : rom, &game_data);
  app::service::RenderRequest req;
  req.room_id = room_id;
  req.overlay_flags = overlay_flags;
  req.scale = scale;
  ASSIGN_OR_RETURN(req.entrance_id, ParseOptionalEntranceId(parser));
  ASSIGN_OR_RETURN(req.graphics_entrance_id,
                   ParseOptionalEntranceId(parser, "graphics-entrance"));

  auto result_or = render_service.RenderDungeonRoom(req);
  if (!result_or.ok())
    return result_or.status();
  const auto& result = *result_or;

  // Write PNG to disk.
  // Recheck identity after rendering, before opening a truncating stream. Use
  // the resolved path, not a caller-supplied parent symlink a second time.
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases(
      "--output", resolved_output, invocation_context));
  RETURN_IF_ERROR(RejectPreviewInputAliases(resolved_output, preview_inputs));
  std::ofstream out(resolved_output, std::ios::binary);
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Cannot open output file: %s", output_path));
  }
  out.write(reinterpret_cast<const char*>(result.png_data.data()),
            static_cast<std::streamsize>(result.png_data.size()));
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Write failed for: %s", output_path));
  }
  out.close();
  if (!out) {
    return absl::InternalError(
        absl::StrFormat("Close failed for: %s", output_path));
  }

  formatter.AddField("room_id", absl::StrFormat("0x%02X", room_id));
  formatter.AddField("output", output_path);
  formatter.AddField("width", result.width);
  formatter.AddField("height", result.height);
  formatter.AddField("size_bytes", static_cast<int>(result.png_data.size()));
  if (preview.has_value()) {
    formatter.BeginObject("object_plan");
    formatter.AddField("mode", "ram_only");
    formatter.AddField("rom_sha256",
                       (*object_plan)["rom_sha256"].get<std::string>());
    formatter.AddField("source_stream_sha256", preview->source_stream_sha256);
    formatter.AddField("edit_count", preview->edit_count);
    formatter.AddField("objects_before", preview->objects_before);
    formatter.AddField("objects_after", preview->objects_after);
    formatter.AddField("rom_file_written", false);
    formatter.EndObject();
  }
  if (req.entrance_id.has_value()) {
    formatter.AddHexField("entrance_id", *req.entrance_id, 2);
  }
  if (req.graphics_entrance_id.has_value()) {
    formatter.AddHexField("graphics_entrance_id", *req.graphics_entrance_id, 2);
  }
  if (compiled != nullptr) {
    formatter.BeginObject("compiled_build");
    formatter.AddField("receipt", compiled->receipt_path);
    formatter.AddField("receipt_sha256", compiled->receipt_sha256);
    formatter.AddField("state", compiled->state);
    formatter.AddField("base_rom_sha256", compiled->base_rom_sha256);
    formatter.AddField("output_rom_sha256", compiled->output_rom_sha256);
    formatter.AddField("output_manifest_sha256", compiled->manifest_sha256);
    formatter.AddField("graphics_pixels", "compiled_rom");
    formatter.AddField(
        "project_graphics_source",
        core::GfxSheetSource::Selected(*project_) ? "not_used" : "none");
    formatter.AddField("project_metadata", "current_descriptor");
    formatter.AddField("source_tree_rehashed", false);
    formatter.EndObject();
  }
  formatter.AddHexField("main_blockset", result.main_blockset, 2);
  AddDefaultEntranceReport(formatter, game_data, room_id);
  formatter.AddField("project_objects_without_data",
                     result.project_objects_without_data);
  if (result.project_objects_without_data > 0) {
    formatter.AddField("warning",
                       "Room uses project-defined objects (0x31/0x32) that "
                       "have no data; pass --project-context=<project.yaze>");
  }
  formatter.BeginArray("slots");
  for (size_t slot = 0; slot < result.graphics_sheets.size(); ++slot) {
    formatter.BeginObject();
    formatter.AddField("slot", static_cast<int>(slot));
    formatter.AddHexField("sheet", result.graphics_sheets[slot], 2);
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
