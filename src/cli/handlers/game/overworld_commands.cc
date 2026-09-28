#include "cli/handlers/game/overworld_commands.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/zelda3_labels.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

absl::Status ValidateMapId(int map_id) {
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Map ID out of range: 0x%02X", map_id));
  }
  return absl::OkStatus();
}

// IDs (map, screen, tile) are hex like every other overworld command;
// coordinates are decimal with an optional 0x prefix (ArgumentParser::GetInt),
// matching the dungeon and overworld-sprite editing commands.
absl::StatusOr<int> ParseMapIdArg(const resources::ArgumentParser& parser,
                                  const std::string& name) {
  const auto value = parser.GetString(name);
  int map_id = 0;
  if (!value.has_value() || !ParseHexString(*value, &map_id)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--%s must be a hex map id (e.g. 0x40 or 40)", name));
  }
  RETURN_IF_ERROR(ValidateMapId(map_id));
  return map_id;
}

absl::StatusOr<int> ParseTileIdArg(const resources::ArgumentParser& parser) {
  const auto value = parser.GetString("tile");
  int tile_id = 0;
  if (!value.has_value() || !ParseHexString(*value, &tile_id)) {
    return absl::InvalidArgumentError(
        "--tile must be a hex tile16 id (e.g. 0x0255)");
  }
  if (tile_id < 0 || tile_id > 0xFFFF) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Tile ID out of range: 0x%X", tile_id));
  }
  return tile_id;
}

absl::StatusOr<int> ParseCoordinateArg(const resources::ArgumentParser& parser,
                                       const std::string& name) {
  auto value = parser.GetInt(name);
  if (!value.ok()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--%s must be a decimal tile coordinate (or 0x-prefixed hex): %s", name,
        value.status().message()));
  }
  return *value;
}

void AddTileLocationFields(resources::OutputFormatter& formatter,
                           const overworld::AreaTileLocation& location) {
  formatter.AddHexField("map_id", location.map_id, 2);
  formatter.AddField("world", overworld::WorldName(location.world));
  formatter.AddHexField("parent_area", location.parent_map, 2);
  formatter.AddField("area_tiles", absl::StrFormat("%dx%d", location.area_width,
                                                   location.area_height));
  formatter.AddField("x", location.area_x);
  formatter.AddField("y", location.area_y);
  formatter.AddHexField("screen", location.screen_id, 2);
  formatter.AddField("screen_x", location.screen_x);
  formatter.AddField("screen_y", location.screen_y);
  formatter.AddField("world_x", location.world_x);
  formatter.AddField("world_y", location.world_y);
}

// Mirrors OverworldEditor::Save's map path: rebuild the tile32 table from the
// edited tile16 grid, then write tile32 definitions (vanilla or expanded
// layout) and the compressed screens. Tile16 definitions are untouched.
absl::Status SaveOverworldTileGrid(Rom& rom, zelda3::Overworld& overworld) {
  const auto profile = zelda3::DetectOverworldRomProfile(rom);
  RETURN_IF_ERROR(overworld.CreateTile32Tilemap());
  if (profile.has_expanded_tile32) {
    RETURN_IF_ERROR(overworld.SaveMap32Expanded());
  } else {
    RETURN_IF_ERROR(overworld.SaveMap32Tiles());
  }
  return overworld.SaveOverworldMaps();
}

// Reload the saved bytes and require every world's tile16 grid to match the
// edited grid exactly.
absl::Status VerifyTileGridRoundTrip(const Rom& rom,
                                     const zelda3::Overworld& expected) {
  Rom reloaded;
  Rom::LoadOptions options;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(reloaded.LoadFromData(rom.vector(), options));
  zelda3::Overworld actual(&reloaded);
  RETURN_IF_ERROR(actual.Load(&reloaded));
  const auto want = expected.map_tiles();
  const auto got = actual.map_tiles();
  const zelda3::OverworldBlockset* want_worlds[] = {
      &want.light_world, &want.dark_world, &want.special_world};
  const zelda3::OverworldBlockset* got_worlds[] = {
      &got.light_world, &got.dark_world, &got.special_world};
  for (int world = 0; world < 3; ++world) {
    const auto& w = *want_worlds[world];
    const auto& g = *got_worlds[world];
    if (w.size() != g.size()) {
      return absl::DataLossError("Reloaded tile grid has a different size");
    }
    for (size_t x = 0; x < w.size(); ++x) {
      if (w[x] == g[x]) {
        continue;
      }
      for (size_t y = 0; y < w[x].size() && y < g[x].size(); ++y) {
        if (w[x][y] != g[x][y]) {
          return absl::DataLossError(absl::StrFormat(
              "Saved overworld does not read back: %s World tile (%d,%d) is "
              "0x%04X, expected 0x%04X",
              overworld::WorldName(world), static_cast<int>(x),
              static_cast<int>(y), g[x][y], w[x][y]));
        }
      }
      return absl::DataLossError("Reloaded tile grid column size differs");
    }
  }
  return absl::OkStatus();
}

