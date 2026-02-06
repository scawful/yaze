#include "cli/handlers/game/minecart_commands.h"

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

constexpr int kCollisionWidth = 64;
constexpr int kCollisionHeight = 64;

constexpr std::array<uint8_t, 11> kDefaultTrackTiles = {0xB0, 0xB1, 0xB2, 0xB3,
                                                        0xB4, 0xB5, 0xB6, 0xBB,
                                                        0xBC, 0xBD, 0xBE};
constexpr std::array<uint8_t, 4> kDefaultStopTiles = {0xB7, 0xB8, 0xB9, 0xBA};
constexpr std::array<uint8_t, 4> kDefaultSwitchTiles = {0xD0, 0xD1, 0xD2, 0xD3};

std::unordered_map<uint8_t, bool> MakeTileSet(
    const std::vector<uint8_t>& tiles) {
  std::unordered_map<uint8_t, bool> result;
  for (uint8_t t : tiles) {
    result[t] = true;
  }
  return result;
}

std::vector<uint8_t> ToVector(const std::array<uint8_t, 11>& a) {
  return std::vector<uint8_t>(a.begin(), a.end());
}
std::vector<uint8_t> ToVector(const std::array<uint8_t, 4>& a) {
  return std::vector<uint8_t>(a.begin(), a.end());
}

absl::StatusOr<int> ParseOptionalHexArg(
    const resources::ArgumentParser& parser, const std::string& name,
    int default_value) {
  auto s = parser.GetString(name);
  if (!s.has_value()) {
    return default_value;
  }
  int v = 0;
  if (!ParseHexString(s.value(), &v)) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid %s format. Must be hex (e.g., 0x31).", name));
  }
  return v;
}

absl::StatusOr<std::vector<int>> ParseRooms(
    const resources::ArgumentParser& parser) {
  std::vector<int> rooms;

  if (parser.HasFlag("all")) {
    rooms.reserve(320);
    for (int i = 0; i < 320; ++i) {
      rooms.push_back(i);
    }
    return rooms;
  }

  auto room_opt = parser.GetString("room");
  if (room_opt.has_value()) {
    int room = 0;
    if (!ParseHexString(room_opt.value(), &room)) {
      return absl::InvalidArgumentError(
          "Invalid room ID format. Must be hex.");
    }
    rooms.push_back(room);
    return rooms;
  }

  auto rooms_opt = parser.GetString("rooms");
  if (!rooms_opt.has_value()) {
    return absl::InvalidArgumentError(
        "Missing required args. Use --room, --rooms, or --all.");
  }

  for (absl::string_view token :
       absl::StrSplit(rooms_opt.value(), ',', absl::SkipEmpty())) {
    std::string t = std::string(absl::StripAsciiWhitespace(token));
    int room = 0;
    if (!ParseHexString(t, &room)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid room in --rooms list: %s", t));
    }
    rooms.push_back(room);
  }

  return rooms;
}

struct MinecartSpriteAudit {
  int sprite_id = 0;
  int x = 0;
  int y = 0;
  int subtype = 0;
  int layer = 0;
  int tile_x = 0;
  int tile_y = 0;
  bool on_stop_tile = false;
};

struct RoomMinecartAudit {
  int room_id = 0;
  bool has_custom_collision_data = false;
  int track_collision_tiles = 0;
  int stop_tiles = 0;
  int switch_tiles = 0;
  std::set<int> track_object_subtypes;
  std::vector<MinecartSpriteAudit> minecart_sprites;
  std::vector<std::string> issues;
};

