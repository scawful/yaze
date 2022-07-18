#include "pseudo_vram.h"

namespace yaze {
namespace app {
namespace gfx {

void pseudo_vram::ChangeGraphicsTileset(
    const std::vector<Bitmap>& graphics_set) {}

void pseudo_vram::ChangeGraphicsPalette(const SNESPalette& graphics_pal) {}

void pseudo_vram::ChangeSpriteTileset(const std::vector<Bitmap>& sprite_set) {}

void pseudo_vram::ChangeSpritePalette(const SNESPalette& sprite_pal) {}

std::vector<Bitmap> CreateGraphicsSet(
    int id, const std::unordered_map<int, Bitmap>& all_graphics) {
  std::vector<Bitmap> graphics_set;
  return graphics_set;
}

std::vector<Bitmap> CreateSpriteSet(
    int id, const std::unordered_map<int, Bitmap>& all_graphics) {
  std::vector<Bitmap> graphics_set;
  return graphics_set;
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze