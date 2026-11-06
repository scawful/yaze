#ifndef YAZE_TEST_TEST_UTILS_OVERWORLD_MODEL_TEST_PEER_H_
#define YAZE_TEST_TEST_UTILS_OVERWORLD_MODEL_TEST_PEER_H_

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
};

}  // namespace yaze::zelda3

#endif  // YAZE_TEST_TEST_UTILS_OVERWORLD_MODEL_TEST_PEER_H_
