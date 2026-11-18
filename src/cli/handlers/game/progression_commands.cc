#include "cli/handlers/game/progression_commands.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/handlers/game/dungeon_census_commands.h"
#include "core/hack_manifest.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_links.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::cli::handlers {

namespace {

// Door types (zelda3::DoorType values) by lock class.
constexpr int kSmallKeyDoor = 0x1C;
constexpr int kBigKeyDoor = 0x1E;
constexpr int kUnopenableBigKeyDoor = 0x2C;
// Door-list markers that split the door stream; not doors.
constexpr int kDoorSwapMarkerA = 0x14;
constexpr int kDoorSwapMarkerB = 0x16;
// Both sides of a two-sided door sit at the same wall offset; allow a little
// slack for hand-placed doors.
constexpr int kDoorPairSlack = 3;

bool IsSmallKeyStairs(int type) {
  return type == 0x20 || type == 0x22 || type == 0x24 || type == 0x26;
}

bool IsBombableDoor(int type) {
  return type == 0x28 || type == 0x2A || type == 0x2E || type == 0x30;
}

bool IsShutterDoor(int type) {
  switch (type) {
    case 0x18:
    case 0x34:
    case 0x36:
    case 0x38:
    case 0x40:
    case 0x42:
    case 0x44:
    case 0x48:
    case 0x4A:
      return true;
    default:
      return false;
  }
}

const char* DoorClass(int type) {
  if (type == kSmallKeyDoor || IsSmallKeyStairs(type))
    return "small_key";
  if (type == kBigKeyDoor || type == kUnopenableBigKeyDoor)
    return "big_key";
  if (IsBombableDoor(type))
    return "bombable";
  if (IsShutterDoor(type))
    return "shutter";
  return nullptr;
}

std::string Lower(std::string value) {
  return absl::AsciiStrToLower(value);
}

absl::StatusOr<int> ParseIdText(std::string text, int max_id) {
  text = std::string(absl::StripAsciiWhitespace(text));
  if (absl::StartsWith(text, "$"))
    text = text.substr(1);
  int value = 0;
  if (text.empty() || !util::ParseHexString(text, &value) || value < 0 ||
      value > max_id) {
    return absl::InvalidArgumentError(
        absl::StrFormat("'%s' is not a hex ID in 0x00-0x%X", text, max_id));
  }
  return value;
}

// Room IDs in a spec may be "0x2A", "$2A", "2A" or a JSON number.
absl::StatusOr<int> ParseSpecRoom(const nlohmann::json& value) {
  if (value.is_number_integer()) {
    const int room = value.get<int>();
    if (room < 0 || room >= zelda3::kDungeonRoomCount) {
      return absl::InvalidArgumentError(
          absl::StrFormat("room %d is out of range", room));
    }
    return room;
  }
  if (value.is_string()) {
    return ParseIdText(value.get<std::string>(), zelda3::kDungeonRoomCount - 1);
  }
  return absl::InvalidArgumentError("room IDs must be hex strings or numbers");
}

std::vector<std::string> StringOrArray(const nlohmann::json& value) {
  std::vector<std::string> out;
  if (value.is_string()) {
    out.push_back(value.get<std::string>());
  } else if (value.is_array()) {
    for (const auto& item : value) {
      if (item.is_string())
        out.push_back(item.get<std::string>());
    }
  }
  return out;
}

const nlohmann::json& FieldOrNull(const nlohmann::json& object,
                                  const char* key) {
  static const nlohmann::json kNull;
  auto it = object.find(key);
  return it == object.end() ? kNull : *it;
}

std::string RuleKindName(ProgressionRuleKind kind) {
  switch (kind) {
    case ProgressionRuleKind::kForbidFeature:
      return "forbid_feature";
    case ProgressionRuleKind::kForbidRequires:
      return "forbid_requires";
    case ProgressionRuleKind::kRequireSource:
      return "require_source";
  }
  return "unknown";
}

std::string RoomHex(int room_id) {
  return absl::StrFormat("0x%03X", room_id);
}

std::string RoomListJson(const std::vector<int>& rooms) {
  std::vector<std::string> items;
  items.reserve(rooms.size());
  for (int room : rooms)
    items.push_back(absl::StrCat("\"", RoomHex(room), "\""));
  return absl::StrCat("[", absl::StrJoin(items, ","), "]");
}

std::string StringListJson(const std::vector<std::string>& values) {
  return nlohmann::json(values).dump();
}

// Reads every dungeon room once through the room census, which also assigns
// each room its owner (project dungeons with --project, else derived owners).
absl::StatusOr<std::vector<ProgressionRoom>> ReadProgressionRooms(
    Rom* rom, const resources::ArgumentParser& parser) {
  zelda3::RoomCensusOptions options;
  core::HackManifest manifest;
  if (auto project_arg = parser.GetString("project"); project_arg.has_value()) {
    RETURN_IF_ERROR(LoadRoomCensusProject(*project_arg, &manifest));
    options.project = &manifest.project_registry();
    options.manifest = &manifest;
  }

  std::vector<ProgressionRoom> rooms(zelda3::kRoomCensusRoomCount);
  options.room_visitor = [&rooms, rom](int room_id, const zelda3::Room& room) {
    if (room_id < 0 || room_id >= static_cast<int>(rooms.size()))
      return;
    ProgressionRoom& out = rooms[room_id];
    out.room_id = room_id;
    out.content = MakeDungeonFindRoom(room, room_id);

    const auto& doors = room.GetDoors();
    for (size_t i = 0; i < doors.size(); ++i) {
      const int type = static_cast<int>(doors[i].type);
      if (type == kDoorSwapMarkerA || type == kDoorSwapMarkerB)
        continue;
      const int direction = static_cast<int>(doors[i].direction);
      const auto [tile_x, tile_y] = doors[i].GetTileCoords();
      out.doors.push_back({.index = static_cast<int>(i),
                           .type = type,
                           .direction = direction,
                           .along = direction <= 1 ? tile_x : tile_y});
    }

    std::set<int> object_ids;
    for (const auto& object : room.GetTileObjects()) {
      const auto object_options = static_cast<int>(object.options());
      if ((object_options & static_cast<int>(zelda3::ObjectOption::Torch)) !=
              0 ||
          (object_options & static_cast<int>(zelda3::ObjectOption::Block)) !=
              0) {
        continue;
      }
      object_ids.insert(static_cast<int>(object.id_) & 0xFFFF);
    }
    out.object_ids.assign(object_ids.begin(), object_ids.end());

    // Custom collision is read straight from its table so the result does not
    // depend on which loaders the census ran.
    std::set<int> collision_values;
    if (auto map = zelda3::LoadCustomCollisionMap(rom, room_id); map.ok()) {
      for (size_t offset = 0; offset < map->tiles.size(); ++offset) {
        if (map->HasOverride(offset))
          collision_values.insert(map->tiles[offset]);
      }
    }
    out.collision_values.assign(collision_values.begin(),
                                collision_values.end());
  };

  ASSIGN_OR_RETURN(auto input, zelda3::CollectRoomCensusInput(rom, options));
  const zelda3::RoomCensus census = zelda3::BuildRoomCensus(input);
  for (size_t i = 0; i < census.rooms.size() && i < rooms.size(); ++i) {
    const auto& entry = census.rooms[i];
    rooms[i].room_id = static_cast<int>(i);
    if (entry.owner_index >= 0 &&
        entry.owner_index < static_cast<int>(census.owners.size())) {
      const auto& owner = census.owners[entry.owner_index];
      rooms[i].owner_id = owner.id;
      rooms[i].owner_name = owner.name;
      rooms[i].interior = owner.interior || entry.interior;
    }
  }
  return rooms;
}

absl::StatusOr<int> ParseRoomArg(const resources::ArgumentParser& parser) {
  auto value = parser.GetString("room");
  if (!value.has_value())
    return -1;
  return ParseIdText(*value, zelda3::kDungeonRoomCount - 1);
}

}  // namespace

