#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/gui/canvas/canvas_types.h"

#include <algorithm>
#include <cmath>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/gui/core/agent_theme.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

std::optional<std::pair<int, int>> MinecartLocalPosition(
    const MinecartTrack& track) {
  if (track.room_id < 0 || track.room_id >= zelda3::kNumberOfRooms)
    return {};
  const int x = track.start_x - (track.room_id & 15) * 512;
  const int y = track.start_y - (track.room_id >> 4) * 512;
  if (x < 0 || x >= 512 || y < 0 || y >= 512)
    return {};
  return std::pair{x, y};
}

absl::StatusOr<size_t> FindCartStartSprite(const zelda3::Room& room,
                                           const MinecartTrack& start) {
  const auto local = MinecartLocalPosition(start);
  if (!local)
    return absl::InvalidArgumentError("Start is outside its room");
  std::optional<size_t> match;
  const auto& sprites = room.GetSprites();
  for (size_t i = 0; i < sprites.size(); ++i) {
    const auto& sprite = sprites[i];
    if (sprite.deleted() || sprite.id() != 0xA3 ||
        sprite.subtype() != start.id || sprite.x() * 16 != local->first ||
        sprite.y() * 16 != local->second)
      continue;
    if (match)
      return absl::FailedPreconditionError("Multiple carts share this start");
    if (sprite.layer() != 0)
      return absl::FailedPreconditionError(
          "Lower-layer stop collision cannot yet be verified");
    match = i;
  }
  if (!match)
    return absl::FailedPreconditionError("No matching cart at this start");
  return *match;
}

absl::Status ValidateCartStartDestination(const zelda3::Room& room,
                                          const MinecartTrack& start) {
  const auto local = MinecartLocalPosition(start);
  if (!local || local->first % 16 || local->second % 16)
    return absl::InvalidArgumentError(
        "Place the cart on the 16 pixel sprite grid");
  if (!room.has_custom_collision())
    return absl::FailedPreconditionError(
        "This room has no verified custom stop collision");
  const auto tile = room.custom_collision()
                        .tiles[(local->second / 8) * 64 + local->first / 8];
  if (tile < 0xB7 || tile > 0xBA)
    return absl::FailedPreconditionError(
        "Place the cart on a stop, not a rail or floor tile");
  for (const auto& sprite : room.GetSprites()) {
    if (!sprite.deleted() && sprite.id() == 0xA3 &&
        sprite.subtype() != start.id && sprite.layer() == 0 &&
        sprite.x() * 16 == local->first && sprite.y() * 16 == local->second)
      return absl::FailedPreconditionError("Another cart occupies this stop");
  }
  return absl::OkStatus();
}

void MinecartCanvasTool::Cancel() {
  before_.reset();
  preview_.reset();
}

bool MinecartCanvasTool::Draw(const gui::CanvasRuntime& rt,
                              const zelda3::Room& room, int room_id,
                              bool editable, bool shortcut_focus,
                              const MinecartCanvasBinding& binding) {
  if (room_id_ != room_id || revision_ != binding.revision || !editable ||
      !binding.enabled) {
    Cancel();
    room_id_ = room_id;
    revision_ = binding.revision;
  }
  if (!binding.enabled)
    return false;
  const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling, rt.scale);
  // CanvasRuntime::canvas_sz is unscaled; match the canvas InvisibleButton.
  const ImVec2 clip_max(rt.canvas_p0.x + rt.canvas_sz.x * transform.scale(),
                        rt.canvas_p0.y + rt.canvas_sz.y * transform.scale());
  const auto& theme = AgentUI::GetTheme();
  const auto mouse = ImGui::GetMousePos();
  const auto room_point = transform.ScreenToRoomPixels(mouse);
  const bool inside = rt.hovered && room_point.x >= 0 && room_point.x < 512 &&
                      room_point.y >= 0 && room_point.y < 512;
  int hovered = -1;
  if (rt.draw_list) {
    rt.draw_list->PushClipRect(rt.canvas_p0, clip_max, true);
    for (const auto& start : binding.tracks) {
      if (start.room_id != room_id)
        continue;
      const auto local = MinecartLocalPosition(start);
      if (!local)
        continue;
      const auto center = transform.RoomPixelsToScreen(
          ImVec2(local->first + 8, local->second + 8));
      const float radius = std::max(8.0f, 8.0f * rt.scale);
      const float dx = mouse.x - center.x, dy = mouse.y - center.y;
      if (inside && dx * dx + dy * dy <= radius * radius)
        hovered = start.id;
      const auto color = ImGui::GetColorU32(start.id == binding.selected
                                                ? theme.status_warning
                                                : theme.selection_primary);
      rt.draw_list->AddCircleFilled(center, radius, color);
      rt.draw_list->AddText(ImVec2(center.x + radius + 3, center.y - 7),
                            ImGui::GetColorU32(theme.text_primary),
                            absl::StrFormat("Cart %d", start.id).c_str());
    }
    rt.draw_list->PopClipRect();
  }
  if (!editable)
    return true;
  // A pan or context-menu gesture cannot also commit a left-button move.
  if (ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
      ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    Cancel();
    return true;
  }
  auto selected = [&](int id) -> const MinecartTrack* {
    for (const auto& start : binding.tracks)
      if (start.id == id && start.room_id == room_id)
        return &start;
    return nullptr;
  };
  if ((shortcut_focus || dragging()) && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    Cancel();
    if (binding.cancel_pick)
      binding.cancel_pick();
    message_ = "Move canceled";
    return true;
  }
  if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const int id = binding.picking >= 0 ? binding.picking : hovered;
    if (const auto* start = selected(id)) {
      if (binding.select)
        binding.select(id);
      auto sprite = FindCartStartSprite(room, *start);
      if (sprite.ok()) {
        before_ = *start;
        preview_ = *start;
        message_.clear();
      } else {
        message_ = std::string(sprite.status().message());
      }
    }
  }
  if (before_) {
    // The marker's visible center is eight pixels from the sprite anchor.
    const int x = static_cast<int>(std::round((room_point.x - 8) / 16)) * 16;
    const int y = static_cast<int>(std::round((room_point.y - 8) / 16)) * 16;
    preview_ = *before_;
    preview_->start_x = (room_id & 15) * 512 + x;
    preview_->start_y = (room_id >> 4) * 512 + y;
    const auto validity =
        inside ? ValidateCartStartDestination(room, *preview_)
               : absl::InvalidArgumentError("Keep the start inside this room");
    message_ = validity.ok() ? "Release to move cart and start"
                             : std::string(validity.message());
    if (rt.draw_list) {
      rt.draw_list->PushClipRect(rt.canvas_p0, clip_max, true);
      const auto a = transform.RoomPixelsToScreen(ImVec2(x, y));
      const auto b = transform.RoomPixelsToScreen(ImVec2(x + 16, y + 16));
      rt.draw_list->AddRect(
          a, b,
          ImGui::GetColorU32(validity.ok() ? theme.status_success
                                           : theme.status_error),
          0, 0, 3);
      rt.draw_list->PopClipRect();
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      if (validity.ok() && binding.move) {
        const auto from = *before_;
        const auto to = *preview_;
        Cancel();
        const auto status = binding.move(from, to);
        message_ = status.ok()
                       ? "Cart start moved; source and room edits are pending"
                       : std::string(status.message());
      }
      Cancel();
      if (binding.cancel_pick)
        binding.cancel_pick();
    }
  } else if (shortcut_focus && !ImGui::GetIO().WantTextInput &&
             !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeySuper) {
    int dx = 0, dy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
      dx = -16;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
      dx = 16;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
      dy = -16;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
      dy = 16;
    if (const auto* start = selected(binding.selected); start && (dx || dy)) {
      auto next = *start;
      next.start_x += dx;
      next.start_y += dy;
      auto status = ValidateCartStartDestination(room, next);
      if (status.ok() && binding.move)
        status = binding.move(*start, next);
      message_ =
          status.ok() ? "Cart start moved" : std::string(status.message());
    }
  }
  if (inside && !message_.empty())
    ImGui::SetTooltip("%s", message_.c_str());
  return true;
}
}  // namespace yaze::editor
