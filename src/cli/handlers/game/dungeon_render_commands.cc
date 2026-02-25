#include "cli/handlers/game/dungeon_render_commands.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/service/render_service.h"
#include "cli/service/resources/command_context.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

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
    else if (tok == "all")
      flags = app::service::RenderOverlay::kAll;
  }
  return flags;
}

}  // namespace

absl::Status DungeonRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"room", "output"});
}

absl::Status DungeonRenderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
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

  // Parse --overlays (optional, default none).
  uint32_t overlay_flags = app::service::RenderOverlay::kNone;
  if (auto ov = parser.GetString("overlays"); ov.has_value()) {
    overlay_flags = ParseOverlays(ov.value());
  }

  // Parse --scale (optional, default 1.0, clamped 0.25–8.0).
  float scale = 1.0f;
  if (auto sc = parser.GetString("scale"); sc.has_value()) {
    try {
      scale = std::stof(sc.value());
      if (scale < 0.25f)
        scale = 0.25f;
      if (scale > 8.0f)
        scale = 8.0f;
    } catch (...) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid scale value: %s", sc.value()));
    }
  }

  // Load GameData (palette groups, tileset tables).
  zelda3::GameData game_data;
  auto gd_status = zelda3::LoadGameData(*rom, game_data);
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

  auto result_or = render_service.RenderDungeonRoom(req);
  if (!result_or.ok())
    return result_or.status();
  const auto& result = *result_or;

  // Write PNG to disk.
  std::ofstream out(output_path, std::ios::binary);
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

  formatter.AddField("room_id", absl::StrFormat("0x%02X", room_id));
  formatter.AddField("output", output_path);
  formatter.AddField("width", result.width);
  formatter.AddField("height", result.height);
  formatter.AddField("size_bytes", static_cast<int>(result.png_data.size()));
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
