#include "cli/handlers/game/dungeon_map_commands.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "app/gfx/types/snes_tile.h"
#include "cli/util/hex_util.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"
#include "zelda3/resource_labels.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze {
namespace cli {
namespace handlers {

using json = nlohmann::json;
using util::ParseHexString;

namespace {

// ASCII character mapping for room tiles
constexpr char kCharWall = '#';
constexpr char kCharFloor = '.';
constexpr char kCharWater = '~';
constexpr char kCharPit = 'v';
constexpr char kCharStair = '>';
constexpr char kCharDoor = 'D';
constexpr char kCharChest = 'C';
constexpr char kCharSpike = 'X';
constexpr char kCharBlock = 'B';
constexpr char kCharTorch = 'T';
constexpr char kCharSwitch = 'S';
constexpr char kCharTrackH = '-';
constexpr char kCharTrackV = '|';
constexpr char kCharTrackX = '+';
constexpr char kCharStopN = 'N';
constexpr char kCharStopS = 's';
constexpr char kCharStopE = 'E';
constexpr char kCharStopW = 'W';
constexpr char kCharUnknown = '?';

// Room dimensions
constexpr int kRoomWidth = 64;
constexpr int kRoomHeight = 64;

constexpr uint8_t kEmptyRoom = 0x0F;
constexpr uint8_t kEmptyGfx = 0xFF;
constexpr uint16_t kNoBossVanilla = 0x000F;
constexpr uint16_t kNoBossSentinel = 0xFFFF;
constexpr int kGridDim = 5;
constexpr int kMapTileCount = 256;
constexpr int kMapGfxBytes = kMapTileCount * 32;

// Classify an object ID to an ASCII character
char ClassifyObject(uint16_t object_id) {
  if (object_id >= 0x000 && object_id <= 0x00F) {
    return kCharFloor;
  }
  if (object_id == 0x020 || object_id == 0x021 || object_id == 0x022) {
    return kCharPit;
  }
  if (object_id >= 0x030 && object_id <= 0x03F) {
    return kCharWater;
  }
  if (object_id >= 0x040 && object_id <= 0x04F) {
    return kCharWater;
  }
  if (object_id >= 0x138 && object_id <= 0x13B) {
    return kCharStair;
  }
  if (object_id == 0x12D || object_id == 0x12E) {
    return kCharStair;
  }
  if (object_id >= 0x0F0 && object_id <= 0x0FF) {
    return kCharChest;
  }
  if (object_id >= 0x0D4 && object_id <= 0x0DF) {
    return kCharBlock;
  }
  if (object_id >= 0x0E0 && object_id <= 0x0EF) {
    return kCharTorch;
  }
  if (object_id >= 0x100 && object_id <= 0x10F) {
    return kCharSpike;
  }
  if (object_id >= 0x110 && object_id <= 0x11F) {
    return kCharSwitch;
  }
  if (object_id >= 0x200 && object_id <= 0x2FF) {
    return kCharWall;
  }
  return kCharUnknown;
}

void PlaceObject(std::vector<std::string>& grid, const zelda3::RoomObject& obj,
                 int layer_filter) {
  if (layer_filter >= 0 && static_cast<int>(obj.layer_) != layer_filter) {
    return;
  }

  int tile_x = obj.x_;
  int tile_y = obj.y_;
  int width = obj.size_ > 0 ? obj.size_ : 1;

  char ch = ClassifyObject(obj.id_);

  for (int dx = 0; dx < width; ++dx) {
    int gx = tile_x + dx;
    int gy = tile_y;

    if (gx >= 0 && gx < kRoomWidth && gy >= 0 && gy < kRoomHeight) {
      if (ch == kCharFloor && grid[gy][gx] != kCharFloor) {
        continue;
      }
      grid[gy][gx] = ch;
    }
  }
}

void PlaceDoors(std::vector<std::string>& grid,
                const std::vector<zelda3::Room::Door>& doors) {
  for (const auto& door : doors) {
    auto [tile_x, tile_y] = door.GetTileCoords();

    for (int dx = 0; dx < 2; ++dx) {
      int gx = tile_x + dx;
      int gy = tile_y;

      if (gx >= 0 && gx < kRoomWidth && gy >= 0 && gy < kRoomHeight) {
        grid[gy][gx] = kCharDoor;
      }
    }
  }
}

void PlaceStairs(std::vector<std::string>& grid,
                 const std::vector<staircase>& stairs) {
  for (const auto& stair : stairs) {
    int tile_x = stair.id;
    int tile_y = stair.room;

    if (tile_x >= 0 && tile_x < kRoomWidth && tile_y >= 0 &&
        tile_y < kRoomHeight) {
      grid[tile_y][tile_x] = kCharStair;
    }
  }
}

void PlaceChests(std::vector<std::string>& grid,
                 const std::vector<chest_data>& chests) {
  (void)grid;
  (void)chests;
}

char ClassifyCustomCollisionTile(uint8_t tile) {
  switch (tile) {
    case 0xB0:
      return kCharTrackH;
    case 0xB1:
      return kCharTrackV;
    case 0xB7:
      return kCharStopN;
    case 0xB8:
      return kCharStopS;
    case 0xB9:
      return kCharStopW;
    case 0xBA:
      return kCharStopE;
    default:
      break;
  }

  if ((tile >= 0xB2 && tile <= 0xB6) || (tile >= 0xBB && tile <= 0xBE)) {
    return kCharTrackX;
  }
  if (tile >= 0xD0 && tile <= 0xD3) {
    return kCharSwitch;
  }
  return 0;
}

void OverlayCustomCollision(std::vector<std::string>& grid, Rom* rom,
                            int room_id) {
  auto map_or = zelda3::LoadCustomCollisionMap(rom, room_id);
  if (!map_or.ok() || !map_or.value().has_data) {
    return;
  }

  const auto& map = map_or.value().tiles;
  for (int y = 0; y < kRoomHeight; ++y) {
    for (int x = 0; x < kRoomWidth; ++x) {
      uint8_t tile = map[static_cast<size_t>(y * kRoomWidth + x)];
      char ch = ClassifyCustomCollisionTile(tile);
      if (ch == 0) {
        continue;
      }

      if (grid[y][x] == kCharFloor || grid[y][x] == kCharUnknown) {
        grid[y][x] = ch;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Pause-map helpers (matching zelda3::LoadDungeonMaps / BuildDungeonMapWritePlan)
// ---------------------------------------------------------------------------

absl::StatusOr<int> ParseSlotText(absl::string_view text) {
  const std::string trimmed(absl::StripAsciiWhitespace(text));
  if (trimmed.empty()) {
    return absl::InvalidArgumentError("--slot cannot be empty");
  }
  int slot = -1;
  if (absl::StartsWithIgnoreCase(trimmed, "0x") ||
      absl::StartsWith(trimmed, "$")) {
    if (!ParseHexString(trimmed, &slot)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid hex --slot value '%s'", trimmed));
    }
  } else if (!absl::SimpleAtoi(trimmed, &slot)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid --slot value '%s' (expected 0..13)", trimmed));
  }
  if (slot < 0 || slot >= zelda3::kNumDungeons) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Dungeon map slot %d out of range (expected 0..%d)",
                        slot, zelda3::kNumDungeons - 1));
  }
  return slot;
}

absl::StatusOr<uint32_t> ParseHexUnsigned(absl::string_view token,
                                          uint32_t max_value,
                                          absl::string_view context) {
  std::string trimmed(absl::StripAsciiWhitespace(token));
  if (absl::StartsWithIgnoreCase(trimmed, "0x")) {
    trimmed = trimmed.substr(2);
  } else if (absl::StartsWith(trimmed, "$")) {
    trimmed = trimmed.substr(1);
  }
  if (trimmed.empty() || trimmed.size() > 8 ||
      !std::all_of(trimmed.begin(), trimmed.end(),
                   [](unsigned char c) { return std::isxdigit(c) != 0; })) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid hex token '%s' for %s", std::string(token), context));
  }
  uint32_t value = 0;
  try {
    size_t consumed = 0;
    value = static_cast<uint32_t>(std::stoul(trimmed, &consumed, 16));
    if (consumed != trimmed.size()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Trailing characters in hex token '%s' for %s",
                          std::string(token), context));
    }
  } catch (...) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Out-of-range hex token '%s' for %s", std::string(token), context));
  }
  if (value > max_value) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Hex value 0x%X exceeds 0x%X for %s", value, max_value, context));
  }
  return value;
}