// ---------------------------------------------------------------------------
// dungeon-key-balance

KeyItemIds ResolveKeyItemIds() {
  KeyItemIds ids;
  const auto& receipts = zelda3::Zelda3Labels::GetItemReceiptNames();
  for (size_t i = 0; i < receipts.size(); ++i) {
    const std::string name =
        std::string(absl::StripAsciiWhitespace(receipts[i]));
    if (name == "Key")
      ids.small_key_receipts.insert(static_cast<int>(i));
    else if (name == "Big Key")
      ids.big_key_receipts.insert(static_cast<int>(i));
  }
  for (int id = 0; id <= 0xFF; ++id) {
    if (id > 0x16 && (id < 0x80 || (id & 1) != 0))
      continue;
    const std::string name = DungeonPotItemName(id);
    if (name == "Key")
      ids.small_key_pots.insert(id);
    else if (name == "Big Key")
      ids.big_key_pots.insert(id);
  }
  return ids;
}

std::vector<DungeonKeyBalance> ComputeDungeonKeyBalances(
    const std::vector<ProgressionRoom>& rooms, const KeyItemIds& ids) {
  std::vector<std::string> order;
  std::map<std::string, std::vector<const ProgressionRoom*>> groups;
  for (const auto& room : rooms) {
    if (room.owner_id.empty())
      continue;
    auto [it, inserted] = groups.try_emplace(room.owner_id);
    if (inserted)
      order.push_back(room.owner_id);
    it->second.push_back(&room);
  }

  std::vector<DungeonKeyBalance> balances;
  for (const std::string& owner_id : order) {
    auto& group = groups[owner_id];
    std::sort(group.begin(), group.end(),
              [](const ProgressionRoom* a, const ProgressionRoom* b) {
                return a->room_id < b->room_id;
              });
    DungeonKeyBalance balance;
    balance.owner_id = owner_id;
    balance.owner_name = group.front()->owner_name;
    balance.interior = group.front()->interior;
    std::map<int, const ProgressionRoom*> by_id;
    for (const ProgressionRoom* room : group) {
      balance.rooms.push_back(room->room_id);
      by_id[room->room_id] = room;
    }

    // Sources.
    for (const ProgressionRoom* room : group) {
      const auto& content = room->content;
      for (size_t i = 0; i < content.chests.size(); ++i) {
        const auto& chest = content.chests[i];
        const bool small = ids.small_key_receipts.count(chest.id) > 0;
        const bool big = ids.big_key_receipts.count(chest.id) > 0;
        if (small || big) {
          balance.sources.push_back({room->room_id, "chest",
                                     static_cast<int>(i), chest.id, big,
                                     chest.name});
        }
      }
      for (size_t i = 0; i < content.pots.size(); ++i) {
        const auto& pot = content.pots[i];
        const bool small = ids.small_key_pots.count(pot.id) > 0;
        const bool big = ids.big_key_pots.count(pot.id) > 0;
        if (small || big) {
          balance.sources.push_back({room->room_id, "pot", static_cast<int>(i),
                                     pot.id, big, pot.name});
        }
      }
      for (size_t i = 0; i < content.sprites.size(); ++i) {
        const auto& sprite = content.sprites[i];
        if (sprite.key_drop == 1 || sprite.key_drop == 2) {
          balance.sources.push_back({room->room_id, "sprite",
                                     static_cast<int>(i), sprite.id,
                                     sprite.key_drop == 2, sprite.name});
        }
      }
    }

    // Sinks. A key door drawn on both sides of a wall is one lock: pair each
    // key door with an unpaired door of the same class in the neighbor room,
    // on the opposite wall, at about the same offset.
    std::set<std::pair<int, int>> paired;  // (room, door index) already used
    for (const ProgressionRoom* room : group) {
      for (const auto& door : room->doors) {
        if (door.type == kUnopenableBigKeyDoor) {
          balance.unopenable_big_key_door_rooms.push_back(room->room_id);
          continue;
        }
        const bool small_door = door.type == kSmallKeyDoor;
        const bool small_stairs = IsSmallKeyStairs(door.type);
        const bool big_door = door.type == kBigKeyDoor;
        if (!small_door && !small_stairs && !big_door)
          continue;
        KeySink sink;
        sink.room_id = room->room_id;
        sink.kind = small_door     ? "small_key_door"
                    : small_stairs ? "small_key_stairs"
                                   : "big_key_door";
        sink.index = door.index;
        sink.raw = door.type;
        sink.big = big_door;
        if (small_stairs) {
          ++balance.small_key_door_sides;
          ++balance.small_key_doors;
          balance.sinks.push_back(sink);
          continue;
        }
        if (small_door)
          ++balance.small_key_door_sides;
        if (paired.count({room->room_id, door.index}) > 0) {
          // The other side of a lock already counted; list it for reference.
          for (const auto& other : balance.sinks) {
            if (other.paired_room == room->room_id &&
                other.paired_index == door.index) {
              sink.paired_room = other.room_id;
              sink.paired_index = other.index;
              break;
            }
          }
          balance.sinks.push_back(sink);
          continue;
        }
        const int neighbor =
            zelda3::DungeonNeighborRoom(room->room_id, door.direction);
        auto neighbor_it = by_id.find(neighbor);
        if (neighbor_it != by_id.end()) {
          for (const auto& other : neighbor_it->second->doors) {
            if (other.type != door.type ||
                other.direction != (door.direction ^ 1) ||
                std::abs(other.along - door.along) > kDoorPairSlack ||
                paired.count({neighbor, other.index}) > 0) {
              continue;
            }
            sink.paired_room = neighbor;
            sink.paired_index = other.index;
            paired.insert({neighbor, other.index});
            break;
          }
        }
        paired.insert({room->room_id, door.index});
        if (small_door)
          ++balance.small_key_doors;
        else
          ++balance.big_key_sinks;
        balance.sinks.push_back(sink);
      }
      for (size_t i = 0; i < room->content.chests.size(); ++i) {
        const auto& chest = room->content.chests[i];
        if (!chest.is_big)
          continue;
        balance.sinks.push_back(
            {room->room_id, "big_chest", static_cast<int>(i), chest.id, true});
        ++balance.big_key_sinks;
      }
    }

    for (const auto& source : balance.sources) {
      if (source.big)
        ++balance.big_key_sources;
      else
        ++balance.small_keys;
    }
    balance.balance = balance.small_keys - balance.small_key_doors;
    if (balance.balance < 0)
      balance.status = "short";
    else if (balance.big_key_sinks > 0 && balance.big_key_sources == 0)
      balance.status = "no_big_key";
    else if (balance.balance > 0)
      balance.status = "surplus";
    else
      balance.status = "ok";

    balance.notes.push_back(
        "key blocks are not modeled by yaze; they are not counted as sinks");
    if (!balance.unopenable_big_key_door_rooms.empty()) {
      balance.notes.push_back(
          "unopenable big key doors (0x2C) are listed, not counted");
    }
    balances.push_back(std::move(balance));
  }
  return balances;
}

