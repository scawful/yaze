#ifndef YAZE_APP_GFX_ARENA_H
#define YAZE_APP_GFX_ARENA_H

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "app/core/platform/sdl_deleter.h"
#include "app/gfx/background_buffer.h"

namespace yaze {
namespace gfx {

// Arena is a class that manages a collection of textures and surfaces
class Arena {
 public:
  static Arena& Get();

  ~Arena();

  SDL_Texture* AllocateTexture(SDL_Renderer* renderer, int width, int height);
  void FreeTexture(SDL_Texture* texture);
  void UpdateTexture(SDL_Texture* texture, SDL_Surface* surface);

  SDL_Surface* AllocateSurface(int width, int height, int depth, int format);
  void FreeSurface(SDL_Surface* surface);

  std::array<gfx::Bitmap, 223>& gfx_sheets() { return gfx_sheets_; }
  auto gfx_sheet(int i) { return gfx_sheets_[i]; }
  auto mutable_gfx_sheet(int i) { return &gfx_sheets_[i]; }
  auto mutable_gfx_sheets() { return &gfx_sheets_; }

  auto& bg1() { return bg1_; }
  auto& bg2() { return bg2_; }

 private:
  Arena();

  BackgroundBuffer bg1_;
  BackgroundBuffer bg2_;

  static constexpr int kTilesPerRow = 64;
  static constexpr int kTilesPerColumn = 64;
  static constexpr int kTotalTiles = kTilesPerRow * kTilesPerColumn;

  std::array<uint16_t, kTotalTiles> layer1_buffer_;
  std::array<uint16_t, kTotalTiles> layer2_buffer_;

  std::array<gfx::Bitmap, 223> gfx_sheets_;

  std::unordered_map<SDL_Texture*,
                     std::unique_ptr<SDL_Texture, core::SDL_Texture_Deleter>>
      textures_;

  std::unordered_map<SDL_Surface*,
                     std::unique_ptr<SDL_Surface, core::SDL_Surface_Deleter>>
      surfaces_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_ARENA_H
