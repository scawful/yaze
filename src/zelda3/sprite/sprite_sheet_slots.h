#ifndef YAZE_ZELDA3_SPRITE_SPRITE_SHEET_SLOTS_H
#define YAZE_ZELDA3_SPRITE_SPRITE_SHEET_SLOTS_H

#include <array>
#include <cstdint>
#include <set>
#include <vector>

#include "rom/rom.h"

namespace yaze::zelda3 {

/**
 * @file sprite_sheet_slots.h
 * @brief Which graphics sheets back sprite OAM tiles 0x000-0x1FF.
 *
 * The game fills eight 64-tile slots: slots 0-3 hold the static sprite sheets
 * (0x73, 0x74 on the overworld or 0x7D in dungeons, 0x79, 0x7A), matching
 * OverworldMap::LoadSpritesBlocksets and Room::LoadRoomGraphics; slots 4-7
 * hold the spriteset's four values + 0x73. OAM tiles with the name-table bit
 * set (0x100-0x1FF) therefore come from the spriteset.
 */

/// Dungeon rooms use spritesets 0x40-0x8F (room header value + 0x40).
constexpr bool IsUnderworldSpriteset(int spriteset) {
  return spriteset >= 0x40;
}

/// The eight sheets for a spriteset's four values.
std::array<uint8_t, 8> SpriteSheetSlots(
    const std::array<uint8_t, 4>& spriteset_values, bool underworld);

/// Slot (0-7) holding 8x8 OAM tile `tile` (name-table bit included).
constexpr int SpriteSlotForTile(int tile) {
  return (tile & 0x1FF) / 0x40;
}

struct SpriteTileIssue {
  enum class Kind {
    kBlank,            // the referenced 8x8 tile is all zero
    kReservedSheet,    // the slot's sheet is reserved by the project
    kUnreadableSheet,  // the slot names a sheet that cannot be read
  };
  Kind kind = Kind::kBlank;
  int tile = 0;
  int slot = 0;
  uint16_t sheet = 0;
};

/// Checks each 8x8 tile a frame draws against the sheets in `slots` (read
/// from the ROM). One issue per tile; tiles in the same unreadable or
/// reserved slot are all listed.
std::vector<SpriteTileIssue> CheckSpriteTiles(
    const Rom& rom, const std::vector<int>& tiles,
    const std::array<uint8_t, 8>& slots,
    const std::set<uint16_t>& reserved_sheets = {});

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_SPRITE_SPRITE_SHEET_SLOTS_H
