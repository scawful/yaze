#ifndef YAZE_APP_EDITOR_OVERWORLD_MAP_TEXTURE_COORDINATOR_H
#define YAZE_APP_EDITOR_OVERWORLD_MAP_TEXTURE_COORDINATOR_H

#include <array>
#include <functional>

#include "app/gfx/core/bitmap.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::gfx {
class IRenderer;
}  // namespace yaze::gfx

namespace yaze::editor {

struct MapTextureContext {
  zelda3::Overworld* overworld = nullptr;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr;
  gfx::IRenderer* renderer = nullptr;

  int* current_world = nullptr;
  int* current_map = nullptr;

  std::function<void(int map_index)> refresh_map_on_demand;
};

class OverworldMapTextureCoordinator {
 public:
  explicit OverworldMapTextureCoordinator(const MapTextureContext& ctx)
      : ctx_(ctx) {}

  void ResetMapBitmaps();
  void ProcessDeferredTextures();
  void EnsureMapTexture(int map_index);
  void PrimeWorldMaps(int world, bool process_texture_queue = false);

 private:
  MapTextureContext ctx_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_MAP_TEXTURE_COORDINATOR_H