absl::Status DungeonKeyBalanceCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (auto format = parser.GetString("format"); format.has_value()) {
    const std::string lowered = Lower(*format);
    if (lowered != "json" && lowered != "table" && lowered != "text") {
      return absl::InvalidArgumentError("--format must be json or table");
    }
  }
  return absl::OkStatus();
}

namespace {

bool MatchesDungeonArg(const DungeonKeyBalance& balance,
                       const std::string& arg) {
  const std::string lowered = Lower(arg);
  if (Lower(balance.owner_id) == lowered ||
      Lower(balance.owner_name) == lowered)
    return true;
  // A bare hex dungeon ID matches the derived "IDnn" owner.
  std::string text = lowered;
  if (absl::StartsWith(text, "$"))
    text = text.substr(1);
  int id = 0;
  if (util::ParseHexString(text, &id)) {
    return Lower(balance.owner_id) == Lower(absl::StrFormat("ID%02X", id));
  }
  return false;
}

std::string FormatKeyBalanceTable(
    const std::vector<const DungeonKeyBalance*>& balances) {
  std::string out =
      absl::StrFormat("%-6s %-24s %5s %5s %5s %4s %4s  %s\n", "owner", "name",
                      "keys", "locks", "bal", "bk+", "bk-", "status");
  for (const DungeonKeyBalance* balance : balances) {
    out += absl::StrFormat("%-6s %-24s %5d %5d %+5d %4d %4d  %s%s\n",
                           balance->owner_id, balance->owner_name.substr(0, 24),
                           balance->small_keys, balance->small_key_doors,
                           balance->balance, balance->big_key_sources,
                           balance->big_key_sinks, balance->status,
                           balance->interior ? " (interior)" : "");
  }
  return out;
}

}  // namespace

absl::Status DungeonKeyBalanceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(auto rooms, ReadProgressionRooms(rom, parser));
  const auto balances = ComputeDungeonKeyBalances(rooms, ResolveKeyItemIds());

  std::vector<const DungeonKeyBalance*> selected;
  const auto dungeon_arg = parser.GetString("dungeon");
  for (const auto& balance : balances) {
    if (dungeon_arg.has_value() && !MatchesDungeonArg(balance, *dungeon_arg))
      continue;
    selected.push_back(&balance);
  }
  if (dungeon_arg.has_value() && selected.empty()) {
    return absl::NotFoundError(
        absl::StrFormat("no dungeon owner matches '%s'", *dungeon_arg));
  }

  std::vector<std::string> failing;
  formatter.BeginObject("Dungeon Key Balance");
  formatter.AddRawText(FormatKeyBalanceTable(selected));
  formatter.BeginArray("dungeons");
  for (const DungeonKeyBalance* balance : selected) {
    formatter.BeginObject();
    formatter.AddField("owner", balance->owner_id);
    formatter.AddField("name", balance->owner_name);
    formatter.AddField("interior", balance->interior);
    formatter.AddRawJsonField("rooms", RoomListJson(balance->rooms));
    formatter.AddField("small_keys", balance->small_keys);
    formatter.AddField("small_key_doors", balance->small_key_doors);
    formatter.AddField("small_key_door_sides", balance->small_key_door_sides);
    formatter.AddField("balance", balance->balance);
    formatter.AddField("big_key_sources", balance->big_key_sources);
    formatter.AddField("big_key_sinks", balance->big_key_sinks);
    formatter.AddField("status", balance->status);
    formatter.BeginArray("sources");
    for (const auto& source : balance->sources) {
      formatter.BeginObject();
      formatter.AddHexField("room", source.room_id, 3);
      formatter.AddField("kind", source.kind);
      formatter.AddField("index", source.index);
      formatter.AddHexField("raw", source.raw, 2);
      formatter.AddField("big", source.big);
      formatter.AddField("name", source.name);
      formatter.EndObject();
    }
    formatter.EndArray();
    formatter.BeginArray("sinks");
    for (const auto& sink : balance->sinks) {
      formatter.BeginObject();
      formatter.AddHexField("room", sink.room_id, 3);
      formatter.AddField("kind", sink.kind);
      formatter.AddField("index", sink.index);
      formatter.AddHexField("raw", sink.raw, 2);
      formatter.AddField("big", sink.big);
      if (sink.paired_room >= 0) {
        formatter.AddHexField("paired_room", sink.paired_room, 3);
        formatter.AddField("paired_index", sink.paired_index);
      }
      formatter.EndObject();
    }
    formatter.EndArray();
    formatter.AddRawJsonField(
        "unopenable_big_key_door_rooms",
        RoomListJson(balance->unopenable_big_key_door_rooms));
    formatter.AddRawJsonField("notes", StringListJson(balance->notes));
    formatter.EndObject();
    if (!balance->interior &&
        (balance->status == "short" || balance->status == "no_big_key")) {
      failing.push_back(
          absl::StrFormat("%s %s", balance->owner_id, balance->status));
    }
  }
  formatter.EndArray();
  formatter.AddField("failing", static_cast<int>(failing.size()));
  formatter.EndObject();

  if (!failing.empty()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "dungeon key balance failed: ", absl::StrJoin(failing, ", ")));
  }
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// progression-audit

std::vector<std::string> ProgressionRoomFeatures(const ProgressionRoom& room) {
  std::set<std::string> features;
  const auto& content = room.content;
  if (content.is_dark)
    features.insert("dark");
  for (const auto& chest : content.chests) {
    if (chest.is_big)
      features.insert("big_chest");
  }
  for (const auto& door : room.doors) {
    features.insert(absl::StrFormat("door:0x%02X", door.type));
    if (const char* door_class = DoorClass(door.type))
      features.insert(absl::StrCat("door_class:", door_class));
  }
  for (int tag : {content.tag1, content.tag2}) {
    if (tag != 0)
      features.insert(absl::StrFormat("tag:0x%02X", tag));
  }
  for (const auto& sprite : content.sprites)
    features.insert(absl::StrFormat("sprite:0x%02X", sprite.id));
  for (int object_id : room.object_ids)
    features.insert(absl::StrFormat("object:0x%03X", object_id));
  for (int value : room.collision_values)
    features.insert(absl::StrFormat("collision:0x%02X", value));
  return {features.begin(), features.end()};
}

absl::StatusOr<std::string> NormalizeProgressionFeature(
    const std::string& feature, const std::string& id) {
  std::string kind = Lower(std::string(absl::StripAsciiWhitespace(feature)));
  std::string value = std::string(absl::StripAsciiWhitespace(id));
  if (const size_t colon = kind.find(':'); colon != std::string::npos) {
    if (!value.empty()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "feature '%s' has an ID and an \"id\" field", feature));
    }
    value = std::string(absl::StripAsciiWhitespace(feature.substr(colon + 1)));
    kind = kind.substr(0, colon);
  }
  if (kind == "dark" || kind == "big_chest") {
    if (!value.empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("feature '%s' takes no ID", kind));
    }
    return kind;
  }
  if (kind == "door_class") {
    const std::string lowered = Lower(value);
    if (lowered != "small_key" && lowered != "big_key" &&
        lowered != "bombable" && lowered != "shutter") {
      return absl::InvalidArgumentError(
          "door_class must be small_key, big_key, bombable or shutter");
    }
    return absl::StrCat("door_class:", lowered);
  }
  int max_id = 0;
  int width = 2;
  if (kind == "door" || kind == "tag" || kind == "sprite" ||
      kind == "collision") {
    max_id = 0xFF;
  } else if (kind == "object") {
    max_id = 0xFFF;
    width = 3;
  } else {
    return absl::InvalidArgumentError(absl::StrFormat(
        "unknown feature '%s' (dark, big_chest, door, door_class, tag, "
        "sprite, object, collision)",
        feature));
  }
  if (value.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("feature '%s' needs an ID", kind));
  }
  ASSIGN_OR_RETURN(int parsed, ParseIdText(value, max_id));
  return absl::StrFormat("%s:0x%0*X", kind, width, parsed);
}

namespace {

absl::StatusOr<ProgressionSpec> ParseSpecObject(const nlohmann::json& root) {
  if (!root.is_object())
    return absl::InvalidArgumentError("spec must be a JSON object");

  ProgressionSpec spec;
  if (root.contains("gates")) {
    if (!root["gates"].is_array())
      return absl::InvalidArgumentError("\"gates\" must be an array");
    for (const auto& gate_json : root["gates"]) {
      if (!gate_json.is_object() || !gate_json.contains("feature") ||
          !gate_json["feature"].is_string()) {
        return absl::InvalidArgumentError(
            "each gate needs a string \"feature\"");
      }
      std::string id;
      if (gate_json.contains("id")) {
        const auto& id_json = gate_json["id"];
        id = id_json.is_string() ? id_json.get<std::string>()
                                 : absl::StrFormat("0x%X", id_json.get<int>());
      }
      ProgressionGate gate;
      ASSIGN_OR_RETURN(gate.feature,
                       NormalizeProgressionFeature(
                           gate_json["feature"].get<std::string>(), id));
      gate.required_items = StringOrArray(FieldOrNull(gate_json, "requires"));
      if (gate.required_items.empty()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "gate '%s' needs \"requires\" (an item or a list)", gate.feature));
      }
      spec.gates.push_back(std::move(gate));
    }
  }

  if (root.contains("regions")) {
    if (!root["regions"].is_object())
      return absl::InvalidArgumentError("\"regions\" must be an object");
    for (const auto& [name, region_json] : root["regions"].items()) {
      ProgressionRegion region;
      region.name = name;
      if (!region_json.is_object()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("region '%s' must be an object", name));
      }
      for (const auto& owner :
           StringOrArray(FieldOrNull(region_json, "owners"))) {
        region.owners.insert(owner);
      }
      if (region_json.contains("rooms")) {
        for (const auto& room_json : region_json["rooms"]) {
          ASSIGN_OR_RETURN(int room, ParseSpecRoom(room_json));
          region.rooms.insert(room);
        }
      }
      for (const auto& include :
           StringOrArray(FieldOrNull(region_json, "includes"))) {
        region.includes.insert(include);
      }
      spec.regions[name] = std::move(region);
    }
    for (const auto& [name, region] : spec.regions) {
      for (const auto& include : region.includes) {
        if (spec.regions.count(include) == 0) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "region '%s' includes unknown region '%s'", name, include));
        }
      }
    }
  }

  if (root.contains("item_sources")) {
    const auto sources = StringOrArray(root["item_sources"]);
    spec.chest_sources = false;
    spec.pot_sources = false;
    for (const auto& source : sources) {
      const std::string lowered = Lower(source);
      if (lowered == "chests" || lowered == "chest")
        spec.chest_sources = true;
      else if (lowered == "pots" || lowered == "pot")
        spec.pot_sources = true;
      else
        return absl::InvalidArgumentError(
            absl::StrFormat("unknown item source '%s' (chests, pots)", source));
    }
  }

  if (root.contains("item_ids")) {
    const auto& ids = root["item_ids"];
    if (!ids.is_object())
      return absl::InvalidArgumentError("\"item_ids\" must be an object");
    for (const auto& [kind, table] : ids.items()) {
      std::map<int, std::string>* target = nullptr;
      if (kind == "chest")
        target = &spec.chest_item_names;
      else if (kind == "pot")
        target = &spec.pot_item_names;
      else
        return absl::InvalidArgumentError(
            absl::StrFormat("unknown item_ids kind '%s' (chest, pot)", kind));
      if (!table.is_object()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("item_ids.%s must be an object", kind));
      }
      for (const auto& [id_text, name] : table.items()) {
        ASSIGN_OR_RETURN(int id, ParseIdText(id_text, 0xFF));
        if (!name.is_string()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("item_ids.%s.%s must be a name", kind, id_text));
        }
        (*target)[id] = name.get<std::string>();
      }
    }
  }

  if (root.contains("rules")) {
    if (!root["rules"].is_array())
      return absl::InvalidArgumentError("\"rules\" must be an array");
    int index = 0;
    for (const auto& rule_json : root["rules"]) {
      ++index;
      if (!rule_json.is_object())
        return absl::InvalidArgumentError("each rule must be an object");
      ProgressionRule rule;
      rule.name = rule_json.value("name", absl::StrFormat("rule %d", index));
      int kinds = 0;
      if (rule_json.contains("forbid_feature")) {
        ++kinds;
        rule.kind = ProgressionRuleKind::kForbidFeature;
        ASSIGN_OR_RETURN(rule.value,
                         NormalizeProgressionFeature(
                             rule_json["forbid_feature"].get<std::string>()));
      }
      if (rule_json.contains("forbid_requires")) {
        ++kinds;
        rule.kind = ProgressionRuleKind::kForbidRequires;
        rule.value = rule_json["forbid_requires"].get<std::string>();
      }
      if (rule_json.contains("require_source")) {
        ++kinds;
        rule.kind = ProgressionRuleKind::kRequireSource;
        rule.value = rule_json["require_source"].get<std::string>();
      }
      if (kinds != 1) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "rule '%s' needs exactly one of forbid_feature, forbid_requires, "
            "require_source",
            rule.name));
      }
      rule.regions = StringOrArray(FieldOrNull(rule_json, "regions"));
      if (rule.regions.empty()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("rule '%s' needs \"regions\"", rule.name));
      }
      for (const auto& region : rule.regions) {
        if (spec.regions.count(region) == 0) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "rule '%s' names unknown region '%s'", rule.name, region));
        }
      }
      spec.rules.push_back(std::move(rule));
    }
  }
  return spec;
}

}  // namespace

