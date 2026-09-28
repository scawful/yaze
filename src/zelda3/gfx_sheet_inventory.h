#ifndef YAZE_ZELDA3_GFX_SHEET_INVENTORY_H
#define YAZE_ZELDA3_GFX_SHEET_INVENTORY_H

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "nlohmann/json_fwd.hpp"
#include "rom/rom.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::zelda3 {

struct GameData;

/**
 * @file gfx_sheet_inventory.h
 * @brief Which graphics sheets exist, which tables use them, and which 16x16
 * blocks are free.
 *
 * Mirrors Oracle's `Scripts/Analysis/gfx_sheet_inventory.py`: a sprite sheet
 * is spriteset value + 0x73, a dungeon room's spriteset is its header value +
 * 0x40, and a 16x16 block is free when its four 3bpp 8x8 tiles are all zero.
 * Raw (115-126) and 2bpp sheets are loaded by engine code, never by the
 * group tables, so they are never offered as free space.
 */

constexpr uint16_t kSpriteSheetBase = 0x73;
constexpr uint16_t kSpriteSheetValueCount = 0x67;
constexpr uint16_t kDungeonSpritesetBase = 0x40;
constexpr int kNumOverworldAreas = 0xA0;
constexpr int kNumDungeonRooms = 0x128;

struct OverworldAreaGfxInfo {
  int world = 0;  // 0 light, 1 dark, 2 special
  uint8_t parent = 0;
  uint8_t area_graphics = 0;
  std::array<uint8_t, 16> static_graphics{};
  // Sprite graphics set per game state (beginning, first part, second part).
  std::array<uint8_t, 3> sprite_graphics{};
};

struct RoomGfxInfo {
  uint8_t blockset = 0;
  uint8_t spriteset = 0;  // header value; spriteset index = value + 0x40
};

struct GfxSheetUsage {
  std::vector<int> main_blocksets;
  std::vector<int> room_blocksets;
  std::vector<int> ow_areas_static;
  // Set for sprite sheets only (gfx 0x73-0xD9).
  std::vector<int> spritesets;
  std::vector<int> ow_areas_sprite;
  std::vector<int> rooms_sprite;

  bool Any() const {
    return !main_blocksets.empty() || !room_blocksets.empty() ||
           !ow_areas_static.empty() || !spritesets.empty() ||
           !ow_areas_sprite.empty() || !rooms_sprite.empty();
  }
};

struct GfxSheetInventoryEntry {
  uint16_t gfx = 0;
  uint32_t pc = 0;
  GfxSheetStorageKind kind = GfxSheetStorageKind::kCompressed3bpp;
  // Set for gfx 0x73-0xD9: the value a spriteset stores for this sheet.
  std::optional<uint8_t> sprite_value;
  // 3bpp sheets only (2bpp sheets are engine-loaded fonts and UI).
  std::optional<size_t> stored_bytes;
  bool has_block_stats = false;
  int empty_8x8 = 0;
  std::vector<int> empty_16x16_blocks;
  bool all_empty = false;
  std::string sha1;  // first 12 hex digits of the 0x600-byte sheet
  std::string error;
  bool reserved = false;
  bool flagged = false;  // looks free, needs the owner's approval
  std::vector<uint16_t> reserved_blocks;
  bool engine_loaded = false;
  std::vector<uint16_t> shares_pointer_with;
  std::vector<uint16_t> same_content_as;
  GfxSheetUsage usage;
  bool unreferenced = false;
  std::vector<std::string> labels;

  /// 16x16 blocks that are empty and not reserved, on a sheet that is not
  /// reserved or engine-loaded. Flagged sheets still report their blocks.
  std::vector<int> FreeBlocks() const;
};

struct GfxSheetInventoryOptions {
  std::set<uint16_t> reserved_sheets;
  std::set<uint16_t> flagged_sheets;
  std::map<uint16_t, std::vector<uint16_t>> reserved_blocks;
  // Names per spriteset value, for example from Oracle's Spritesets sheet.
  std::map<uint8_t, std::set<std::string>> sprite_value_labels;
};

struct GfxSheetInventory {
  std::vector<GfxSheetInventoryEntry> sheets;  // kGfxSheetCount entries
  std::vector<std::array<uint8_t, 4>> spritesets;
  std::vector<std::array<uint8_t, 8>> main_blocksets;
  std::vector<std::array<uint8_t, 4>> room_blocksets;
  std::map<int, OverworldAreaGfxInfo> overworld_areas;
  std::map<int, RoomGfxInfo> rooms;
};

/// Reads the sheet pointer tables and group tables and joins usage from the
/// given overworld areas and rooms (either map may be empty).
absl::StatusOr<GfxSheetInventory> BuildGfxSheetInventory(
    const Rom& rom, const std::map<int, OverworldAreaGfxInfo>& areas,
    const std::map<int, RoomGfxInfo>& rooms,
    const GfxSheetInventoryOptions& options = {});

/// Area graphics for all 160 overworld areas at game state 0 (the values
/// `z3ed overworld-describe-map` reports). Constructs each OverworldMap and
/// runs LoadAreaGraphics(); no map tiles are decompressed.
std::map<int, OverworldAreaGfxInfo> CollectOverworldAreaGfx(
    Rom& rom, GameData* game_data = nullptr);

/// Blockset and spriteset header bytes (header +2, +3) for all 296 dungeon
/// rooms, read the way LoadRoomHeaderFromRom() reads them. Rooms whose
/// header pointer is out of range are left out.
std::map<int, RoomGfxInfo> CollectRoomGfx(const Rom& rom);

/// Where one spriteset or roomset is used.
struct GfxGroupUsage {
  // Spritesets: overworld areas per game state (0 beginning, 1 first part,
  // 2 second part).
  std::array<std::vector<int>, 3> ow_areas_by_state;
  // Roomsets: overworld areas whose area graphics (room blockset) match.
  std::vector<int> ow_areas;
  // Spritesets: rooms whose header value + 0x40 matches.
  // Roomsets: rooms whose header blockset matches.
  std::vector<int> rooms;
};

GfxGroupUsage FindSpritesetUsage(
    int spriteset, const std::map<int, OverworldAreaGfxInfo>& areas,
    const std::map<int, RoomGfxInfo>& rooms);

GfxGroupUsage FindRoomsetUsage(int roomset,
                               const std::map<int, OverworldAreaGfxInfo>& areas,
                               const std::map<int, RoomGfxInfo>& rooms);

/// JSON with the same keys and value formats as Oracle's reference inventory
/// (`sheets`, `spritesets`, `main_blocksets`, `room_blocksets`,
/// `overworld_areas`, `rooms`), plus `flagged`, `reserved_blocks` and
/// `free_16x16_blocks` per sheet.
nlohmann::json GfxSheetInventoryToJson(const GfxSheetInventory& inventory);

const char* GfxSheetInventoryKindName(GfxSheetStorageKind kind);

/// Parses "0xNN Name" cells in columns 2-5 of Oracle's
/// "Oracle of Secrets Data Sheet - Spritesets.csv".
std::map<uint8_t, std::set<std::string>> ParseSpritesetLabelCsv(
    const std::string& csv_text);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_GFX_SHEET_INVENTORY_H
