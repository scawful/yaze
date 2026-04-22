#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/ui/reporting/dungeon_issue_report_storage.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/animation/animator.h"
#include "app/gui/canvas/canvas_menu.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#ifdef YAZE_WITH_GRPC
#include "app/service/screenshot_utils.h"
#endif
#include "app/editor/dungeon/ui_constants.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_coordinates.h"
#include "editor/dungeon/object_selection.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/log.h"
#include "util/macro.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/editor_dungeon_state.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {

namespace {

// Layout constants — shared with other dungeon TUs via ui_constants.h. Pulled
// into this anonymous namespace via a using-directive so call sites read as
// `kDungeonRoomPixelSize` rather than the fully-qualified form.
using namespace dungeon_ui;  // NOLINT(google-build-using-namespace)

constexpr char kConnectedCurrentRoomHudId[] =
    "##DungeonConnectedCurrentRoomHUD";
constexpr char kConnectedCanvasControlsId[] =
    "##DungeonConnectedCanvasControls";
constexpr char kConnectedCanvasOverviewId[] =
    "##DungeonConnectedCanvasOverview";

bool IsExitDoorType(zelda3::DoorType type) {
  switch (type) {
    case zelda3::DoorType::FancyDungeonExit:
    case zelda3::DoorType::FancyDungeonExitLower:
    case zelda3::DoorType::CaveExit:
    case zelda3::DoorType::LitCaveExitLower:
    case zelda3::DoorType::ExitLower:
    case zelda3::DoorType::UnusedCaveExit:
    case zelda3::DoorType::BombableCaveExit:
    case zelda3::DoorType::WaterfallDoor:
    case zelda3::DoorType::ExitMarker:
      return true;
    default:
      return false;
  }
}

std::pair<int, int> ConnectedDoorDelta(zelda3::DoorDirection dir) {
  switch (dir) {
    case zelda3::DoorDirection::North:
      return {0, -1};
    case zelda3::DoorDirection::South:
      return {0, 1};
    case zelda3::DoorDirection::West:
      return {-1, 0};
    case zelda3::DoorDirection::East:
      return {1, 0};
  }
  return {0, 0};
}

bool IsConnectedSlotOccupied(
    const std::map<std::pair<int, int>, int>& occupied_slots, int col,
    int row) {
  return occupied_slots.contains({col, row});
}

std::pair<int, int> FindConnectedDoorPlacement(
    const std::map<std::pair<int, int>, int>& occupied_slots, int source_col,
    int source_row, zelda3::DoorDirection dir) {
  const auto [dx, dy] = ConnectedDoorDelta(dir);
  int best_score = std::numeric_limits<int>::max();
  std::pair<int, int> best = {source_col + dx, source_row + dy};

  for (int radius = 1; radius <= 24; ++radius) {
    for (int row = source_row - radius; row <= source_row + radius; ++row) {
      for (int col = source_col - radius; col <= source_col + radius; ++col) {
        if (IsConnectedSlotOccupied(occupied_slots, col, row)) {
          continue;
        }

        const int rel_col = col - source_col;
        const int rel_row = row - source_row;
        const int forward = (rel_col * dx) + (rel_row * dy);
        if (forward <= 0) {
          continue;
        }

        const int lateral = std::abs((rel_col * dy) - (rel_row * dx));
        const int distance_from_ideal =
            std::abs(rel_col - dx) + std::abs(rel_row - dy);
        const int overshoot = std::max(0, forward - 1);
        const int score =
            (distance_from_ideal * 16) + (lateral * 20) + (overshoot * 6);
        if (score < best_score) {
          best_score = score;
          best = {col, row};
        }
      }
    }
    if (best_score != std::numeric_limits<int>::max()) {
      return best;
    }
  }

  return best;
}

std::pair<int, int> FindConnectedTransportPlacement(
    const std::map<std::pair<int, int>, int>& occupied_slots, int source_col,
    int source_row) {
  int best_score = std::numeric_limits<int>::max();
  std::pair<int, int> best = {source_col + 1, source_row + 1};

  for (int radius = 1; radius <= 24; ++radius) {
    for (int row = source_row - radius; row <= source_row + radius; ++row) {
      for (int col = source_col - radius; col <= source_col + radius; ++col) {
        if (IsConnectedSlotOccupied(occupied_slots, col, row)) {
          continue;
        }

        const int rel_col = col - source_col;
        const int rel_row = row - source_row;
        const int distance = std::abs(rel_col) + std::abs(rel_row);
        if (distance == 0) {
          continue;
        }

        const bool diagonal = rel_col != 0 && rel_row != 0;
        const bool axis_aligned = rel_col == 0 || rel_row == 0;
        const int distance_bias = std::abs(distance - 2);
        const int score = (distance_bias * 10) +
                          (diagonal       ? 0
                           : axis_aligned ? 8
                                          : 4) +
                          ((std::abs(rel_col) + std::abs(rel_row)) > 3 ? 4 : 0);
        if (score < best_score) {
          best_score = score;
          best = {col, row};
        }
      }
    }
    if (best_score != std::numeric_limits<int>::max()) {
      return best;
    }
  }

  return best;
}

ImVec2 GetConnectedRoomOrigin(int grid_col, int grid_row, int min_col,
                              int min_row);

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

std::tuple<int, int, DungeonConnectedLinkType> MakeConnectedLinkKey(
    const DungeonConnectedRoomLink& link) {
  const auto ordered_rooms = std::minmax(link.from_room_id, link.to_room_id);
  return std::make_tuple(ordered_rooms.first, ordered_rooms.second, link.type);
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

ImVec2 GetConnectedRoomOrigin(int grid_col, int grid_row, int min_col,
                              int min_row) {
  const int col = grid_col - min_col;
  const int row = grid_row - min_row;
  const float step = kDungeonRoomPixelSize + kConnectedRoomGap;
  return ImVec2(static_cast<float>(col) * step, static_cast<float>(row) * step);
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
  const auto& agent_theme = AgentUI::GetTheme();

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
      ImGui::SetTooltip("Drag controls\nDouble-click to snap back");
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
    ImGui::TextDisabled("%d room%s", connected_room_count,
                        connected_room_count == 1 ? "" : "s");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::TextDisabled("%.0f%%", scale * 100.0f);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Connected zoom\nF fit\n0 reset\n+/- zoom");
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::SmallButton("Fit")) {
      actions.fit = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Fit all connected rooms in view (F)");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
      actions.reset = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Reset zoom and recenter on current room (0)");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Mini", &actions.show_overview);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Show connected overview map\nClick a room on the overview to jump");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Room", &actions.show_preview);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Show full-resolution preview of the current room alongside the "
          "matrix");
    }

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
    ImGui::TextDisabled("Overview");
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
      const ImU32 fill =
          is_center ? ImGui::GetColorU32(theme.selection_primary)
          : is_highlighted
              ? ImGui::GetColorU32(gui::ConvertColorToImVec4(theme.warning))
              : IM_COL32(110, 146, 126, 210);
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
        const ImVec2 mouse = ImGui::GetMousePos();
        const float content_x = std::clamp(
            (mouse.x - map_min.x) / overview_scale, 0.0f, content_size.x);
        const float content_y = std::clamp(
            (mouse.y - map_min.y) / overview_scale, 0.0f, content_size.y);
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
            "[%03X] %s\nRelease to open room view", actions.hovered_room_id,
            zelda3::GetRoomLabel(actions.hovered_room_id).c_str());
      }
    }
  }
  ImGui::End();

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);
  return actions;
}

enum class TrackDir : uint8_t { North, East, South, West };

using TrackDirectionMasks = yaze::editor::TrackDirectionMasks;

}  // namespace

// Use shared GetObjectName() from zelda3/dungeon/room_object.h
using zelda3::GetObjectName;
using zelda3::GetObjectSubtype;

void DungeonCanvasViewer::SetProject(const project::YazeProject* project) {
  project_ = project;
  ApplyTrackCollisionConfig();
}

void DungeonCanvasViewer::ApplyTrackCollisionConfig() {
  auto apply_list = [](std::array<bool, 256>& dest,
                       const std::vector<uint16_t>& values) {
    dest.fill(false);
    for (uint16_t value : values) {
      if (value < 256) {
        dest[value] = true;
      }
    }
  };
  auto ids_valid = [](const std::vector<uint16_t>& values) {
    std::array<bool, 256> seen{};
    for (uint16_t value : values) {
      if (value >= seen.size() || seen[value]) {
        return false;
      }
      seen[value] = true;
    }
    return true;
  };

  std::vector<uint16_t> default_track_tile_list;
  for (uint16_t tile = 0xB0; tile <= 0xBE; ++tile) {
    default_track_tile_list.push_back(tile);
  }
  const std::vector<uint16_t> default_stop_tile_list = {0xB7, 0xB8, 0xB9, 0xBA};
  const std::vector<uint16_t> default_switch_tile_list = {0xD0, 0xD1, 0xD2,
                                                          0xD3};

  const auto& track_tiles =
      (project_ && !project_->dungeon_overlay.track_tiles.empty())
          ? project_->dungeon_overlay.track_tiles
          : default_track_tile_list;
  apply_list(track_collision_config_.track_tiles, track_tiles);
  track_tile_order_ = track_tiles;

  const auto& stop_tiles =
      (project_ && !project_->dungeon_overlay.track_stop_tiles.empty())
          ? project_->dungeon_overlay.track_stop_tiles
          : default_stop_tile_list;
  apply_list(track_collision_config_.stop_tiles, stop_tiles);

  const auto& switch_tiles =
      (project_ && !project_->dungeon_overlay.track_switch_tiles.empty())
          ? project_->dungeon_overlay.track_switch_tiles
          : default_switch_tile_list;
  apply_list(track_collision_config_.switch_tiles, switch_tiles);
  switch_tile_order_ = switch_tiles;

  track_direction_map_enabled_ =
      (track_tile_order_.size() == default_track_tile_list.size()) &&
      (switch_tile_order_.size() == default_switch_tile_list.size()) &&
      ids_valid(track_tile_order_) && ids_valid(switch_tile_order_);

  minecart_sprite_ids_.reset();
  std::vector<uint16_t> minecart_ids = {0xA3};
  if (project_ && !project_->dungeon_overlay.minecart_sprite_ids.empty()) {
    minecart_ids = project_->dungeon_overlay.minecart_sprite_ids;
  }
  for (uint16_t id : minecart_ids) {
    if (id < minecart_sprite_ids_.size()) {
      minecart_sprite_ids_[id] = true;
    }
  }

  collision_overlay_cache_.clear();
}

