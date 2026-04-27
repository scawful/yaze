#ifndef YAZE_APP_GFX_TYPES_SHEET_ROLE_H_
#define YAZE_APP_GFX_TYPES_SHEET_ROLE_H_

#include <cstdint>

namespace yaze {
namespace gfx {

// Enumerates the role a graphics sheet plays in the rendering pipeline.
//
// Roles are slot-based, not sheet-id-based: the same physical sheet can take
// different roles depending on which blockset/roomset/spriteset slot it
// occupies. The role determines which palette region the sheet should render
// against. This mirrors the implicit mapping in tile16_editor.cc's
// GetPaletteSlotForSheet / GetPaletteBaseForSheet (sheet 0,3,4 -> AUX1,
// 1,2 -> MAIN, 5,6 -> AUX2, 7 -> ANIMATED).
//
// Sheet-id partitioning (kDungeonMain/kHud) exists only as a coarse fallback
// for callers that have a raw sheet ID without slot context. It mirrors the
// initial palette seeding in zelda3/game_data.cc:ProcessSheetBitmap.
enum class SheetRole : uint8_t {
  kUnclassified = 0,

  // Overworld blockset slot 0 (sometimes called "main blockset graphics" -
  // includes house tiles in town tilesets, dungeon entrances, etc).
  kOverworldMain,

  // Overworld blockset slots 1-2 - general area background graphics
  // (terrain, ground textures).
  kOverworldGfx,

  // Overworld blockset slots 3-4 - secondary aux graphics
  // (trees, decoration, area-specific bg detail).
  kOverworldAreaAux1,

  // Overworld blockset slots 5-6 - tertiary aux graphics
  // (cave/special area graphics, often shared between similar areas).
  kOverworldAreaAux2,

  // Overworld blockset slot 7 - animated tiles (water, lava, torches).
  kOverworldAnimated,

  // Spriteset slot - sprite/enemy graphics (sheets 113-127 in default load).
  kSpriteAux1,

  // HUD / item icons / font graphics (sheets 128+).
  kHud,

  // Dungeon graphics (sheets 0-112 in the default seeding partition).
  kDungeonMain,
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_TYPES_SHEET_ROLE_H_