std::string FloorName(int floor_index, int nbr_of_basement) {
  if (floor_index < nbr_of_basement) {
    return absl::StrFormat("B%d", nbr_of_basement - floor_index);
  }
  return absl::StrFormat("%dF", floor_index - nbr_of_basement + 1);
}

std::optional<int> DerivedBossFloorSigned(const zelda3::DungeonMap& map) {
  if (map.boss_room == kNoBossVanilla || map.boss_room == kNoBossSentinel) {
    return std::nullopt;
  }
  for (size_t f = 0; f < map.floor_rooms.size(); ++f) {
    const auto& rooms = map.floor_rooms[f];
    if (std::find(rooms.begin(), rooms.end(), map.boss_room) != rooms.end()) {
      return static_cast<int>(f) - static_cast<int>(map.nbr_of_basement);
    }
  }
  return std::nullopt;
}

uint16_t ExpectedBossFloorWord(const zelda3::DungeonMap& map) {
  const auto floor_signed = DerivedBossFloorSigned(map);
  if (!floor_signed.has_value()) {
    return kNoBossSentinel;
  }
  return static_cast<uint16_t>(static_cast<uint8_t>(*floor_signed));
}

absl::StatusOr<std::vector<std::array<uint16_t, 4>>> ReadPauseMapTemplates(
    const Rom& rom, const zelda3::DungeonMapTile16Layout& layout) {
  std::vector<std::array<uint16_t, 4>> templates(layout.template_count);
  for (int i = 0; i < layout.template_count; ++i) {
    for (int q = 0; q < 4; ++q) {
      ASSIGN_OR_RETURN(uint16_t word,
                       rom.ReadWord(layout.pc_address + i * 8 + q * 2));
      templates[i][q] = word;
    }
  }
  return templates;
}

struct RoomChestMarker {
  uint8_t room_id = 0;
  std::vector<chest_data> chests;
};

std::vector<RoomChestMarker> CollectRoomChestsForMap(
    Rom* rom, const zelda3::DungeonMap& map) {
  std::set<int> room_ids;
  for (const auto& floor : map.floor_rooms) {
    for (uint8_t room : floor) {
      if (room != kEmptyRoom) {
        room_ids.insert(room);
      }
    }
  }
  std::vector<RoomChestMarker> markers;
  if (rom == nullptr || !rom->is_loaded()) {
    return markers;
  }
  for (int room_id : room_ids) {
    if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
      continue;
    }
    zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
    room.LoadChests();
    if (!room.GetChests().empty()) {
      markers.push_back({static_cast<uint8_t>(room_id), room.GetChests()});
    }
  }
  return markers;
}

std::string FormatRowHexBytes(const std::array<uint8_t, zelda3::kNumRooms>& row,
                              int row_index) {
  return absl::StrFormat(
      "%02X %02X %02X %02X %02X", row[row_index * kGridDim + 0],
      row[row_index * kGridDim + 1], row[row_index * kGridDim + 2],
      row[row_index * kGridDim + 3], row[row_index * kGridDim + 4]);
}

json TemplateWordsToJsonArray(const std::array<uint16_t, 4>& words) {
  return json::array(
      {absl::StrFormat("%04X", words[0]), absl::StrFormat("%04X", words[1]),
       absl::StrFormat("%04X", words[2]), absl::StrFormat("%04X", words[3])});
}

absl::StatusOr<json> BuildSlotExportJson(
    Rom* rom, int slot, const zelda3::DungeonMap& map,
    const zelda3::DungeonMapTile16Layout& layout,
    const std::vector<std::array<uint16_t, 4>>& templates,
    bool include_all_templates, bool include_chests) {
  ASSIGN_OR_RETURN(uint16_t rooms_ptr_word,
                   rom->ReadWord(zelda3::kDungeonMapRoomsPtr + slot * 2));
  ASSIGN_OR_RETURN(uint16_t gfx_ptr_word,
                   rom->ReadWord(zelda3::kDungeonMapGfxPtr + slot * 2));
  ASSIGN_OR_RETURN(uint16_t floor_count_word,
                   rom->ReadWord(zelda3::kDungeonMapFloors + slot * 2));
  ASSIGN_OR_RETURN(uint16_t boss_room_word,
                   rom->ReadWord(zelda3::kDungeonMapBossRooms + slot * 2));
  ASSIGN_OR_RETURN(uint16_t boss_floor_rom_word,
                   rom->ReadWord(zelda3::kDungeonMapBossFloors + slot * 2));

  json floors_json = json::array();
  json shape_rows_json = json::array();
  std::map<uint8_t, uint8_t> room_to_shape;
  std::vector<std::string> shape_conflicts;
  std::set<int> used_templates;

  const size_t floor_count = map.floor_rooms.size();
  for (size_t f = 0; f < floor_count; ++f) {
    json rows_json = json::array();
    json gfx_rows_json = json::array();
    for (int y = 0; y < kGridDim; ++y) {
      rows_json.push_back(FormatRowHexBytes(map.floor_rooms[f], y));
      gfx_rows_json.push_back(FormatRowHexBytes(map.floor_gfx[f], y));
    }
    floors_json.push_back({
        {"name", FloorName(static_cast<int>(f), map.nbr_of_basement)},
        {"rows", std::move(rows_json)},
    });
    shape_rows_json.push_back(std::move(gfx_rows_json));

    for (int cell = 0; cell < zelda3::kNumRooms; ++cell) {
      const uint8_t room_id = map.floor_rooms[f][cell];
      const uint8_t shape_id = map.floor_gfx[f][cell];
      if (room_id == kEmptyRoom) {
        continue;
      }
      used_templates.insert(shape_id);
      auto [it, inserted] = room_to_shape.emplace(room_id, shape_id);
      if (!inserted && it->second != shape_id) {
        shape_conflicts.push_back(
            absl::StrFormat("room %02X has differing templates (%02X vs %02X); "
                            "metadata.shape_rows preserves per-cell values",
                            room_id, it->second, shape_id));
      }
    }
  }

  json shapes_json = json::object();
  for (const auto& [room_id, shape_id] : room_to_shape) {
    shapes_json[absl::StrFormat("%02X", room_id)] =
        absl::StrFormat("%02X", shape_id);
  }

  json templates_json = json::object();
  if (include_all_templates) {
    for (int i = 0; i < layout.template_count; ++i) {
      if (used_templates.contains(i) ||
          std::any_of(templates[i].begin(), templates[i].end(),
                      [](uint16_t w) { return w != 0; })) {
        templates_json[absl::StrFormat("%02X", i)] =
            TemplateWordsToJsonArray(templates[i]);
      }
    }
  } else {
    for (int t : used_templates) {
      if (t >= 0 && t < layout.template_count) {
        templates_json[absl::StrFormat("%02X", t)] =
            TemplateWordsToJsonArray(templates[t]);
      }
    }
  }

  const bool has_boss =
      (map.boss_room != kNoBossVanilla && map.boss_room != kNoBossSentinel);
  const std::string boss_str =
      has_boss ? absl::StrFormat("%02X", map.boss_room) : "0F";
  const auto derived_boss_floor = DerivedBossFloorSigned(map);
  const int8_t rom_boss_floor_signed =
      static_cast<int8_t>(boss_floor_rom_word & 0xFF);
  const bool boss_icon_hidden = (boss_floor_rom_word & 0x8000) != 0;

  json metadata = {
      {"nbr_of_floor", static_cast<int>(map.nbr_of_floor)},
      {"nbr_of_basement", static_cast<int>(map.nbr_of_basement)},
      {"boss_room_word", absl::StrFormat("0x%04X", boss_room_word)},
      {"boss_floor", derived_boss_floor.has_value() ? json(*derived_boss_floor)
                                                    : json(nullptr)},
      {"boss_floor_rom_word", absl::StrFormat("0x%04X", boss_floor_rom_word)},
      {"boss_floor_rom_signed",
       has_boss ? json(static_cast<int>(rom_boss_floor_signed))
                : json(nullptr)},
      {"boss_icon_hidden", boss_icon_hidden},
      {"shape_rows", std::move(shape_rows_json)},
      {"pointers",
       {
           {"rooms_snes", absl::StrFormat("0x0A%04X", rooms_ptr_word)},
           {"rooms_pc",
            absl::StrFormat("0x%05X", SnesToPc(0x0A0000 | rooms_ptr_word))},
           {"gfx_snes", absl::StrFormat("0x0A%04X", gfx_ptr_word)},
           {"gfx_pc",
            absl::StrFormat("0x%05X", SnesToPc(0x0A0000 | gfx_ptr_word))},
           {"floor_count_word", absl::StrFormat("0x%04X", floor_count_word)},
       }},
  };
  if (!shape_conflicts.empty()) {
    metadata["shape_conflicts"] = shape_conflicts;
  }

  if (include_chests) {
    json chests_json = json::array();
    for (const auto& marker : CollectRoomChestsForMap(rom, map)) {
      json items_json = json::array();
      for (const auto& chest : marker.chests) {
        items_json.push_back({
            {"item_id", absl::StrFormat("0x%02X", chest.id)},
            {"item_name", zelda3::GetItemReceiptLabel(chest.id)},
            {"is_big_chest", chest.size},
        });
      }
      chests_json.push_back({
          {"room", absl::StrFormat("%02X", marker.room_id)},
          {"chests", std::move(items_json)},
      });
    }
    metadata["chests"] = std::move(chests_json);
  }

  json spec = {
      {"slot", slot},
      {"floors", std::move(floors_json)},
      {"shapes", std::move(shapes_json)},
      {"templates", std::move(templates_json)},
      {"boss", boss_str},
      {"metadata", std::move(metadata)},
  };
  return spec;
}

absl::Status WriteTextFileAtomically(const std::filesystem::path& path,
                                     const std::string& contents) {
  const auto parent = path.parent_path().empty() ? std::filesystem::path(".")
                                                 : path.parent_path();
  std::error_code ec;
  if (!std::filesystem::exists(parent, ec) ||
      !std::filesystem::is_directory(parent, ec)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Parent directory does not exist: %s", parent.string()));
  }
  const std::filesystem::path tmp_path =
      parent / (path.filename().string() + ".yaze-tmp");
  {
    std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot open temporary file for writing: %s", tmp_path.string()));
    }
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    out.flush();
    if (!out.good()) {
      std::filesystem::remove(tmp_path, ec);
      return absl::InternalError(absl::StrFormat(
          "Failed writing temporary file: %s", tmp_path.string()));
    }
  }
  std::filesystem::rename(tmp_path, path, ec);
  if (ec) {
    std::error_code rm_ec;
    std::filesystem::remove(tmp_path, rm_ec);
    return absl::InternalError(absl::StrFormat("Failed to replace %s: %s",
                                               path.string(), ec.message()));
  }
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// Headless PNG preview renderer using util::EncodeIndexedPng
// ---------------------------------------------------------------------------

// Decode 256 4bpp SNES 8x8 tiles (8192 bytes) into 8x8 color-index grids.
absl::StatusOr<std::vector<std::array<uint8_t, 64>>> LoadMapTilesFromBin(
    const std::filesystem::path& bin_path) {
  ASSIGN_OR_RETURN(const auto raw,
                   ::yaze::util::ReadBinaryFile(bin_path.string()));
  if (raw.size() < kMapGfxBytes) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Map GFX binary %s is too small (%zu bytes; need %d)",
                        bin_path.string(), raw.size(), kMapGfxBytes));
  }
  std::vector<std::array<uint8_t, 64>> tiles(kMapTileCount);
  for (int c = 0; c < kMapTileCount; ++c) {
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        uint8_t val = 0;
        for (int p = 0; p < 4; ++p) {
          const size_t idx =
              static_cast<size_t>(c * 32 + (p >> 1) * 16 + y * 2 + (p & 1));
          if ((raw[idx] >> (7 - x)) & 1) {
            val |= static_cast<uint8_t>(1 << p);
          }
        }
        tiles[c][y * 8 + x] = val;
      }
    }
  }
  return tiles;
}

absl::StatusOr<std::vector<std::array<uint8_t, 64>>> LoadMapTilesFromRom(
    Rom* rom) {
  auto game_data = std::make_unique<zelda3::GameData>();
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, *game_data, options));

  std::vector<std::array<uint8_t, 64>> tiles(kMapTileCount);
  for (int sheet_idx = 0; sheet_idx < 4; ++sheet_idx) {
    const auto pixels = game_data->sheet_store.Sheet(212 + sheet_idx);
    if (pixels.size() != 128 * 32) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Dungeon map GFX sheet %d is unavailable in this ROM",
                          212 + sheet_idx));
    }
    for (int ty = 0; ty < 4; ++ty) {
      for (int tx = 0; tx < 16; ++tx) {
        const int c = sheet_idx * 64 + ty * 16 + tx;
        for (int y = 0; y < 8; ++y) {
          for (int x = 0; x < 8; ++x) {
            tiles[c][y * 8 + x] =
                pixels[(ty * 8 + y) * 128 + (tx * 8 + x)] & 0x0F;
          }
        }
      }
    }
  }
  return tiles;
}

const std::array<uint8_t, 5>* TinyGlyph(char ch) {
  static constexpr std::array<uint8_t, 5> kDigits[10] = {
      {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
      {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2},
      {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
  static constexpr std::array<uint8_t, 5> kHexAF[6] = {
      {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3},
      {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4}};
  static constexpr std::array<uint8_t, 5> kS = {3, 4, 2, 1, 6};
  static constexpr std::array<uint8_t, 5> kL = {4, 4, 4, 4, 7};
  static constexpr std::array<uint8_t, 5> kO = {2, 5, 5, 5, 2};
  static constexpr std::array<uint8_t, 5> kT = {7, 2, 2, 2, 2};
  static constexpr std::array<uint8_t, 5> kColon = {0, 2, 0, 2, 0};

  if (ch >= '0' && ch <= '9') {
    return &kDigits[ch - '0'];
  }
  if (ch >= 'a' && ch <= 'z') {
    ch = static_cast<char>(ch - 'a' + 'A');
  }
  if (ch >= 'A' && ch <= 'F') {
    return &kHexAF[ch - 'A'];
  }
  switch (ch) {
    case 'S':
      return &kS;
    case 'L':
      return &kL;
    case 'O':
      return &kO;
    case 'T':
      return &kT;
    case ':':
      return &kColon;
    default:
      return nullptr;
  }
}

void DrawTinyText(std::vector<uint8_t>& img, int width, int height, int x0,
                  int y0, absl::string_view text, uint8_t color, int scale) {
  int cursor_x = x0;
  for (char ch : text) {
    const auto* glyph = TinyGlyph(ch);
    if (glyph != nullptr) {
      for (int gy = 0; gy < 5; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
          if (((*glyph)[gy] >> (2 - gx)) & 1) {
            for (int sy = 0; sy < scale; ++sy) {
              for (int sx = 0; sx < scale; ++sx) {
                const int px = cursor_x + gx * scale + sx;
                const int py = y0 + gy * scale + sy;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                  img[static_cast<size_t>(py * width + px)] = color;
                }
              }
            }
          }
        }
      }
    }
    cursor_x += 4 * scale;
  }
}

absl::StatusOr<std::vector<uint8_t>> RenderPauseMapPreviewPng(
    Rom* rom, int slot, const zelda3::DungeonMap& map,
    const std::vector<std::array<uint16_t, 4>>& templates,
    const std::optional<std::filesystem::path>& gfx_bin_path) {
  std::vector<std::array<uint8_t, 64>> tiles;
  if (gfx_bin_path.has_value()) {
    ASSIGN_OR_RETURN(tiles, LoadMapTilesFromBin(*gfx_bin_path));
  } else {
    ASSIGN_OR_RETURN(tiles, LoadMapTilesFromRom(rom));
  }

  std::set<uint8_t> chest_rooms;
  for (const auto& marker : CollectRoomChestsForMap(rom, map)) {
    chest_rooms.insert(marker.room_id);
  }

  ASSIGN_OR_RETURN(uint16_t boss_floor_rom_word,
                   rom->ReadWord(zelda3::kDungeonMapBossFloors + slot * 2));
  const bool boss_icon_hidden = (boss_floor_rom_word & 0x8000) != 0;
  const auto derived_boss_floor = DerivedBossFloorSigned(map);

  const int floor_count =
      std::max<int>(1, static_cast<int>(map.floor_rooms.size()));
  constexpr int kScale = 3;
  constexpr int kCellPx = 16 * kScale;  // 48 px
  constexpr int kGap = 12;
  constexpr int kHeaderH = 30;
  const int width = floor_count * (kGridDim * kCellPx + kGap) + kGap;
  const int height = kHeaderH + kGridDim * kCellPx + kGap;

  // Indices 0..7 match tile pixels; 8..15 are UI/label/marker colors.
  const std::vector<std::array<uint8_t, 4>> palette = {
      {20, 20, 32, 255},     // 0: room backdrop
      {72, 72, 120, 255},    // 1: floor low
      {130, 130, 190, 255},  // 2: floor mid
      {190, 190, 230, 255},  // 3: wall/door highlight
      {250, 250, 250, 255},  // 4: bright accent
      {220, 100, 70, 255},   // 5: warm accent
      {100, 190, 100, 255},  // 6: green accent
      {240, 210, 90, 255},   // 7: gold accent
      {36, 36, 44, 255},     // 8: canvas background
      {52, 52, 64, 255},     // 9: grid border
      {230, 230, 150, 255},  // 10: floor label
      {230, 230, 230, 255},  // 11: title label
      {255, 255, 255, 255},  // 12: room hex label
      {255, 70, 70, 255},    // 13: boss border & label
      {255, 215, 0, 255},    // 14: chest marker fill
      {40, 28, 12, 255},     // 15: chest marker border
  };

  std::vector<uint8_t> img(static_cast<size_t>(width * height), 8);

  auto fill_rect = [&](int rx, int ry, int rw, int rh, uint8_t color) {
    for (int dy = 0; dy < rh; ++dy) {
      const int py = ry + dy;
      if (py < 0 || py >= height)
        continue;
      for (int dx = 0; dx < rw; ++dx) {
        const int px = rx + dx;
        if (px < 0 || px >= width)
          continue;
        img[static_cast<size_t>(py * width + px)] = color;
      }
    }
  };
  auto draw_rect_outline = [&](int rx, int ry, int rw, int rh, int thickness,
                               uint8_t color) {
    fill_rect(rx, ry, rw, thickness, color);
    fill_rect(rx, ry + rh - thickness, rw, thickness, color);
    fill_rect(rx, ry, thickness, rh, color);
    fill_rect(rx + rw - thickness, ry, thickness, rh, color);
  };

  DrawTinyText(img, width, height, kGap, 4, absl::StrFormat("SLOT %d", slot),
               11, 2);

  for (size_t fi = 0; fi < map.floor_rooms.size(); ++fi) {
    const int ox = kGap + static_cast<int>(fi) * (kGridDim * kCellPx + kGap);
    const std::string fname =
        FloorName(static_cast<int>(fi), map.nbr_of_basement);
    DrawTinyText(img, width, height, ox, 18, fname, 10, 2);

    for (int cell = 0; cell < zelda3::kNumRooms; ++cell) {
      const int x0 = ox + (cell % kGridDim) * kCellPx;
      const int y0 = kHeaderH + (cell / kGridDim) * kCellPx;
      draw_rect_outline(x0, y0, kCellPx, kCellPx, 1, 9);

      const uint8_t room_id = map.floor_rooms[fi][cell];
      if (room_id == kEmptyRoom) {
        continue;
      }

      const uint8_t shape_id = map.floor_gfx[fi][cell];
      if (shape_id < templates.size()) {
        const auto& words = templates[shape_id];
        for (int q = 0; q < 4; ++q) {
          const uint16_t w = words[q];
          const int c_idx = static_cast<int>(w & 0x3FF) - 0x300;
          if (c_idx < 0 || c_idx >= kMapTileCount)
            continue;
          const bool hflip = (w & 0x4000) != 0;
          const bool vflip = (w & 0x8000) != 0;
          const int qx = (q & 1) * 8;
          const int qy = (q >> 1) * 8;
          for (int ty = 0; ty < 8; ++ty) {
            const int sy = vflip ? (7 - ty) : ty;
            for (int tx = 0; tx < 8; ++tx) {
              const int sx = hflip ? (7 - tx) : tx;
              const uint8_t px_val = tiles[c_idx][sy * 8 + sx] & 7;
              if (px_val != 0) {
                fill_rect(x0 + (qx + tx) * kScale, y0 + (qy + ty) * kScale,
                          kScale, kScale, px_val);
              }
            }
          }
        }
      }

      const bool is_boss =
          !boss_icon_hidden && room_id == map.boss_room &&
          derived_boss_floor.has_value() &&
          static_cast<int>(fi) ==
              static_cast<int>(map.nbr_of_basement) + *derived_boss_floor;
      if (is_boss) {
        draw_rect_outline(x0, y0, kCellPx, kCellPx, 2, 13);
      }
      if (chest_rooms.contains(room_id)) {
        fill_rect(x0 + kCellPx - 10, y0 + 2, 8, 8, 15);
        fill_rect(x0 + kCellPx - 9, y0 + 3, 6, 6, 14);
      }
      DrawTinyText(img, width, height, x0 + 3, y0 + 3,
                   absl::StrFormat("%02X", room_id), is_boss ? 13 : 12, 1);
    }
  }

  return ::yaze::util::EncodeIndexedPng(width, height, img, palette);
}

// ---------------------------------------------------------------------------
// Pause-map spec parsing & apply helpers
// ---------------------------------------------------------------------------

struct ParsedDungeonSpec {
  int slot = -1;
  zelda3::DungeonMap map{kNoBossVanilla, 0, 0, {}, {}};
};

struct TemplateWrite {
  int slot_id = 0;
  std::array<uint16_t, 4> words{};
};

absl::Status ParseTemplatesObject(
    const json& templates_obj, int template_count,
    std::map<int, std::array<uint16_t, 4>>& out_templates) {
  if (!templates_obj.is_object()) {
    return absl::InvalidArgumentError("'templates' must be a JSON object");
  }
  for (const auto& [key, val] : templates_obj.items()) {
    ASSIGN_OR_RETURN(
        uint32_t slot_id,
        ParseHexUnsigned(key, static_cast<uint32_t>(template_count - 1),
                         "template slot"));
    if (!val.is_array() || val.size() != 4) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Template %s must be an array of 4 16-bit hex words", key));
    }
    std::array<uint16_t, 4> words{};
    for (size_t q = 0; q < 4; ++q) {
      if (!val[q].is_string()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Template %s word %zu must be a hex string", key, q));
      }
      ASSIGN_OR_RETURN(
          uint32_t w,
          ParseHexUnsigned(val[q].get<std::string>(), 0xFFFF,
                           absl::StrFormat("template %s[%zu]", key, q)));
      words[q] = static_cast<uint16_t>(w);
    }
    auto [it, inserted] =
        out_templates.emplace(static_cast<int>(slot_id), words);
    if (!inserted && it->second != words) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Conflicting definitions for template slot %02X in input", slot_id));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::array<uint8_t, zelda3::kNumRooms>> ParseFloorGrid(
    const json& floor_obj, absl::string_view floor_name) {
  std::array<uint8_t, zelda3::kNumRooms> grid{};
  grid.fill(kEmptyRoom);

  if (floor_obj.contains("rows")) {
    const auto& rows = floor_obj.at("rows");
    if (!rows.is_array() || rows.size() != kGridDim) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Floor '%s' rows must be an array of %d strings",
                          floor_name, kGridDim));
    }
    for (int y = 0; y < kGridDim; ++y) {
      if (!rows[y].is_string()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Floor '%s' row %d must be a string", floor_name, y));
      }
      std::vector<std::string> tokens =
          absl::StrSplit(rows[y].get<std::string>(), absl::ByAnyChar(" \t,"),
                         absl::SkipEmpty());
      if (tokens.size() != kGridDim) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Floor '%s' row %d must have %d room bytes (got %zu)", floor_name,
            y, kGridDim, tokens.size()));
      }
      for (int x = 0; x < kGridDim; ++x) {
        ASSIGN_OR_RETURN(
            uint32_t byte_val,
            ParseHexUnsigned(
                tokens[x], 0xFF,
                absl::StrFormat("floor %s cell (%d,%d)", floor_name, x, y)));
        grid[y * kGridDim + x] = static_cast<uint8_t>(byte_val);
      }
    }
    return grid;
  }

  if (floor_obj.contains("grid")) {
    const auto& arr = floor_obj.at("grid");
    if (!arr.is_array() || arr.size() != zelda3::kNumRooms) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Floor '%s' grid must have %d entries", floor_name,
                          zelda3::kNumRooms));
    }
    for (int i = 0; i < zelda3::kNumRooms; ++i) {
      if (arr[i].is_number_integer()) {
        int v = arr[i].get<int>();
        if (v < 0 || v > 0xFF) {
          return absl::InvalidArgumentError("Room ID in grid out of range");
        }
        grid[i] = static_cast<uint8_t>(v);
      } else if (arr[i].is_string()) {
        ASSIGN_OR_RETURN(uint32_t v, ParseHexUnsigned(arr[i].get<std::string>(),
                                                      0xFF, "grid entry"));
        grid[i] = static_cast<uint8_t>(v);
      } else {
        return absl::InvalidArgumentError("Grid entries must be int or hex");
      }
    }
    return grid;
  }

  return absl::InvalidArgumentError(
      absl::StrFormat("Floor '%s' must contain 'rows' or 'grid'", floor_name));
}

absl::StatusOr<ParsedDungeonSpec> ParseDungeonSpecObject(
    const json& spec, int template_count, std::optional<int> cli_slot) {
  if (!spec.is_object()) {
    return absl::InvalidArgumentError("Dungeon spec must be a JSON object");
  }
  int slot = -1;
  if (spec.contains("slot")) {
    if (!spec.at("slot").is_number_integer()) {
      return absl::InvalidArgumentError("Spec 'slot' must be an integer 0..13");
    }
    slot = spec.at("slot").get<int>();
    if (slot < 0 || slot >= zelda3::kNumDungeons) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Spec 'slot' %d out of range (0..%d)", slot,
                          zelda3::kNumDungeons - 1));
    }
    if (cli_slot.has_value() && *cli_slot != slot) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "--slot=%d does not match spec 'slot'=%d", *cli_slot, slot));
    }
  } else if (cli_slot.has_value()) {
    slot = *cli_slot;
  } else {
    return absl::InvalidArgumentError(
        "Spec must specify 'slot' (0..13) or --slot must be passed");
  }

  if (!spec.contains("floors") || !spec.at("floors").is_array()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Slot %d spec must contain a 'floors' array", slot));
  }
  const auto& floors_json = spec.at("floors");

  int basements = 0;
  int uppers = 0;
  std::vector<std::string> floor_names;
  std::vector<std::array<uint8_t, zelda3::kNumRooms>> floor_rooms;
  for (size_t fi = 0; fi < floors_json.size(); ++fi) {
    const auto& fobj = floors_json[fi];
    if (!fobj.is_object() || !fobj.contains("name") ||
        !fobj.at("name").is_string()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Slot %d floor[%zu] must have a string 'name'", slot, fi));
    }
    const std::string name = fobj.at("name").get<std::string>();
    if (absl::StartsWith(name, "B") || absl::StartsWith(name, "b")) {
      ++basements;
    } else {
      ++uppers;
    }
    floor_names.push_back(name);
    ASSIGN_OR_RETURN(auto grid, ParseFloorGrid(fobj, name));
    floor_rooms.push_back(grid);
  }
  if (basements > 0x0F || uppers > 0x0F) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Slot %d exceeds max 15 basements or 15 upper floors", slot));
  }
  for (size_t fi = 0; fi < floor_names.size(); ++fi) {
    const std::string expected = FloorName(static_cast<int>(fi), basements);
    if (floor_names[fi] != expected) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Slot %d floor[%zu] named '%s' must be '%s' (floors must run lowest "
          "basement first without gaps)",
          slot, fi, floor_names[fi], expected));
    }
  }

  // Parse room -> template mapping from "shapes" (or metadata.shape_rows fallback
  // when "shapes" is omitted).
  std::map<uint8_t, uint8_t> shapes_map;
  bool has_shapes_map = false;
  if (spec.contains("shapes")) {
    const auto& shapes = spec.at("shapes");
    if (!shapes.is_object()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Slot %d 'shapes' must be an object", slot));
    }
    has_shapes_map = true;
    for (const auto& [r_key, t_val] : shapes.items()) {
      ASSIGN_OR_RETURN(uint32_t room_id,
                       ParseHexUnsigned(r_key, 0xFF, "shapes room key"));
      uint32_t tmpl_id = 0;
      if (t_val.is_string()) {
        ASSIGN_OR_RETURN(
            tmpl_id, ParseHexUnsigned(t_val.get<std::string>(),
                                      static_cast<uint32_t>(template_count - 1),
                                      absl::StrFormat("shapes[%s]", r_key)));
      } else if (t_val.is_number_integer()) {
        int raw = t_val.get<int>();
        if (raw < 0 || raw >= template_count) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Template %d out of range for room %s", raw, r_key));
        }
        tmpl_id = static_cast<uint32_t>(raw);
      } else {
        return absl::InvalidArgumentError(absl::StrFormat(
            "shapes[%s] must be a hex string or integer", r_key));
      }
      shapes_map[static_cast<uint8_t>(room_id)] = static_cast<uint8_t>(tmpl_id);
    }
  }

  std::vector<std::array<uint8_t, zelda3::kNumRooms>> floor_gfx(
      floor_rooms.size());
  for (auto& g : floor_gfx) {
    g.fill(kEmptyGfx);
  }

  const json* meta_shape_rows = nullptr;
  if (!has_shapes_map && spec.contains("metadata") &&
      spec.at("metadata").is_object() &&
      spec.at("metadata").contains("shape_rows") &&
      spec.at("metadata").at("shape_rows").is_array() &&
      spec.at("metadata").at("shape_rows").size() == floor_rooms.size()) {
    meta_shape_rows = &spec.at("metadata").at("shape_rows");
  }

  for (size_t fi = 0; fi < floor_rooms.size(); ++fi) {
    std::array<uint8_t, zelda3::kNumRooms> fallback_gfx{};
    fallback_gfx.fill(kEmptyGfx);
    if (meta_shape_rows != nullptr) {
      json row_wrapper = {{"rows", (*meta_shape_rows)[fi]}};
      ASSIGN_OR_RETURN(fallback_gfx,
                       ParseFloorGrid(row_wrapper, floor_names[fi]));
    }
    for (int cell = 0; cell < zelda3::kNumRooms; ++cell) {
      const uint8_t room_id = floor_rooms[fi][cell];
      if (room_id == kEmptyRoom) {
        floor_gfx[fi][cell] = kEmptyGfx;
        continue;
      }
      if (has_shapes_map) {
        auto it = shapes_map.find(room_id);
        if (it == shapes_map.end()) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Slot %d room %02X on floor %s is missing from 'shapes'", slot,
              room_id, floor_names[fi]));
        }
        floor_gfx[fi][cell] = it->second;
      } else if (meta_shape_rows != nullptr) {
        if (fallback_gfx[cell] >= template_count) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Slot %d shape %02X out of range", slot, fallback_gfx[cell]));
        }
        floor_gfx[fi][cell] = fallback_gfx[cell];
      } else {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Slot %d spec requires 'shapes' for room %02X", slot, room_id));
      }
    }
  }

  // Parse boss room.
  uint16_t boss_room = kNoBossVanilla;
  if (spec.contains("boss") && !spec.at("boss").is_null()) {
    if (spec.at("boss").is_string()) {
      const std::string boss_text = absl::AsciiStrToLower(
          absl::StripAsciiWhitespace(spec.at("boss").get<std::string>()));
      if (boss_text.empty() || boss_text == "none") {
        boss_room = kNoBossVanilla;
      } else {
        ASSIGN_OR_RETURN(uint32_t bval,
                         ParseHexUnsigned(boss_text, 0xFFFF, "boss"));
        if (bval == kNoBossVanilla || bval == kNoBossSentinel) {
          boss_room = static_cast<uint16_t>(bval);
        } else if (bval > 0xFF) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Boss room 0x%X must be 00..FF or FFFF", bval));
        } else {
          boss_room = static_cast<uint16_t>(bval);
        }
      }
    } else if (spec.at("boss").is_number_integer()) {
      const int bval = spec.at("boss").get<int>();
      if (bval < 0 || (bval > 0xFF && bval != kNoBossSentinel)) {
        return absl::InvalidArgumentError("Boss room integer out of range");
      }
      boss_room = static_cast<uint16_t>(bval);
    } else {
      return absl::InvalidArgumentError("Spec 'boss' must be hex string/int");
    }
  }

  zelda3::DungeonMap map(boss_room, static_cast<uint8_t>(uppers),
                         static_cast<uint8_t>(basements),
                         std::move(floor_rooms), std::move(floor_gfx));
  if (boss_room != kNoBossVanilla && boss_room != kNoBossSentinel &&
      !DerivedBossFloorSigned(map).has_value()) {
    return absl::NotFoundError(absl::StrFormat(
        "Slot %d boss room %02X is not present on any floor grid", slot,
        boss_room));
  }

  return ParsedDungeonSpec{slot, std::move(map)};
}

int CountActiveRooms(const zelda3::DungeonMap& map) {
  int count = 0;
  for (const auto& floor : map.floor_rooms) {
    for (uint8_t room : floor) {
      if (room != kEmptyRoom) {
        ++count;
      }
    }
  }
  return count;
}

absl::Status VerifyPauseMapReadback(
    Rom& verify_rom, const std::vector<zelda3::DungeonMap>& expected_maps,
    const std::vector<int>& modified_slots,
    const zelda3::DungeonMapTile16Layout& layout,
    const std::vector<TemplateWrite>& template_writes) {
  zelda3::DungeonMapLabels labels;
  ASSIGN_OR_RETURN(const auto loaded,
                   zelda3::LoadDungeonMaps(verify_rom, labels));
  if (loaded.size() != expected_maps.size()) {
    return absl::DataLossError("Readback dungeon map count mismatch");
  }
  for (int slot : modified_slots) {
    const auto& exp = expected_maps[slot];
    const auto& got = loaded[slot];
    if (got.boss_room != exp.boss_room ||
        got.nbr_of_floor != exp.nbr_of_floor ||
        got.nbr_of_basement != exp.nbr_of_basement ||
        got.floor_rooms != exp.floor_rooms || got.floor_gfx != exp.floor_gfx) {
      return absl::DataLossError(
          absl::StrFormat("Readback mismatch in dungeon slot %d", slot));
    }
    ASSIGN_OR_RETURN(
        uint16_t boss_floor_word,
        verify_rom.ReadWord(zelda3::kDungeonMapBossFloors + slot * 2));
    const uint16_t expected_word = ExpectedBossFloorWord(exp);
    if (boss_floor_word != expected_word) {
      return absl::DataLossError(
          absl::StrFormat("Readback boss_floor mismatch in slot %d: got "
                          "0x%04X, expected 0x%04X",
                          slot, boss_floor_word, expected_word));
    }
  }
  for (const auto& tw : template_writes) {
    for (int q = 0; q < 4; ++q) {
      ASSIGN_OR_RETURN(uint16_t w, verify_rom.ReadWord(layout.pc_address +
                                                       tw.slot_id * 8 + q * 2));
      if (w != tw.words[q]) {
        return absl::DataLossError(
            absl::StrFormat("Readback template mismatch at %02X[%d]: got "
                            "0x%04X, expected 0x%04X",
                            tw.slot_id, q, w, tw.words[q]));
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace

// ---------------------------------------------------------------------------
// dungeon-map (ASCII room visualization)
// ---------------------------------------------------------------------------

absl::Status DungeonMapCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  auto layer_opt = parser.GetString("layer");

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex (e.g., 0x07).");
  }

  int layer_filter = -1;
  if (layer_opt.has_value()) {
    if (!ParseHexString(layer_opt.value(), &layer_filter)) {
      return absl::InvalidArgumentError(
          "Invalid layer format. Must be 0, 1, or 2.");
    }
    if (layer_filter < 0 || layer_filter > 2) {
      return absl::InvalidArgumentError("Layer must be 0, 1, or 2.");
    }
  }

  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  std::vector<std::string> grid(kRoomHeight,
                                std::string(kRoomWidth, kCharFloor));

  for (int x = 0; x < kRoomWidth; ++x) {
    grid[0][x] = kCharWall;
    grid[kRoomHeight - 1][x] = kCharWall;
  }
  for (int y = 0; y < kRoomHeight; ++y) {
    grid[y][0] = kCharWall;
    grid[y][kRoomWidth - 1] = kCharWall;
  }

  const auto& objects = room.GetTileObjects();
  for (const auto& obj : objects) {
    PlaceObject(grid, obj, layer_filter);
  }

  PlaceDoors(grid, room.GetDoors());
  PlaceStairs(grid, room.GetStairs());
  PlaceChests(grid, room.GetChests());
  OverlayCustomCollision(grid, rom, room_id);

  formatter.BeginObject("dungeon_map");
  formatter.AddField("room_id", absl::StrFormat("0x%02X", room_id));
  formatter.AddField("room_name", zelda3::GetRoomLabel(room_id));
  formatter.AddField("width", kRoomWidth);
  formatter.AddField("height", kRoomHeight);
  formatter.AddField("layer_filter", layer_filter >= 0
                                         ? absl::StrFormat("%d", layer_filter)
                                         : "all");
  formatter.AddField("object_count", static_cast<int>(objects.size()));
  formatter.AddField("door_count", static_cast<int>(room.GetDoors().size()));

  formatter.BeginObject("legend");
  formatter.AddField("#", "wall");
  formatter.AddField(".", "floor");
  formatter.AddField("~", "water");
  formatter.AddField("v", "pit");
  formatter.AddField(">", "stair");
  formatter.AddField("D", "door");
  formatter.AddField("C", "chest");
  formatter.AddField("X", "spike");
  formatter.AddField("B", "block");
  formatter.AddField("T", "torch");
  formatter.AddField("S", "switch");
  formatter.AddField("-", "track_h");
  formatter.AddField("|", "track_v");
  formatter.AddField("+", "track_x");
  formatter.AddField("N/s/E/W", "stop_tiles");
  formatter.EndObject();

  formatter.BeginArray("map");
  std::string header = "    ";
  for (int x = 0; x < kRoomWidth; x += 10) {
    header += absl::StrFormat("%-10d", x);
  }
  formatter.AddArrayItem(header);

  for (int y = 0; y < kRoomHeight; ++y) {
    std::string row = absl::StrFormat("%2d  %s", y, grid[y]);
    formatter.AddArrayItem(row);
  }
  formatter.EndArray();

  formatter.EndObject();

  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-pause-map-export
// ---------------------------------------------------------------------------

absl::Status DungeonPauseMapExportCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  std::optional<int> single_slot;
  if (const auto slot_opt = parser.GetString("slot"); slot_opt.has_value()) {
    if (absl::AsciiStrToLower(absl::StripAsciiWhitespace(*slot_opt)) != "all") {
      ASSIGN_OR_RETURN(single_slot, ParseSlotText(*slot_opt));
    }
  } else if (parser.HasFlag("slot")) {
    return absl::InvalidArgumentError("--slot requires a value (0..13 or all)");
  }

  if (const auto out_opt = parser.GetString("out");
      out_opt.has_value() && out_opt->empty()) {
    return absl::InvalidArgumentError("--out cannot be empty");
  } else if (!out_opt.has_value() && parser.HasFlag("out")) {
    return absl::InvalidArgumentError("--out requires a file path");
  }

  if (const auto preview_opt = parser.GetString("render-preview");
      preview_opt.has_value()) {
    if (preview_opt->empty()) {
      return absl::InvalidArgumentError("--render-preview cannot be empty");
    }
    if (!single_slot.has_value()) {
      return absl::InvalidArgumentError(
          "--render-preview requires a single --slot <0..13>");
    }
  } else if (parser.HasFlag("render-preview")) {
    return absl::InvalidArgumentError("--render-preview requires a file path");
  }

  if (const auto bin_opt = parser.GetString("gfx-bin");
      bin_opt.has_value() && bin_opt->empty()) {
    return absl::InvalidArgumentError("--gfx-bin cannot be empty");
  } else if (!bin_opt.has_value() && parser.HasFlag("gfx-bin")) {
    return absl::InvalidArgumentError("--gfx-bin requires a file path");
  }

  return absl::OkStatus();
}

absl::Status DungeonPauseMapExportCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status DungeonPauseMapExportCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  std::optional<int> target_slot;
  if (const auto slot_opt = parser.GetString("slot"); slot_opt.has_value()) {
    if (absl::AsciiStrToLower(absl::StripAsciiWhitespace(*slot_opt)) != "all") {
      ASSIGN_OR_RETURN(target_slot, ParseSlotText(*slot_opt));
    }
  }

  const bool include_templates = parser.HasFlag("include-templates");
  (void)parser.HasFlag("include-chests");
  const bool include_chests = true;

  std::optional<std::filesystem::path> out_path;
  if (const auto out_opt = parser.GetString("out"); out_opt.has_value()) {
    ASSIGN_OR_RETURN(out_path, resources::ResolveStableArtifactPath(*out_opt));
    RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", *out_path,
                                                        invocation_context));
  }

  std::optional<std::filesystem::path> preview_path;
  if (const auto preview_opt = parser.GetString("render-preview");
      preview_opt.has_value()) {
    ASSIGN_OR_RETURN(preview_path,
                     resources::ResolveStableArtifactPath(*preview_opt));
    RETURN_IF_ERROR(resources::RejectArtifactRomAliases(
        "--render-preview", *preview_path, invocation_context));
  }

  std::optional<std::filesystem::path> gfx_bin_path;
  if (const auto bin_opt = parser.GetString("gfx-bin"); bin_opt.has_value()) {
    gfx_bin_path = std::filesystem::path(*bin_opt);
  }

  ASSIGN_OR_RETURN(const auto layout,
                   zelda3::ResolveDungeonMapTile16Layout(*rom));
  zelda3::DungeonMapLabels labels;
  ASSIGN_OR_RETURN(const auto maps, zelda3::LoadDungeonMaps(*rom, labels));
  ASSIGN_OR_RETURN(const auto templates, ReadPauseMapTemplates(*rom, layout));

  ASSIGN_OR_RETURN(uint8_t exp_check,
                   rom->ReadByte(zelda3::kDungeonMapExpCheck));

  json payload;
  if (target_slot.has_value()) {
    ASSIGN_OR_RETURN(
        payload,
        BuildSlotExportJson(rom, *target_slot, maps[*target_slot], layout,
                            templates, include_templates, include_chests));
  } else {
    json dungeons_array = json::array();
    for (int d = 0; d < zelda3::kNumDungeons; ++d) {
      ASSIGN_OR_RETURN(
          json slot_json,
          BuildSlotExportJson(rom, d, maps[d], layout, templates,
                              /*include_all_templates=*/false, include_chests));
      dungeons_array.push_back(std::move(slot_json));
    }
    json all_templates = json::object();
    for (int i = 0; i < layout.template_count; ++i) {
      if (include_templates ||
          std::any_of(templates[i].begin(), templates[i].end(),
                      [](uint16_t w) { return w != 0; })) {
        all_templates[absl::StrFormat("%02X", i)] =
            TemplateWordsToJsonArray(templates[i]);
      }
    }
    payload = {
        {"format_version", 1},
        {"tile16_layout",
         {
             {"exp_check_pc",
              absl::StrFormat("0x%05X", zelda3::kDungeonMapExpCheck)},
             {"exp_check_byte", absl::StrFormat("0x%02X", exp_check)},
             {"relocated", layout.relocated},
             {"table_pc", absl::StrFormat("0x%06X", layout.pc_address)},
             {"template_count", layout.template_count},
         }},
        {"dungeons", std::move(dungeons_array)},
        {"templates", std::move(all_templates)},
    };
  }

  if (out_path.has_value()) {
    RETURN_IF_ERROR(WriteTextFileAtomically(*out_path, payload.dump(2) + "\n"));
  }

  if (preview_path.has_value()) {
    ASSIGN_OR_RETURN(
        const auto png_bytes,
        RenderPauseMapPreviewPng(rom, *target_slot, maps[*target_slot],
                                 templates, gfx_bin_path));
    RETURN_IF_ERROR(
        ::yaze::util::WriteBinaryFile(preview_path->string(), png_bytes));
  }

  formatter.BeginObject("Pause Map Export");
  formatter.AddField("status", "success");
  formatter.AddField("relocated_tile16", layout.relocated);
  formatter.AddHexField("tile16_table_pc",
                        static_cast<uint64_t>(layout.pc_address), 6);
  formatter.AddField("template_count", layout.template_count);
  if (target_slot.has_value()) {
    formatter.AddField("slot", *target_slot);
  } else {
    formatter.AddField("slot", "all");
  }
  if (out_path.has_value()) {
    formatter.AddField("out_path", out_path->string());
  }
  if (preview_path.has_value()) {
    formatter.AddField("preview_path", preview_path->string());
  }
  if (!out_path.has_value() || formatter.IsJson()) {
    formatter.AddRawJsonField("export", payload.dump());
  }
  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-pause-map-apply