// Dry-run and --write run the same save path on the in-memory ROM and verify
// that a fresh load reads back the edited grid; only --write commits it.
absl::Status ApplyTileEdit(Rom& rom, zelda3::Overworld& overworld,
                           const overworld::AreaTileLocation& location,
                           uint16_t tile_id, bool do_write, bool mock_rom,
                           resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const uint16_t before,
                   overworld::ReadAreaTile(overworld, location));
  if (before == tile_id) {
    formatter.AddField("write_status", "not-needed");
    return absl::OkStatus();
  }

  const std::vector<uint8_t> before_rom = rom.vector();
  ScopedRomTransaction transaction(rom);
  RETURN_IF_ERROR(overworld::WriteAreaTile(overworld, location, tile_id));
  RETURN_IF_ERROR(SaveOverworldTileGrid(rom, overworld));
  RETURN_IF_ERROR(VerifyTileGridRoundTrip(rom, overworld));

  // Every changed byte must fall inside the regions an overworld map save is
  // documented to own (tile32 quadrants, tile16 table, compressed map banks
  // and their pointer tables); anything else fails closed.
  const auto save_ranges = overworld.GetProjectedWriteRanges();
  const auto& after_rom = rom.vector();
  int changed_bytes = 0;
  int changed_ranges = 0;
  int outside_bytes = 0;
  size_t first_outside = 0;
  bool in_range = false;
  for (size_t i = 0; i < after_rom.size(); ++i) {
    const bool differs =
        i >= before_rom.size() || after_rom[i] != before_rom[i];
    changed_bytes += differs ? 1 : 0;
    changed_ranges += (differs && !in_range) ? 1 : 0;
    in_range = differs;
    if (!differs) {
      continue;
    }
    bool owned = false;
    for (const auto& [start, end] : save_ranges) {
      owned |= i >= start && i < end;
    }
    if (!owned && outside_bytes++ == 0) {
      first_outside = i;
    }
  }
  formatter.AddField("changed_bytes", changed_bytes);
  formatter.AddField("changed_ranges", changed_ranges);
  formatter.AddField("bytes_outside_save_ranges", outside_bytes);
  if (outside_bytes > 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Overworld save changed %d byte(s) outside its documented ranges, "
        "first at PC $%06X; nothing was written",
        outside_bytes, first_outside));
  }
  formatter.AddField("verified_grid", true);

  if (!do_write) {
    formatter.AddField("write_status", "dry-run");
    return absl::OkStatus();  // the transaction restores the ROM
  }
  if (mock_rom) {
    formatter.AddField("save_status", "mock-rom-skipped");
    return absl::OkStatus();
  }

  Rom::SaveSettings save_settings;
  save_settings.require_backup = true;
  RETURN_IF_ERROR(rom.SaveToFile(save_settings));
  transaction.Commit();
  formatter.AddField("save_status", "saved");

  // The file on disk must be exactly the verified image.
  Rom reopened;
  RETURN_IF_ERROR(reopened.LoadFromFile(rom.filename()));
  if (reopened.vector() != rom.vector()) {
    return absl::DataLossError(
        "Saved ROM file differs from the verified image");
  }
  formatter.AddField("verified_reload", true);
  return absl::OkStatus();
}

}  // namespace

