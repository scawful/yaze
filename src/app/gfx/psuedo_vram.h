#ifndef YAZE_APP_GFX_PSUEDO_VRAM_H
#define YAZE_APP_GFX_PSUEDO_VRAM_H

#include <SDL2/SDL.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {
namespace gfx {

// VRAM: 64 KB of VRAM for screen maps and tile sets (backgrounds and objects)
// OAM: 512 + 32 bytes for objects (Object Attribute Memory)
// CGRAM: 512 bytes for palette data
// Palette: 256 entries; 15-Bit color (BGR555) for a total of 32,768 colors.
// Resolution: between 256x224 and 512x448.

class psuedo_vram {
 public:
  void ChangeGraphicsSet(const std::vector<Bitmap>& graphics_set);
  void ChangeGraphicsPalette(const SNESPalette& graphics_pal);
  void ChangeSpriteSet(const std::vector<Bitmap>& sprite_set);
  void ChangeSpritePalette(const SNESPalette& sprite_pal);

 private:
  std::unordered_map<uint32_t, Bitmap> m_vram;
  static const uint32_t REAL_VRAM_SIZE = 0x8000;
};

std::vector<Bitmap> CreateGraphicsSet(
    int id, const std::unordered_map<uint32_t, Bitmap>& all_graphics);
std::vector<Bitmap> CreateSpriteSet(
    int id, const std::unordered_map<uint32_t, Bitmap>& all_graphics);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif