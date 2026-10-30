#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/gui/canvas/canvas_types.h"

#include <algorithm>
#include <cmath>

#include "absl/strings/str_cat.h"
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

std::vector<MinecartOverviewNode> CollectMinecartOverviewNodes(
    const zelda3::Room& room, std::span<const MinecartTrack> tracks) {
  std::vector<MinecartOverviewNode> nodes;
  auto collision_at = [&](MinecartOverviewNode& node) {
    const int x = node.pixel_x / 8, y = node.pixel_y / 8;
    const size_t offset = y * 64 + x;
    if (node.layer == 0 && room.has_custom_collision() &&
        room.custom_collision().HasOverride(offset)) {
      node.collision = room.custom_collision().tiles[offset];
      node.has_collision = true;
    }
  };
  for (const auto& track : tracks) {
    if (track.room_id != room.id() || track.id < 0 ||
        track.id >= static_cast<int>(kMinecartTrackSlotCount) ||
        (track.room_id == 0 && track.start_x == 0 && track.start_y == 0)) {
      continue;
    }
    const auto local = MinecartLocalPosition(track);
    if (!local) {
      continue;
    }
    MinecartOverviewNode node{MinecartOverviewNodeKind::kCartStart, track.id,
                              local->first, local->second};
    node.paired_actor = FindCartStartSprite(room, track).ok();
    collision_at(node);
    nodes.push_back(node);
  }
  for (const auto& sprite : room.GetSprites()) {
    if (sprite.deleted() || sprite.IsOverlord() || sprite.x() >= 32 ||
        sprite.y() >= 32) {
      continue;
    }
    MinecartOverviewNodeKind kind;
    switch (sprite.id()) {
      case 0xA3:
        kind = MinecartOverviewNodeKind::kCart;
        break;
      case 0xAF:
        kind = MinecartOverviewNodeKind::kSwitch;
        break;
      case 0xB0:
        kind = MinecartOverviewNodeKind::kRotatingTrack;
        break;
      default:
        continue;
    }
    MinecartOverviewNode node{kind, sprite.subtype(), sprite.x() * 16,
                              sprite.y() * 16, sprite.layer()};
    // A correctly paired start represents its cart too. Keep mismatched carts
    // visible so a stale source start cannot hide a live sprite edit.
    if (kind == MinecartOverviewNodeKind::kCart &&
        std::any_of(nodes.begin(), nodes.end(), [&](const auto& start) {
          return start.kind == MinecartOverviewNodeKind::kCartStart &&
                 start.paired_actor && start.id == node.id &&
                 start.pixel_x == node.pixel_x &&
                 start.pixel_y == node.pixel_y && node.layer == 0;
        })) {
      continue;
    }
    collision_at(node);
    nodes.push_back(node);
  }
  if (!room.has_custom_collision()) {
    return nodes;
  }
  const auto& collision = room.custom_collision();
  for (size_t offset = 0; offset < collision.tiles.size(); ++offset) {
    const uint8_t value = collision.tiles[offset];
    const bool stop = value >= 0xB7 && value <= 0xBA;
    const bool rotating = value >= 0xD0 && value <= 0xD3;
    if ((!stop && !rotating) || !collision.HasOverride(offset)) {
      continue;
    }
    const int x = static_cast<int>(offset % 64) * 8;
    const int y = static_cast<int>(offset / 64) * 8;
    if (rotating &&
        std::any_of(nodes.begin(), nodes.end(), [&](const auto& actor) {
          return actor.kind == MinecartOverviewNodeKind::kRotatingTrack &&
                 actor.layer == 0 && actor.pixel_x == x && actor.pixel_y == y;
        })) {
      continue;
    }
    nodes.push_back({stop ? MinecartOverviewNodeKind::kStop
                          : MinecartOverviewNodeKind::kRotatingCollision,
                     -1, x, y, 0, value, true});
  }
  return nodes;
}

bool IsMinecartOverviewNodeSelected(
    const MinecartOverviewNode& node,
    std::span<const MinecartOverviewNode> room_nodes,
    const MinecartOverviewSelection& selection) {
  using Kind = MinecartOverviewNodeKind;
  if (node.kind == Kind::kSwitch || node.kind == Kind::kRotatingTrack) {
    return selection.channel >= 0 && node.id == selection.channel;
  }
  if (node.kind == Kind::kCartStart || node.kind == Kind::kCart) {
    return selection.cart_id >= 0 && node.id == selection.cart_id;
  }
  return std::any_of(
      room_nodes.begin(), room_nodes.end(), [&](const auto& other) {
        return other.layer == 0 && other.pixel_x == node.pixel_x &&
               other.pixel_y == node.pixel_y &&
               ((node.kind == Kind::kStop &&
                 (other.kind == Kind::kCartStart ||
                  other.kind == Kind::kCart) &&
                 selection.cart_id >= 0 && other.id == selection.cart_id) ||
                (node.kind == Kind::kRotatingCollision &&
                 other.kind == Kind::kRotatingTrack && selection.channel >= 0 &&
                 other.id == selection.channel));
      });
}

std::string DescribeMinecartOverviewNode(const MinecartOverviewNode& node) {
  using Kind = MinecartOverviewNodeKind;
  std::string text;
  switch (node.kind) {
    case Kind::kCartStart:
      text = absl::StrFormat(
          "Cart start C%d (%s)", node.id,
          node.paired_actor ? "paired cart" : "no matching cart");
      break;
    case Kind::kCart:
      text = absl::StrFormat("Placed cart C%d (start does not match)", node.id);
      break;
    case Kind::kStop:
      text = "Stop collision";
      break;
    case Kind::kSwitch:
      text = absl::StrFormat("Switch channel S%d", node.id);
      break;
    case Kind::kRotatingTrack:
      text = absl::StrFormat("Rotating track channel S%d", node.id);
      break;
    case Kind::kRotatingCollision:
      text = "Rotating-track collision (no actor at this anchor)";
      break;
  }
  absl::StrAppend(&text, absl::StrFormat(" | tile (%d,%d) | %s layer",
                                         node.pixel_x / 8, node.pixel_y / 8,
                                         node.layer == 0 ? "upper" : "lower"));
  if (node.has_collision) {
    absl::StrAppend(&text,
                    absl::StrFormat(" | collision $%02X", node.collision));
  } else if (node.kind != Kind::kSwitch) {
    absl::StrAppend(&text, " | custom collision unavailable at anchor");
  }
  return text;
}

std::string DrawMinecartOverviewNodes(
    ImDrawList* draw_list, const DungeonCanvasTransform& transform,
    std::span<const MinecartOverviewNode> nodes,
    const MinecartOverviewSelection& selection, bool hover_enabled) {
  if (!draw_list) {
    return {};
  }
  using Kind = MinecartOverviewNodeKind;
  const auto& theme = AgentUI::GetTheme();
  const float radius = std::clamp(4.0f + transform.scale() * 3.0f, 4.0f, 9.0f);
  std::string hovered;
  for (const auto& node : nodes) {
    const bool collision_only =
        node.kind == Kind::kStop || node.kind == Kind::kRotatingCollision;
    const float half_size = collision_only ? 4.0f : 8.0f;
    const auto center = transform.RoomPixelsToScreen(
        ImVec2(node.pixel_x + half_size, node.pixel_y + half_size));
    const bool selected =
        IsMinecartOverviewNodeSelected(node, nodes, selection);
    const auto base_color = node.kind == Kind::kSwitch ? theme.status_error
                            : node.kind == Kind::kRotatingTrack ||
                                    node.kind == Kind::kRotatingCollision
                                ? theme.transport_color
                            : node.kind == Kind::kStop
                                ? theme.status_success
                                : theme.selection_primary;
    const ImU32 color = ImGui::GetColorU32(base_color);
    draw_list->AddCircleFilled(center, radius + 1.5f, IM_COL32(0, 0, 0, 220),
                               12);
    if (collision_only) {
      draw_list->AddRectFilled(ImVec2(center.x - radius, center.y - radius),
                               ImVec2(center.x + radius, center.y + radius),
                               color, 1.0f);
    } else if (node.kind == Kind::kSwitch ||
               node.kind == Kind::kRotatingTrack) {
      draw_list->AddQuadFilled(ImVec2(center.x, center.y - radius),
                               ImVec2(center.x + radius, center.y),
                               ImVec2(center.x, center.y + radius),
                               ImVec2(center.x - radius, center.y), color);
    } else {
      draw_list->AddCircleFilled(center, radius, color, 12);
    }
    if (selected) {
      draw_list->AddCircle(center, radius + 3.0f,
                           ImGui::GetColorU32(theme.status_warning), 12, 2.0f);
    }
    if (node.id >= 0 && (selected || transform.scale() >= 0.24f)) {
      const char prefix = node.kind == Kind::kSwitch          ? 'S'
                          : node.kind == Kind::kRotatingTrack ? 'R'
                                                              : 'C';
      const auto label = absl::StrFormat("%c%d", prefix, node.id);
      const ImVec2 position(center.x + radius + 3.0f, center.y - 7.0f);
      draw_list->AddText(ImVec2(position.x + 1, position.y + 1),
                         IM_COL32(0, 0, 0, 255), label.c_str());
      draw_list->AddText(position, ImGui::GetColorU32(theme.text_primary),
                         label.c_str());
    }
    const auto mouse = ImGui::GetMousePos();
    const float dx = mouse.x - center.x, dy = mouse.y - center.y;
    if (hover_enabled && dx * dx + dy * dy <= (radius + 3) * (radius + 3)) {
      if (!hovered.empty()) {
        hovered += '\n';
      }
      hovered += DescribeMinecartOverviewNode(node);
    }
  }
  return hovered;
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
          0.0f, 3.0f);
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
