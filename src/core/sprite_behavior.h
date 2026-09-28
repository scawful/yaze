#ifndef YAZE_CORE_SPRITE_BEHAVIOR_H_
#define YAZE_CORE_SPRITE_BEHAVIOR_H_

#include <array>
#include <string>
#include <vector>

namespace yaze::project {
// Ordered action IDs are runtime SprAction values, never sprite subtypes.
struct SpriteBehaviorAction {
  std::string name = "Idle";
  int animation = 0;  // Index into the asset's ZSM animation groups.
  bool block_player = false;
  int message_id = -1;  // -1 disables solicited dialogue.
  int message_next = -1;
  bool move = false;
  int x_speed = 0;
  int y_speed = 0;
  bool bounce_tiles = false;
  int timer_ticks = 0;
  int timer_next = -1;
};

struct SpriteBehavior {
  std::string
      profile;  // Empty disables the model; oracle_actions_v1 supported.
  std::array<std::string, 3> source_sha256;  // macros, functions, symbols.
  std::vector<SpriteBehaviorAction> actions;
};
}  // namespace yaze::project
#endif
