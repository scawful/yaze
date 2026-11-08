#include "cli/handlers/game/dungeon_render_commands.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/service/render_service.h"
#include "cli/service/resources/command_context.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

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
    else if (tok == "all")
      flags = app::service::RenderOverlay::kAll;
  }
  return flags;
}

}  // namespace

absl::Status DungeonRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"room", "output"}));
  if (parser.GetString("output")->empty()) {
    return absl::InvalidArgumentError("--output cannot be empty");
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

  // Load GameData (palette groups, tileset tables).
  zelda3::LoadOptions load_options;
  if (project_ != nullptr) {
    ASSIGN_OR_RETURN(auto layout, core::ResolveGfxSheetLayout(*rom, *project_));
    // Keep ordinary ROM decoding unchanged. Expanded inspection must use the
    // identity-bound project layout rather than the legacy sheet limit.
    if (layout.explicit_layout) {
      load_options.gfx_sheet_layout = std::move(layout);
    }
  }
  zelda3::GameData game_data;
  auto gd_status = zelda3::LoadGameData(*rom, game_data, load_options);
  if (!gd_status.ok()) {
    return absl::InternalError(
        absl::StrFormat("Failed to load game data: %s", gd_status.message()));
  }

  // Render.
  app::service::RenderService render_service(rom, &game_data);
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
  if (req.entrance_id.has_value()) {
    formatter.AddHexField("entrance_id", *req.entrance_id, 2);
  }
  if (req.graphics_entrance_id.has_value()) {
    formatter.AddHexField("graphics_entrance_id", *req.graphics_entrance_id, 2);
  }
  formatter.AddHexField("main_blockset", result.main_blockset, 2);
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
