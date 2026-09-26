#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_EDITOR_INTERNAL_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_EDITOR_INTERNAL_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "app/editor/sprite/zsprite.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"

namespace yaze {
namespace editor {
namespace internal {

// 8x8 OAM tile ids a ZSM frame draws, sorted and unique. A 16x16 entry covers
// id, id+1, id+16 and id+17, as SpriteDrawer draws it.
inline std::vector<int> FrameTiles8x8(const zsprite::Frame& frame) {
  std::vector<int> tiles;
  for (const auto& tile : frame.Tiles) {
    tiles.push_back(tile.id);
    if (tile.size) {
      tiles.push_back(tile.id + 1);
      tiles.push_back(tile.id + 16);
      tiles.push_back(tile.id + 17);
    }
  }
  std::sort(tiles.begin(), tiles.end());
  tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
  return tiles;
}

// The preview palettes SpriteEditor uses without a binding: the global
// sprite palettes, then aux1/aux2/aux3 palettes until there are 8 rows.
inline gfx::PaletteGroup DefaultSpritePreviewPalettes(
    const gfx::PaletteGroup& global, const gfx::PaletteGroup& aux1,
    const gfx::PaletteGroup& aux2, const gfx::PaletteGroup& aux3) {
  gfx::PaletteGroup palettes;
  for (size_t i = 0; i < global.size() && i < 8; i++) {
    palettes.AddPalette(global.palette(i));
  }
  while (palettes.size() < 8) {
    if (palettes.size() < 4 && aux1.size() > 0) {
      palettes.AddPalette(aux1.palette(palettes.size() % aux1.size()));
    } else if (palettes.size() < 6 && aux2.size() > 0) {
      palettes.AddPalette(aux2.palette((palettes.size() - 4) % aux2.size()));
    } else if (aux3.size() > 0) {
      palettes.AddPalette(aux3.palette((palettes.size() - 6) % aux3.size()));
    } else {
      palettes.AddPalette(gfx::SnesPalette());
    }
  }
  return palettes;
}

// Eight 16-color sprite palettes (OAM palettes 0-7) from CGRAM rows 8-15.
inline gfx::PaletteGroup SpritePalettesFromCgram(
    const std::array<SDL_Color, 256>& cgram) {
  gfx::PaletteGroup palettes;
  for (int row = 8; row < 16; ++row) {
    gfx::SnesPalette palette;
    for (int i = 0; i < 16; ++i) {
      const SDL_Color& c = cgram[row * 16 + i];
      palette.AddColor(gfx::SnesColor(c.r, c.g, c.b));
    }
    palettes.AddPalette(palette);
  }
  return palettes;
}

// Initialize a sprite-preview bitmap once and queue its texture for creation.
// Idempotent: returns immediately if the bitmap is already active.
//
// SpriteEditor used to call Create()+Reformat() inline, never queueing a
// CREATE texture command, so canvas_rendering's `if (!texture()) return;`
// guard skipped the draw silently and the preview was always blank. This
// helper closes that gap and stamps the bitmap as a composite output (the
// pixels come from SpriteDrawer rendering OAM tiles into the surface, not
// from direct user paint).
//
// Safe to call every render frame; the `is_active()` short-circuit ensures
// only the first call does any work.
inline void EnsureSpritePreviewBitmapReady(
    gfx::Bitmap& bmp, int width, int height, int depth,
    const std::vector<uint8_t>& gfx_buffer) {
  if (bmp.is_active()) {
    return;
  }
  // Graphics sheets are input to the drawer, not initial composite pixels.
  const std::vector<uint8_t> blank(
      gfx_buffer.empty() ? 0 : static_cast<size_t>(width) * height, 0);
  bmp.Create(width, height, depth, blank);
  if (!bmp.is_active()) {
    // Create bailed (empty buffer, surface allocation failed). Don't call
    // Reformat. It would unconditionally allocate a fresh surface and set
    // active=true with no pixel data, masking the load failure and queuing
    // a CREATE for an effectively-blank bitmap. Leave the bitmap inactive
    // so the next frame's call retries Create with a real buffer.
    return;
  }
  bmp.Reformat(depth);
  if (bmp.surface() != nullptr) {
    bmp.CreateTexture();
  }
  bmp.metadata().purpose = gfx::Bitmap::BitmapPurpose::kCompositeOutput;
}

// SpriteDrawer writes Bitmap::mutable_data(); the renderer reads the SDL surface.
inline void PublishSpritePreviewPixels(gfx::Bitmap& bmp) {
  bmp.UpdateSurfacePixels();
  bmp.UpdateTexture();
}

}  // namespace internal
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_EDITOR_INTERNAL_H_
