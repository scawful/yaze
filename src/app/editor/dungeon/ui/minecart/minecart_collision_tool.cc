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
bool IsRail(uint8_t value) {
  return (value >= 0xB0 && value <= 0xBE) || IsSwitch(value);
}
}  // namespace

absl::StatusOr<zelda3::CustomCollisionMap> PreviewMinecartCollisionStamp(
    const zelda3::CustomCollisionMap& before,
    const MinecartCollisionStamp& stamp) {
  if ((stamp.size != 1 && stamp.size != 2) || stamp.x < 0 || stamp.y < 0 ||
      stamp.x > 64 - stamp.size || stamp.y > 64 - stamp.size ||
      !IsRail(stamp.value)) {
    return absl::InvalidArgumentError("Choose a rail stamp inside the room");
  }
  auto after = before;
  for (int y = stamp.y; y < stamp.y + stamp.size; ++y) {
    for (int x = stamp.x; x < stamp.x + stamp.size; ++x) {
      const auto old = before.tiles[y * 64 + x];
      if (old != 0 && !IsRail(old)) {
        return absl::FailedPreconditionError(
            "Other collision occupies this cell; use the collision inspector");
      }
      if ((IsStop(old) && !IsStop(stamp.value)) ||
          (IsSwitch(old) && !IsSwitch(stamp.value))) {
        return absl::FailedPreconditionError(
            "Stop or switch protected; choose the matching behavior tool");
      }
      after.tiles[y * 64 + x] = stamp.value;
    }
  }
  after.has_data = true;
  return after;
}

void MinecartCollisionTool::Draw(const gui::CanvasRuntime& rt,
                                 const zelda3::CustomCollisionMap& collision,
                                 int room_id, uint8_t value, int size,
                                 const MinecartCollisionApply& apply) {
  const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling, rt.scale);
  auto [x, y] = transform.ScreenToRoomTiles(ImGui::GetMousePos(), 8);
  if (!rt.hovered || x < 0 || y < 0 || x >= 64 || y >= 64) {
    message_.clear();
    return;
  }
  // Two-cell stamps align with the 16 pixel art / sprite grid.
  if (size == 2) {
    x &= ~1;
    y &= ~1;
  }
  const MinecartCollisionStamp stamp{x, y, size, value};
  const auto preview = PreviewMinecartCollisionStamp(collision, stamp);
  const auto delta = ImGui::GetIO().MouseDelta;
  if (message_.empty() || delta.x != 0 || delta.y != 0 || !preview.ok()) {
    message_ = preview.ok() ? "Click to stamp. Escape returns to selection."
                            : std::string(preview.status().message());
  }
  const auto& theme = AgentUI::GetTheme();
  if (rt.draw_list) {
    const ImVec2 max(rt.canvas_p0.x + rt.canvas_sz.x * transform.scale(),
                     rt.canvas_p0.y + rt.canvas_sz.y * transform.scale());
    rt.draw_list->PushClipRect(rt.canvas_p0, max, true);
    const auto color = ImGui::GetColorU32(preview.ok() ? theme.status_warning
                                                       : theme.status_error);
    for (int dy = 0; dy < size; ++dy) {
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
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && preview.ok() && apply &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Middle) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    const auto status = apply(room_id, collision, stamp);
    if (!status.ok())
      message_ = std::string(status.message());
  }
  ImGui::BeginTooltip();
  ImGui::Text("Upper layer: %d x %d cells at %d, %d", size, size, x, y);
  ImGui::TextUnformatted(message_.c_str());
  ImGui::EndTooltip();
}

}  // namespace yaze::editor
