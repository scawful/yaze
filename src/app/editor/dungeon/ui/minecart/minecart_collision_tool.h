#ifndef YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_COLLISION_TOOL_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_COLLISION_TOOL_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/custom_collision.h"

namespace yaze::gui {
struct CanvasRuntime;
}

namespace yaze::editor {

struct MinecartCollisionSource {
  int x = 0;
  int y = 0;
};

// Explicit behavior stamps, never inferred from a rail object's artwork.
struct MinecartCollisionStamp {
  int x = 0;
  int y = 0;
  int size = 2;  // 1 or 2 cells on the 8 pixel collision grid.
  uint8_t value = 0xB1;
  // A move clears this footprint's overrides before stamping the destination.
  // It does not replace the underlying room collision with a guessed rail.
  std::optional<MinecartCollisionSource> source = std::nullopt;
};

absl::StatusOr<zelda3::CustomCollisionMap> PreviewMinecartCollisionStamp(
    const zelda3::CustomCollisionMap& before,
    const MinecartCollisionStamp& stamp);

using MinecartCollisionApply = std::function<absl::Status(
    int, const zelda3::CustomCollisionMap&, const MinecartCollisionStamp&)>;

// One click stamps; dragging an existing behavior node emits one atomic move.
// The callback receives the gesture's original map for shared CAS / undo.
class MinecartCollisionTool {
 public:
  void Draw(const gui::CanvasRuntime& rt,
            const zelda3::CustomCollisionMap& collision, int room_id,
            uint8_t value, int size, const MinecartCollisionApply& apply);
  void Cancel();
  const std::string& message() const { return message_; }
  bool dragging() const { return drag_.has_value(); }

 private:
  struct Drag {
    MinecartCollisionStamp stamp;
    zelda3::CustomCollisionMap before;
    float mouse_start_x = 0;
    float mouse_start_y = 0;
    int room_id = -1;
    bool moved = false;
  };

  std::optional<Drag> drag_;
  int last_frame_ = -1;
  std::string message_;
};

}  // namespace yaze::editor

#endif
