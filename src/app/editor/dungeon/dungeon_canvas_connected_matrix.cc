#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
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
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

using namespace dungeon_ui;  // NOLINT(google-build-using-namespace)

constexpr char kConnectedCurrentRoomHudId[] =
    "##DungeonConnectedCurrentRoomHUD";
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

// The graph sits this far inside the overview's scroll content. Half the
// window on each side lets any room reach the view center and lets graphs
// smaller than the window be centered.
ImVec2 ConnectedPanMargin(const ImVec2& window_size) {
  return ImVec2(std::floor(window_size.x * 0.5f),
                std::floor(window_size.y * 0.5f));
}

// Scroll that places canvas point `world` at `view_point`, measured from the
// visible viewport's top-left.
ImVec2 ConnectedScrollFor(const ImVec2& world, float scale,
                          const ImVec2& margin, const ImVec2& view_point) {
  return ImVec2(margin.x + (world.x * scale) - view_point.x,
                margin.y + (world.y * scale) - view_point.y);
}

// Clamps against the extent the host reserves for `scale`, which may not be
// applied to the window's own scroll limits until the next frame.
void SetConnectedScroll(const ImVec2& scroll, const ImVec2& content_size,
                        float scale, const ImVec2& margin,
                        const ImVec2& window_size) {
  const float max_x = std::max(
      0.0f, (content_size.x * scale) + (margin.x * 2.0f) - window_size.x);
  const float max_y = std::max(
      0.0f, (content_size.y * scale) + (margin.y * 2.0f) - window_size.y);
  ImGui::SetScrollX(std::clamp(scroll.x, 0.0f, max_x));
  ImGui::SetScrollY(std::clamp(scroll.y, 0.0f, max_y));
}

void HandleConnectedCanvasZoom(gui::Canvas& canvas, const ImVec2& content_size,
                               const ImVec2& margin, const ImVec2& view_min,
                               const ImVec2& window_size) {
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
  const ImVec2 view_point(mouse_pos.x - view_min.x, mouse_pos.y - view_min.y);
  const ImVec2 world(
      (ImGui::GetScrollX() + view_point.x - margin.x) / old_scale,
      (ImGui::GetScrollY() + view_point.y - margin.y) / old_scale);
  canvas.set_global_scale(new_scale);
  SetConnectedScroll(ConnectedScrollFor(world, new_scale, margin, view_point),
                     content_size, new_scale, margin, window_size);
}

void HandleConnectedCanvasPan(bool allow_hover_pan, bool left_pan_active) {
  const bool hovered =
      allow_hover_pan &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  const bool drag_middle =
      hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
  const bool drag_right =
      hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right);
  const bool drag_left =
      left_pan_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
  if (!drag_middle && !drag_right && !drag_left) {
    return;
  }

  const ImVec2 delta = ImGui::GetIO().MouseDelta;
  ImGui::SetScrollX(
      std::clamp(ImGui::GetScrollX() - delta.x, 0.0f, ImGui::GetScrollMaxX()));
  ImGui::SetScrollY(
      std::clamp(ImGui::GetScrollY() - delta.y, 0.0f, ImGui::GetScrollMaxY()));
}

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

// Keeps short inline items on the current row while they fit.
void SameLineIfFits(float next_width) {
  const float row_end = ImGui::GetItemRectMax().x;
  const float limit =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  if (row_end + ImGui::GetStyle().ItemSpacing.x + next_width <= limit) {
    ImGui::SameLine();
  }
}

struct ConnectedCanvasOverviewActions {
  bool hovered = false;
  // Canvas-space point the minimap asked to center in the view.
  std::optional<ImVec2> center_on;
  std::optional<int> activated_room_id;
  int hovered_room_id = -1;
};

ConnectedCanvasOverviewActions DrawConnectedCanvasOverview(
    const DungeonCanvasViewer::ConnectedRoomGraphData& connected_graph,
    const ImVec2& content_size, const ImVec2& view_world_min,
    const ImVec2& view_world_size, int center_room_id, int highlighted_room_id,
    const ImVec2& window_pos) {
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

  ImVec4 overview_bg = gui::ConvertColorToImVec4(theme.editor_background);
  overview_bg.w = 0.78f;
  const gui::StyleColorGuard overview_colors({
      {ImGuiCol_WindowBg, overview_bg},
      {ImGuiCol_Border, gui::ConvertColorToImVec4(theme.selection_primary)},
  });
  const gui::StyleVarGuard overview_vars({
      {ImGuiStyleVar_WindowRounding, 4.0f},
      {ImGuiStyleVar_WindowBorderSize, 1.0f},
      {ImGuiStyleVar_WindowPadding, ImVec2(inner_padding, inner_padding)},
  });

  if (ImGui::Begin(kConnectedCanvasOverviewId, nullptr, flags)) {
    actions.hovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGui::TextDisabled(tr("Overview"));
    ImGui::InvisibleButton("##ConnectedOverviewMap", map_size,
                           ImGuiButtonFlags_MouseButtonLeft);

    const ImVec2 map_min = ImGui::GetItemRectMin();
    const ImVec2 map_max = ImGui::GetItemRectMax();
    auto* draw_list = ImGui::GetWindowDrawList();

    ImVec4 map_fill = gui::ConvertColorToImVec4(theme.editor_background);
    map_fill.w = 230.0f / 255.0f;
    draw_list->AddRectFilled(map_min, map_max, ImGui::GetColorU32(map_fill),
                             4.0f);
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
      ImVec4 dim_outline = gui::ConvertColorToImVec4(theme.editor_background);
      dim_outline.w = 220.0f / 255.0f;
      const ImU32 outline = is_center || is_highlighted
                                ? ImGui::GetColorU32(gui::ConvertColorToImVec4(
                                      theme.text_primary))
                                : ImGui::GetColorU32(dim_outline);
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

    // The view may extend past the graph into the pan margin.
    const ImVec2 viewport_min = overview_screen_point(view_world_min);
    const ImVec2 viewport_rect_max =
        overview_screen_point(ImVec2(view_world_min.x + view_world_size.x,
                                     view_world_min.y + view_world_size.y));
    draw_list->PushClipRect(map_min, map_max, true);
    draw_list->AddRectFilled(viewport_min, viewport_rect_max,
                             IM_COL32(255, 255, 255, 20), 2.0f);
    draw_list->AddRect(viewport_min, viewport_rect_max,
                       IM_COL32(255, 255, 255, 220), 2.0f, 0, 1.5f);
    draw_list->PopClipRect();

    if (map_hovered) {
      actions.hovered = true;
      if (actions.hovered_room_id >= 0 &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        actions.activated_room_id = actions.hovered_room_id;
      } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                 ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 current_mouse = ImGui::GetMousePos();
        actions.center_on =
            ImVec2(std::clamp((current_mouse.x - map_min.x) / overview_scale,
                              0.0f, content_size.x),
                   std::clamp((current_mouse.y - map_min.y) / overview_scale,
                              0.0f, content_size.y));
      }

      if (actions.hovered_room_id >= 0) {
        ImGui::SetTooltip(
            tr("[%03X] %s\nDouble-click to open room view"),
            actions.hovered_room_id,
            zelda3::GetRoomLabel(actions.hovered_room_id).c_str());
      }
    }
  }
  ImGui::End();

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

ImVec2 DungeonCanvasViewer::GetConnectedScrollContentSize(int center_room_id,
                                                          ImVec2 window_size) {
  const ImVec2 content = GetConnectedContentSize(center_room_id);
  const float scale = ConnectedCanvasScale();
  const ImVec2 margin = ConnectedPanMargin(window_size);
  return ImVec2((content.x * scale) + (margin.x * 2.0f),
                (content.y * scale) + (margin.y * 2.0f));
}

void DungeonCanvasViewer::DrawConnectedInspectorControls(
    int center_room_id, const std::function<void(int)>& on_jump_to_room) {
  if (center_room_id >= 0 && center_room_id < zelda3::kNumberOfRooms && rom_ &&
      rom_->is_loaded() && rooms_ &&
      connected_graph_cache_start_room_id_ != center_room_id) {
    connected_graph_cache_ = BuildConnectedRoomGraph(center_room_id);
    connected_graph_cache_start_room_id_ = center_room_id;
  }
  const bool have_graph =
      connected_graph_cache_start_room_id_ == center_room_id;
  const ConnectedRoomGraphData& graph = connected_graph_cache_;
  const auto& agent_theme = AgentUI::GetTheme();
  gui::AutoWidgetScope automation_scope("Dungeon/Overview");
  ImGui::PushID("ConnectedOverviewControls");

  // Dungeon jump.
  const std::string current_dungeon =
      dungeon_project_labels::GetDungeonNameForRoom(project_, center_room_id);
  ImGui::SetNextItemWidth(-1);
  const bool jump_open =
      ImGui::BeginCombo("##OverviewDungeonJump",
                        current_dungeon.empty() ? tr("Jump to dungeon...")
                                                : current_dungeon.c_str(),
                        ImGuiComboFlags_HeightLarge);
  gui::AutoRegisterLastItem("combo", "OverviewDungeonJump",
                            "Jump to a dungeon by name");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Open a dungeon's entrance room by name"));
  }
  if (jump_open) {
    if (ImGui::IsWindowAppearing()) {
      dungeon_jump_targets_ = BuildDungeonJumpTargets(rom_, project_);
      dungeon_jump_filter_[0] = '\0';
      ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##OverviewDungeonFilter", tr("Filter dungeons"),
                             dungeon_jump_filter_,
                             sizeof(dungeon_jump_filter_));
    std::string filter = dungeon_jump_filter_;
    std::transform(filter.begin(), filter.end(), filter.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    bool any_shown = false;
    for (size_t i = 0; i < dungeon_jump_targets_.size(); ++i) {
      const auto& target = dungeon_jump_targets_[i];
      std::string lower = target.name;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (lower.find(filter) == std::string::npos) {
        continue;
      }
      any_shown = true;
      ImGui::PushID(static_cast<int>(i));
      const std::string label =
          absl::StrFormat("%s  [%03X]", target.name, target.room_id);
      if (ImGui::Selectable(label.c_str(), target.name == current_dungeon) &&
          on_jump_to_room) {
        on_jump_to_room(target.room_id);
      }
      gui::AutoRegisterLastItem("selectable", target.name);
      ImGui::PopID();
    }
    if (!any_shown) {
      ImGui::TextDisabled("%s", dungeon_jump_targets_.empty()
                                    ? tr("No dungeons found in this ROM")
                                    : tr("No matching dungeons"));
    }
    ImGui::EndCombo();
  }

  // Zoom.
  ImGui::AlignTextToFramePadding();
  ImGui::Text(tr("Zoom %.0f%%"), ConnectedCanvasScale() * 100.0f);
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_ZOOM_OUT "##OverviewZoomOut")) {
    connected_canvas_zoom_request_ /= 1.12f;
  }
  gui::AutoRegisterLastItem("button", "OverviewZoomOut", "Zoom out (-)");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_ZOOM_IN "##OverviewZoomIn")) {
    connected_canvas_zoom_request_ *= 1.12f;
  }
  gui::AutoRegisterLastItem("button", "OverviewZoomIn", "Zoom in (+)");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_FIT_SCREEN " Fit##OverviewFit")) {
    connected_canvas_fit_requested_ = true;
  }
  gui::AutoRegisterLastItem("button", "OverviewFit",
                            "Fit all rooms in view (F)");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_RESTART_ALT "##OverviewReset")) {
    connected_canvas_reset_requested_ = true;
  }
  gui::AutoRegisterLastItem("button", "OverviewReset",
                            "Reset zoom and center the current room (0)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Reset zoom and center the current room (0)"));
  }

  ImGui::Checkbox(tr("Minimap##OverviewMinimap"), &show_connected_overview_);
  gui::AutoRegisterLastItem("checkbox", "OverviewMinimap");
  SameLineIfFits(ImGui::CalcTextSize(tr("Current room")).x +
                 ImGui::GetFrameHeight() * 2.0f);
  ImGui::Checkbox(tr("Current room##OverviewRoomPreview"),
                  &show_connected_current_room_preview_);
  gui::AutoRegisterLastItem("checkbox", "OverviewRoomPreview");

  DrawConnectedControlsLegend();

  // Graph summary.
  const int room_count = have_graph ? graph.room_count : 0;
  ImGui::TextDisabled(tr("%d room%s"), room_count, room_count == 1 ? "" : "s");
  if (have_graph && graph.unlinked_room_count > 0) {
    const std::string unlinked =
        absl::StrCat(graph.unlinked_room_count, " ", tr("unlinked"));
    SameLineIfFits(ImGui::CalcTextSize(unlinked.c_str()).x);
    ImGui::TextColored(agent_theme.status_warning, "%s", unlinked.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "%s",
          tr("Project rooms shown in this dungeon scope but not reachable from "
             "the current room through resolved ROM doors, staircases, or "
             "holewarps. They stay visible so broken connections are easier to "
             "repair."));
    }
  }
  if (have_graph) {
    for (const std::string& floor_label : graph.floor_order) {
      const int floor_count = CountConnectedRoomsOnFloor(graph, floor_label);
      if (floor_count <= 0) {
        continue;
      }
      const std::string text =
          absl::StrFormat("%s:%d", floor_label, floor_count);
      SameLineIfFits(ImGui::CalcTextSize(text.c_str()).x);
      ImGui::TextColored(
          GetConnectedFloorBaseColor(agent_theme, graph, floor_label), "%s",
          text.c_str());
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("%s rooms in this unified connected view"),
                          floor_label.c_str());
      }
    }
  }
  const size_t issue_count = have_graph ? graph.staircase_issues.size() : 0;
  if (issue_count > 0) {
    ImGui::TextColored(gui::ConvertColorToImVec4(
                           gui::ThemeManager::Get().GetCurrentTheme().warning),
                       tr("%zu possible issue%s"), issue_count,
                       issue_count == 1 ? "" : "s");
    if (ImGui::IsItemHovered()) {
      std::string tooltip(
          "Provisional staircase diagnostics based on placement order.\n"
          "These do not establish runtime slot use. Room 000 is valid.\n"
          "Review header destinations manually; automatic clearing is "
          "disabled.");
      for (const auto& issue : graph.staircase_issues) {
        absl::StrAppend(
            &tooltip,
            absl::StrFormat("\n[%03X] %s", issue.from_room_id,
                            FormatDungeonStaircaseIssueDescription(issue)));
      }
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }
  const size_t out_of_scope_count =
      have_graph ? graph.out_of_scope_links.size() : 0;
  if (have_graph && graph.dungeon_scope_active && out_of_scope_count > 0) {
    ImGui::TextColored(gui::ConvertColorToImVec4(
                           gui::ThemeManager::Get().GetCurrentTheme().info),
                       tr("%zu out-of-scope"), out_of_scope_count);
    if (ImGui::IsItemHovered()) {
      std::string tooltip(
          "Resolved links that leave the current dungeon group. Hidden from\n"
          "the visual matrix to keep the view scoped to one dungeon, but\n"
          "still surfaced here so you can see cross-dungeon connections.");
      for (const auto& link : graph.out_of_scope_links) {
        absl::StrAppend(
            &tooltip,
            absl::StrFormat("\n[%03X] %s", link.from_room_id,
                            FormatDungeonConnectedLinkDescription(link)));
      }
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }
  ImGui::TextDisabled("%s", tr("Stair mapping unverified"));
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s",
        tr("Staircase links and unused-slot notices are placement-order "
           "estimates. "
           "The game uses staircase-family counters and collision indices. "
           "Automatic clearing is disabled; review Destinations in the room "
           "inspector."));
  }
  if (!connected_action_status_message_.empty()) {
    ImGui::TextColored(connected_action_status_is_error_
                           ? agent_theme.status_error
                           : agent_theme.status_success,
                       "%s", connected_action_status_message_.c_str());
  }
  {
    gui::StyleColorGuard hint_color(
        ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(
        "%s", tr("Double-click a room to open it. Drag to pan; scroll or "
                 "+/- to zoom. F fits all rooms; 0 recenters."));
  }
  ImGui::PopID();
}

