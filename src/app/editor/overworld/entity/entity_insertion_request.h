#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_INSERTION_REQUEST_H_
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_INSERTION_REQUEST_H_

#include <cstdint>
#include <string>

#include "app/editor/overworld/canvas/overworld_context_target.h"

namespace yaze::editor {

// The popup transition delays insertion. Keep its type and destination together
// instead of looking up the active map/game state on the next frame.
struct OverworldEntityInsertionRequest {
  std::string type;
  OverworldContextTarget target;
  uint8_t item_id = 0;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_INSERTION_REQUEST_H_
