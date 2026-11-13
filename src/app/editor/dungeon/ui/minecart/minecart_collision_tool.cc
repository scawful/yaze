#include "app/editor/dungeon/ui/minecart/minecart_collision_tool.h"

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/gui/canvas/canvas_types.h"
#include "app/gui/core/agent_theme.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {
bool IsStop(uint8_t value) {
  return value >= 0xB7 && value <= 0xBA;
}
bool IsSwitch(uint8_t value) {
  return value >= 0xD0 && value <= 0xD3;
}
bool IsMovableNode(uint8_t value) {
  return IsStop(value) || (value >= 0xBB && value <= 0xBE) || IsSwitch(value);
}
bool IsRail(uint8_t value) {
  return (value >= 0xB0 && value <= 0xBE) || IsSwitch(value);
}
bool IsInside(int x, int y, int size) {
  return (size == 1 || size == 2) && x >= 0 && y >= 0 && x <= 64 - size &&
         y <= 64 - size;
}
bool MatchesNode(const zelda3::CustomCollisionMap& map, int x, int y, int size,
                 uint8_t value) {
  if (!IsMovableNode(value) || !IsInside(x, y, size))
    return false;
  for (int dy = 0; dy < size; ++dy) {
    for (int dx = 0; dx < size; ++dx) {
      const int offset = (y + dy) * 64 + x + dx;
      if (!map.HasOverride(offset) || map.tiles[offset] != value)
        return false;
    }
  }
  return true;
}
}  // namespace

absl::StatusOr<zelda3::CustomCollisionMap> PreviewMinecartCollisionStamp(
    const zelda3::CustomCollisionMap& before,
    const MinecartCollisionStamp& stamp) {
  if (!IsInside(stamp.x, stamp.y, stamp.size) ||
      (stamp.value != 0 && !IsRail(stamp.value))) {
    return absl::InvalidArgumentError("Choose a rail stamp inside the room");
  }
  if (stamp.source && !MatchesNode(before, stamp.source->x, stamp.source->y,
                                   stamp.size, stamp.value)) {
    return absl::FailedPreconditionError(
        "Move requires a matching stop, T junction, or switch override");
  }
  auto after = before;
  if (stamp.source) {
    for (int y = stamp.source->y; y < stamp.source->y + stamp.size; ++y) {
      for (int x = stamp.source->x; x < stamp.source->x + stamp.size; ++x) {
        after.ClearOverride(y * 64 + x);
      }
    }
  }
  for (int y = stamp.y; y < stamp.y + stamp.size; ++y) {
    for (int x = stamp.x; x < stamp.x + stamp.size; ++x) {
      const auto old = before.tiles[y * 64 + x];
      if (before.HasOverride(y * 64 + x) && !IsRail(old)) {
        return absl::FailedPreconditionError(
            "Other collision occupies this cell; use the collision inspector");
      }
      if (stamp.value != 0 && ((IsStop(old) && !IsStop(stamp.value)) ||
                               (IsSwitch(old) && !IsSwitch(stamp.value)))) {
        return absl::FailedPreconditionError(
            "Stop or switch protected; choose the matching behavior tool");
      }
      if (stamp.value == 0) {
        // The rail tool's erase stamp removes an override, not floor physics.
        after.ClearOverride(y * 64 + x);
      } else {
        after.SetOverride(y * 64 + x, stamp.value);
      }
    }
  }
  after.has_data = before.has_data || stamp.value != 0;
  return after;
}

void MinecartCollisionTool::Cancel() {
  drag_.reset();
  message_.clear();
}

void MinecartCollisionTool::Draw(const gui::CanvasRuntime& rt,
                                 const zelda3::CustomCollisionMap& collision,
                                 int room_id, uint8_t value, int size,
                                 const MinecartCollisionApply& apply) {
  const int frame = ImGui::GetFrameCount();
  // An unvisited frame means stamping was disabled or another canvas took over.
  if (drag_ && (drag_->room_id != room_id || drag_->stamp.value != value ||
                drag_->stamp.size != size || last_frame_ < frame - 1)) {
    drag_.reset();
  }
  last_frame_ = frame;
  if (size != 1 && size != 2) {
    drag_.reset();
    message_ = "Choose a one-cell or two-cell collision stamp";
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
      ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
      ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    drag_.reset();
    message_ = "Collision move canceled";
    return;
  }
  const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling, rt.scale);
  auto [x, y] = ImGui::IsMousePosValid()
                    ? transform.ScreenToRoomTiles(ImGui::GetMousePos(), 8)
                    : std::pair{-1, -1};
  const bool inside = rt.hovered && x >= 0 && y >= 0 && x < 64 && y < 64;
  // Two-cell stamps align with the 16 pixel art / sprite grid.
  if (size == 2) {
    x &= ~1;
    y &= ~1;
  }
  MinecartCollisionStamp stamp{x, y, size, value};
  if (drag_)
    stamp.source = drag_->stamp.source;
  const auto& before = drag_ ? drag_->before : collision;
  const auto preview = PreviewMinecartCollisionStamp(before, stamp);
  const auto delta = ImGui::GetIO().MouseDelta;
  if (inside && (drag_ || message_.empty() || delta.x != 0 || delta.y != 0 ||
                 !preview.ok())) {
    message_ = !preview.ok() ? std::string(preview.status().message())
               : drag_ ? "Release to move. Escape / right / middle cancels."
               : IsMovableNode(value)
                   ? "Drag a matching node to move. Click elsewhere to stamp."
                   : "Click to stamp. Escape leaves stamping.";
  }
  const auto& theme = AgentUI::GetTheme();
  if (rt.draw_list) {
    const ImVec2 max(rt.canvas_p0.x + rt.canvas_sz.x * transform.scale(),
                     rt.canvas_p0.y + rt.canvas_sz.y * transform.scale());
    rt.draw_list->PushClipRect(rt.canvas_p0, max, true);
    // Existing matching nodes remain visible while the cursor moves elsewhere.
    for (int my = 0; my < 64; my += size == 2 ? 2 : 1) {
      for (int mx = 0; mx < 64; mx += size == 2 ? 2 : 1) {
        if (!MatchesNode(collision, mx, my, size, value))
          continue;
        const auto p = transform.RoomPixelsToScreen(ImVec2(mx * 8, my * 8));
        const auto q = transform.RoomPixelsToScreen(
            ImVec2((mx + size) * 8, (my + size) * 8));
        rt.draw_list->AddRect(p, q, ImGui::GetColorU32(theme.selection_primary),
                              0.0f, 2.0f);
      }
    }
    const auto color = ImGui::GetColorU32(preview.ok() ? theme.status_warning
                                                       : theme.status_error);
    for (int dy = 0; inside && dy < size; ++dy) {
      for (int dx = 0; dx < size; ++dx) {
        const auto p =
            transform.RoomPixelsToScreen(ImVec2((x + dx) * 8, (y + dy) * 8));
        const auto q = transform.RoomPixelsToScreen(
            ImVec2((x + dx + 1) * 8, (y + dy + 1) * 8));
        rt.draw_list->AddRectFilled(p, q, (color & 0x00FFFFFF) | 0x55000000);
        rt.draw_list->AddRect(p, q, color);
      }
    }
    rt.draw_list->PopClipRect();
  }
  if (drag_) {
    const auto mouse = ImGui::GetMousePos();
    const float dx = mouse.x - drag_->mouse_start_x;
    const float dy = mouse.y - drag_->mouse_start_y;
    const float threshold = ImGui::GetIO().MouseDragThreshold;
    drag_->moved |= dx * dx + dy * dy >= threshold * threshold;
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      if (inside && preview.ok() && apply && drag_->moved &&
          (x != stamp.source->x || y != stamp.source->y)) {
        const auto status = apply(room_id, drag_->before, stamp);
        if (!status.ok())
          message_ = std::string(status.message());
      }
      drag_.reset();
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      drag_.reset();
    }
  } else if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
             preview.ok() && apply) {
    if (MatchesNode(collision, x, y, size, value)) {
      stamp.source = MinecartCollisionSource{x, y};
      const auto mouse = ImGui::GetMousePos();
      drag_ = Drag{stamp, collision, mouse.x, mouse.y, room_id};
    } else {
      const auto status = apply(room_id, collision, stamp);
      if (!status.ok())
        message_ = std::string(status.message());
    }
  }
  if (!inside)
    return;
  ImGui::BeginTooltip();
  ImGui::Text("Upper layer: %d x %d cells at %d, %d", size, size, x, y);
  if (drag_)
    ImGui::Text("Source override: %d, %d", drag_->stamp.source->x,
                drag_->stamp.source->y);
  if (drag_ || IsMovableNode(value))
    ImGui::TextUnformatted(
        "Move clears the old override; underlying room collision returns.");
  ImGui::TextUnformatted(message_.c_str());
  ImGui::EndTooltip();
}

}  // namespace yaze::editor