RoomMinecartAudit AuditRoom(Rom* rom, int room_id, int track_object_id,
                            int minecart_sprite_id,
                            bool include_track_objects_without_collision) {
  RoomMinecartAudit audit;
  audit.room_id = room_id;

  // Load room header, objects, sprites.
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadObjects();
  room.LoadSprites();

  for (const auto& obj : room.GetTileObjects()) {
    if (static_cast<int>(obj.id_) != track_object_id) {
      continue;
    }
    int subtype = obj.size_ & 0x1F;
    audit.track_object_subtypes.insert(subtype);
  }

  // Collision audit.
  std::unordered_map<uint8_t, bool> track_tiles =
      MakeTileSet(ToVector(kDefaultTrackTiles));
  std::unordered_map<uint8_t, bool> stop_tiles =
      MakeTileSet(ToVector(kDefaultStopTiles));
  std::unordered_map<uint8_t, bool> switch_tiles =
      MakeTileSet(ToVector(kDefaultSwitchTiles));

  std::unordered_set<int> stop_positions;
  auto map_or = zelda3::LoadCustomCollisionMap(rom, room_id);
  if (map_or.ok() && map_or.value().has_data) {
    audit.has_custom_collision_data = true;
    const auto& map = map_or.value().tiles;
    for (int y = 0; y < kCollisionHeight; ++y) {
      for (int x = 0; x < kCollisionWidth; ++x) {
        uint8_t tile = map[static_cast<size_t>(y * kCollisionWidth + x)];
        if (track_tiles[tile]) {
          ++audit.track_collision_tiles;
        }
        if (stop_tiles[tile]) {
          ++audit.stop_tiles;
          stop_positions.insert(y * kCollisionWidth + x);
        }
        if (switch_tiles[tile]) {
          ++audit.switch_tiles;
        }
      }
    }
  }

  // Sprite audit.
  for (const auto& sprite : room.GetSprites()) {
    if (static_cast<int>(sprite.id()) != minecart_sprite_id) {
      continue;
    }
    MinecartSpriteAudit spr;
    spr.sprite_id = sprite.id();
    spr.x = sprite.x();
    spr.y = sprite.y();
    spr.subtype = sprite.subtype();
    spr.layer = sprite.layer();
    spr.tile_x = spr.x * 2;
    spr.tile_y = spr.y * 2;
    if (spr.tile_x >= 0 && spr.tile_x < kCollisionWidth && spr.tile_y >= 0 &&
        spr.tile_y < kCollisionHeight) {
      spr.on_stop_tile =
          stop_positions.find(spr.tile_y * kCollisionWidth + spr.tile_x) !=
          stop_positions.end();
    }
    audit.minecart_sprites.push_back(spr);
  }

  // Issues (heuristics).
  const bool has_track_collision =
      (audit.track_collision_tiles + audit.stop_tiles + audit.switch_tiles) > 0;
  const bool has_track_objects = !audit.track_object_subtypes.empty();
  const bool has_minecart_sprites = !audit.minecart_sprites.empty();
  const bool track_objects_signal =
      has_track_objects && (include_track_objects_without_collision ||
                            has_track_collision || has_minecart_sprites);
  bool any_on_stop = false;
  for (const auto& spr : audit.minecart_sprites) {
    if (spr.on_stop_tile) {
      any_on_stop = true;
      break;
    }
  }

  if ((track_objects_signal || has_minecart_sprites) &&
      !audit.has_custom_collision_data) {
    audit.issues.push_back(
        "Room uses minecart objects/sprites but has no custom collision data.");
  }
  if (has_minecart_sprites && !has_track_collision) {
    audit.issues.push_back(
        "Minecart sprite present but room has no minecart collision tiles.");
  }
  if (track_objects_signal && !has_track_collision) {
    audit.issues.push_back(
        "Track objects present but room has no minecart collision tiles.");
  }
  if (has_track_collision && !has_track_objects) {
    audit.issues.push_back(
        "Minecart collision tiles present but no track objects (0x31) found.");
  }
  if (has_minecart_sprites && audit.stop_tiles > 0 && !any_on_stop) {
    audit.issues.push_back(
        "Minecart sprite present but none placed on a stop tile (B7-BA).");
  }
  for (const auto& spr : audit.minecart_sprites) {
    if (track_objects_signal && !audit.track_object_subtypes.empty() &&
        audit.track_object_subtypes.find(spr.subtype) ==
            audit.track_object_subtypes.end()) {
      audit.issues.push_back(absl::StrFormat(
          "Minecart sprite subtype %d is not referenced by any track objects in "
          "this room.",
          spr.subtype));
    }
  }
  if (has_track_collision && audit.stop_tiles == 0) {
    audit.issues.push_back("Minecart collision tiles present but no stop tiles.");
  }

  return audit;
}

}  // namespace

absl::Status DungeonMinecartAuditCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (parser.HasFlag("all")) {
    return absl::OkStatus();
  }
  if (parser.GetString("room").has_value()) {
    return absl::OkStatus();
  }
  if (parser.GetString("rooms").has_value()) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      "Missing required args. Use --room, --rooms, or --all.");
}

absl::Status DungeonMinecartAuditCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(auto rooms, ParseRooms(parser));

  ASSIGN_OR_RETURN(int track_object_id,
                   ParseOptionalHexArg(parser, "track-object-id", 0x31));
  ASSIGN_OR_RETURN(int minecart_sprite_id,
                   ParseOptionalHexArg(parser, "minecart-sprite-id", 0xA3));

  const bool only_issues = parser.HasFlag("only-issues");
  const bool only_matches = parser.HasFlag("only-matches");
  const bool include_track_objects = parser.HasFlag("include-track-objects");

  formatter.BeginObject("Dungeon Minecart Audit");
  formatter.AddField("total_rooms_requested", static_cast<int>(rooms.size()));
  formatter.AddHexField("track_object_id", track_object_id, 2);
  formatter.AddHexField("minecart_sprite_id", minecart_sprite_id, 2);

  int rooms_emitted = 0;
  int rooms_with_issues = 0;

  formatter.BeginArray("rooms");
  for (int room_id : rooms) {
    RoomMinecartAudit audit =
        AuditRoom(rom, room_id, track_object_id, minecart_sprite_id,
                  include_track_objects);

    const bool has_track_collision =
        (audit.track_collision_tiles + audit.stop_tiles + audit.switch_tiles) >
        0;
    const bool has_track_objects = !audit.track_object_subtypes.empty();
    const bool has_minecart_sprites = !audit.minecart_sprites.empty();

    if (!audit.issues.empty()) {
      ++rooms_with_issues;
    }
    if (only_issues && audit.issues.empty()) {
      continue;
    }
    if (only_matches && !has_track_collision &&
        !(include_track_objects && has_track_objects) &&
        !has_minecart_sprites) {
      continue;
    }

    formatter.BeginObject();
    formatter.AddField("room_id", audit.room_id);
    formatter.AddHexField("room_id_hex", audit.room_id, 2);
    formatter.AddField("has_custom_collision_data", audit.has_custom_collision_data);
    formatter.AddField("track_collision_tiles", audit.track_collision_tiles);
    formatter.AddField("stop_tiles", audit.stop_tiles);
    formatter.AddField("switch_tiles", audit.switch_tiles);

    formatter.BeginArray("track_object_subtypes");
    for (int subtype : audit.track_object_subtypes) {
      formatter.AddArrayItem(absl::StrFormat("%d", subtype));
    }
    formatter.EndArray();

    formatter.BeginArray("minecart_sprites");
    for (const auto& spr : audit.minecart_sprites) {
      formatter.BeginObject();
      formatter.AddHexField("sprite_id", spr.sprite_id, 2);
      formatter.AddField("x", spr.x);
      formatter.AddField("y", spr.y);
      formatter.AddField("subtype", spr.subtype);
      formatter.AddField("layer", spr.layer);
      formatter.AddField("tile_x", spr.tile_x);
      formatter.AddField("tile_y", spr.tile_y);
      formatter.AddField("on_stop_tile", spr.on_stop_tile);
      formatter.EndObject();
    }
    formatter.EndArray();

    formatter.BeginArray("issues");
    for (const auto& issue : audit.issues) {
      formatter.AddArrayItem(issue);
    }
    formatter.EndArray();

    formatter.EndObject();
    ++rooms_emitted;
  }
  formatter.EndArray();

  formatter.AddField("rooms_emitted", rooms_emitted);
  formatter.AddField("rooms_with_issues", rooms_with_issues);
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
