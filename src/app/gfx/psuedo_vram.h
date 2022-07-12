#ifndef YAZE_APP_GFX_PSUEDO_VRAM_H
#define YAZE_APP_GFX_PSUEDO_VRAM_H

#include <SDL2/SDL.h>

#include <cstdint>
#include <unordered_map>

#include "app/gfx/bitmap.h"

namespace yaze {
namespace app {
namespace gfx {

// Picture Processor Unit: 15-Bit

// Video RAM: 64 KB of VRAM for screen maps (for 'background' layers) and tile
// sets (for backgrounds and objects); 512 + 32 bytes of 'OAM' (Object Attribute
// Memory) for objects; 512 bytes of 'CGRAM' for palette data.

// Palette: 256 entries; 15-Bit color (BGR555) for a total of 32,768 colors.
// Maximum colors per layer per scanline: 256. Maximum colors on-screen: 32,768
// (using color arithmetic for transparency effects).

// Resolution: between 256x224 and 512x448. Most games used 256x224 pixels since
// higher resolutions caused slowdown, flicker, and/or had increased limitations
// on layers and colors (due to memory bandwidth constraints); the higher
// resolutions were used for less processor-intensive games, in-game menus,
// text, and high resolution images.

// Maximum onscreen objects (sprites): 128 (32 per line, up to 34 8x8 tiles per
// line).

// Maximum number of sprite pixels on one scanline: 256. The renderer was
// designed such that it would drop the frontmost sprites instead of the
// rearmost sprites if a scanline exceeded the limit, allowing for creative
// clipping effects.

// Most common display modes: Pixel-to-pixel text mode 1 (16 colors per tile; 3
// scrolling layers) and affine mapped text mode 7 (256 colors per tile; one
// rotating/scaling layer).
class psuedo_vram {
 public:
 private:
  std::unordered_map<uint32_t, Bitmap> m_vram;
  static const uint32_t REAL_VRAM_SIZE = 0x8000;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif