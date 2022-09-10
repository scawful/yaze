#ifndef YAZE_APP_ZELDA3_PALETTES_H
#define YAZE_APP_ZELDA3_PALETTES_H

#include <cstdint>

#include "app/core/common.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {
// 32 (0,0)
static gfx::PaletteGroup HudPalettes(2);

// 35 colors each, 7x5 (0,2 on grid)
static gfx::PaletteGroup overworld_MainPalettes(6);

// 21 colors each, 7x3 (8,2 and 8,5 on grid)
static gfx::PaletteGroup overworld_AuxPalettes(20);

// 7 colors each 7x1 (0,7 on grid)
static gfx::PaletteGroup overworld_AnimatedPalettes(14);
static gfx::PaletteGroup globalSprite_Palettes(2);   // 60 (1,9)
static gfx::PaletteGroup armors_Palettes(5);         // 15
static gfx::PaletteGroup swords_Palettes(4);         // 3
static gfx::PaletteGroup spritesAux1_Palettes(12);   // 7
static gfx::PaletteGroup spritesAux2_Palettes(11);   // 7
static gfx::PaletteGroup spritesAux3_Palettes(24);   // 7
static gfx::PaletteGroup shields_Palettes(3);        // 4
static gfx::PaletteGroup dungeonsMain_Palettes(20);  // 15*6

// 8*20
static gfx::PaletteGroup overworld_BackgroundPalette(core::kNumOverworldMaps);

// 3 hardcoded grass colors
static gfx::SNESPalette overworld_GrassPalettes(3);                                         
static gfx::PaletteGroup object3D_Palettes(2);  // 15*6
static gfx::PaletteGroup overworld_Mini_Map_Palettes(2);  // 16*8
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif