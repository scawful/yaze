#ifndef YAZE_TEST_TEST_UTILS_OVERWORLD_MODEL_TEST_PEER_H_
#define YAZE_TEST_TEST_UTILS_OVERWORLD_MODEL_TEST_PEER_H_

#include <utility>
#include <vector>

#include "rom/lyra_layout_descriptor.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::zelda3 {

// Gesture-only fixtures supply their own grids and pixels. Materialize the
// matching map inventory without inventing a ROM or building graphics.
class OverworldModelTestPeer {
 public:
  static void InitializeMapSlots(Overworld& world, int count) {
    world.overworld_maps_.resize(count);
    for (auto& map : world.overworld_maps_)
      map.built_ = true;
  }

  // Inject page-local Tile16 rows without a full revision-5 ImportPageTile16
  // load. Keeps page state private on Overworld; no public setter.
  static void SetPageTile16(Overworld& world,
                            std::vector<rom::NativePageTile16Definition> rows) {
    world.page_tile16_ = std::move(rows);
    world.loaded_page_tile16_ = world.page_tile16_;
  }
};

}  // namespace yaze::zelda3

#endif  // YAZE_TEST_TEST_UTILS_OVERWORLD_MODEL_TEST_PEER_H_