absl::StatusOr<ProgressionSpec> ParseProgressionSpec(const std::string& text) {
  try {
    return ParseSpecObject(nlohmann::json::parse(text));
  } catch (const nlohmann::json::exception& error) {
    // Parse errors and wrong field types (a number where a name belongs).
    return absl::InvalidArgumentError(
        absl::StrCat("invalid progression spec: ", error.what()));
  }
}

namespace {

bool RoomInRegion(const ProgressionRoom& room, const ProgressionSpec& spec,
                  const std::string& name, std::set<std::string>* visiting) {
  if (!visiting->insert(name).second)
    return false;  // include cycle
  auto it = spec.regions.find(name);
  if (it == spec.regions.end())
    return false;
  const ProgressionRegion& region = it->second;
  if (region.rooms.count(room.room_id) > 0)
    return true;
  if (!room.owner_id.empty() && region.owners.count(room.owner_id) > 0)
    return true;
  for (const auto& include : region.includes) {
    if (RoomInRegion(room, spec, include, visiting))
      return true;
  }
  return false;
}

bool RoomInRegion(const ProgressionRoom& room, const ProgressionSpec& spec,
                  const std::string& name) {
  std::set<std::string> visiting;
  return RoomInRegion(room, spec, name, &visiting);
}

bool InSpecRooms(const ProgressionSpec& spec, int room_id) {
  for (const auto& [name, region] : spec.regions) {
    if (region.rooms.count(room_id) > 0)
      return true;
  }
  return false;
}

}  // namespace

ProgressionAudit RunProgressionAudit(const std::vector<ProgressionRoom>& rooms,
                                     const ProgressionSpec& spec) {
  ProgressionAudit audit;
  std::map<std::string, const ProgressionGate*> gates;
  for (const auto& gate : spec.gates)
    gates[gate.feature] = &gate;

  for (const auto& room : rooms) {
    if (room.owner_id.empty() && !InSpecRooms(spec, room.room_id))
      continue;
    ProgressionRoomResult result;
    result.room_id = room.room_id;
    result.owner_id = room.owner_id;
    for (const auto& [name, region] : spec.regions) {
      if (RoomInRegion(room, spec, name))
        result.regions.push_back(name);
    }
    result.features = ProgressionRoomFeatures(room);
    std::set<std::string> required;
    for (const auto& feature : result.features) {
      auto it = gates.find(feature);
      if (it == gates.end())
        continue;
      result.gated_features.push_back(feature);
      for (const auto& item : it->second->required_items) {
        if (required.insert(item).second)
          result.required_items.push_back(item);
      }
    }
    std::set<std::string> given;
    auto add_give = [&](const std::string& item, const std::string& where) {
      result.gives.push_back(absl::StrFormat("%s (%s)", item, where));
      if (given.insert(item).second)
        result.give_items.push_back(item);
    };
    if (spec.chest_sources) {
      for (size_t i = 0; i < room.content.chests.size(); ++i) {
        const auto& chest = room.content.chests[i];
        auto named = spec.chest_item_names.find(chest.id);
        add_give(
            named != spec.chest_item_names.end() ? named->second : chest.name,
            absl::StrFormat("chest %d", static_cast<int>(i)));
      }
    }
    if (spec.pot_sources) {
      for (size_t i = 0; i < room.content.pots.size(); ++i) {
        const auto& pot = room.content.pots[i];
        if (pot.id == 0)
          continue;
        auto named = spec.pot_item_names.find(pot.id);
        add_give(named != spec.pot_item_names.end() ? named->second : pot.name,
                 absl::StrFormat("pot %d", static_cast<int>(i)));
      }
    }
    for (const auto& item : result.required_items)
      audit.items[item].required_by.push_back(room.room_id);
    for (const auto& item : result.give_items)
      audit.items[item].given_by.push_back(room.room_id);
    audit.rooms.push_back(std::move(result));
  }

  for (const auto& rule : spec.rules) {
    ProgressionRuleResult result;
    result.name = rule.name;
    result.kind = RuleKindName(rule.kind);
    result.value = rule.value;
    result.regions = rule.regions;
    const std::string wanted = Lower(rule.value);
    bool any_source = false;
    int rooms_checked = 0;
    for (const auto& room : audit.rooms) {
      bool in_scope = false;
      for (const auto& region : rule.regions) {
        if (std::find(room.regions.begin(), room.regions.end(), region) !=
            room.regions.end()) {
          in_scope = true;
          break;
        }
      }
      if (!in_scope)
        continue;
      ++rooms_checked;
      switch (rule.kind) {
        case ProgressionRuleKind::kForbidFeature:
          if (std::find(room.features.begin(), room.features.end(),
                        rule.value) != room.features.end()) {
            result.offending_rooms.push_back(room.room_id);
          }
          break;
        case ProgressionRuleKind::kForbidRequires:
          for (const auto& item : room.required_items) {
            if (Lower(item) == wanted) {
              result.offending_rooms.push_back(room.room_id);
              break;
            }
          }
          break;
        case ProgressionRuleKind::kRequireSource:
          for (const auto& item : room.give_items) {
            if (Lower(item) == wanted)
              any_source = true;
          }
          break;
      }
    }
    if (rule.kind == ProgressionRuleKind::kRequireSource) {
      result.pass = any_source;
      result.detail =
          any_source ? "source found"
                     : absl::StrFormat("no chest or pot gives '%s' in %d rooms",
                                       rule.value, rooms_checked);
    } else {
      result.pass = result.offending_rooms.empty();
      result.detail = absl::StrFormat(
          "%d of %d rooms offend",
          static_cast<int>(result.offending_rooms.size()), rooms_checked);
    }
    if (rooms_checked == 0 && rule.kind != ProgressionRuleKind::kRequireSource)
      result.detail = "no rooms in these regions";
    if (!result.pass)
      ++audit.rules_failed;
    audit.rules.push_back(std::move(result));
  }
  return audit;
}

absl::Status ProgressionAuditCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"spec"}));
  if (auto room = ParseRoomArg(parser); !room.ok())
    return room.status();
  return absl::OkStatus();
}

absl::Status ProgressionAuditCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const std::string spec_path = *parser.GetString("spec");
  std::ifstream file(spec_path);
  if (!file) {
    return absl::NotFoundError(
        absl::StrFormat("cannot read spec '%s'", spec_path));
  }
  std::stringstream text;
  text << file.rdbuf();
  ASSIGN_OR_RETURN(ProgressionSpec spec, ParseProgressionSpec(text.str()));
  ASSIGN_OR_RETURN(int only_room, ParseRoomArg(parser));
  const bool all_rooms = parser.HasFlag("all-rooms");

  ASSIGN_OR_RETURN(auto rooms, ReadProgressionRooms(rom, parser));
  const ProgressionAudit audit = RunProgressionAudit(rooms, spec);

  std::string table;
  formatter.BeginObject("Progression Audit");
  formatter.AddField("spec", spec_path);
  formatter.BeginArray("rules");
  for (const auto& rule : audit.rules) {
    formatter.BeginObject();
    formatter.AddField("name", rule.name);
    formatter.AddField("kind", rule.kind);
    formatter.AddField("value", rule.value);
    formatter.AddRawJsonField("regions", StringListJson(rule.regions));
    formatter.AddField("pass", rule.pass);
    formatter.AddRawJsonField("offending_rooms",
                              RoomListJson(rule.offending_rooms));
    formatter.AddField("detail", rule.detail);
    formatter.EndObject();
    table +=
        absl::StrFormat("%s  %s (%s %s): %s\n", rule.pass ? "PASS" : "FAIL",
                        rule.name, rule.kind, rule.value, rule.detail);
    if (!rule.offending_rooms.empty()) {
      std::vector<std::string> hex;
      for (int room : rule.offending_rooms)
        hex.push_back(RoomHex(room));
      table += absl::StrCat("      rooms: ", absl::StrJoin(hex, " "), "\n");
    }
  }
  formatter.EndArray();

  formatter.BeginArray("items");
  for (const auto& [item, result] : audit.items) {
    formatter.BeginObject();
    formatter.AddField("item", item);
    formatter.AddRawJsonField("required_by", RoomListJson(result.required_by));
    formatter.AddRawJsonField("given_by", RoomListJson(result.given_by));
    formatter.EndObject();
  }
  formatter.EndArray();

  int listed = 0;
  formatter.BeginArray("rooms");
  for (const auto& room : audit.rooms) {
    if (only_room >= 0 && room.room_id != only_room)
      continue;
    if (only_room < 0 && !all_rooms && room.gated_features.empty() &&
        room.gives.empty()) {
      continue;
    }
    ++listed;
    formatter.BeginObject();
    formatter.AddHexField("room", room.room_id, 3);
    formatter.AddField("owner", room.owner_id);
    formatter.AddRawJsonField("regions", StringListJson(room.regions));
    formatter.AddRawJsonField("gated_features",
                              StringListJson(room.gated_features));
    formatter.AddRawJsonField("required_items",
                              StringListJson(room.required_items));
    formatter.AddRawJsonField("gives", StringListJson(room.gives));
    if (all_rooms || only_room >= 0)
      formatter.AddRawJsonField("features", StringListJson(room.features));
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginObject("summary");
  formatter.AddField("rooms_audited", static_cast<int>(audit.rooms.size()));
  formatter.AddField("rooms_listed", listed);
  formatter.AddField("rules", static_cast<int>(audit.rules.size()));
  formatter.AddField("rules_failed", audit.rules_failed);
  formatter.EndObject();
  formatter.AddRawText(table);
  formatter.EndObject();

  if (audit.rules_failed > 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "progression audit: %d of %d rules failed", audit.rules_failed,
        static_cast<int>(audit.rules.size())));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
