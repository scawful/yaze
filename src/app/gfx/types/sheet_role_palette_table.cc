#include "app/gfx/types/sheet_role_palette_table.h"

namespace yaze {
namespace gfx {

namespace {

// Sheet-id partition mirrors zelda3/game_data.cc ProcessSheetBitmap defaults:
//   id < 113  -> dungeon_main
//   id < 128  -> sprites_aux1
//   id >= 128 -> hud
constexpr int kDungeonSheetIdEnd = 113;
constexpr int kSpriteSheetIdEnd = 128;

}  // namespace

SheetRole RoleForBlocksetSlot(int slot_index) {
  // Mirrors Tile16Editor::GetPaletteSlotForSheet/GetPaletteBaseForSheet.
  switch (slot_index) {
    case 0:
      return SheetRole::kOverworldMain;
    case 1:
    case 2:
      return SheetRole::kOverworldGfx;
    case 3:
    case 4:
      return SheetRole::kOverworldAreaAux1;
    case 5:
    case 6:
      return SheetRole::kOverworldAreaAux2;
    case 7:
      return SheetRole::kOverworldAnimated;
    default:
      return SheetRole::kUnclassified;
  }
}

SheetRole RoleForRoomsetSlot(int slot_index) {
  // Roomsets override blockset slots 4..7.
  if (slot_index < 0 || slot_index > 3) {
    return SheetRole::kUnclassified;
  }
  return RoleForBlocksetSlot(slot_index + 4);
}

SheetRole RoleForSpritesetSlot(int slot_index) {
  if (slot_index < 0 || slot_index > 3) {
    return SheetRole::kUnclassified;
  }
  return SheetRole::kSpriteAux1;
}

SheetRole RoleForSheetId(int sheet_id) {
  if (sheet_id < 0)
    return SheetRole::kUnclassified;
  if (sheet_id < kDungeonSheetIdEnd)
    return SheetRole::kDungeonMain;
  if (sheet_id < kSpriteSheetIdEnd)
    return SheetRole::kSpriteAux1;
  return SheetRole::kHud;
}

SheetRolePaletteBinding DefaultBindingFor(SheetRole role) {
  switch (role) {
    case SheetRole::kOverworldMain:
      // Slot 0 routes to AUX1 region; tile16 reports cgram row 2.
      return {"ow_aux", 0, 2};
    case SheetRole::kOverworldGfx:
      return {"ow_main", 0, 2};
    case SheetRole::kOverworldAreaAux1:
      return {"ow_aux", 0, 2};
    case SheetRole::kOverworldAreaAux2:
      return {"ow_aux", 1, 5};
    case SheetRole::kOverworldAnimated:
      return {"ow_animated", 0, 7};
    case SheetRole::kSpriteAux1:
      return {"sprites_aux1", 0, 0};
    case SheetRole::kHud:
      return {"hud", 0, 0};
    case SheetRole::kDungeonMain:
      return {"dungeon_main", 0, 0};
    case SheetRole::kUnclassified:
    default:
      return {"", 0, 0};
  }
}

}  // namespace gfx
}  // namespace yaze
