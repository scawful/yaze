#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include "absl/status/status.h"

namespace yaze {
namespace app {
namespace editor {
class SpriteEditor {
  public:
    absl::Status Update();
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H