#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/ui_constants.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

using namespace dungeon_ui;  // NOLINT(google-build-using-namespace)

constexpr char kConnectedCurrentRoomHudId[] =
    "##DungeonConnectedCurrentRoomHUD";
constexpr char kConnectedCanvasControlsId[] =
    "##DungeonConnectedCanvasControls";
constexpr char kConnectedCanvasOverviewId[] =
    "##DungeonConnectedCanvasOverview";

ImVec2 GetConnectedRoomOrigin(int grid_col, int grid_row, int min_col,
                              int min_row) {
  const int col = grid_col - min_col;
  const int row = grid_row - min_row;
  const float step = kDungeonRoomPixelSize + kConnectedRoomGap;
  return ImVec2(static_cast<float>(col) * step, static_cast<float>(row) * step);
}

ImVec2 GetConnectedRoomCenter(int grid_col, int grid_row, int min_col,
                              int min_row) {
  const ImVec2 origin =
      GetConnectedRoomOrigin(grid_col, grid_row, min_col, min_row);
  return ImVec2(origin.x + (kDungeonRoomPixelSize * 0.5f),
                origin.y + (kDungeonRoomPixelSize * 0.5f));
}

ImVec2 GetConnectedRoomAnchor(int grid_col, int grid_row,
                              zelda3::DoorDirection dir, int min_col,
                              int min_row) {
  const ImVec2 origin =
      GetConnectedRoomOrigin(grid_col, grid_row, min_col, min_row);
  const float half = kDungeonRoomPixelSize * 0.5f;
  switch (dir) {
    case zelda3::DoorDirection::North:
      return ImVec2(origin.x + half, origin.y);
    case zelda3::DoorDirection::South:
      return ImVec2(origin.x + half, origin.y + kDungeonRoomPixelSize);
    case zelda3::DoorDirection::West:
      return ImVec2(origin.x, origin.y + half);
    case zelda3::DoorDirection::East:
      return ImVec2(origin.x + kDungeonRoomPixelSize, origin.y + half);
  }
  return GetConnectedRoomCenter(grid_col, grid_row, min_col, min_row);
}

zelda3::DoorDirection GetConnectedTravelDirection(int from_col, int from_row,
                                                  int to_col, int to_row) {
  const int delta_col = to_col - from_col;
  const int delta_row = to_row - from_row;
  if (std::abs(delta_col) >= std::abs(delta_row)) {
    return delta_col >= 0 ? zelda3::DoorDirection::East
                          : zelda3::DoorDirection::West;
  }
  return delta_row >= 0 ? zelda3::DoorDirection::South
                        : zelda3::DoorDirection::North;
}

ImVec2 ToConnectedCanvasScreenPoint(const gui::CanvasRuntime& canvas_rt,
                                    const ImVec2& canvas_point) {
  return ImVec2(canvas_rt.canvas_p0.x + canvas_point.x * canvas_rt.scale,
                canvas_rt.canvas_p0.y + canvas_point.y * canvas_rt.scale);
}

std::pair<ImVec2, ImVec2> GetConnectedLinkEndpoints(
    const DungeonConnectedRoomLink& link, int from_col, int from_row,
    int to_col, int to_row, int min_col, int min_row) {
  if (link.type == DungeonConnectedLinkType::Door) {
    return {GetConnectedRoomAnchor(from_col, from_row, link.direction, min_col,
                                   min_row),
            GetConnectedRoomAnchor(to_col, to_row, OppositeDir(link.direction),
                                   min_col, min_row)};
  }

  const zelda3::DoorDirection from_dir =
      GetConnectedTravelDirection(from_col, from_row, to_col, to_row);
  const zelda3::DoorDirection to_dir =
      GetConnectedTravelDirection(to_col, to_row, from_col, from_row);
  return {
      GetConnectedRoomAnchor(from_col, from_row, from_dir, min_col, min_row),
      GetConnectedRoomAnchor(to_col, to_row, to_dir, min_col, min_row)};
}

ImU32 GetConnectedLinkColor(const AgentUITheme& theme,
                            DungeonConnectedLinkType type) {
  switch (type) {
    case DungeonConnectedLinkType::Door:
      return ImGui::GetColorU32(ImVec4(theme.dungeon_object_door.x,
                                       theme.dungeon_object_door.y,
                                       theme.dungeon_object_door.z, 0.72f));
    case DungeonConnectedLinkType::Staircase:
      return ImGui::GetColorU32(ImVec4(theme.dungeon_object_stairs.x,
                                       theme.dungeon_object_stairs.y,
                                       theme.dungeon_object_stairs.z, 0.78f));
    case DungeonConnectedLinkType::Holewarp:
      return ImGui::GetColorU32(ImVec4(theme.transport_color.x,
                                       theme.transport_color.y,
                                       theme.transport_color.z, 0.82f));
  }
  return ImGui::GetColorU32(theme.selection_primary);
}

ImVec4 WithAlpha(ImVec4 color, float alpha) {
  color.w = alpha;
  return color;
}

ImVec4 GetConnectedFloorBaseColor(
    const AgentUITheme& theme,
    const DungeonCanvasViewer::ConnectedRoomGraphData& graph,
    const std::string& floor_label) {
  auto it = std::find(graph.floor_order.begin(), graph.floor_order.end(),
                      floor_label);
  const int floor_index =
      it == graph.floor_order.end()
          ? 0
          : static_cast<int>(std::distance(graph.floor_order.begin(), it));
  switch (floor_index % 6) {
    case 0:
      return theme.selection_primary;
    case 1:
      return theme.transport_color;
    case 2:
      return theme.status_success;
    case 3:
      return theme.dungeon_object_stairs;
    case 4:
      return theme.dungeon_object_door;
    default:
      return theme.selection_secondary;
  }
}

ImU32 GetConnectedFloorColor(
    const AgentUITheme& theme,
    const DungeonCanvasViewer::ConnectedRoomGraphData& graph, int room_id,
    float alpha) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return ImGui::GetColorU32(WithAlpha(theme.text_secondary_gray, alpha));
  }
  const std::string& floor_label =
      graph.room_floor_labels[static_cast<size_t>(room_id)];
  if (floor_label.empty()) {
    return ImGui::GetColorU32(WithAlpha(theme.text_secondary_gray, alpha));
  }
  return ImGui::GetColorU32(
      WithAlpha(GetConnectedFloorBaseColor(theme, graph, floor_label), alpha));
}

int CountConnectedRoomsOnFloor(
    const DungeonCanvasViewer::ConnectedRoomGraphData& graph,
    const std::string& floor_label) {
  int count = 0;
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    if (!graph.room_mask[static_cast<size_t>(room_id)]) {
      continue;
    }
    if (graph.room_floor_labels[static_cast<size_t>(room_id)] == floor_label) {
      ++count;
    }
  }
  return count;
}

int CountAutoFixableStaircaseIssues(
    const std::vector<DungeonStaircaseIssue>& issues) {
  return static_cast<int>(std::count_if(
      issues.begin(), issues.end(), [](const DungeonStaircaseIssue& issue) {
        return issue.kind == DungeonStaircaseIssueKind::UnusedHeader &&
               issue.slot_index >= 0 && issue.slot_index < 4 &&
               issue.header_room_id > 0;
      }));
}

void DrawConnectedLegendItem(const char* label, ImU32 color,
                             DungeonConnectedLinkType type) {
  constexpr float kSwatchWidth = 12.0f;
  constexpr float kSwatchHeight = 10.0f;
  const ImVec2 swatch_min = ImGui::GetCursorScreenPos();
  const ImVec2 swatch_size(kSwatchWidth, kSwatchHeight);
  ImGui::Dummy(swatch_size);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const float mid_y = swatch_min.y + (kSwatchHeight * 0.5f);
  const ImVec2 line_start(swatch_min.x + 1.5f, mid_y);
  const ImVec2 line_end(swatch_min.x + kSwatchWidth - 1.5f, mid_y);
  draw_list->AddLine(line_start, line_end, color, 1.75f);
  if (type != DungeonConnectedLinkType::Door) {
    draw_list->AddCircleFilled(line_start, 1.8f, color, 10);
    draw_list->AddCircleFilled(line_end, 1.8f, color, 10);
  }

  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextDisabled("%s", label);
}

ImVec2 GetConnectedCanvasSize(int min_col, int max_col, int min_row,
                              int max_row) {
  const int cols = std::max(1, (max_col - min_col) + 1);
  const int rows = std::max(1, (max_row - min_row) + 1);
  return ImVec2(
      (cols * kDungeonRoomPixelSize) + ((cols - 1) * kConnectedRoomGap),
      (rows * kDungeonRoomPixelSize) + ((rows - 1) * kConnectedRoomGap));
}

void HandleConnectedCanvasZoom(gui::Canvas& canvas,
                               const gui::CanvasRuntime& canvas_rt,
                               const ImVec2& content_size) {
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
    return;
  }

  const float wheel = ImGui::GetIO().MouseWheel;
  if (std::abs(wheel) < 0.001f) {
    return;
  }

  const float old_scale = canvas.global_scale();
  const float zoom_factor = wheel > 0.0f ? 1.12f : (1.0f / 1.12f);
  const float new_scale =
      std::clamp(old_scale * zoom_factor, kConnectedCanvasMinScale,
                 kConnectedCanvasMaxScale);
  if (std::abs(new_scale - old_scale) < 0.0001f) {
    return;
  }

  const ImVec2 mouse_pos = ImGui::GetMousePos();
  const ImVec2 mouse_local(mouse_pos.x - canvas_rt.canvas_p0.x,
                           mouse_pos.y - canvas_rt.canvas_p0.y);
  const float world_x = (ImGui::GetScrollX() + mouse_local.x) / old_scale;
  const float world_y = (ImGui::GetScrollY() + mouse_local.y) / old_scale;
  canvas.set_global_scale(new_scale);

  const float max_scroll_x =
      std::max(0.0f, (content_size.x * new_scale) - canvas_rt.canvas_sz.x);
  const float max_scroll_y =
      std::max(0.0f, (content_size.y * new_scale) - canvas_rt.canvas_sz.y);
  ImGui::SetScrollX(
      std::clamp((world_x * new_scale) - mouse_local.x, 0.0f, max_scroll_x));
  ImGui::SetScrollY(
      std::clamp((world_y * new_scale) - mouse_local.y, 0.0f, max_scroll_y));
}

void HandleConnectedCanvasPan(int hovered_room_id) {
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
    return;
  }

  const bool drag_middle = ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
  const bool drag_right = ImGui::IsMouseDragging(ImGuiMouseButton_Right);
  const bool drag_left_background =
      hovered_room_id < 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
  if (!drag_middle && !drag_right && !drag_left_background) {
    return;
  }

  const ImVec2 delta = ImGui::GetIO().MouseDelta;
  const float new_scroll_x =
      std::clamp(ImGui::GetScrollX() - delta.x, 0.0f, ImGui::GetScrollMaxX());
  const float new_scroll_y =
      std::clamp(ImGui::GetScrollY() - delta.y, 0.0f, ImGui::GetScrollMaxY());
  ImGui::SetScrollX(new_scroll_x);
  ImGui::SetScrollY(new_scroll_y);
}

struct ConnectedCanvasControlActions {
  bool fit = false;
  bool reset = false;
  bool reset_position = false;
  bool hovered = false;
  bool show_overview = true;
  bool show_preview = true;
  std::optional<ImVec2> moved_window_pos;
};

struct ConnectedControlsCompactActions {
  bool fit = false;
  bool reset = false;
};

void DrawConnectedControlsLegend() {
  const auto& agent_theme = AgentUI::GetTheme();
  DrawConnectedLegendItem(
      "Door",
      GetConnectedLinkColor(agent_theme, DungeonConnectedLinkType::Door),
      DungeonConnectedLinkType::Door);
  ImGui::SameLine(0.0f, 8.0f);
  DrawConnectedLegendItem(
      "Stair",
      GetConnectedLinkColor(agent_theme, DungeonConnectedLinkType::Staircase),
      DungeonConnectedLinkType::Staircase);
  ImGui::SameLine(0.0f, 8.0f);
  DrawConnectedLegendItem(
      "Warp",
      GetConnectedLinkColor(agent_theme, DungeonConnectedLinkType::Holewarp),
      DungeonConnectedLinkType::Holewarp);
}

ConnectedControlsCompactActions DrawConnectedControlsCompact(
    float scale, int connected_room_count, bool* show_overview,
    bool* show_preview) {
  ConnectedControlsCompactActions actions;
  ImGui::TextDisabled(tr("%d room%s"), connected_room_count,
                      connected_room_count == 1 ? "" : "s");
  ImGui::SameLine(0.0f, 8.0f);
  ImGui::TextDisabled("%.0f%%", scale * 100.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Connected zoom\nF fit\n0 reset\n+/- zoom"));
  }

  ImGui::SameLine(0.0f, 6.0f);
  if (ImGui::SmallButton(ICON_MD_FIT_SCREEN "##ConnectedFit")) {
    actions.fit = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Fit all connected rooms in view (F)"));
  }

  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_RESTART_ALT "##ConnectedReset")) {
    actions.reset = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Reset zoom and recenter on current room (0)"));
  }

  if (show_overview) {
    ImGui::SameLine();
    ImGui::Checkbox(tr("Mini##ConnectedMini"), show_overview);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr(
          "Show connected overview map\nClick a room on the overview to jump"));
    }
  }

  if (show_preview) {
    ImGui::SameLine();
    ImGui::Checkbox(tr("Room##ConnectedRoomPreview"), show_preview);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Show full-resolution preview of the current room"));
    }
  }

  ImGui::SameLine(0.0f, 8.0f);
  DrawConnectedControlsLegend();
  return actions;
}

struct ConnectedCanvasOverviewActions {
  bool hovered = false;
  std::optional<ImVec2> target_scroll;
  std::optional<int> activated_room_id;
  int hovered_room_id = -1;
};

ConnectedCanvasControlActions DrawConnectedCanvasControls(
    const ImVec2& pos, const ImVec2& pivot, float scale,
    int connected_room_count, bool show_overview, bool show_preview) {
  ConnectedCanvasControlActions actions;
  actions.show_overview = show_overview;
  actions.show_preview = show_preview;
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
  ImGui::SetNextWindowBgAlpha(0.84f);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_AlwaysAutoResize;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.76f));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        gui::ConvertColorToImVec4(theme.accent));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 4.0f));

  if (ImGui::Begin(kConnectedCanvasControlsId, nullptr, flags)) {
    actions.hovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (ImGui::SmallButton("::")) {}
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Drag controls\nDouble-click to snap back"));
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        actions.reset_position = true;
      }
    }
    if (ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      const ImVec2 window_pos = ImGui::GetWindowPos();
      const ImVec2 new_pos(window_pos.x + ImGui::GetIO().MouseDelta.x,
                           window_pos.y + ImGui::GetIO().MouseDelta.y);
      ImGui::SetWindowPos(new_pos);
      actions.moved_window_pos = new_pos;
    }
    ImGui::SameLine(0.0f, 6.0f);
    const auto compact_actions = DrawConnectedControlsCompact(
        scale, connected_room_count, &actions.show_overview,
        &actions.show_preview);
    actions.fit = compact_actions.fit;
    actions.reset = compact_actions.reset;
  }
  ImGui::End();

  ImGui::PopStyleVar(5);
  ImGui::PopStyleColor(2);
  return actions;
}

ConnectedCanvasOverviewActions DrawConnectedCanvasOverview(
    const DungeonCanvasViewer::ConnectedRoomGraphData& connected_graph,
    const ImVec2& content_size, const ImVec2& viewport_size,
    float content_scale, float scroll_x, float scroll_y, int center_room_id,
    int highlighted_room_id, const ImVec2& window_pos) {
  ConnectedCanvasOverviewActions actions;
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  const float inner_padding = 10.0f;
  const float header_height = 18.0f;
  const float map_max_width =
      kConnectedOverviewMaxWidth - (inner_padding * 2.0f);
  const float map_max_height = kConnectedOverviewMaxHeight - header_height -
                               (inner_padding * 2.0f) - 8.0f;
  const float overview_scale =
      std::min(map_max_width / std::max(1.0f, content_size.x),
               map_max_height / std::max(1.0f, content_size.y));
  const ImVec2 map_size(std::max(72.0f, content_size.x * overview_scale),
                        std::max(56.0f, content_size.y * overview_scale));
  const ImVec2 window_size(map_size.x + (inner_padding * 2.0f),
                           map_size.y + header_height + (inner_padding * 2.0f));
  ImGui::SetNextWindowPos(window_pos);
  ImGui::SetNextWindowSize(window_size);
  ImGui::SetNextWindowBgAlpha(0.84f);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.78f));
  ImGui::PushStyleColor(ImGuiCol_Border,
                        gui::ConvertColorToImVec4(theme.selection_primary));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(inner_padding, inner_padding));

  if (ImGui::Begin(kConnectedCanvasOverviewId, nullptr, flags)) {
    actions.hovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGui::TextDisabled(tr("Overview"));
    ImGui::InvisibleButton("##ConnectedOverviewMap", map_size,
                           ImGuiButtonFlags_MouseButtonLeft);

    const ImVec2 map_min = ImGui::GetItemRectMin();
    const ImVec2 map_max = ImGui::GetItemRectMax();
    auto* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(map_min, map_max, IM_COL32(12, 16, 14, 230), 4.0f);
    draw_list->AddRect(map_min, map_max,
                       ImGui::GetColorU32(theme.selection_primary), 4.0f);

    auto overview_screen_point = [&](const ImVec2& canvas_point) {
      return ImVec2(map_min.x + (canvas_point.x * overview_scale),
                    map_min.y + (canvas_point.y * overview_scale));
    };

    const bool map_hovered =
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const ImVec2 mouse = ImGui::GetMousePos();
    if (map_hovered) {
      for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
        if (!connected_graph.room_mask[static_cast<size_t>(room_id)]) {
          continue;
        }
        const auto& placement =
            connected_graph.room_positions[static_cast<size_t>(room_id)];
        if (!placement.placed) {
          continue;
        }

        const ImVec2 room_origin = GetConnectedRoomOrigin(
            placement.col, placement.row, connected_graph.min_col,
            connected_graph.min_row);
        const ImVec2 room_min = overview_screen_point(room_origin);
        const ImVec2 room_max(
            room_min.x + (kDungeonRoomPixelSize * overview_scale),
            room_min.y + (kDungeonRoomPixelSize * overview_scale));
        if (mouse.x >= room_min.x && mouse.x <= room_max.x &&
            mouse.y >= room_min.y && mouse.y <= room_max.y) {
          actions.hovered_room_id = room_id;
          break;
        }
      }
    }

    for (const auto& link : connected_graph.links) {
      const auto& from_placement =
          connected_graph
              .room_positions[static_cast<size_t>(link.from_room_id)];
      const auto& to_placement =
          connected_graph.room_positions[static_cast<size_t>(link.to_room_id)];
      if (!from_placement.placed || !to_placement.placed) {
        continue;
      }

      const auto [canvas_start, canvas_end] = GetConnectedLinkEndpoints(
          link, from_placement.col, from_placement.row, to_placement.col,
          to_placement.row, connected_graph.min_col, connected_graph.min_row);
      draw_list->AddLine(overview_screen_point(canvas_start),
                         overview_screen_point(canvas_end),
                         GetConnectedLinkColor(AgentUI::GetTheme(), link.type),
                         1.0f);
    }

    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      if (!connected_graph.room_mask[static_cast<size_t>(room_id)]) {
        continue;
      }
      const auto& placement =
          connected_graph.room_positions[static_cast<size_t>(room_id)];
      if (!placement.placed) {
        continue;
      }

      const ImVec2 room_origin = GetConnectedRoomOrigin(
          placement.col, placement.row, connected_graph.min_col,
          connected_graph.min_row);
      const ImVec2 room_min = overview_screen_point(room_origin);
      const ImVec2 room_max(
          room_min.x + (kDungeonRoomPixelSize * overview_scale),
          room_min.y + (kDungeonRoomPixelSize * overview_scale));
      const bool is_center = room_id == center_room_id;
      const bool is_highlighted =
          room_id == actions.hovered_room_id || room_id == highlighted_room_id;
      const bool is_unlinked = !placement.connected_to_start;
      const ImU32 fill =
          is_center ? ImGui::GetColorU32(theme.selection_primary)
          : is_highlighted
              ? ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.warning))
          : is_unlinked
              ? ImGui::GetColorU32(
                    WithAlpha(AgentUI::GetTheme().status_warning, 0.52f))
              : GetConnectedFloorColor(AgentUI::GetTheme(), connected_graph,
                                       room_id, 0.52f);
      const ImU32 outline = is_center || is_highlighted
                                ? IM_COL32(255, 255, 255, 255)
                                : IM_COL32(30, 38, 32, 220);
      draw_list->AddRectFilled(room_min, room_max, fill, 1.5f);
      draw_list->AddRect(room_min, room_max, outline, 1.5f);
    }

    auto draw_room_tag = [&](int room_id, ImU32 fill_color) {
      if (room_id < 0 || room_id >= zelda3::kNumberOfRooms ||
          !connected_graph.room_mask[static_cast<size_t>(room_id)]) {
        return;
      }

      const auto& placement =
          connected_graph.room_positions[static_cast<size_t>(room_id)];
      if (!placement.placed) {
        return;
      }

      const ImVec2 room_origin = GetConnectedRoomOrigin(
          placement.col, placement.row, connected_graph.min_col,
          connected_graph.min_row);
      const ImVec2 room_min = overview_screen_point(room_origin);

      char label[8];
      std::snprintf(label, sizeof(label), "%03X", room_id);
      const float font_size = std::max(9.0f, ImGui::GetFontSize() - 1.0f);
      const ImVec2 text_size = ImGui::GetFont()->CalcTextSizeA(
          font_size, std::numeric_limits<float>::max(), 0.0f, label);
      ImVec2 label_min(room_min.x + 2.0f, room_min.y + 2.0f);
      const ImVec2 label_size(text_size.x + 8.0f, text_size.y + 6.0f);

      if (label_min.x + label_size.x > map_max.x) {
        label_min.x = map_max.x - label_size.x;
      }
      if (label_min.y + label_size.y > map_max.y) {
        label_min.y = std::max(map_min.y, room_min.y - label_size.y - 2.0f);
      }
      label_min.x =
          std::clamp(label_min.x, map_min.x, map_max.x - label_size.x);
      label_min.y =
          std::clamp(label_min.y, map_min.y, map_max.y - label_size.y);

      const ImVec2 label_max(label_min.x + label_size.x,
                             label_min.y + label_size.y);
      draw_list->AddRectFilled(label_min, label_max, fill_color, 2.0f);
      draw_list->AddRect(label_min, label_max, IM_COL32(255, 255, 255, 235),
                         2.0f);
      draw_list->AddText(ImGui::GetFont(), font_size,
                         ImVec2(label_min.x + 4.0f, label_min.y + 3.0f),
                         IM_COL32(255, 255, 255, 255), label);
    };

    draw_room_tag(center_room_id, ImGui::GetColorU32(theme.selection_primary));
    const int tagged_hover_room = actions.hovered_room_id >= 0
                                      ? actions.hovered_room_id
                                      : highlighted_room_id;
    if (tagged_hover_room >= 0 && tagged_hover_room != center_room_id) {
      draw_room_tag(
          tagged_hover_room,
          ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.warning)));
    }

    const ImVec2 viewport_min(
        map_min.x + ((scroll_x / content_scale) * overview_scale),
        map_min.y + ((scroll_y / content_scale) * overview_scale));
    const ImVec2 viewport_rect_size(
        (viewport_size.x / content_scale) * overview_scale,
        (viewport_size.y / content_scale) * overview_scale);
    const ImVec2 viewport_rect_max(viewport_min.x + viewport_rect_size.x,
                                   viewport_min.y + viewport_rect_size.y);
    draw_list->AddRectFilled(viewport_min, viewport_rect_max,
                             IM_COL32(255, 255, 255, 20), 2.0f);
    draw_list->AddRect(viewport_min, viewport_rect_max,
                       IM_COL32(255, 255, 255, 220), 2.0f, 0, 1.5f);

    if (map_hovered) {
      actions.hovered = true;
      const bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
      if (actions.hovered_room_id >= 0 &&
          ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !dragging) {
        actions.activated_room_id = actions.hovered_room_id;
      } else if ((actions.hovered_room_id < 0 &&
                  ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
                 dragging) {
        const ImVec2 current_mouse = ImGui::GetMousePos();
        const float content_x =
            std::clamp((current_mouse.x - map_min.x) / overview_scale, 0.0f,
                       content_size.x);
        const float content_y =
            std::clamp((current_mouse.y - map_min.y) / overview_scale, 0.0f,
                       content_size.y);
        const float max_scroll_x =
            std::max(0.0f, (content_size.x * content_scale) - viewport_size.x);
        const float max_scroll_y =
            std::max(0.0f, (content_size.y * content_scale) - viewport_size.y);
        actions.target_scroll = ImVec2(
            std::clamp((content_x * content_scale) - (viewport_size.x * 0.5f),
                       0.0f, max_scroll_x),
            std::clamp((content_y * content_scale) - (viewport_size.y * 0.5f),
                       0.0f, max_scroll_y));
      }

      if (actions.hovered_room_id >= 0) {
        ImGui::SetTooltip(
            tr("[%03X] %s\nRelease to open room view"), actions.hovered_room_id,
            zelda3::GetRoomLabel(actions.hovered_room_id).c_str());
      }
    }
  }
  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
  return actions;
}

}  // namespace

ImVec2 DungeonCanvasViewer::GetConnectedContentSize(int center_room_id) {
  if (center_room_id < 0 || center_room_id >= zelda3::kNumberOfRooms ||
      !rooms_) {
    return ImVec2(0.0f, 0.0f);
  }
  if (connected_graph_cache_start_room_id_ != center_room_id) {
    connected_graph_cache_ = BuildConnectedRoomGraph(center_room_id);
    connected_graph_cache_start_room_id_ = center_room_id;
  }
  const auto& g = connected_graph_cache_;
  return GetConnectedCanvasSize(g.min_col, g.max_col, g.min_row, g.max_row);
}

float DungeonCanvasViewer::ConnectedCanvasScale() const {
  return std::clamp(connected_canvas_.global_scale(), kConnectedCanvasMinScale,
                    kConnectedCanvasMaxScale);
}

void DungeonCanvasViewer::DrawConnectedToolbarControls(int center_room_id) {
  if (center_room_id >= 0 && center_room_id < zelda3::kNumberOfRooms && rom_ &&
      rom_->is_loaded() && rooms_) {
    if (connected_graph_cache_start_room_id_ != center_room_id) {
      connected_graph_cache_ = BuildConnectedRoomGraph(center_room_id);
      connected_graph_cache_start_room_id_ = center_room_id;
    }
  }

  const int connected_room_count =
      connected_graph_cache_start_room_id_ == center_room_id
          ? connected_graph_cache_.room_count
          : 0;
  ImGui::TextDisabled(tr("%d room%s"), connected_room_count,
                      connected_room_count == 1 ? "" : "s");
  if (connected_graph_cache_start_room_id_ == center_room_id &&
      connected_graph_cache_.unlinked_room_count > 0) {
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(AgentUI::GetTheme().status_warning, tr("%d unlinked"),
                       connected_graph_cache_.unlinked_room_count);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Project rooms shown in this dungeon scope but not reachable from "
             "the current room through resolved ROM doors, staircases, or "
             "holewarps. They stay visible so broken connections are easier to "
             "repair."));
    }
  }
  if (connected_graph_cache_start_room_id_ == center_room_id &&
      !connected_graph_cache_.floor_order.empty()) {
    const auto& agent_theme = AgentUI::GetTheme();
    for (const std::string& floor_label : connected_graph_cache_.floor_order) {
      const int floor_count =
          CountConnectedRoomsOnFloor(connected_graph_cache_, floor_label);
      if (floor_count <= 0) {
        continue;
      }
      ImGui::SameLine(0.0f, 6.0f);
      ImGui::TextColored(GetConnectedFloorBaseColor(
                             agent_theme, connected_graph_cache_, floor_label),
                         "%s:%d", floor_label.c_str(), floor_count);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("%s rooms in this unified connected view"),
                          floor_label.c_str());
      }
    }
  }
  const size_t issue_count =
      connected_graph_cache_start_room_id_ == center_room_id
          ? connected_graph_cache_.staircase_issues.size()
          : 0;
  if (issue_count > 0) {
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(gui::ConvertColorToImVec4(
                           gui::ThemeManager::Get().GetCurrentTheme().warning),
                       tr("%zu issue%s"), issue_count,
                       issue_count == 1 ? "" : "s");
    if (ImGui::IsItemHovered()) {
      std::string tooltip(
          "Staircase configuration anomalies in this connected component:\n"
          "  Stale headers (no consuming object), missing destinations\n"
          "  (placed object but header is 0/invalid), and extras (objects\n"
          "  beyond the 4 header slots). Hover a room to localise.");
      for (const auto& issue : connected_graph_cache_.staircase_issues) {
        absl::StrAppend(
            &tooltip,
            absl::StrFormat("\n[%03X] %s", issue.from_room_id,
                            FormatDungeonStaircaseIssueDescription(issue)));
      }
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }
  const int auto_fixable_count =
      connected_graph_cache_start_room_id_ == center_room_id
          ? CountAutoFixableStaircaseIssues(
                connected_graph_cache_.staircase_issues)
          : 0;
  if (auto_fixable_count > 0) {
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::SmallButton(ICON_MD_CLEAR " Clear stale##ConnectedFixStale")) {
      ApplyConnectedStaircaseIssueAutoFixes(center_room_id);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr(
          "Clear stale non-zero staircase header slots that no placed "
          "interroom-stair object consumes. This marks room headers dirty but "
          "does not guess missing destinations or delete extra objects."));
    }
  }
  const size_t out_of_scope_count =
      connected_graph_cache_start_room_id_ == center_room_id
          ? connected_graph_cache_.out_of_scope_links.size()
          : 0;
  if (connected_graph_cache_.dungeon_scope_active && out_of_scope_count > 0) {
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(gui::ConvertColorToImVec4(
                           gui::ThemeManager::Get().GetCurrentTheme().info),
                       tr("%zu out-of-scope"), out_of_scope_count);
    if (ImGui::IsItemHovered()) {
      std::string tooltip(
          "Resolved links that leave the current dungeon group. Hidden from\n"
          "the visual matrix to keep the view scoped to one dungeon, but\n"
          "still surfaced here so you can see cross-dungeon connections.");
      for (const auto& link : connected_graph_cache_.out_of_scope_links) {
        absl::StrAppend(
            &tooltip,
            absl::StrFormat("\n[%03X] %s", link.from_room_id,
                            FormatDungeonConnectedLinkDescription(link)));
      }
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }
  if (!connected_action_status_message_.empty()) {
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(connected_action_status_is_error_
                           ? AgentUI::GetTheme().status_error
                           : AgentUI::GetTheme().status_success,
                       "%s", connected_action_status_message_.c_str());
  }
  ImGui::SameLine(0.0f, 8.0f);
  const auto compact_actions = DrawConnectedControlsCompact(
      ConnectedCanvasScale(), connected_room_count, &show_connected_overview_,
      &show_connected_current_room_preview_);
  if (compact_actions.fit) {
    connected_canvas_fit_requested_ = true;
  }
  if (compact_actions.reset) {
    connected_canvas_reset_requested_ = true;
  }
}

std::optional<int> DungeonCanvasViewer::DrawConnectedRoomMatrix(
    int center_room_id) {
  current_room_id_ = center_room_id;
  if (center_room_id < 0 || center_room_id >= zelda3::kNumberOfRooms) {
    ImGui::Text(tr("Invalid room ID: %d"), center_room_id);
    return std::nullopt;
  }
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text(tr("ROM not loaded"));
    return std::nullopt;
  }
  if (!rooms_) {
    ImGui::TextDisabled(tr("Room data unavailable"));
    return std::nullopt;
  }

  if (!connected_canvas_initialized_) {
    connected_canvas_.set_global_scale(kConnectedCanvasDefaultScale);
    // The connected-room matrix is a read-only graph view of neighbor rooms;
    // editing happens in the main canvas only. Declaring kPreviewOnly lets
    // future canvas surfaces (cursors, context-menu defaults) skip
    // edit-affordances on this surface without each call site repeating it.
    connected_canvas_.GetConfig().role = gui::CanvasRole::kPreviewOnly;
    connected_canvas_initialized_ = true;
  }
  connected_canvas_.set_global_scale(
      std::clamp(connected_canvas_.global_scale(), kConnectedCanvasMinScale,
                 kConnectedCanvasMaxScale));

  if (connected_graph_cache_start_room_id_ != center_room_id) {
    connected_graph_cache_ = BuildConnectedRoomGraph(center_room_id);
    connected_graph_cache_start_room_id_ = center_room_id;
  }
  const ConnectedRoomGraphData& connected_graph = connected_graph_cache_;
  const auto& connected_rooms = connected_graph.room_mask;
  const int connected_room_count = connected_graph.room_count;

  const ImVec2 content_size =
      GetConnectedCanvasSize(connected_graph.min_col, connected_graph.max_col,
                             connected_graph.min_row, connected_graph.max_row);

  gui::BeginNoPadding();
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = content_size;
  frame_opts.draw_grid = false;
  frame_opts.draw_context_menu = false;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;

  auto canvas_rt = gui::BeginCanvas(connected_canvas_, frame_opts);
  gui::EndNoPadding();

  HandleConnectedCanvasZoom(connected_canvas_, canvas_rt, content_size);
  canvas_rt.scale = connected_canvas_.global_scale();

  const ImVec2 window_pos = ImGui::GetWindowPos();
  const ImVec2 window_content_min = ImGui::GetWindowContentRegionMin();
  const ImVec2 window_content_max = ImGui::GetWindowContentRegionMax();
  const ImVec2 viewport_screen_min(window_pos.x + window_content_min.x,
                                   window_pos.y + window_content_min.y);
  const ImVec2 viewport_screen_max(window_pos.x + window_content_max.x,
                                   window_pos.y + window_content_max.y);
  const ImVec2 viewport_size(
      std::max(1.0f, window_content_max.x - window_content_min.x),
      std::max(1.0f, window_content_max.y - window_content_min.y));
  const bool reserve_connected_side_panel =
      show_connected_overview_ || show_connected_current_room_preview_;
  const float desired_side_panel_width =
      std::max(kConnectedOverviewMaxWidth + 20.0f,
               kConnectedCurrentRoomPreviewWidth + 20.0f);
  float connected_side_panel_width = 0.0f;
  if (reserve_connected_side_panel) {
    const float raw_side_width = std::min(
        desired_side_panel_width, std::max(0.0f, viewport_size.x - 320.0f));
    const float min_threshold = connected_side_panel_visible_last_frame_
                                    ? kConnectedSidePanelApproxMinWidth -
                                          kConnectedSidePanelHideHysteresis
                                    : kConnectedSidePanelApproxMinWidth;
    if (raw_side_width >= min_threshold) {
      connected_side_panel_width = raw_side_width;
    }
  }
  connected_side_panel_visible_last_frame_ = connected_side_panel_width > 0.0f;
  const float reserved_side_width =
      connected_side_panel_width > 0.0f
          ? connected_side_panel_width + kConnectedSidePanelGap
          : 0.0f;
  const ImVec2 map_viewport_size(
      std::max(1.0f, viewport_size.x - reserved_side_width), viewport_size.y);
  const ImVec2 map_viewport_screen_max(
      viewport_screen_min.x + map_viewport_size.x, viewport_screen_max.y);
  const ImVec2 side_panel_screen_min(
      map_viewport_screen_max.x + kConnectedSidePanelGap,
      viewport_screen_min.y);
  const ImVec2 side_panel_screen_max(viewport_screen_max.x,
                                     viewport_screen_max.y);
  const bool using_connected_side_panel = connected_side_panel_width > 0.0f;
  const ImVec2 mouse_pos = ImGui::GetMousePos();
  const bool mouse_in_map_viewport = mouse_pos.x >= viewport_screen_min.x &&
                                     mouse_pos.x <= map_viewport_screen_max.x &&
                                     mouse_pos.y >= viewport_screen_min.y &&
                                     mouse_pos.y <= viewport_screen_max.y;

  auto center_room_scroll = [&](float scale_value, int room_id) {
    const auto& placement =
        connected_graph.room_positions[static_cast<size_t>(room_id)];
    const ImVec2 room_origin = GetConnectedRoomOrigin(
        placement.col, placement.row, connected_graph.min_col,
        connected_graph.min_row);
    const float max_scroll_x =
        std::max(0.0f, (content_size.x * scale_value) - map_viewport_size.x);
    const float max_scroll_y =
        std::max(0.0f, (content_size.y * scale_value) - map_viewport_size.y);
    return ImVec2(
        std::clamp(
            ((room_origin.x + (kDungeonRoomPixelSize * 0.5f)) * scale_value) -
                (map_viewport_size.x * 0.5f),
            0.0f, max_scroll_x),
        std::clamp(
            ((room_origin.y + (kDungeonRoomPixelSize * 0.5f)) * scale_value) -
                (map_viewport_size.y * 0.5f),
            0.0f, max_scroll_y));
  };

  auto center_content_scroll = [&](float scale_value) {
    return ImVec2(
        std::max(0.0f,
                 ((content_size.x * scale_value) - map_viewport_size.x) * 0.5f),
        std::max(0.0f, ((content_size.y * scale_value) - map_viewport_size.y) *
                           0.5f));
  };

  auto set_scale_around_view_center = [&](float new_scale) {
    const float old_scale = connected_canvas_.global_scale();
    const float clamped_scale = std::clamp(new_scale, kConnectedCanvasMinScale,
                                           kConnectedCanvasMaxScale);
    if (std::abs(clamped_scale - old_scale) < 0.0001f) {
      return;
    }

    const float world_x =
        (ImGui::GetScrollX() + (map_viewport_size.x * 0.5f)) / old_scale;
    const float world_y =
        (ImGui::GetScrollY() + (map_viewport_size.y * 0.5f)) / old_scale;
    connected_canvas_.set_global_scale(clamped_scale);
    const float max_scroll_x =
        std::max(0.0f, (content_size.x * clamped_scale) - map_viewport_size.x);
    const float max_scroll_y =
        std::max(0.0f, (content_size.y * clamped_scale) - map_viewport_size.y);
    ImGui::SetScrollX(
        std::clamp((world_x * clamped_scale) - (map_viewport_size.x * 0.5f),
                   0.0f, max_scroll_x));
    ImGui::SetScrollY(
        std::clamp((world_y * clamped_scale) - (map_viewport_size.y * 0.5f),
                   0.0f, max_scroll_y));
    canvas_rt.scale = clamped_scale;
  };

  auto apply_fit = [&]() {
    const auto fit =
        gui::ComputeZoomToFit(content_size, map_viewport_size, 24.0f);
    const float fit_scale = std::clamp(fit.scale, kConnectedCanvasMinScale,
                                       kConnectedCanvasMaxScale);
    connected_canvas_.set_global_scale(fit_scale);
    const ImVec2 fit_scroll = center_content_scroll(fit_scale);
    ImGui::SetScrollX(fit_scroll.x);
    ImGui::SetScrollY(fit_scroll.y);
    canvas_rt.scale = fit_scale;
  };

  auto apply_reset = [&]() {
    connected_canvas_.set_global_scale(kConnectedCanvasDefaultScale);
    const ImVec2 reset_scroll =
        center_room_scroll(kConnectedCanvasDefaultScale, center_room_id);
    ImGui::SetScrollX(reset_scroll.x);
    ImGui::SetScrollY(reset_scroll.y);
    canvas_rt.scale = kConnectedCanvasDefaultScale;
    last_connected_matrix_room_id_ = -1;
  };

  const ImGuiIO& io = ImGui::GetIO();
  const bool allow_connected_shortcuts = ImGui::IsWindowFocused() &&
                                         !io.WantTextInput && !io.KeyCtrl &&
                                         !io.KeyAlt && !io.KeySuper;
  if (allow_connected_shortcuts) {
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
      apply_fit();
    } else if (ImGui::IsKeyPressed(ImGuiKey_0, false) ||
               ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) {
      apply_reset();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
               ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
      set_scale_around_view_center(connected_canvas_.global_scale() * 1.12f);
    } else if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
               ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
      set_scale_around_view_center(connected_canvas_.global_scale() / 1.12f);
    }
  }

  if (connected_canvas_fit_requested_) {
    apply_fit();
    connected_canvas_fit_requested_ = false;
  }
  if (connected_canvas_reset_requested_) {
    apply_reset();
    connected_canvas_reset_requested_ = false;
  }

  if (last_connected_matrix_room_id_ != center_room_id) {
    const ImVec2 target_scroll =
        center_room_scroll(canvas_rt.scale, center_room_id);
    ImGui::SetScrollX(target_scroll.x);
    ImGui::SetScrollY(target_scroll.y);
    last_connected_matrix_room_id_ = center_room_id;
  }

  const float scaled_step =
      (kDungeonRoomPixelSize + kConnectedRoomGap) * canvas_rt.scale;
  const float scroll_x = ImGui::GetScrollX();
  const float scroll_y = ImGui::GetScrollY();
  const int connected_cols =
      std::max(1, (connected_graph.max_col - connected_graph.min_col) + 1);
  const int connected_rows =
      std::max(1, (connected_graph.max_row - connected_graph.min_row) + 1);
  const int first_col =
      std::max(0, static_cast<int>(std::floor(scroll_x / scaled_step)) - 1);
  const int first_row =
      std::max(0, static_cast<int>(std::floor(scroll_y / scaled_step)) - 1);
  const int last_col = std::min(
      connected_cols - 1, static_cast<int>(std::ceil(
                              (scroll_x + map_viewport_size.x) / scaled_step)) +
                              1);
  const int last_row = std::min(
      connected_rows - 1, static_cast<int>(std::ceil(
                              (scroll_y + map_viewport_size.y) / scaled_step)) +
                              1);

  std::vector<int> visible_rooms;
  visible_rooms.reserve(static_cast<size_t>(connected_room_count));
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    if (!connected_rooms[static_cast<size_t>(room_id)]) {
      continue;
    }

    const auto& placement =
        connected_graph.room_positions[static_cast<size_t>(room_id)];
    if (!placement.placed) {
      continue;
    }

    const int local_col = placement.col - connected_graph.min_col;
    const int local_row = placement.row - connected_graph.min_row;
    if (local_col < first_col || local_col > last_col ||
        local_row < first_row || local_row > last_row) {
      continue;
    }

    visible_rooms.push_back(room_id);
    (void)PrepareRoomCompositeBitmap(room_id);
  }

  if (renderer_) {
    gfx::Arena::Get().ProcessTextureQueue(renderer_);
  }

  const auto& theme = AgentUI::GetTheme();
  const ImU32 outline_color = ImGui::GetColorU32(
      ImVec4(gui::GetOutlineVec4().x, gui::GetOutlineVec4().y,
             gui::GetOutlineVec4().z, 0.55f));
  const ImU32 active_outline = ImGui::GetColorU32(theme.selection_primary);
  const ImU32 hover_outline = ImGui::GetColorU32(theme.status_warning);
  const ImU32 label_bg = ImGui::GetColorU32(ImVec4(0.05f, 0.08f, 0.06f, 0.88f));
  const ImU32 placeholder_fill = ImGui::GetColorU32(
      ImVec4(theme.editor_background.x, theme.editor_background.y,
             theme.editor_background.z, 0.95f));
  const ImU32 link_shadow = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.32f));
  const float link_thickness =
      std::clamp(1.1f + (canvas_rt.scale * 3.4f), 1.35f, 3.0f);
  const float transport_node_radius =
      std::clamp(2.4f + (canvas_rt.scale * 7.0f), 3.0f, 6.0f);

  if (using_connected_side_panel) {
    canvas_rt.draw_list->AddRectFilled(side_panel_screen_min,
                                       side_panel_screen_max,
                                       IM_COL32(10, 14, 12, 232), 6.0f);
    canvas_rt.draw_list->AddLine(
        ImVec2(side_panel_screen_min.x - (kConnectedSidePanelGap * 0.5f),
               side_panel_screen_min.y),
        ImVec2(side_panel_screen_min.x - (kConnectedSidePanelGap * 0.5f),
               side_panel_screen_max.y),
        ImGui::GetColorU32(ImVec4(gui::GetOutlineVec4().x,
                                  gui::GetOutlineVec4().y,
                                  gui::GetOutlineVec4().z, 0.4f)),
        1.0f);
  }

  for (const auto& link : connected_graph.links) {
    const auto& from_placement =
        connected_graph.room_positions[static_cast<size_t>(link.from_room_id)];
    const auto& to_placement =
        connected_graph.room_positions[static_cast<size_t>(link.to_room_id)];
    if (!from_placement.placed || !to_placement.placed) {
      continue;
    }
    const auto [canvas_start, canvas_end] = GetConnectedLinkEndpoints(
        link, from_placement.col, from_placement.row, to_placement.col,
        to_placement.row, connected_graph.min_col, connected_graph.min_row);
    const ImVec2 screen_start =
        ToConnectedCanvasScreenPoint(canvas_rt, canvas_start);
    const ImVec2 screen_end =
        ToConnectedCanvasScreenPoint(canvas_rt, canvas_end);
    const ImU32 link_color = GetConnectedLinkColor(theme, link.type);
    canvas_rt.draw_list->AddLine(screen_start, screen_end, link_shadow,
                                 link_thickness + 2.0f);
    canvas_rt.draw_list->AddLine(screen_start, screen_end, link_color,
                                 link_thickness);
  }

  int hovered_room_id = -1;
  for (int room_id : visible_rooms) {
    const auto& placement =
        connected_graph.room_positions[static_cast<size_t>(room_id)];
    const ImVec2 room_origin = GetConnectedRoomOrigin(
        placement.col, placement.row, connected_graph.min_col,
        connected_graph.min_row);
    const ImVec2 screen_min(
        canvas_rt.canvas_p0.x + room_origin.x * canvas_rt.scale,
        canvas_rt.canvas_p0.y + room_origin.y * canvas_rt.scale);
    const ImVec2 screen_max(
        screen_min.x + (kDungeonRoomPixelSize * canvas_rt.scale),
        screen_min.y + (kDungeonRoomPixelSize * canvas_rt.scale));

    if (gfx::Bitmap* composite = PrepareRoomCompositeBitmap(room_id);
        composite && composite->texture()) {
      canvas_rt.draw_list->AddImage((ImTextureID)(intptr_t)composite->texture(),
                                    screen_min, screen_max, ImVec2(0, 0),
                                    ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
    } else {
      canvas_rt.draw_list->AddRectFilled(screen_min, screen_max,
                                         placeholder_fill, 6.0f);
    }

    if (!placement.connected_to_start) {
      canvas_rt.draw_list->AddRectFilled(
          screen_min, screen_max,
          ImGui::GetColorU32(WithAlpha(theme.status_warning, 0.12f)), 6.0f);
    }

    const std::string& floor_label =
        connected_graph.room_floor_labels[static_cast<size_t>(room_id)];
    if (!floor_label.empty()) {
      const float strip_width = std::clamp(6.0f * canvas_rt.scale, 3.0f, 8.0f);
      canvas_rt.draw_list->AddRectFilled(
          screen_min, ImVec2(screen_min.x + strip_width, screen_max.y),
          GetConnectedFloorColor(theme, connected_graph, room_id, 0.82f), 6.0f,
          ImDrawFlags_RoundCornersLeft);
    }

    const bool hovered =
        mouse_in_map_viewport &&
        ImGui::IsMouseHoveringRect(screen_min, screen_max, false) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (hovered) {
      hovered_room_id = room_id;
    }

    const ImU32 border_color = room_id == center_room_id ? active_outline
                               : hovered                 ? hover_outline
                                                         : outline_color;
    const float border_thickness = room_id == center_room_id
                                       ? kConnectedRoomHighlightThickness
                                       : kConnectedRoomOutlineThickness;
    canvas_rt.draw_list->AddRect(screen_min, screen_max, border_color, 6.0f, 0,
                                 border_thickness);

    char room_label[192];
    if (floor_label.empty()) {
      std::snprintf(
          room_label, sizeof(room_label), "[%03X] %s", room_id,
          dungeon_project_labels::GetRoomLabel(project_, room_id).c_str());
    } else {
      std::snprintf(
          room_label, sizeof(room_label), "[%03X] %s - %s", room_id,
          floor_label.c_str(),
          dungeon_project_labels::GetRoomLabel(project_, room_id).c_str());
    }
    const ImVec2 label_size = ImGui::CalcTextSize(room_label);
    const ImVec2 label_min(screen_min.x + kConnectedRoomLabelPadding,
                           screen_min.y + kConnectedRoomLabelPadding);
    const ImVec2 label_max(label_min.x + label_size.x + 12.0f,
                           label_min.y + kConnectedRoomLabelHeight);
    canvas_rt.draw_list->AddRectFilled(label_min, label_max, label_bg, 5.0f);
    canvas_rt.draw_list->AddText(ImVec2(label_min.x + 6.0f, label_min.y + 6.0f),
                                 ImGui::GetColorU32(theme.text_primary),
                                 room_label);
  }

  for (const auto& link : connected_graph.links) {
    if (link.type == DungeonConnectedLinkType::Door) {
      continue;
    }

    const auto& from_placement =
        connected_graph.room_positions[static_cast<size_t>(link.from_room_id)];
    const auto& to_placement =
        connected_graph.room_positions[static_cast<size_t>(link.to_room_id)];
    if (!from_placement.placed || !to_placement.placed) {
      continue;
    }

    const auto [canvas_start, canvas_end] = GetConnectedLinkEndpoints(
        link, from_placement.col, from_placement.row, to_placement.col,
        to_placement.row, connected_graph.min_col, connected_graph.min_row);
    const ImVec2 screen_start =
        ToConnectedCanvasScreenPoint(canvas_rt, canvas_start);
    const ImVec2 screen_end =
        ToConnectedCanvasScreenPoint(canvas_rt, canvas_end);
    const ImU32 link_color = GetConnectedLinkColor(theme, link.type);
    canvas_rt.draw_list->AddCircleFilled(screen_start, transport_node_radius,
                                         link_color, 12);
    canvas_rt.draw_list->AddCircleFilled(screen_end, transport_node_radius,
                                         link_color, 12);
    canvas_rt.draw_list->AddCircle(screen_start, transport_node_radius + 1.5f,
                                   link_shadow, 12, 1.25f);
    canvas_rt.draw_list->AddCircle(screen_end, transport_node_radius + 1.5f,
                                   link_shadow, 12, 1.25f);
  }

  ConnectedCanvasControlActions control_actions;
  if (!connected_controls_inline_) {
    const ImVec2 connected_controls_pos =
        using_connected_side_panel ? side_panel_screen_min
        : connected_controls_custom_position_
            ? connected_controls_custom_position_value_
            : ImVec2(viewport_screen_max.x - 12.0f,
                     viewport_screen_min.y + 12.0f);
    const ImVec2 connected_controls_pivot =
        using_connected_side_panel || connected_controls_custom_position_
            ? ImVec2(0.0f, 0.0f)
            : ImVec2(1.0f, 0.0f);
    control_actions = DrawConnectedCanvasControls(
        connected_controls_pos, connected_controls_pivot,
        connected_canvas_.global_scale(), connected_room_count,
        show_connected_overview_, show_connected_current_room_preview_);
    if (!using_connected_side_panel &&
        control_actions.moved_window_pos.has_value()) {
      connected_controls_custom_position_ = true;
      connected_controls_custom_position_value_ =
          *control_actions.moved_window_pos;
    }
    if (control_actions.reset_position) {
      connected_controls_custom_position_ = false;
      connected_controls_custom_position_value_ = ImVec2(0.0f, 0.0f);
    }
    show_connected_overview_ = control_actions.show_overview;
    show_connected_current_room_preview_ = control_actions.show_preview;
    if (control_actions.fit) {
      apply_fit();
    }
    if (control_actions.reset) {
      apply_reset();
    }
  }

  ConnectedCanvasOverviewActions overview_actions;
  if (show_connected_overview_) {
    overview_actions = DrawConnectedCanvasOverview(
        connected_graph, content_size, map_viewport_size,
        connected_canvas_.global_scale(), ImGui::GetScrollX(),
        ImGui::GetScrollY(), center_room_id, hovered_room_id,
        using_connected_side_panel
            ? ImVec2(side_panel_screen_min.x,
                     side_panel_screen_min.y + kConnectedControlsApproxHeight +
                         8.0f)
            : ImVec2(
                  viewport_screen_max.x - kConnectedOverviewMaxWidth - 12.0f,
                  viewport_screen_max.y - kConnectedOverviewMaxHeight - 12.0f));
  }
  if (overview_actions.target_scroll.has_value()) {
    ImGui::SetScrollX(overview_actions.target_scroll->x);
    ImGui::SetScrollY(overview_actions.target_scroll->y);
  }
  if (overview_actions.activated_room_id.has_value()) {
    if (*overview_actions.activated_room_id != center_room_id) {
      NavigateToRoom(*overview_actions.activated_room_id);
    }
    gui::EndCanvas(connected_canvas_, canvas_rt, frame_opts);
    return *overview_actions.activated_room_id;
  }

  if (!control_actions.hovered && !overview_actions.hovered &&
      hovered_room_id >= 0) {
    if (rooms_ && hovered_room_id < static_cast<int>(rooms_->size())) {
      std::string tooltip = absl::StrFormat(
          "[%03X] %s", hovered_room_id,
          dungeon_project_labels::GetRoomLabel(project_, hovered_room_id)
              .c_str());
      const auto& hovered_placement =
          connected_graph.room_positions[static_cast<size_t>(hovered_room_id)];
      const std::string& floor_label =
          connected_graph
              .room_floor_labels[static_cast<size_t>(hovered_room_id)];
      if (!floor_label.empty()) {
        absl::StrAppend(&tooltip, "\nFloor: ", floor_label);
      }
      if (!hovered_placement.connected_to_start) {
        absl::StrAppend(
            &tooltip,
            "\nUnlinked: no resolved ROM path from the current room.");
      }
      if (zelda3::Room* hovered_room =
              EnsureRoomLoadedForConnectedView(hovered_room_id)) {
        const auto diagnostics = CollectDungeonConnectedRoomLinkDiagnostics(
            hovered_room_id, *hovered_room,
            [this](int neighbor_room_id, zelda3::DoorDirection dir) {
              return RoomHasNonExitDoorInDirection(neighbor_room_id, dir);
            });
        if (!diagnostics.links.empty()) {
          absl::StrAppend(&tooltip, "\nConnections:");
          for (const auto& link : diagnostics.links) {
            absl::StrAppend(&tooltip, "\n  ",
                            FormatDungeonConnectedLinkDescription(link));
          }
        }
        if (!diagnostics.staircase_issues.empty()) {
          absl::StrAppend(&tooltip, "\nStaircase issues:");
          for (const auto& issue : diagnostics.staircase_issues) {
            absl::StrAppend(&tooltip, "\n  ",
                            FormatDungeonStaircaseIssueDescription(issue));
          }
        }
      }
      absl::StrAppend(&tooltip, "\nClick to open room view");
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      if (hovered_room_id != center_room_id) {
        NavigateToRoom(hovered_room_id);
      }
      gui::EndCanvas(connected_canvas_, canvas_rt, frame_opts);
      return hovered_room_id;
    }
  }

  if (show_connected_current_room_preview_) {
    if (gfx::Bitmap* current_room = PrepareRoomCompositeBitmap(center_room_id);
        current_room && current_room->texture()) {
      const float preview_scale =
          kConnectedCurrentRoomPreviewWidth /
          std::max(1.0f, static_cast<float>(current_room->width()));
      const ImVec2 preview_size(current_room->width() * preview_scale,
                                current_room->height() * preview_scale);
      const ImVec2 preview_pos(
          using_connected_side_panel ? side_panel_screen_min.x
                                     : viewport_screen_max.x - preview_size.x -
                                           kConnectedCurrentRoomPreviewPadding,
          using_connected_side_panel
              ? side_panel_screen_max.y - kConnectedPreviewApproxHeight
              : viewport_screen_min.y + kConnectedCurrentRoomPreviewPadding);
      gui::DrawCanvasHUD(
          kConnectedCurrentRoomHudId, preview_pos, ImVec2(0, 0), [&]() {
            ImGui::Text(tr("Current Room"));
            ImGui::TextDisabled(
                "[%03X] %s", center_room_id,
                dungeon_project_labels::GetRoomLabel(project_, center_room_id)
                    .c_str());
            ImGui::Image((ImTextureID)(intptr_t)current_room->texture(),
                         preview_size);
          });
    }
  }

  const bool side_panel_hovered = using_connected_side_panel &&
                                  mouse_pos.x >= side_panel_screen_min.x &&
                                  mouse_pos.x <= side_panel_screen_max.x &&
                                  mouse_pos.y >= side_panel_screen_min.y &&
                                  mouse_pos.y <= side_panel_screen_max.y;
  HandleConnectedCanvasPan((control_actions.hovered ||
                            overview_actions.hovered || side_panel_hovered)
                               ? center_room_id
                               : hovered_room_id);

  gui::EndCanvas(connected_canvas_, canvas_rt, frame_opts);
  return std::nullopt;
}

}  // namespace yaze::editor