absl::Status OverworldGetTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int map_id, ParseMapIdArg(parser, "map"));
  ASSIGN_OR_RETURN(const int x, ParseCoordinateArg(parser, "x"));
  ASSIGN_OR_RETURN(const int y, ParseCoordinateArg(parser, "y"));

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto location,
                   overworld::ResolveAreaTileForMap(overworld, map_id, x, y));
  ASSIGN_OR_RETURN(const uint16_t tile,
                   overworld::ReadAreaTile(overworld, location));

  formatter.BeginObject("Overworld Tile");
  AddTileLocationFields(formatter, location);
  formatter.AddHexField("tile_id", tile, 4);
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status OverworldSetTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status OverworldSetTileCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  ASSIGN_OR_RETURN(const int map_id, ParseMapIdArg(parser, "map"));
  ASSIGN_OR_RETURN(const int x, ParseCoordinateArg(parser, "x"));
  ASSIGN_OR_RETURN(const int y, ParseCoordinateArg(parser, "y"));
  ASSIGN_OR_RETURN(const int tile_value, ParseTileIdArg(parser));
  const bool do_write = parser.HasFlag("write");
  const bool mock_rom = parser.HasFlag("mock-rom");
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Same guard as the overworld sprite editors: never write a ROM that lives
  // in an Oracle of Secrets checkout unless the caller opts in explicitly.
  const auto active_path = invocation_context.active_rom_path.value_or(
      std::filesystem::path(rom->filename()));
  if (do_write && !mock_rom && !parser.HasFlag("allow-project-rom") &&
      IsOracleProjectRomPath(active_path)) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "Refusing --write to %s: it is inside an Oracle of Secrets checkout "
        "(Oracle_main.asm next to its Roms/ folder). Copy the ROM (with "
        "Data/dungeons/custom_collision.json) and write the copy, or pass "
        "--allow-project-rom deliberately.",
        active_path.string()));
  }

  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto location,
                   overworld::ResolveAreaTileForMap(overworld, map_id, x, y));
  const int tile16_count = static_cast<int>(overworld.tiles16().size());
  if (tile_value >= tile16_count) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Tile ID 0x%04X is past the last tile16 definition (0x%04X)",
        tile_value, tile16_count - 1));
  }
  ASSIGN_OR_RETURN(const uint16_t before,
                   overworld::ReadAreaTile(overworld, location));

  formatter.BeginObject("Overworld Tile Write");
  formatter.AddField("mode", do_write ? "write" : "dry-run");
  AddTileLocationFields(formatter, location);
  formatter.AddHexField("tile_before", before, 4);
  formatter.AddHexField("tile_after", tile_value, 4);
  const absl::Status status = ApplyTileEdit(*rom, overworld, location,
                                            static_cast<uint16_t>(tile_value),
                                            do_write, mock_rom, formatter);
  if (!status.ok()) {
    formatter.AddField("error", std::string(status.message()));
  }
  formatter.EndObject();
  return status;
}