void DungeonCanvasViewer::Draw(int room_id) {
  DrawDungeonCanvas(room_id);
}

void DungeonCanvasViewer::DrawDungeonCanvas(int room_id) {
  current_room_id_ = room_id;
  // Validate room_id and ROM
  if (room_id < 0 || room_id >= 0x128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  ImGui::BeginGroup();

  // CRITICAL: Canvas coordinate system for dungeons
  // The canvas system uses a two-stage scaling model:
  // 1. Canvas size: UNSCALED content dimensions (512x512 for dungeon rooms)
  // 2. Viewport size: canvas_size * global_scale (handles zoom)
  // 3. Grid lines: grid_step * global_scale (auto-scales with zoom)
  // 4. Bitmaps: drawn with scale = global_scale (matches viewport)
  constexpr int kRoomPixelWidth = 512;  // 64 tiles * 8 pixels (UNSCALED)
  constexpr int kRoomPixelHeight = 512;
  constexpr int kDungeonTileSize = 8;  // Dungeon tiles are 8x8 pixels

  // Configure canvas frame options for the new BeginCanvas/EndCanvas pattern
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = ImVec2(kRoomPixelWidth, kRoomPixelHeight);
  frame_opts.draw_grid = show_grid_;
  frame_opts.grid_step = static_cast<float>(custom_grid_size_);
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;

  // Keep the shared canvas menu in sync with dungeon-local grid state so the
  // generic View Controls menu can own zoom/grid commands.
  auto canvas_config = canvas_.GetConfig();
  canvas_config.enable_grid = show_grid_;
  canvas_config.grid_step = static_cast<float>(custom_grid_size_);
  canvas_.ApplyConfigSnapshot(canvas_config);
  canvas_.SetShowBuiltinContextMenu(true);

  if (rooms_) {
    auto& room = (*rooms_)[room_id];

    // Check if critical properties changed and trigger reload
    if (prev_blockset_ != room.blockset() || prev_palette_ != room.palette() ||
        prev_layout_ != room.layout_id() ||
        prev_spriteset_ != room.spriteset()) {
      if (room.rom() && room.rom()->is_loaded()) {
        room.ReloadGraphics(current_entrance_blockset_);
      }

      prev_blockset_ = room.blockset();
      prev_palette_ = room.palette();
      prev_layout_ = room.layout_id();
      prev_spriteset_ = room.spriteset();
    }
    if (header_visible_) {
      DrawRoomHeader(room, room_id);
    }
  }

  // Compact layer/overlay toggle bar (always visible above canvas)
  DrawCompactLayerToggles(room_id);

  ImGui::EndGroup();

  // Set up context menu items BEFORE DrawBackground so DrawContextMenu can be
  // called immediately after (OpenPopupOnItemClick requires this ordering)
  PopulateCanvasContextMenu(room_id);

  // CRITICAL: Begin canvas frame using modern BeginCanvas/EndCanvas pattern
  // This replaces DrawBackground + DrawContextMenu with a unified frame
  auto canvas_rt = gui::BeginCanvas(canvas_, frame_opts);
  SyncCanvasCaptureRegion(canvas_rt);
  ConsumePendingCanvasScroll(canvas_rt);
  canvas_rt.scrolling = canvas_.scrolling();

  // Update touch handler for long-press gesture detection
  touch_handler_.ProcessForCanvas(canvas_rt.canvas_p0, canvas_rt.canvas_sz,
                                  canvas_rt.hovered);
  touch_handler_.Update();

  DrawHeaderHiddenMetadataHud(room_id);

  DrawPersistentDebugWindows(room_id);

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    DrawRoomCanvasContent(canvas_rt, room, room_id);
  }

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    DrawRoomCanvasOverlays(canvas_rt, room, room_id);
  }

  DrawCoordinateOverlayHud(room_id);

  // End canvas frame - this draws grid/overlay based on frame_opts
  gui::EndCanvas(canvas_, canvas_rt, frame_opts);

  // Pull generic view-control changes back out of the shared canvas state so
  // subsequent frames and dungeon-specific UI stay in sync.
  show_grid_ = canvas_.GetConfig().enable_grid;
  set_custom_grid_size(static_cast<int>(canvas_.GetConfig().grid_step));
}

}  // namespace yaze::editor