std::optional<int> DungeonCanvasViewer::DrawConnectedRoomMatrix(
    int center_room_id) {
  // This is a read-only overview. A release here must not complete a gesture
  // that began in the editable room canvas when the user returns to that view.
  CancelMinecartGesture();
  crumble_floor_view_.Stop();
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
  PruneConnectedRoomCompositeOutputs();

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

  const ImVec2 window_size = ImGui::GetWindowSize();
  const ImVec2 pan_margin = ConnectedPanMargin(window_size);
  // The visible area, not the content region: with the host's explicit
  // (overscrolled) content size, the content region spans the whole map.
  const ImGuiWindow* host_window = ImGui::GetCurrentWindow();
  const ImVec2 viewport_screen_min(
      host_window->InnerRect.Min.x + host_window->WindowPadding.x,
      host_window->InnerRect.Min.y + host_window->WindowPadding.y);
  const ImVec2 viewport_screen_max(
      std::max(viewport_screen_min.x + 1.0f,
               host_window->InnerRect.Max.x - host_window->WindowPadding.x),
      std::max(viewport_screen_min.y + 1.0f,
               host_window->InnerRect.Max.y - host_window->WindowPadding.y));
  const ImVec2 viewport_size(viewport_screen_max.x - viewport_screen_min.x,
                             viewport_screen_max.y - viewport_screen_min.y);
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

  // The graph starts `pan_margin` into the scroll content.
  ImGui::SetCursorPos(pan_margin);
  gui::BeginNoPadding();
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = content_size;
  frame_opts.draw_grid = false;
  frame_opts.draw_context_menu = false;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;

  auto canvas_rt = gui::BeginCanvas(connected_canvas_, frame_opts);
  gui::EndNoPadding();

  HandleConnectedCanvasZoom(connected_canvas_, content_size, pan_margin,
                            viewport_screen_min, window_size);
  canvas_rt.scale = connected_canvas_.global_scale();

  const ImVec2 map_view_center(map_viewport_size.x * 0.5f,
                               map_viewport_size.y * 0.5f);
  auto center_on = [&](const ImVec2& world, float scale_value) {
    SetConnectedScroll(
        ConnectedScrollFor(world, scale_value, pan_margin, map_view_center),
        content_size, scale_value, pan_margin, window_size);
  };
  auto room_center = [&](int room_id) {
    const auto& placement =
        connected_graph.room_positions[static_cast<size_t>(room_id)];
    const ImVec2 room_origin = GetConnectedRoomOrigin(
        placement.col, placement.row, connected_graph.min_col,
        connected_graph.min_row);
    return ImVec2(room_origin.x + (kDungeonRoomPixelSize * 0.5f),
                  room_origin.y + (kDungeonRoomPixelSize * 0.5f));
  };

  auto set_scale_around_view_center = [&](float new_scale) {
    const float old_scale = connected_canvas_.global_scale();
    const float clamped_scale = std::clamp(new_scale, kConnectedCanvasMinScale,
                                           kConnectedCanvasMaxScale);
    if (std::abs(clamped_scale - old_scale) < 0.0001f) {
      return;
    }
    const ImVec2 world(
        (ImGui::GetScrollX() + map_view_center.x - pan_margin.x) / old_scale,
        (ImGui::GetScrollY() + map_view_center.y - pan_margin.y) / old_scale);
    connected_canvas_.set_global_scale(clamped_scale);
    center_on(world, clamped_scale);
    canvas_rt.scale = clamped_scale;
  };

  auto apply_fit = [&]() {
    const auto fit =
        gui::ComputeZoomToFit(content_size, map_viewport_size, 24.0f);
    const float fit_scale = std::clamp(fit.scale, kConnectedCanvasMinScale,
                                       kConnectedCanvasMaxScale);
    connected_canvas_.set_global_scale(fit_scale);
    center_on(ImVec2(content_size.x * 0.5f, content_size.y * 0.5f), fit_scale);
    canvas_rt.scale = fit_scale;
  };

  auto apply_reset = [&]() {
    connected_canvas_.set_global_scale(kConnectedCanvasDefaultScale);
    center_on(room_center(center_room_id), kConnectedCanvasDefaultScale);
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
  if (std::abs(connected_canvas_zoom_request_ - 1.0f) > 0.0001f) {
    set_scale_around_view_center(connected_canvas_.global_scale() *
                                 connected_canvas_zoom_request_);
    connected_canvas_zoom_request_ = 1.0f;
  }

  // A window's first frame reports a placeholder content region; center once
  // the real viewport is known.
  if (last_connected_matrix_room_id_ != center_room_id &&
      !ImGui::IsWindowAppearing()) {
    center_on(room_center(center_room_id), canvas_rt.scale);
    last_connected_matrix_room_id_ = center_room_id;
  }

  const float scaled_step =
      (kDungeonRoomPixelSize + kConnectedRoomGap) * canvas_rt.scale;
  // Scroll measured from the graph origin; negative inside the pan margin.
  const float scroll_x = ImGui::GetScrollX() - pan_margin.x;
  const float scroll_y = ImGui::GetScrollY() - pan_margin.y;
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
    (void)PrepareConnectedRoomCompositeBitmap(room_id);
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

    if (gfx::Bitmap* composite = PrepareConnectedRoomCompositeBitmap(room_id);
        composite && composite->texture()) {
      canvas_rt.draw_list->AddImage(
          (ImTextureID)(intptr_t)composite->texture(), screen_min, screen_max,
          ImVec2(0, 0), ImVec2(1, 1),
          ImGui::GetColorU32(IM_COL32(255, 255, 255, 255)));
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

  ConnectedCanvasOverviewActions overview_actions;
  if (show_connected_overview_) {
    const float scale = connected_canvas_.global_scale();
    overview_actions = DrawConnectedCanvasOverview(
        connected_graph, content_size,
        ImVec2((ImGui::GetScrollX() - pan_margin.x) / scale,
               (ImGui::GetScrollY() - pan_margin.y) / scale),
        ImVec2(map_viewport_size.x / scale, map_viewport_size.y / scale),
        center_room_id, hovered_room_id,
        using_connected_side_panel
            ? side_panel_screen_min
            : ImVec2(
                  viewport_screen_max.x - kConnectedOverviewMaxWidth - 12.0f,
                  viewport_screen_max.y - kConnectedOverviewMaxHeight - 12.0f));
  }
  if (overview_actions.center_on.has_value()) {
    center_on(*overview_actions.center_on, connected_canvas_.global_scale());
  }
  if (overview_actions.activated_room_id.has_value()) {
    if (*overview_actions.activated_room_id != center_room_id) {
      NavigateToRoom(*overview_actions.activated_room_id);
    }
    gui::EndCanvas(connected_canvas_, canvas_rt, frame_opts);
    return *overview_actions.activated_room_id;
  }

  const bool panning = connected_left_pan_active_ &&
                       ImGui::IsMouseDragging(ImGuiMouseButton_Left);
  if (!overview_actions.hovered && !panning && hovered_room_id >= 0) {
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
      if (const auto it =
              connected_graph.room_diagnostics.find(hovered_room_id);
          it != connected_graph.room_diagnostics.end()) {
        const auto& diagnostics = it->second;
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
      absl::StrAppend(&tooltip, "\nDouble-click to open room view");
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      if (hovered_room_id != center_room_id) {
        NavigateToRoom(hovered_room_id);
      }
      gui::EndCanvas(connected_canvas_, canvas_rt, frame_opts);
      return hovered_room_id;
    }
  }

  if (show_connected_current_room_preview_) {
    if (gfx::Bitmap* current_room =
            PrepareConnectedRoomCompositeBitmap(center_room_id);
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
  // Rooms open on double-click, so a left drag pans from anywhere on the map.
  const bool map_hovered =
      mouse_in_map_viewport && !overview_actions.hovered &&
      !side_panel_hovered &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    connected_left_pan_active_ = map_hovered;
  } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    connected_left_pan_active_ = false;
  }
  HandleConnectedCanvasPan(map_hovered, connected_left_pan_active_);

  gui::EndCanvas(connected_canvas_, canvas_rt, frame_opts);
  return std::nullopt;
}

}  // namespace yaze::editor
