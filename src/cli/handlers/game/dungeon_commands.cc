#include "cli/handlers/game/dungeon_commands.h"

#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

// Helper to load sprite registry from file if --sprite-registry flag is provided
absl::Status MaybeLoadSpriteRegistry(const resources::ArgumentParser& parser) {
  auto registry_path = parser.GetString("sprite-registry");
  if (!registry_path.has_value()) {
    return absl::OkStatus();  // Flag not provided, nothing to load
  }

  std::ifstream file(registry_path.value());
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open sprite registry: %s",
                        registry_path.value()));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return zelda3::GetResourceLabels().ImportOracleSpriteRegistry(buffer.str());
}

}  // namespace

absl::Status DungeonListSpritesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // Load custom sprite registry if provided (e.g., Oracle of Secrets)
  auto registry_status = MaybeLoadSpriteRegistry(parser);
  if (!registry_status.ok()) {
    return registry_status;
  }

  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Sprites");
  formatter.AddField("room_id", room_id);

  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();
  const auto& sprites = room.GetSprites();

  formatter.AddField("total_sprites", static_cast<int>(sprites.size()));
  formatter.AddField("status", "success");

  formatter.BeginArray("sprites");
  for (const auto& sprite : sprites) {
    formatter.BeginObject();
    formatter.AddHexField("sprite_id", sprite.id(), 2);
    formatter.AddField("name", zelda3::ResolveSpriteName(sprite.id()));
    formatter.AddField("x", sprite.x());
    formatter.AddField("y", sprite.y());
    formatter.AddField("subtype", sprite.subtype());
    formatter.AddField("layer", sprite.layer());
    if (sprite.key_drop() > 0) {
      formatter.AddField("key_drop", sprite.key_drop());
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonDescribeRoomCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // Load custom sprite registry if provided (e.g., Oracle of Secrets)
  auto registry_status = MaybeLoadSpriteRegistry(parser);
  if (!registry_status.ok()) {
    return registry_status;
  }

  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.AddField("room_id", room_id);

  // Load full room to get objects, doors, and stairs
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  formatter.AddField("status", "success");
  formatter.AddField("name", absl::StrFormat("Room %d", room.id()));
  formatter.AddField("room_id", room.id());
  formatter.AddField("room_type", "Dungeon Room");

  // Room properties from Room data
  formatter.BeginObject("properties");
  formatter.AddField("blockset", room.blockset);
  formatter.AddField("spriteset", room.spriteset);
  formatter.AddField("palette", room.palette);
  formatter.AddField("layout", room.layout);
  formatter.AddField("floor1", room.floor1());
  formatter.AddField("floor2", room.floor2());
  formatter.AddField("effect", static_cast<int>(room.effect()));
  formatter.AddField("tag1", static_cast<int>(room.tag1()));
  formatter.AddField("tag2", static_cast<int>(room.tag2()));

  // Check object counts for simple heuristics
  formatter.AddField("object_count",
                     static_cast<int>(room.GetTileObjects().size()));

  formatter.EndObject();

  // Export Doors
  formatter.BeginArray("doors");
  for (const auto& door : room.GetDoors()) {
    formatter.BeginObject();
    formatter.AddField("position", door.position);
    formatter.AddField("direction", std::string(door.GetDirectionName()));
    formatter.AddField("type", std::string(door.GetTypeName()));
    auto [tx, ty] = door.GetTileCoords();
    formatter.AddField("tile_x", tx);
    formatter.AddField("tile_y", ty);
    formatter.EndObject();
  }
  formatter.EndArray();

  // Export Staircases
  formatter.BeginArray("staircases");
  for (const auto& stair : room.GetStairs()) {
    formatter.BeginObject();
    formatter.AddField("tile_x", stair.id); // 'id' field stores X in struct staircase
    formatter.AddField("tile_y", stair.room); // 'room' field stores Y in struct staircase
    formatter.AddField("label", stair.label);
    formatter.EndObject();
  }
  formatter.EndArray();

  // Export Chests
  formatter.BeginArray("chests");
  for (const auto& chest : room.GetChests()) {
    formatter.BeginObject();
    formatter.AddHexField("item_id", chest.id, 2);
    formatter.AddField("item_name", zelda3::GetItemLabel(chest.id));
    formatter.AddField("is_big_chest", chest.size);
    formatter.EndObject();
  }
  formatter.EndArray();

  return absl::OkStatus();
}

absl::Status DungeonListChestsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_opt = parser.GetString("room");

  bool has_room_filter = room_id_opt.has_value();
  int room_filter = -1;
  if (has_room_filter) {
    if (!ParseHexString(room_id_opt.value(), &room_filter)) {
      return absl::InvalidArgumentError(
          "Invalid room ID format. Must be hex.");
    }
  }

  int total_rooms = 0;
  int rooms_with_chests = 0;
  int total_chests = 0;
  std::map<int, int> item_counts;

  formatter.BeginObject("Dungeon Chests");
  formatter.AddField("room_filter",
                     has_room_filter
                         ? absl::StrFormat("0x%02X", room_filter)
                         : "all");

  formatter.BeginArray("rooms");
  int start_room = has_room_filter ? room_filter : 0;
  int end_room = has_room_filter ? room_filter : zelda3::NumberOfRooms - 1;

  for (int room_id = start_room; room_id <= end_room; ++room_id) {
    total_rooms++;
    zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
    room.LoadChests();

    const auto& chests = room.GetChests();
    if (chests.empty()) {
      continue;
    }

    rooms_with_chests++;
    total_chests += static_cast<int>(chests.size());

    formatter.BeginObject();
    formatter.AddField("room_id", absl::StrFormat("0x%02X", room_id));
    formatter.AddField("room_name", zelda3::GetRoomLabel(room_id));
    formatter.AddField("chest_count", static_cast<int>(chests.size()));

    formatter.BeginArray("chests");
    int chest_index = 0;
    for (const auto& chest : chests) {
      formatter.BeginObject();
      formatter.AddField("index", chest_index++);
      formatter.AddHexField("item_id", chest.id, 2);
      formatter.AddField("item_name", zelda3::GetItemLabel(chest.id));
      formatter.AddField("is_big_chest", chest.size);
      formatter.EndObject();

      if (chest.id != 0) {
        item_counts[chest.id]++;
      }
    }
    formatter.EndArray();
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginObject("summary");
  formatter.AddField("total_rooms", total_rooms);
  formatter.AddField("rooms_with_chests", rooms_with_chests);
  formatter.AddField("total_chests", total_chests);
  formatter.AddField("unique_items", static_cast<int>(item_counts.size()));

  formatter.BeginArray("duplicate_items");
  for (const auto& [item_id, count] : item_counts) {
    if (count < 2) {
      continue;
    }
    formatter.BeginObject();
    formatter.AddHexField("item_id", item_id, 2);
    formatter.AddField("item_name", zelda3::GetItemLabel(item_id));
    formatter.AddField("count", count);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status DungeonGetEntranceCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();
  bool is_spawn_point = parser.HasFlag("spawn");

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex.");
  }

  zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(entrance_id),
                                is_spawn_point);

  formatter.AddField("entrance_id", absl::StrFormat("0x%02X", entrance_id));
  formatter.AddField("is_spawn_point", is_spawn_point);
  formatter.AddField("room_id", absl::StrFormat("0x%04X", entrance.room_));
  formatter.AddField("exit_id", absl::StrFormat("0x%04X", entrance.exit_));

  formatter.BeginObject("position");
  formatter.AddField("x", entrance.x_position_);
  formatter.AddField("y", entrance.y_position_);
  formatter.EndObject();

  formatter.BeginObject("camera");
  formatter.AddField("x", entrance.camera_x_);
  formatter.AddField("y", entrance.camera_y_);
  formatter.AddField("trigger_x", entrance.camera_trigger_x_);
  formatter.AddField("trigger_y", entrance.camera_trigger_y_);
  formatter.EndObject();

  formatter.BeginObject("properties");
  formatter.AddField("blockset", absl::StrFormat("0x%02X", entrance.blockset_));
  formatter.AddField("floor", absl::StrFormat("0x%02X", entrance.floor_));
  formatter.AddField("dungeon_id",
                     absl::StrFormat("0x%02X", entrance.dungeon_id_));
  formatter.AddField("door", absl::StrFormat("0x%02X", entrance.door_));
  formatter.AddField("ladder_bg",
                     absl::StrFormat("0x%02X", entrance.ladder_bg_));
  formatter.AddField("scrolling",
                     absl::StrFormat("0x%02X", entrance.scrolling_));
  formatter.AddField("scroll_quadrant",
                     absl::StrFormat("0x%02X", entrance.scroll_quadrant_));
  formatter.AddField("music", absl::StrFormat("0x%02X", entrance.music_));
  formatter.EndObject();

  formatter.BeginObject("camera_boundaries");
  formatter.AddField("qn",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qn_));
  formatter.AddField("fn",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fn_));
  formatter.AddField("qs",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qs_));
  formatter.AddField("fs",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fs_));
  formatter.AddField("qw",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qw_));
  formatter.AddField("fw",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fw_));
  formatter.AddField("qe",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qe_));
  formatter.AddField("fe",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fe_));
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonExportRoomCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Export");
  formatter.AddField("room_id", room_id);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  auto& room = room_or.value();

  // Export room data
  formatter.AddField("status", "success");
  formatter.AddField("room_width", "Unknown");
  formatter.AddField("room_height", "Unknown");
  formatter.AddField("room_name", absl::StrFormat("Room %d", room.id()));

  // Add room data as JSON
  formatter.BeginObject("room_data");
  formatter.AddField("tiles", "Room tile data would be exported here");
  formatter.AddField("sprites", "Room sprite data would be exported here");
  formatter.AddField("doors", "Room door data would be exported here");
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonListObjectsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Objects");
  formatter.AddField("room_id", room_id);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  auto& room = room_or.value();

  // Load objects if not already loaded (GetTileObjects might be empty otherwise)
  room.LoadObjects();

  const auto& objects = room.GetTileObjects();
  formatter.AddField("total_objects", static_cast<int>(objects.size()));
  formatter.AddField("status", "success");

  formatter.BeginArray("objects");
  for (const auto& obj : objects) {
    formatter.BeginObject("");
    formatter.AddField("id", obj.id_);
    formatter.AddField("id_hex", absl::StrFormat("0x%04X", obj.id_));
    formatter.AddField("x", obj.x_);
    formatter.AddField("y", obj.y_);
    formatter.AddField("size", obj.size_);
    formatter.AddField("layer", static_cast<int>(obj.layer_));
    // Add decoded type info if available
    int type = zelda3::RoomObject::DetermineObjectType((obj.id_ & 0xFF),
                                                       (obj.id_ >> 8));
    formatter.AddField("type", type);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonGetRoomTilesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Tiles");
  formatter.AddField("room_id", room_id);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  auto& room = room_or.value();

  // TODO: Implement tile data retrieval from room
  formatter.AddField("room_width", "Unknown");
  formatter.AddField("room_height", "Unknown");
  formatter.AddField("total_tiles", "Unknown");
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Tile data retrieval requires room tile parsing");

  formatter.BeginArray("tiles");
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonSetRoomPropertyCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  auto property = parser.GetString("property").value();
  auto value = parser.GetString("value").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Dungeon Room Property Set");
  formatter.AddField("room_id", room_id);
  formatter.AddField("property", property);
  formatter.AddField("value", value);

  // Use existing dungeon system
  zelda3::DungeonEditorSystem dungeon_editor(rom);
  auto room_or = dungeon_editor.GetRoom(room_id);
  if (!room_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", room_or.status().ToString());
    formatter.EndObject();
    return room_or.status();
  }

  // TODO: Implement property setting
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Property setting requires room property system");
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonRoomHeaderCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  formatter.BeginObject("Room Header Debug");
  formatter.AddField("room_id", room_id);
  formatter.AddHexField("room_id_hex", room_id, 2);

  // Show ROM address constants
  formatter.BeginObject("rom_addresses");
  formatter.AddHexField("kRoomHeaderPointer", zelda3::kRoomHeaderPointer, 4);
  formatter.AddHexField("kRoomHeaderPointerBank",
                        zelda3::kRoomHeaderPointerBank, 4);
  formatter.EndObject();

  // Read the pointer table address
  int header_pointer = (rom->data()[zelda3::kRoomHeaderPointer + 2] << 16) +
                       (rom->data()[zelda3::kRoomHeaderPointer + 1] << 8) +
                       (rom->data()[zelda3::kRoomHeaderPointer]);
  int header_pointer_pc = SnesToPc(header_pointer);

  formatter.BeginObject("pointer_table");
  formatter.AddHexField("snes_address", header_pointer, 6);
  formatter.AddHexField("pc_address", header_pointer_pc, 6);
  formatter.EndObject();

  // Read the room's specific header address from the table
  int table_offset = header_pointer_pc + (room_id * 2);
  int room_header_addr = (rom->data()[zelda3::kRoomHeaderPointerBank] << 16) +
                         (rom->data()[table_offset + 1] << 8) +
                         rom->data()[table_offset];
  int room_header_pc = SnesToPc(room_header_addr);

  formatter.BeginObject("room_header_address");
  formatter.AddHexField("table_offset_pc", table_offset, 6);
  formatter.AddHexField("snes_address", room_header_addr, 6);
  formatter.AddHexField("pc_address", room_header_pc, 6);
  formatter.EndObject();

  // Read and display raw header bytes (14 bytes)
  formatter.BeginArray("raw_bytes");
  for (int i = 0; i < 14; ++i) {
    if (room_header_pc + i < static_cast<int>(rom->size())) {
      formatter.AddArrayItem(
          absl::StrFormat("0x%02X", rom->data()[room_header_pc + i]));
    }
  }
  formatter.EndArray();

  // Decode the header bytes
  if (room_header_pc >= 0 &&
      room_header_pc + 13 < static_cast<int>(rom->size())) {
    uint8_t byte0 = rom->data()[room_header_pc];
    uint8_t byte1 = rom->data()[room_header_pc + 1];
    uint8_t byte2 = rom->data()[room_header_pc + 2];
    uint8_t byte3 = rom->data()[room_header_pc + 3];

    formatter.BeginObject("decoded");
    formatter.AddField("bg2", (byte0 >> 5) & 0x07);
    formatter.AddField("collision", (byte0 >> 2) & 0x07);
    formatter.AddField("is_light", (byte0 & 0x01) == 1);
    formatter.AddField("palette", byte1 & 0x3F);
    formatter.AddField("blockset", byte2);
    formatter.AddField("spriteset", byte3);
    formatter.AddField("effect", rom->data()[room_header_pc + 4]);
    formatter.AddField("tag1", rom->data()[room_header_pc + 5]);
    formatter.AddField("tag2", rom->data()[room_header_pc + 6]);
    formatter.AddField("holewarp", rom->data()[room_header_pc + 9]);
    formatter.AddField("stair1_room", rom->data()[room_header_pc + 10]);
    formatter.AddField("stair2_room", rom->data()[room_header_pc + 11]);
    formatter.AddField("stair3_room", rom->data()[room_header_pc + 12]);
    formatter.AddField("stair4_room", rom->data()[room_header_pc + 13]);
    formatter.EndObject();
  } else {
    formatter.AddField("error", "Room header address out of range");
  }

  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
