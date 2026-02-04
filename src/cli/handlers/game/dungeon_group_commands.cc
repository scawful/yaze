#include "cli/handlers/game/dungeon_group_commands.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

// ROM addresses for dungeon tables
constexpr int kEntranceDungeon = 0x1548B;      // Dungeon ID per entrance
constexpr int kDungeonsStartRooms = 0x7939;    // Start room per dungeon
constexpr int kDungeonsEndRooms = 0x792D;      // End room per dungeon (unused)
constexpr int kDungeonsBossRooms = 0x10954;    // Boss room per dungeon
constexpr int kNumberOfDungeons = 14;          // Vanilla dungeons
constexpr int kNumberOfEntrances = 0x84;       // Total entrances

// Dungeon names (vanilla + common custom slots)
const std::vector<std::string> kDungeonNames = {
    "Sewers",                    // 0x00
    "Hyrule Castle",             // 0x01
    "Eastern Palace",            // 0x02
    "Desert Palace",             // 0x03
    "Agahnim's Tower",           // 0x04
    "Swamp Palace",              // 0x05
    "Palace of Darkness",        // 0x06
    "Misery Mire",               // 0x07
    "Skull Woods",               // 0x08
    "Ice Palace",                // 0x09
    "Tower of Hera",             // 0x0A
    "Thieves' Town",             // 0x0B
    "Turtle Rock",               // 0x0C
    "Ganon's Tower",             // 0x0D
    "Custom Dungeon 0E",         // 0x0E
    "Custom Dungeon 0F",         // 0x0F
    "Custom Dungeon 10",         // 0x10
    "Custom Dungeon 11",         // 0x11
    "Custom Dungeon 12",         // 0x12
    "Custom Dungeon 13",         // 0x13
    "Custom Dungeon 14",         // 0x14 (Oracle custom)
    "Custom Dungeon 15",         // 0x15
    "Custom Dungeon 16",         // 0x16
    "Custom Dungeon 17",         // 0x17
    "Custom Dungeon 18",         // 0x18
    "Custom Dungeon 19",         // 0x19
    "Custom Dungeon 1A",         // 0x1A
    "Custom Dungeon 1B",         // 0x1B
    "Custom Dungeon 1C",         // 0x1C
    "Custom Dungeon 1D",         // 0x1D
    "Custom Dungeon 1E",         // 0x1E
    "Custom Dungeon 1F",         // 0x1F
};

struct DungeonInfo {
  int id;
  std::string name;
  int start_room;
  int boss_room;
  std::set<int> rooms;
};

std::string GetDungeonName(int id) {
  if (id >= 0 && id < static_cast<int>(kDungeonNames.size())) {
    return kDungeonNames[id];
  }
  return absl::StrFormat("Dungeon %02X", id);
}

}  // namespace

absl::Status DungeonGroupCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // Parse optional filters
  auto dungeon_id_opt = parser.GetString("dungeon");
  bool list_only = parser.HasFlag("list");

  int dungeon_filter = -1;
  if (dungeon_id_opt.has_value()) {
    if (!ParseHexString(dungeon_id_opt.value(), &dungeon_filter)) {
      return absl::InvalidArgumentError(
          "Invalid dungeon ID format. Must be hex (e.g., 0x02).");
    }
  }

  // Build dungeon info from ROM tables
  std::map<int, DungeonInfo> dungeons;

  // Read start rooms and boss rooms from ROM
  for (int i = 0; i < kNumberOfDungeons; ++i) {
    DungeonInfo info;
    info.id = i;
    info.name = GetDungeonName(i);

    // Read start room (1 byte per dungeon)
    info.start_room = rom->data()[kDungeonsStartRooms + i];

    // Read boss room (2 bytes per dungeon)
    uint16_t boss_room_addr = kDungeonsBossRooms + (i * 2);
    info.boss_room = rom->data()[boss_room_addr] |
                     (rom->data()[boss_room_addr + 1] << 8);

    // Boss room 0xFFFF means no boss
    if (info.boss_room == 0xFFFF) {
      info.boss_room = -1;
    }

    dungeons[i] = info;
  }

  // Scan entrances to find room->dungeon mappings
  std::map<int, int> room_to_dungeon;
  for (int entrance_id = 0; entrance_id < kNumberOfEntrances; ++entrance_id) {
    zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(entrance_id), false);

    int dungeon_id = entrance.dungeon_id_;
    int room_id = entrance.room_;

    // Track the room's dungeon assignment
    room_to_dungeon[room_id] = dungeon_id;

    // Add room to dungeon's room set
    if (dungeons.find(dungeon_id) == dungeons.end()) {
      // Create entry for custom dungeons discovered via entrances
      DungeonInfo info;
      info.id = dungeon_id;
      info.name = GetDungeonName(dungeon_id);
      info.start_room = -1;
      info.boss_room = -1;
      dungeons[dungeon_id] = info;
    }
    dungeons[dungeon_id].rooms.insert(room_id);
  }

  // Also scan spawn points for additional room mappings
  for (int spawn_id = 0; spawn_id < 0x14; ++spawn_id) {
    zelda3::RoomEntrance spawn(rom, static_cast<uint8_t>(spawn_id), true);

    int dungeon_id = spawn.dungeon_id_;
    int room_id = spawn.room_;

    room_to_dungeon[room_id] = dungeon_id;

    if (dungeons.find(dungeon_id) != dungeons.end()) {
      dungeons[dungeon_id].rooms.insert(room_id);
    }
  }

  // Find rooms not assigned to any dungeon
  std::set<int> unassigned_rooms;
  for (int room_id = 0; room_id < zelda3::NumberOfRooms; ++room_id) {
    if (room_to_dungeon.find(room_id) == room_to_dungeon.end()) {
      unassigned_rooms.insert(room_id);
    }
  }

  // Output
  formatter.BeginObject("dungeon_groups");

  // List mode - compact dungeon listing
  if (list_only) {
    formatter.BeginArray("dungeons");
    for (const auto& [id, info] : dungeons) {
      if (dungeon_filter >= 0 && id != dungeon_filter) {
        continue;
      }
      formatter.BeginObject();
      formatter.AddField("id", absl::StrFormat("0x%02X", id));
      formatter.AddField("name", info.name);
      formatter.AddField("room_count", static_cast<int>(info.rooms.size()));
      formatter.EndObject();
    }
    formatter.EndArray();
  } else {
    // Full output with room lists
    formatter.BeginArray("dungeons");
    for (const auto& [id, info] : dungeons) {
      if (dungeon_filter >= 0 && id != dungeon_filter) {
        continue;
      }

      // Skip empty dungeons unless specifically requested
      if (info.rooms.empty() && dungeon_filter < 0) {
        continue;
      }

      formatter.BeginObject();
      formatter.AddField("id", absl::StrFormat("0x%02X", id));
      formatter.AddField("name", info.name);

      if (info.start_room >= 0) {
        formatter.AddField("start_room",
                           absl::StrFormat("0x%02X", info.start_room));
      } else {
        formatter.AddField("start_room", "null");
      }

      if (info.boss_room >= 0) {
        formatter.AddField("boss_room",
                           absl::StrFormat("0x%02X", info.boss_room));
      } else {
        formatter.AddField("boss_room", "null");
      }

      // Room list
      formatter.BeginArray("rooms");
      for (int room_id : info.rooms) {
        formatter.BeginObject();
        formatter.AddField("id", absl::StrFormat("0x%02X", room_id));
        // Bounds check for kRoomNames (array size is 297)
        if (room_id >= 0 && room_id < 297) {
          formatter.AddField("name", std::string(zelda3::kRoomNames[room_id]));
        } else {
          formatter.AddField("name", absl::StrFormat("Room 0x%02X", room_id));
        }
        formatter.EndObject();
      }
      formatter.EndArray();

      formatter.EndObject();
    }
    formatter.EndArray();

    // Unassigned rooms (only in full output)
    if (dungeon_filter < 0 && !unassigned_rooms.empty()) {
      formatter.BeginArray("unassigned_rooms");
      for (int room_id : unassigned_rooms) {
        formatter.AddArrayItem(absl::StrFormat("0x%02X", room_id));
      }
      formatter.EndArray();
    }
  }

  // Statistics
  formatter.BeginObject("stats");
  int total_assigned = 0;
  for (const auto& [id, info] : dungeons) {
    total_assigned += static_cast<int>(info.rooms.size());
  }
  formatter.AddField("total_dungeons", static_cast<int>(dungeons.size()));
  formatter.AddField("assigned_rooms", total_assigned);
  formatter.AddField("unassigned_rooms",
                     static_cast<int>(unassigned_rooms.size()));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