absl::Status OverworldFindTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int tile_id, ParseTileIdArg(parser));

  overworld::TileSearchOptions options;
  if (parser.GetString("map").has_value()) {
    ASSIGN_OR_RETURN(const int map_id, ParseMapIdArg(parser, "map"));
    options.map_id = map_id;
  }
  if (auto world = parser.GetString("world"); world.has_value()) {
    ASSIGN_OR_RETURN(const int world_id,
                     overworld::ParseWorldSpecifier(*world));
    options.world = world_id;
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to find tile matches
  auto matches_or = overworld::FindTileMatches(
      overworld, static_cast<uint16_t>(tile_id), options);
  if (!matches_or.ok()) {
    return matches_or.status();
  }
  const auto& matches = matches_or.value();

  // Format the output
  formatter.BeginObject("Overworld Tile Search");
  formatter.AddField("tile_id", absl::StrFormat("0x%04X", tile_id));
  formatter.AddField("matches_found", static_cast<int>(matches.size()));
  formatter.AddField("coordinates",
                     "x/y are area-relative (overworld-get-tile/set-tile "
                     "input); local_x/local_y are within map_id; "
                     "global_x/global_y index the world grid");

  formatter.BeginArray("matches");
  for (const auto& match : matches) {
    formatter.BeginObject();
    formatter.AddField("map_id", absl::StrFormat("0x%02X", match.map_id));
    formatter.AddField("world", overworld::WorldName(match.world));
    if (auto area = overworld::LocateScreenTile(overworld, match.map_id,
                                                match.local_x, match.local_y);
        area.ok()) {
      formatter.AddField("parent_area",
                         absl::StrFormat("0x%02X", area->parent_map));
      formatter.AddField("x", area->area_x);
      formatter.AddField("y", area->area_y);
    }
    formatter.AddField("local_x", match.local_x);
    formatter.AddField("local_y", match.local_y);
    formatter.AddField("global_x", match.global_x);
    formatter.AddField("global_y", match.global_y);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldDescribeMapCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int screen_id, ParseMapIdArg(parser, "screen"));

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to build the map summary
  auto summary_or = overworld::BuildMapSummary(overworld, screen_id);
  if (!summary_or.ok()) {
    return summary_or.status();
  }
  const auto& summary = summary_or.value();

  // Format the output using OutputFormatter
  formatter.AddField("screen_id", absl::StrFormat("0x%02X", summary.map_id));
  formatter.AddField("world", overworld::WorldName(summary.world));

  formatter.BeginObject("grid");
  formatter.AddField("x", summary.map_x);
  formatter.AddField("y", summary.map_y);
  formatter.AddField("local_index", summary.local_index);
  formatter.EndObject();

  formatter.BeginObject("size");
  formatter.AddField("label", summary.area_size);
  formatter.AddField("is_large", summary.is_large_map);
  formatter.AddField("parent", absl::StrFormat("0x%02X", summary.parent_map));
  formatter.AddField("quadrant", summary.large_quadrant);
  // Coordinate bounds for overworld-get-tile/set-tile on this area.
  if (const auto* parent = overworld.overworld_map(summary.parent_map);
      parent != nullptr) {
    const auto [columns, rows] = overworld::AreaScreenSpan(*parent);
    formatter.AddField("area_tiles",
                       absl::StrFormat("%dx%d", columns * 32, rows * 32));
  }
  formatter.EndObject();

  formatter.AddField("message_id",
                     absl::StrFormat("0x%04X", summary.message_id));
  formatter.AddField("area_graphics",
                     absl::StrFormat("0x%02X", summary.area_graphics));
  formatter.AddField("area_palette",
                     absl::StrFormat("0x%02X", summary.area_palette));
  formatter.AddField("main_palette",
                     absl::StrFormat("0x%02X", summary.main_palette));
  formatter.AddField("animated_gfx",
                     absl::StrFormat("0x%02X", summary.animated_gfx));
  formatter.AddField("subscreen_overlay",
                     absl::StrFormat("0x%04X", summary.subscreen_overlay));
  formatter.AddField("area_specific_bg_color",
                     absl::StrFormat("0x%04X", summary.area_specific_bg_color));

  // Entrances
  formatter.BeginArray("entrances");
  auto entrances_or = zelda3::LoadEntrances(rom);
  if (entrances_or.ok()) {
    for (const auto& entrance : entrances_or.value()) {
      if (entrance.map_id_ == screen_id) {
        formatter.BeginObject();
        formatter.AddField("entrance_id", entrance.entrance_id_);
        formatter.AddField("x", entrance.x_);
        formatter.AddField("y", entrance.y_);
        formatter.AddField("is_hole", false);
        formatter.EndObject();
      }
    }
  }
  // Holes (as entrances)
  auto holes_or = zelda3::LoadHoles(rom);
  if (holes_or.ok()) {
    for (const auto& hole : holes_or.value()) {
      if (hole.map_id_ == screen_id) {
        formatter.BeginObject();
        formatter.AddField("entrance_id", hole.entrance_id_);
        formatter.AddField("x", hole.x_);
        formatter.AddField("y", hole.y_);
        formatter.AddField("is_hole", true);
        formatter.EndObject();
      }
    }
  }
  formatter.EndArray();

  // Exits
  formatter.BeginArray("exits");
  auto exits_or = zelda3::LoadExits(rom);
  if (exits_or.ok()) {
    for (const auto& exit : exits_or.value()) {
      if (exit.map_id_ == screen_id) {
        formatter.BeginObject();
        formatter.AddField("room_id", exit.room_id_);
        formatter.AddField("x", exit.x_);
        formatter.AddField("y", exit.y_);
        formatter.EndObject();
      }
    }
  }
  formatter.EndArray();

  formatter.BeginArray("sprite_graphics");
  for (uint8_t gfx : summary.sprite_graphics) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", gfx));
  }
  formatter.EndArray();

  formatter.BeginArray("sprite_palettes");
  for (uint8_t pal : summary.sprite_palettes) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", pal));
  }
  formatter.EndArray();

  formatter.BeginArray("area_music");
  for (uint8_t music : summary.area_music) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", music));
  }
  formatter.EndArray();

  formatter.BeginArray("static_graphics");
  for (uint8_t sgfx : summary.static_graphics) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", sgfx));
  }
  formatter.EndArray();

  formatter.BeginObject("overlay");
  formatter.AddField("enabled", summary.has_overlay);
  formatter.AddField("id", absl::StrFormat("0x%04X", summary.overlay_id));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListWarpsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Build the query
  overworld::WarpQuery query;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    query.map_id = map_id;
  }

  // Call the helper function to collect warp entries
  auto warps_or = overworld::CollectWarpEntries(overworld, query);
  if (!warps_or.ok()) {
    return warps_or.status();
  }
  const auto& warps = warps_or.value();

  // Format the output
  formatter.BeginObject("Overworld Warps");
  formatter.AddField("screen_filter", screen_id_str);
  formatter.AddField("total_warps", static_cast<int>(warps.size()));

  formatter.BeginArray("warps");
  for (const auto& warp : warps) {
    formatter.BeginObject();
    formatter.AddField("type", overworld::WarpTypeName(warp.type));
    formatter.AddField("map_id", absl::StrFormat("0x%02X", warp.map_id));
    formatter.AddField("world", overworld::WorldName(warp.world));
    formatter.AddField("position",
                       absl::StrFormat("(%d,%d)", warp.pixel_x, warp.pixel_y));
    formatter.AddField("map_pos", absl::StrFormat("0x%04X", warp.map_pos));

    if (warp.entrance_id.has_value()) {
      formatter.AddField("entrance_id",
                         absl::StrFormat("0x%02X", warp.entrance_id.value()));
    }
    if (warp.entrance_name.has_value()) {
      formatter.AddField("entrance_name", warp.entrance_name.value());
    }
    if (warp.room_id.has_value()) {
      formatter.AddField("room_id",
                         absl::StrFormat("0x%04X", warp.room_id.value()));
    }

    formatter.AddField("deleted", warp.deleted);
    formatter.AddField("is_hole", warp.is_hole);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListSpritesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  overworld::SpriteQuery query;
  std::string phase_filter = "all";
  if (auto phase_str = parser.GetString("phase"); phase_str.has_value()) {
    int phase = -1;
    if (!absl::SimpleAtoi(*phase_str, &phase) || phase < 0 || phase > 2) {
      return absl::InvalidArgumentError("--phase must be 0, 1, or 2");
    }
    query.phase = phase;
    phase_filter = *phase_str;
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Sprites are stored on the parent area; a child screen resolves to it.
  std::optional<int> requested_screen;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    const auto* map = overworld.overworld_map(map_id);
    if (map == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Screen 0x%02X is out of range", map_id));
    }
    requested_screen = map_id;
    query.map_id = map->parent();
  }

  auto sprites_or = overworld::CollectOverworldSprites(overworld, query);
  if (!sprites_or.ok()) {
    return sprites_or.status();
  }
  const auto& sprites = sprites_or.value();

  formatter.BeginObject("Overworld Sprites");
  formatter.AddField("screen_filter", screen_id_str);
  if (requested_screen.has_value()) {
    formatter.AddField("parent_area", absl::StrFormat("0x%02X", *query.map_id));
  }
  formatter.AddField("phase_filter", phase_filter);
  formatter.AddField(
      "phase_legend",
      "0=beginning, 1=first_part, 2=second_part (vanilla: game state "
      "<2 / 2 / >=3; Oracle ZSOW: GameState 0-1 / GameState 2 day / night or "
      "GameState 3)");
  formatter.AddField("total_sprites", static_cast<int>(sprites.size()));

  formatter.BeginArray("sprites");
  for (const auto& sprite : sprites) {
    formatter.BeginObject();
    formatter.AddField("phase", sprite.phase);
    formatter.AddField("phase_name", overworld::SpritePhaseName(sprite.phase));
    formatter.AddField("list_index", sprite.list_index);
    formatter.AddField("sprite_id",
                       absl::StrFormat("0x%02X", sprite.sprite_id));
    formatter.AddField("map_id", absl::StrFormat("0x%02X", sprite.map_id));
    formatter.AddField("world", overworld::WorldName(sprite.world));
    formatter.AddField("position",
                       absl::StrFormat("(%d,%d)", sprite.x, sprite.y));
    formatter.AddField(
        "tile", absl::StrFormat("(%d,%d)", sprite.local_x, sprite.local_y));

    if (sprite.sprite_name.has_value()) {
      formatter.AddField("name", sprite.sprite_name.value());
    }

    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldListItemsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Optional screen filter
  std::optional<int> map_filter;
  if (screen_id_str != "all") {
    int map_id;
    if (!ParseHexString(screen_id_str, &map_id)) {
      return absl::InvalidArgumentError(
          "Invalid screen ID format. Must be hex.");
    }
    map_filter = map_id;
  }

  auto maps = overworld.overworld_maps();
  ASSIGN_OR_RETURN(auto items, zelda3::LoadItems(rom, maps));

  const auto& item_names = zelda3::Zelda3Labels::GetItemNames();

  formatter.BeginObject("Overworld Items");
  formatter.AddField("screen_filter", screen_id_str);

  formatter.BeginArray("items");
  int total_items = 0;
  for (const auto& item : items) {
    if (map_filter.has_value() &&
        static_cast<int>(item.room_map_id_) != map_filter.value()) {
      continue;
    }

    std::string world_name = "Unknown";
    auto world_or = overworld::InferWorldFromMapId(item.room_map_id_);
    if (world_or.ok()) {
      world_name = overworld::WorldName(world_or.value());
    }

    formatter.BeginObject();
    formatter.AddField("item_id", absl::StrFormat("0x%02X", item.id_));

    if (item.id_ < item_names.size()) {
      formatter.AddField("item_name", item_names[item.id_]);
    }

    formatter.AddField("map_id", absl::StrFormat("0x%02X", item.room_map_id_));
    formatter.AddField("world", world_name);
    formatter.AddField("tile_pos",
                       absl::StrFormat("(%d,%d)", item.game_x_, item.game_y_));
    formatter.AddField("pixel_pos",
                       absl::StrFormat("(%d,%d)", item.x_, item.y_));
    formatter.EndObject();

    total_items++;
  }
  formatter.EndArray();
  formatter.AddField("total_items", total_items);
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldGetEntranceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex.");
  }

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // Call the helper function to get entrance details
  auto details_or = overworld::GetEntranceDetails(overworld, entrance_id);
  if (!details_or.ok()) {
    return details_or.status();
  }
  const auto& details = details_or.value();

  // Format the output
  formatter.BeginObject("Overworld Entrance");
  formatter.AddField("entrance_id",
                     absl::StrFormat("0x%02X", details.entrance_id));
  formatter.AddField("map_id", absl::StrFormat("0x%02X", details.map_id));
  formatter.AddField("world", overworld::WorldName(details.world));
  formatter.AddField("position",
                     absl::StrFormat("(%d,%d)", details.x, details.y));
  formatter.AddField("area_position", absl::StrFormat("(%d,%d)", details.area_x,
                                                      details.area_y));
  formatter.AddField("map_pos", absl::StrFormat("0x%04X", details.map_pos));
  formatter.AddField("is_hole", details.is_hole);

  if (details.entrance_name.has_value()) {
    formatter.AddField("name", details.entrance_name.value());
  }

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status OverworldTileStatsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto screen_id_str = parser.GetString("screen").value_or("all");

  // Load the Overworld from ROM
  zelda3::Overworld overworld(rom);
  auto ow_status = overworld.Load(rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  // TODO: Implement comprehensive tile statistics
  // The AnalyzeTileUsage helper requires a specific tile_id,
  // so we need a different approach to gather overall tile statistics.
  // This could involve:
  // 1. Iterating through all tiles in the overworld maps
  // 2. Building a frequency map of tile usage
  // 3. Computing statistics like unique tiles, most common tiles, etc.

  formatter.BeginObject("Overworld Tile Statistics");
  formatter.AddField("screen_filter", screen_id_str);
  formatter.AddField("status", "partial_implementation");
  formatter.AddField("message",
                     "Comprehensive tile statistics not yet implemented. "
                     "Use overworld-find-tile for specific tile analysis.");
  formatter.AddField("total_tiles", 0);
  formatter.AddField("unique_tiles", 0);

  formatter.BeginArray("tile_counts");
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
