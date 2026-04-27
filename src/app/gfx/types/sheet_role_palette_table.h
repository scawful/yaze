#ifndef YAZE_APP_GFX_TYPES_SHEET_ROLE_PALETTE_TABLE_H_
#define YAZE_APP_GFX_TYPES_SHEET_ROLE_PALETTE_TABLE_H_

#include <cstdint>
#include <string_view>

#include "app/gfx/types/sheet_role.h"

namespace yaze {
namespace gfx {

// Default palette wiring for a given SheetRole. The fields are derived from
// tile16_editor's slot mapping plus the seeding partition in
// zelda3/game_data.cc:ProcessSheetBitmap.
struct SheetRolePaletteBinding {
  // Name accepted by PaletteGroupMap::get_group (e.g. "ow_main", "ow_aux",
  // "sprites_aux1"). Empty for kUnclassified.
  std::string_view palette_group_name;

  // Which palette within the group to use as a default starting point.
  uint8_t default_sub_index;

  // CGRAM base row in the 16-row palette layout. Used by tile16-style
  // overworld renderers that select rows by (base_row + button).
  uint8_t cgram_base_row;
};

// Map a blockset slot (0-7) to a role. Mirrors tile16_editor.cc.
SheetRole RoleForBlocksetSlot(int slot_index);

// Map a roomset slot (0-3) to a role. Roomsets override blockset slots 4-7.
SheetRole RoleForRoomsetSlot(int slot_index);

// Map a spriteset slot (0-3) to a role. All currently resolve to kSpriteAux1.
SheetRole RoleForSpritesetSlot(int slot_index);

// Coarse fallback by raw sheet ID. Mirrors the seeding partition in
// game_data.cc (sheets < 113 -> dungeon_main, < 128 -> sprites_aux1,
// otherwise -> hud). Used when the caller has no slot context.
SheetRole RoleForSheetId(int sheet_id);

// Default palette wiring for a role. kUnclassified returns an empty binding
// (palette_group_name is empty, sub_index/base_row are zero).
SheetRolePaletteBinding DefaultBindingFor(SheetRole role);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_TYPES_SHEET_ROLE_PALETTE_TABLE_H_
