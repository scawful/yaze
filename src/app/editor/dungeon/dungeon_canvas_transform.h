#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_TRANSFORM_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_TRANSFORM_H_

#include <cmath>
#include <utility>

#include "imgui/imgui.h"

namespace yaze::editor {

// Pure coordinate transform shared by dungeon interaction and overlay paths.
// Room pixels are always unscaled and relative to the 512x512 room origin.
class DungeonCanvasTransform {
 public:
  DungeonCanvasTransform(ImVec2 canvas_origin, ImVec2 scrolling, float scale)
      : canvas_origin_(canvas_origin),
        scrolling_(scrolling),
        scale_(std::isfinite(scale) && scale > 0.0f ? scale : 1.0f) {}

  [[nodiscard]] ImVec2 room_origin_screen() const {
    return ImVec2(canvas_origin_.x + scrolling_.x,
                  canvas_origin_.y + scrolling_.y);
  }

  [[nodiscard]] float scale() const { return scale_; }

  [[nodiscard]] ImVec2 ScreenToRoomPixels(ImVec2 screen) const {
    const ImVec2 origin = room_origin_screen();
    return ImVec2((screen.x - origin.x) / scale_,
                  (screen.y - origin.y) / scale_);
  }

  [[nodiscard]] std::pair<int, int> ScreenToRoomPixelCoordinates(
      ImVec2 screen) const {
    const ImVec2 room = ScreenToRoomPixels(screen);
    return {static_cast<int>(std::floor(room.x)),
            static_cast<int>(std::floor(room.y))};
  }

  [[nodiscard]] std::pair<int, int> ScreenToRoomTiles(ImVec2 screen,
                                                      int tile_size) const {
    const ImVec2 room = ScreenToRoomPixels(screen);
    return {static_cast<int>(std::floor(room.x / tile_size)),
            static_cast<int>(std::floor(room.y / tile_size))};
  }

  [[nodiscard]] ImVec2 RoomPixelsToScreen(ImVec2 room) const {
    const ImVec2 origin = room_origin_screen();
    return ImVec2(origin.x + room.x * scale_, origin.y + room.y * scale_);
  }

  [[nodiscard]] ImVec2 RoomSizeToScreen(ImVec2 room_size) const {
    return ImVec2(room_size.x * scale_, room_size.y * scale_);
  }

 private:
  ImVec2 canvas_origin_;
  ImVec2 scrolling_;
  float scale_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_CANVAS_TRANSFORM_H_
