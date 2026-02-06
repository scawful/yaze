#include "cli/handlers/game/dungeon_collision_commands.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

absl::StatusOr<std::unordered_set<int>> ParseTileFilter(
    const resources::ArgumentParser& parser) {
  std::unordered_set<int> tiles;
  auto tiles_opt = parser.GetString("tiles");
  if (!tiles_opt.has_value()) {
    return tiles;
  }

  for (absl::string_view token :
       absl::StrSplit(tiles_opt.value(), ',', absl::SkipEmpty())) {
    std::string t = std::string(absl::StripAsciiWhitespace(token));
    int v = 0;
    if (!ParseHexString(t, &v)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid tile value in --tiles: %s", t));
    }
    if (v < 0 || v > 0xFF) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Tile value out of range (0x00-0xFF): %s", t));
    }
    tiles.insert(v);
  }

  return tiles;
}

}  // namespace

absl::Status DungeonListCustomCollisionCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id = 0;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  ASSIGN_OR_RETURN(auto filter_tiles, ParseTileFilter(parser));

  const bool list_all = parser.HasFlag("all");
  const bool list_nonzero =
      parser.HasFlag("nonzero") || (!list_all && filter_tiles.empty());

  formatter.BeginObject("Dungeon Custom Collision");
  formatter.AddField("room_id", room_id);
  formatter.AddHexField("room_id_hex", room_id, 2);
  formatter.AddField("filter_mode",
                     !filter_tiles.empty()
                         ? "tiles"
                         : (list_all ? "all" : (list_nonzero ? "nonzero" : "all")));

  auto map_or = zelda3::LoadCustomCollisionMap(rom, room_id);
  if (!map_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", map_or.status().ToString());
    formatter.EndObject();
    return map_or.status();
  }

  const auto& map = map_or.value();
  formatter.AddField("has_data", map.has_data);

  int nonzero_count = 0;
  for (uint8_t tile : map.tiles) {
    if (tile != 0) {
      ++nonzero_count;
    }
  }
  formatter.AddField("nonzero_tiles", nonzero_count);

  formatter.BeginArray("tiles");
  int match_count = 0;
  if (map.has_data) {
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        uint8_t tile = map.tiles[static_cast<size_t>(y * 64 + x)];

        if (!filter_tiles.empty()) {
          if (filter_tiles.find(static_cast<int>(tile)) == filter_tiles.end()) {
            continue;
          }
        } else if (list_nonzero) {
          if (tile == 0) {
            continue;
          }
        } else if (!list_all) {
          // Default behavior if neither filter nor flags are set is nonzero.
          if (tile == 0) {
            continue;
          }
        }

        formatter.BeginObject();
        formatter.AddField("x", x);
        formatter.AddField("y", y);
        formatter.AddHexField("tile", tile, 2);
        formatter.EndObject();
        ++match_count;
      }
    }
  }
  formatter.EndArray();

  formatter.AddField("match_count", match_count);
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
