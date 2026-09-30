#ifndef YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_COLLISION_TOOL_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_COLLISION_TOOL_H_

#include <functional>
#include <string>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/custom_collision.h"

namespace yaze::gui {
struct CanvasRuntime;
}

namespace yaze::editor {

// Explicit behavior stamps, never inferred from a rail object's artwork.
struct MinecartCollisionStamp {
  int x = 0;
  int y = 0;
  int size = 2;  // 1 or 2 cells on the 8 pixel collision grid.
  uint8_t value = 0xB1;
};

absl::StatusOr<zelda3::CustomCollisionMap> PreviewMinecartCollisionStamp(
    const zelda3::CustomCollisionMap& before,
    const MinecartCollisionStamp& stamp);

using MinecartCollisionApply = std::function<absl::Status(
    int, const zelda3::CustomCollisionMap&, const MinecartCollisionStamp&)>;

// A hover preview followed by one click / one shared dungeon undo action.
// No draft survives navigation, a source reload, or a mode change.
class MinecartCollisionTool {
 public:
  void Draw(const gui::CanvasRuntime& rt,
            const zelda3::CustomCollisionMap& collision, int room_id,
            uint8_t value, int size, const MinecartCollisionApply& apply);
  const std::string& message() const { return message_; }

 private:
  std::string message_;
};

}  // namespace yaze::editor

#endif