// ---------------------------------------------------------------------------

absl::Status DungeonPauseMapApplyCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"in"}));
  if (parser.GetString("in")->empty()) {
    return absl::InvalidArgumentError("--in cannot be empty");
  }
  if (parser.HasFlag("dry-run") && parser.HasFlag("write")) {
    return absl::InvalidArgumentError(
        "Pass at most one of --dry-run or --write");
  }
  if (const auto slot_opt = parser.GetString("slot"); slot_opt.has_value()) {
    RETURN_IF_ERROR(ParseSlotText(*slot_opt).status());
  } else if (parser.HasFlag("slot")) {
    return absl::InvalidArgumentError("--slot requires a value (0..13)");
  }
  if (const auto report_opt = parser.GetString("report");
      report_opt.has_value() && report_opt->empty()) {
    return absl::InvalidArgumentError("--report cannot be empty");
  } else if (!report_opt.has_value() && parser.HasFlag("report")) {
    return absl::InvalidArgumentError("--report requires a file path");
  }
  return absl::OkStatus();
}

absl::Status DungeonPauseMapApplyCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status DungeonPauseMapApplyCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  const bool do_write = parser.HasFlag("write");
  const bool allow_template_overwrite =
      parser.HasFlag("allow-template-overwrite");
  const bool mock_rom = parser.HasFlag("mock-rom");

  if (do_write && project_ != nullptr &&
      project_->rom_metadata.write_policy ==
          project::RomWritePolicy::kReadOnly) {
    return absl::PermissionDeniedError(
        "Pause-map apply is disabled by the project's read-only policy");
  }

  std::optional<int> cli_slot;
  if (const auto slot_opt = parser.GetString("slot"); slot_opt.has_value()) {
    ASSIGN_OR_RETURN(cli_slot, ParseSlotText(*slot_opt));
  }

  const std::filesystem::path in_path(*parser.GetString("in"));
  std::optional<std::filesystem::path> report_path;
  if (const auto report_opt = parser.GetString("report");
      report_opt.has_value()) {
    ASSIGN_OR_RETURN(report_path,
                     resources::ResolveStableArtifactPath(*report_opt));
    RETURN_IF_ERROR(resources::RejectArtifactRomAliases(
        "--report", *report_path, invocation_context));
    ASSIGN_OR_RETURN(const bool aliases_in,
                     resources::PathsAlias(*report_path, in_path));
    if (aliases_in) {
      return absl::InvalidArgumentError("--report cannot alias --in spec file");
    }
  }

  std::ifstream in_stream(in_path, std::ios::binary);
  if (!in_stream.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open spec file: %s", in_path.string()));
  }
  std::stringstream ss;
  ss << in_stream.rdbuf();
  const json input_json = json::parse(ss.str(), nullptr, false);
  if (input_json.is_discarded() || !input_json.is_object()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid JSON in spec file: %s", in_path.string()));
  }

  ASSIGN_OR_RETURN(const auto layout,
                   zelda3::ResolveDungeonMapTile16Layout(*rom));
  zelda3::DungeonMapLabels labels;
  ASSIGN_OR_RETURN(const auto current_maps,
                   zelda3::LoadDungeonMaps(*rom, labels));
  ASSIGN_OR_RETURN(const auto current_templates,
                   ReadPauseMapTemplates(*rom, layout));

  std::map<int, std::array<uint16_t, 4>> requested_templates;
  if (input_json.contains("templates")) {
    RETURN_IF_ERROR(ParseTemplatesObject(input_json.at("templates"),
                                         layout.template_count,
                                         requested_templates));
  }

  std::vector<ParsedDungeonSpec> specs;
  if (input_json.contains("dungeons")) {
    if (cli_slot.has_value()) {
      return absl::InvalidArgumentError(
          "--slot cannot be combined with a multi-dungeon 'dungeons' bundle");
    }
    const auto& dungeons_arr = input_json.at("dungeons");
    if (!dungeons_arr.is_array() || dungeons_arr.empty()) {
      return absl::InvalidArgumentError("'dungeons' must be a non-empty array");
    }
    for (const auto& entry : dungeons_arr) {
      if (entry.is_object() && entry.contains("templates")) {
        RETURN_IF_ERROR(ParseTemplatesObject(
            entry.at("templates"), layout.template_count, requested_templates));
      }
      ASSIGN_OR_RETURN(
          auto parsed,
          ParseDungeonSpecObject(entry, layout.template_count, std::nullopt));
      specs.push_back(std::move(parsed));
    }
  } else {
    ASSIGN_OR_RETURN(
        auto parsed,
        ParseDungeonSpecObject(input_json, layout.template_count, cli_slot));
    specs.push_back(std::move(parsed));
  }

  std::vector<zelda3::DungeonMap> proposed_maps = current_maps;
  std::vector<int> modified_slots;
  std::set<int> seen_slots;
  for (const auto& spec : specs) {
    if (!seen_slots.insert(spec.slot).second) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate slot %d in input spec", spec.slot));
    }
    proposed_maps[spec.slot] = spec.map;
    modified_slots.push_back(spec.slot);
  }

  // Validate template overwrites against existing ROM templates.
  std::vector<TemplateWrite> template_writes;
  for (const auto& [slot_id, words] : requested_templates) {
    const auto& existing = current_templates[slot_id];
    if (existing == words) {
      continue;
    }
    const bool existing_non_zero = std::any_of(
        existing.begin(), existing.end(), [](uint16_t w) { return w != 0; });
    if (existing_non_zero && !allow_template_overwrite) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Template slot %02X already contains non-zero words "
          "(%04X %04X %04X %04X); pass --allow-template-overwrite to replace "
          "it",
          slot_id, existing[0], existing[1], existing[2], existing[3]));
    }
    template_writes.push_back({slot_id, words});
  }

  ASSIGN_OR_RETURN(const auto map_plan,
                   zelda3::BuildDungeonMapWritePlan(*rom, proposed_maps));

  // Combine template writes + bank $0A writes and ensure disjointness & bounds.
  struct PlannedWrite {
    uint32_t pc_address = 0;
    std::vector<uint8_t> data;
  };
  std::vector<PlannedWrite> all_writes;
  for (const auto& tw : template_writes) {
    std::vector<uint8_t> bytes;
    bytes.reserve(8);
    for (uint16_t w : tw.words) {
      bytes.push_back(static_cast<uint8_t>(w & 0xFF));
      bytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    }
    all_writes.push_back(
        {static_cast<uint32_t>(layout.pc_address + tw.slot_id * 8),
         std::move(bytes)});
  }
  for (const auto& w : map_plan.writes) {
    all_writes.push_back({static_cast<uint32_t>(w.pc_address), w.data});
  }

  std::vector<std::pair<uint32_t, uint32_t>> extents;
  extents.reserve(all_writes.size());
  for (const auto& w : all_writes) {
    const uint32_t end = w.pc_address + static_cast<uint32_t>(w.data.size());
    if (w.data.empty() || end > rom->size()) {
      return absl::OutOfRangeError(
          "Planned pause-map write exceeds ROM bounds");
    }
    extents.emplace_back(w.pc_address, end);
  }
  std::sort(extents.begin(), extents.end());
  for (size_t i = 1; i < extents.size(); ++i) {
    if (extents[i].first < extents[i - 1].second) {
      return absl::InternalError(
          "Overlapping planned writes in pause-map apply");
    }
  }

  // Always simulate on an in-RAM copy first to compute exact byte diffs and
  // validate readback before touching the caller's ROM.
  Rom sim_rom;
  RETURN_IF_ERROR(sim_rom.LoadFromData(rom->vector()));
  for (const auto& w : all_writes) {
    RETURN_IF_ERROR(
        sim_rom.WriteVector(static_cast<int>(w.pc_address), w.data));
  }
  RETURN_IF_ERROR(VerifyPauseMapReadback(sim_rom, proposed_maps, modified_slots,
                                         layout, template_writes));

  size_t changed_bytes = 0;
  for (size_t i = 0; i < rom->size(); ++i) {
    if (rom->vector()[i] != sim_rom.vector()[i]) {
      ++changed_bytes;
    }
  }

  bool readback_verified = false;
  std::string write_status = "not_requested";
  std::string save_status = "not_requested";

  if (do_write) {
    ScopedRomTransaction transaction(*rom);
    if (rom::IsNativeSpecialOverworldLayout(rom->InspectLyraLayout())) {
      const auto baseline = rom->vector();
      auto candidate = baseline;
      std::vector<std::pair<uint32_t, uint32_t>> ranges;
      ranges.reserve(all_writes.size());
      for (const auto& w : all_writes) {
        std::copy(w.data.begin(), w.data.end(),
                  candidate.begin() + w.pc_address);
        ranges.emplace_back(
            w.pc_address, w.pc_address + static_cast<uint32_t>(w.data.size()));
      }
      RETURN_IF_ERROR(rom->CommitNativeNonOverworldImage(
          baseline, std::move(candidate), ranges));
    } else {
      rom::WriteFence fence;
      for (const auto& w : all_writes) {
        RETURN_IF_ERROR(fence.Allow(
            w.pc_address, w.pc_address + static_cast<uint32_t>(w.data.size()),
            "pause-map-apply"));
      }
      rom::ScopedWriteFence scoped_fence(rom, &fence);
      for (const auto& w : all_writes) {
        RETURN_IF_ERROR(
            rom->WriteVector(static_cast<int>(w.pc_address), w.data));
      }
    }
    write_status = "success";

    if (mock_rom || rom->filename().empty()) {
      save_status = mock_rom ? "mock-rom-skipped" : "in-memory-only";
      transaction.Commit();
      Rom verify_rom;
      RETURN_IF_ERROR(verify_rom.LoadFromData(rom->vector()));
      RETURN_IF_ERROR(VerifyPauseMapReadback(
          verify_rom, proposed_maps, modified_slots, layout, template_writes));
      readback_verified = true;
    } else {
      Rom::SaveSettings save_settings;
      save_settings.require_backup = true;
      RETURN_IF_ERROR(rom->SaveToFile(save_settings));
      save_status = "saved";
      transaction.Commit();
      Rom verify_rom;
      RETURN_IF_ERROR(verify_rom.LoadFromFile(rom->filename()));
      RETURN_IF_ERROR(VerifyPauseMapReadback(
          verify_rom, proposed_maps, modified_slots, layout, template_writes));
      readback_verified = true;
    }
  }

  json slots_summary = json::array();
  for (int slot : modified_slots) {
    const auto& before_map = current_maps[slot];
    const auto& after_map = proposed_maps[slot];
    slots_summary.push_back({
        {"slot", slot},
        {"floors_before", static_cast<int>(before_map.floor_rooms.size())},
        {"floors_after", static_cast<int>(after_map.floor_rooms.size())},
        {"rooms_before", CountActiveRooms(before_map)},
        {"rooms_after", CountActiveRooms(after_map)},
        {"boss_before", absl::StrFormat("0x%04X", before_map.boss_room)},
        {"boss_after", absl::StrFormat("0x%04X", after_map.boss_room)},
        {"expected_boss_floor_word",
         absl::StrFormat("0x%04X", ExpectedBossFloorWord(after_map))},
    });
  }

  json report = {
      {"command", GetName()},
      {"status", "success"},
      {"mode", do_write ? "write" : "dry-run"},
      {"dry_run", !do_write},
      {"in_path", in_path.string()},
      {"slots_applied", slots_summary},
      {"template_writes", static_cast<int>(template_writes.size())},
      {"bank0a_writes", static_cast<int>(map_plan.writes.size())},
      {"total_planned_writes", static_cast<int>(all_writes.size())},
      {"changed_bytes", static_cast<int>(changed_bytes)},
      {"write_status", write_status},
      {"save_status", save_status},
      {"simulated_readback_verified", true},
      {"readback_verified", readback_verified},
  };

  if (report_path.has_value()) {
    RETURN_IF_ERROR(
        WriteTextFileAtomically(*report_path, report.dump(2) + "\n"));
  }

  formatter.BeginObject("Pause Map Apply");
  formatter.AddField("status", "success");
  formatter.AddField("mode", do_write ? "write" : "dry-run");
  formatter.AddField("in_path", in_path.string());
  formatter.AddField("slots_count", static_cast<int>(modified_slots.size()));
  formatter.AddField("template_writes",
                     static_cast<int>(template_writes.size()));
  formatter.AddField("bank0a_writes", static_cast<int>(map_plan.writes.size()));
  formatter.AddField("changed_bytes", static_cast<int>(changed_bytes));
  formatter.AddField("write_status", write_status);
  formatter.AddField("save_status", save_status);
  formatter.AddField("simulated_readback_verified", true);
  formatter.AddField("readback_verified", readback_verified);
  if (report_path.has_value()) {
    formatter.AddField("report_path", report_path->string());
  }
  formatter.AddRawJsonField("slots_applied", slots_summary.dump());
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
