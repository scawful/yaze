#ifndef YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_CANVAS_TOOL_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_CANVAS_TOOL_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "app/editor/dungeon/minecart_track_source.h"

namespace yaze::gui {
struct CanvasRuntime;
}

namespace yaze::zelda3 {
class Room;
}

namespace yaze::editor {

// Minecart table coordinates are world pixels, without a $1000 offset.
std::optional<std::pair<int, int>> MinecartLocalPosition(
    const MinecartTrack& track);
absl::StatusOr<size_t> FindCartStartSprite(const zelda3::Room& room,
                                           const MinecartTrack& start);
absl::Status ValidateCartStartDestination(const zelda3::Room& room,
                                          const MinecartTrack& start);

struct MinecartCanvasBinding {
  std::span<const MinecartTrack> tracks;
  int selected = -1;
  int picking = -1;
  uint64_t revision = 0;
  bool enabled = false;
  std::function<void(int)> select;
  std::function<absl::Status(const MinecartTrack&, const MinecartTrack&)> move;
  std::function<void()> cancel_pick;
};

// Per-viewer transient gesture; drafts remain in the existing track document.
class MinecartCanvasTool {
 public:
  // Returns true while this tool owns room input, including empty-space clicks.
  bool Draw(const gui::CanvasRuntime& rt, const zelda3::Room& room, int room_id,
            bool editable, bool shortcut_focus,
            const MinecartCanvasBinding& binding);
  void Cancel();
  bool dragging() const { return before_.has_value(); }
  const std::optional<MinecartTrack>& preview() const { return preview_; }
  const std::string& message() const { return message_; }

 private:
  std::optional<MinecartTrack> before_;
  std::optional<MinecartTrack> preview_;
  int room_id_ = -1;
  uint64_t revision_ = 0;
  std::string message_;
};

}  // namespace yaze::editor
#endif
