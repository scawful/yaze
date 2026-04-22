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

constexpr int kRoomMatrixCols = 16;
constexpr int kRoomMatrixRows = 19;
constexpr int kRoomPropertyColumns = 2;
constexpr char kConnectedCurrentRoomHudId[] =
    "##DungeonConnectedCurrentRoomHUD";
constexpr char kConnectedCanvasControlsId[] =
    "##DungeonConnectedCanvasControls";
constexpr char kConnectedCanvasOverviewId[] =
    "##DungeonConnectedCanvasOverview";

const char* GetObjectStreamLabel(int layer_value);

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

const char* GetObjectStreamLabel(int layer_value) {
  switch (layer_value) {
    case 0:
      return "Primary";
    case 1:
      return "BG2 overlay";
    case 2:
      return "BG1 overlay";
    default:
      return "Unknown";
  }
}

}  // namespace

// NeighborRoomId and OppositeDir are defined inline in dungeon_canvas_viewer.h.

std::vector<DungeonConnectedRoomLink> CollectDungeonConnectedRoomLinks(
    int room_id, const zelda3::Room& room,
    const std::function<bool(int, zelda3::DoorDirection)>&
        has_reciprocal_door) {
  std::vector<DungeonConnectedRoomLink> links;

  for (const auto& door : room.GetDoors()) {
    if (IsExitDoorType(door.type)) {
      continue;
    }

    const int neighbor = NeighborRoomId(room_id, door.direction);
    if (neighbor < 0) {
      continue;
    }
    if (has_reciprocal_door &&
        !has_reciprocal_door(neighbor, OppositeDir(door.direction))) {
      continue;
    }

    links.push_back(DungeonConnectedRoomLink{
        room_id, neighbor, DungeonConnectedLinkType::Door, door.direction});
  }

  for (int i = 0; i < 4; ++i) {
    const int stair_room = static_cast<int>(room.staircase_room(i));
    if (stair_room <= 0 || stair_room >= zelda3::kNumberOfRooms) {
      continue;
    }
    links.push_back(DungeonConnectedRoomLink{
        room_id, stair_room, DungeonConnectedLinkType::Staircase,
        zelda3::DoorDirection::North});
  }

  const int holewarp_room = static_cast<int>(room.holewarp());
  if (holewarp_room > 0 && holewarp_room < zelda3::kNumberOfRooms) {
    links.push_back(DungeonConnectedRoomLink{room_id, holewarp_room,
                                             DungeonConnectedLinkType::Holewarp,
                                             zelda3::DoorDirection::North});
  }

  return links;
}

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

zelda3::Room* DungeonCanvasViewer::EnsureRoomLoadedForConnectedView(
    int room_id) {
  if (!rooms_ || !rom_ || !rom_->is_loaded() || room_id < 0 ||
      room_id >= zelda3::kNumberOfRooms) {
    return nullptr;
  }

  auto& room = (*rooms_)[room_id];
  room.SetRom(rom_);
  room.SetGameData(game_data_);
  if (!room.IsLoaded()) {
    room = zelda3::LoadRoomFromRom(rom_, room_id);
    room.SetGameData(game_data_);
  }
  return &room;
}

bool DungeonCanvasViewer::RoomHasNonExitDoorInDirection(
    int room_id, zelda3::DoorDirection dir) {
  zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
  if (!room) {
    return false;
  }

  for (const auto& door : room->GetDoors()) {
    if (door.direction == dir && !IsExitDoorType(door.type)) {
      return true;
    }
  }
  return false;
}

DungeonCanvasViewer::ConnectedRoomGraphData
DungeonCanvasViewer::BuildConnectedRoomGraph(int start_room_id) {
  ConnectedRoomGraphData graph;
  if (start_room_id < 0 || start_room_id >= zelda3::kNumberOfRooms) {
    return graph;
  }

  zelda3::Room* start_room = EnsureRoomLoadedForConnectedView(start_room_id);
  if (!start_room) {
    return graph;
  }

  const uint8_t start_blockset = start_room->blockset();
  graph.room_mask[static_cast<size_t>(start_room_id)] = true;
  graph.room_positions[static_cast<size_t>(start_room_id)] = {0, 0, true};
  graph.room_count = 1;
  graph.min_col = graph.max_col = 0;
  graph.min_row = graph.max_row = 0;

  std::map<std::pair<int, int>, int> occupied_slots;
  occupied_slots[{0, 0}] = start_room_id;
  std::queue<int> to_visit;
  std::set<std::tuple<int, int, DungeonConnectedLinkType>> seen_links;
  to_visit.push(start_room_id);

  auto room_matches_cluster = [&](int room_id) {
    zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
    return room != nullptr && room->blockset() == start_blockset;
  };

  auto track_room_bounds = [&](int room_id) {
    const auto& placement = graph.room_positions[static_cast<size_t>(room_id)];
    graph.min_col = std::min(graph.min_col, placement.col);
    graph.max_col = std::max(graph.max_col, placement.col);
    graph.min_row = std::min(graph.min_row, placement.row);
    graph.max_row = std::max(graph.max_row, placement.row);
  };

  while (!to_visit.empty()) {
    const int room_id = to_visit.front();
    to_visit.pop();

    zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
    if (!room || room->blockset() != start_blockset) {
      continue;
    }

    const auto outgoing_links = CollectDungeonConnectedRoomLinks(
        room_id, *room,
        [this](int neighbor_room_id, zelda3::DoorDirection dir) {
          return RoomHasNonExitDoorInDirection(neighbor_room_id, dir);
        });

    for (const auto& link : outgoing_links) {
      if (link.to_room_id < 0 || link.to_room_id >= zelda3::kNumberOfRooms) {
        continue;
      }
      if (!room_matches_cluster(link.to_room_id)) {
        continue;
      }

      if (seen_links.insert(MakeConnectedLinkKey(link)).second) {
        graph.links.push_back(link);
      }

      if (!graph.room_mask[static_cast<size_t>(link.to_room_id)]) {
        const auto& source_placement =
            graph.room_positions[static_cast<size_t>(room_id)];
        const std::pair<int, int> placement =
            (link.type == DungeonConnectedLinkType::Door)
                ? FindConnectedDoorPlacement(
                      occupied_slots, source_placement.col,
                      source_placement.row, link.direction)
                : FindConnectedTransportPlacement(occupied_slots,
                                                  source_placement.col,
                                                  source_placement.row);
        graph.room_mask[static_cast<size_t>(link.to_room_id)] = true;
        graph.room_positions[static_cast<size_t>(link.to_room_id)] = {
            placement.first, placement.second, true};
        occupied_slots[placement] = link.to_room_id;
        ++graph.room_count;
        track_room_bounds(link.to_room_id);
        to_visit.push(link.to_room_id);
      }
    }
  }

  return graph;
}

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

std::optional<int> DungeonCanvasViewer::DrawConnectedRoomMatrix(
    int center_room_id) {
  current_room_id_ = center_room_id;
  if (center_room_id < 0 || center_room_id >= zelda3::kNumberOfRooms) {
    ImGui::Text("Invalid room ID: %d", center_room_id);
    return std::nullopt;
  }
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return std::nullopt;
  }
  if (!rooms_) {
    ImGui::TextDisabled("Room data unavailable");
    return std::nullopt;
  }

  if (!connected_canvas_initialized_) {
    connected_canvas_.set_global_scale(kConnectedCanvasDefaultScale);
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

  // The caller (DungeonWorkbenchContent::DrawCanvasPane) owns the scroll
  // container and has already called ImGui::SetNextWindowContentSize +
  // BeginChild with the matching horizontal-scroll flags. Keeping the zero-
  // padding scope local to the canvas draw keeps the scroll host free of
  // padding gymnastics.
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
    // Hysteresis: once the panel is visible it stays visible down to
    // (min - kConnectedSidePanelHideHysteresis) before collapsing. Prevents
    // the ~220 px horizontal layout bounce reported when dragging the pane
    // splitter across the threshold.
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
    std::snprintf(room_label, sizeof(room_label), "[%03X] %s", room_id,
                  zelda3::GetRoomLabel(room_id).c_str());
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
  const ConnectedCanvasControlActions control_actions =
      DrawConnectedCanvasControls(
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
      ImGui::SetTooltip("[%03X] %s\nClick to open room view", hovered_room_id,
                        zelda3::GetRoomLabel(hovered_room_id).c_str());
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
            ImGui::Text("Current Room");
            ImGui::TextDisabled("[%03X] %s", center_room_id,
                                zelda3::GetRoomLabel(center_room_id).c_str());
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
  has_canvas_capture_region_ =
      canvas_rt.canvas_sz.x > 1.0f && canvas_rt.canvas_sz.y > 1.0f;
  if (has_canvas_capture_region_) {
    canvas_capture_x_ = static_cast<int>(canvas_rt.canvas_p0.x);
    canvas_capture_y_ = static_cast<int>(canvas_rt.canvas_p0.y);
    canvas_capture_width_ = static_cast<int>(canvas_rt.canvas_sz.x);
    canvas_capture_height_ = static_cast<int>(canvas_rt.canvas_sz.y);
  }

  // Handle pending scroll request using the canvas's internal scrolling model.
  if (pending_scroll_target_.has_value()) {
    const auto [target_x, target_y] = pending_scroll_target_.value();
    float scale = canvas_.global_scale();
    if (scale <= 0.0f) {
      scale = 1.0f;
    }

    const float pixel_x =
        static_cast<float>(target_x * kDungeonTileSize) * scale;
    const float pixel_y =
        static_cast<float>(target_y * kDungeonTileSize) * scale;
    const ImVec2 view_size = canvas_rt.canvas_sz;
    const ImVec2 content_size(static_cast<float>(kRoomPixelWidth) * scale,
                              static_cast<float>(kRoomPixelHeight) * scale);

    const ImVec2 desired_scroll((view_size.x * 0.5f) - pixel_x,
                                (view_size.y * 0.5f) - pixel_y);
    canvas_.set_scrolling(
        gui::ClampScroll(desired_scroll, content_size, view_size));
    canvas_rt.scrolling = canvas_.scrolling();

    pending_scroll_target_.reset();
  }

  // Update touch handler for long-press gesture detection
  touch_handler_.ProcessForCanvas(canvas_rt.canvas_p0, canvas_rt.canvas_sz,
                                  canvas_rt.hovered);
  touch_handler_.Update();

  // When the header is hidden (e.g. split/compare stitched views), draw a small
  // in-canvas label so the user always knows what they're looking at.
  if (!header_visible_ && show_header_hidden_metadata_hud_) {
    const auto& label = zelda3::GetRoomLabel(room_id);
    char text1[160];
    snprintf(text1, sizeof(text1), "[%03X] %s", room_id, label.c_str());

    char text2[96] = {};
    bool show_meta = false;
    if (rooms_ && room_id >= 0 && room_id < static_cast<int>(rooms_->size())) {
      const auto& room = (*rooms_)[room_id];
      if (!object_interaction_enabled_) {
        snprintf(text2, sizeof(text2), "B:%02X P:%02X L:%02X S:%02X  RO",
                 room.blockset(), room.palette(), room.layout_id(),
                 room.spriteset());
      } else {
        snprintf(text2, sizeof(text2), "B:%02X P:%02X L:%02X S:%02X",
                 room.blockset(), room.palette(), room.layout_id(),
                 room.spriteset());
      }
      show_meta = true;
    } else if (!object_interaction_enabled_) {
      snprintf(text2, sizeof(text2), "Read-only");
      show_meta = true;
    }

    const float pad = 10.0f;
    const ImVec2 hud_pos(canvas_.zero_point().x + pad,
                         canvas_.zero_point().y + pad);
    const ImVec2 hud_size(0, 0);  // Auto-resize

    gui::DrawCanvasHUD("##MetadataHUD", hud_pos, hud_size, [&]() {
      ImGui::TextUnformatted(text1);
      if (show_meta) {
        ImGui::TextDisabled("%s", text2);
      }
    });
  }

  // Draw persistent debug overlays
  if (show_room_debug_info_ && rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 10, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Room Debug Info", &show_room_debug_info_,
                     ImGuiWindowFlags_NoCollapse)) {
      ImGui::Text("Room: 0x%03X (%d)", room_id, room_id);
      ImGui::Separator();
      ImGui::Text("Graphics");
      ImGui::Text("  Blockset: 0x%02X", room.blockset());
      ImGui::Text("  Palette: 0x%02X", room.palette());
      ImGui::Text("  Layout: 0x%02X", room.layout_id());
      ImGui::Text("  Spriteset: 0x%02X", room.spriteset());
      ImGui::Separator();
      ImGui::Text("Content");
      ImGui::Text("  Objects: %zu", room.GetTileObjects().size());
      ImGui::Text("  Sprites: %zu", room.GetSprites().size());
      ImGui::Separator();
      ImGui::Text("Buffers");
      auto& bg1 = room.bg1_buffer().bitmap();
      auto& bg2 = room.bg2_buffer().bitmap();
      ImGui::Text("  BG1: %dx%d %s", bg1.width(), bg1.height(),
                  bg1.texture() ? "(has texture)" : "(NO TEXTURE)");
      ImGui::Text("  BG2: %dx%d %s", bg2.width(), bg2.height(),
                  bg2.texture() ? "(has texture)" : "(NO TEXTURE)");
      ImGui::Separator();
      ImGui::Text("Layers (4-way)");
      auto& layer_mgr = GetRoomLayerManager(room_id);
      bool bg1l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout);
      bool bg1o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects);
      bool bg2l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout);
      bool bg2o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects);
      if (ImGui::Checkbox("BG1 Layout", &bg1l))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, bg1l);
      if (ImGui::Checkbox("BG1 Objects", &bg1o))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, bg1o);
      if (ImGui::Checkbox("BG2 Layout", &bg2l))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, bg2l);
      if (ImGui::Checkbox("BG2 Objects", &bg2o))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, bg2o);
      int blend = static_cast<int>(
          layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout));
      if (ImGui::SliderInt("BG2 Blend", &blend, 0, 4)) {
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout,
                                    static_cast<zelda3::LayerBlendMode>(blend));
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                                    static_cast<zelda3::LayerBlendMode>(blend));
      }

      ImGui::Separator();
      ImGui::Text("Layout Override");
      static bool enable_override = false;
      ImGui::Checkbox("Enable Override", &enable_override);
      if (enable_override) {
        ImGui::SliderInt("Layout ID", &layout_override_, 0, 7);
      } else {
        layout_override_ = -1;  // Disable override
      }

      ImGui::Separator();
      ImGui::Text("Preview State");
      if (auto* editor_state =
              dynamic_cast<zelda3::EditorDungeonState*>(room.GetDungeonState());
          editor_state != nullptr) {
        bool preview_state_changed = false;

        bool water_face_active = editor_state->IsWaterFaceActive(room_id);
        if (ImGui::Checkbox("Water Face Active", &water_face_active)) {
          editor_state->SetWaterFaceActive(room_id, water_face_active);
          preview_state_changed = true;
        }

        bool dam_floodgate_open = editor_state->IsDamFloodgateOpen(room_id);
        if (ImGui::Checkbox("Dam Floodgate Open", &dam_floodgate_open)) {
          editor_state->SetDamFloodgateOpen(room_id, dam_floodgate_open);
          preview_state_changed = true;
        }

        bool wall_moved = editor_state->IsWallMoved(room_id);
        if (ImGui::Checkbox("Moving Wall Shifted", &wall_moved)) {
          editor_state->SetWallMoved(room_id, wall_moved);
          preview_state_changed = true;
        }

        bool floor_bombed = editor_state->IsFloorBombable(room_id);
        if (ImGui::Checkbox("Bombed Floor Open", &floor_bombed)) {
          editor_state->SetFloorBombable(room_id, floor_bombed);
          preview_state_changed = true;
        }

        bool rupee_floor_active = editor_state->IsRupeeFloorActive(room_id);
        if (ImGui::Checkbox("Rupee Floor Active", &rupee_floor_active)) {
          editor_state->SetRupeeFloorActive(room_id, rupee_floor_active);
          preview_state_changed = true;
        }

        if (ImGui::Button("Reset Preview State")) {
          editor_state->Reset();
          preview_state_changed = true;
        }

        if (preview_state_changed) {
          room.ReloadGraphics(current_entrance_blockset_);
        }
      } else {
        ImGui::TextDisabled(
            "Preview state controls require EditorDungeonState.");
      }

      if (show_object_bounds_) {
        ImGui::Separator();
        ImGui::Text("Object Outline Filters");
        ImGui::Text("By Type:");
        ImGui::Checkbox("Type 1", &object_outline_toggles_.show_type1_objects);
        ImGui::Checkbox("Type 2", &object_outline_toggles_.show_type2_objects);
        ImGui::Checkbox("Type 3", &object_outline_toggles_.show_type3_objects);
        ImGui::Text("By Layer:");
        ImGui::Checkbox("Primary (main pass)",
                        &object_outline_toggles_.show_layer0_objects);
        ImGui::Checkbox("BG2 overlay",
                        &object_outline_toggles_.show_layer1_objects);
        ImGui::Checkbox("BG1 overlay",
                        &object_outline_toggles_.show_layer2_objects);
      }
    }
    ImGui::End();
  }

  if (show_texture_debug_ && rooms_ && rom_->is_loaded()) {
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 320, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Texture Debug", &show_texture_debug_,
                     ImGuiWindowFlags_NoCollapse)) {
      auto& room = (*rooms_)[room_id];
      auto& bg1 = room.bg1_buffer().bitmap();
      auto& bg2 = room.bg2_buffer().bitmap();

      auto ensure_bitmap_texture = [this](gfx::Bitmap& bitmap) {
        if (!renderer_ || !bitmap.is_active() || bitmap.width() <= 0) {
          return;
        }
        if (!bitmap.texture()) {
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::CREATE, &bitmap);
        } else if (bitmap.modified()) {
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::UPDATE, &bitmap);
        }
      };

      ensure_bitmap_texture(bg1);
      ensure_bitmap_texture(bg2);
      if (renderer_) {
        gfx::Arena::Get().ProcessTextureQueue(renderer_);
      }

      ImGui::Text("BG1 Bitmap");
      ImGui::Text("  Size: %dx%d", bg1.width(), bg1.height());
      ImGui::Text("  Active: %s", bg1.is_active() ? "YES" : "NO");
      ImGui::Text("  Texture: 0x%p", bg1.texture());
      ImGui::Text("  Modified: %s", bg1.modified() ? "YES" : "NO");

      if (bg1.texture()) {
        ImGui::Text("  Preview:");
        ImGui::Image((ImTextureID)(intptr_t)bg1.texture(), ImVec2(128, 128));
      }

      ImGui::Separator();
      ImGui::Text("BG2 Bitmap");
      ImGui::Text("  Size: %dx%d", bg2.width(), bg2.height());
      ImGui::Text("  Active: %s", bg2.is_active() ? "YES" : "NO");
      ImGui::Text("  Texture: 0x%p", bg2.texture());
      ImGui::Text("  Modified: %s", bg2.modified() ? "YES" : "NO");

      if (bg2.texture()) {
        ImGui::Text("  Preview:");
        ImGui::Image((ImTextureID)(intptr_t)bg2.texture(), ImVec2(128, 128));
      }
    }
    ImGui::End();
  }

  if (show_layer_info_) {
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 580, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Layer Info", &show_layer_info_,
                     ImGuiWindowFlags_NoCollapse)) {
      ImGui::Text("Canvas Scale: %.2f", canvas_.global_scale());
      ImGui::Text("Canvas Size: %.0fx%.0f", canvas_.width(), canvas_.height());
      auto& layer_mgr = GetRoomLayerManager(room_id);
      ImGui::Separator();
      ImGui::Text("Layer Visibility (4-way):");

      // Display each layer with visibility and blend mode
      for (int i = 0; i < 4; ++i) {
        auto layer = static_cast<zelda3::LayerType>(i);
        bool visible = layer_mgr.IsLayerVisible(layer);
        auto blend = layer_mgr.GetLayerBlendMode(layer);
        ImGui::Text("  %s: %s (%s)",
                    zelda3::RoomLayerManager::GetLayerName(layer),
                    visible ? "VISIBLE" : "hidden",
                    zelda3::RoomLayerManager::GetBlendModeName(blend));
      }

      ImGui::Separator();
      ImGui::Text("Draw Order:");
      auto draw_order = layer_mgr.GetDrawOrder();
      for (int i = 0; i < 4; ++i) {
        ImGui::Text("  %d: %s", i + 1,
                    zelda3::RoomLayerManager::GetLayerName(draw_order[i]));
      }
      ImGui::Text("BG2 On Top: %s", layer_mgr.IsBG2OnTop() ? "YES" : "NO");
    }
    ImGui::End();
  }

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Update object interaction context
    object_interaction_.SetCurrentRoom(rooms_, room_id);

    if (!room.AreObjectsLoaded()) {
      room.LoadObjects();
    }

    if (!room.AreSpritesLoaded()) {
      room.LoadSprites();
    }

    if (!room.ArePotItemsLoaded()) {
      room.LoadPotItems();
    }

    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    bool needs_render = !bg1_bitmap.is_active() || bg1_bitmap.width() == 0;

    static int last_rendered_room = -1;
    static bool has_rendered = false;
    if (needs_render && (last_rendered_room != room_id || !has_rendered)) {
      (void)LoadAndRenderRoomGraphics(room_id);
      last_rendered_room = room_id;
      has_rendered = true;
    }

    // CRITICAL: Process texture queue BEFORE drawing to ensure textures are
    // ready This must happen before DrawRoomBackgroundLayers() attempts to draw
    // bitmaps
    if (rom_ && rom_->is_loaded()) {
      gfx::Arena::Get().ProcessTextureQueue(renderer_);
    }

    // Draw the room's background layers to canvas
    // This already includes objects rendered by ObjectDrawer in
    // Room::RenderObjectsToBackground()
    DrawRoomBackgroundLayers(room_id);

    // Draw mask highlights when mask selection mode is active
    // This helps visualize which objects are BG2 overlays
    if (object_interaction_.IsMaskModeActive()) {
      DrawMaskHighlights(canvas_rt, room);
    }

    // Render entity overlays (sprites, pot items) as colored squares with labels
    // (Entities are not part of the background buffers)
    RenderEntityOverlay(canvas_rt, room);

    // Handle object interaction if enabled
    if (object_interaction_enabled_) {
      object_interaction_.HandleCanvasMouseInput();
      object_interaction_.CheckForObjectSelection();
      object_interaction_
          .DrawSelectionHighlights();  // Draw object selection highlights
      object_interaction_
          .DrawEntitySelectionHighlights();  // Draw door/sprite/item selection
      object_interaction_.DrawGhostPreview();  // Draw placement preview
      // Context menu is handled by BeginCanvas via frame_opts.draw_context_menu

      // --- DRAG SOURCES for selected objects/entities ---
      // Emit drag source for the primary selected tile object
      const auto selected = object_interaction_.GetSelectedObjectIndices();
      if (selected.size() == 1) {
        const auto& objects = room.GetTileObjects();
        size_t idx = selected.front();
        if (idx < objects.size()) {
          const auto& obj = objects[idx];
          gui::BeginRoomObjectDragSource(static_cast<uint16_t>(obj.id_),
                                         room_id, obj.x_, obj.y_);
        }
      }

      // Emit drag source for selected sprite entity
      if (object_interaction_.HasEntitySelection()) {
        const auto sel = object_interaction_.GetSelectedEntity();
        if (sel.type == EntityType::Sprite) {
          const auto& sprites = room.GetSprites();
          if (sel.index < sprites.size()) {
            const auto& sprite = sprites[sel.index];
            gui::BeginSpriteDragSource(sprite.id(), room_id);
          }
        }
      }

      // Touch long-press context menu for entity interaction
      HandleTouchLongPressContextMenu(canvas_rt, room);
    }

    // --- DROP TARGETS on canvas ---
    // Accept room object drops (reposition from another room or palette)
    gui::RoomObjectDragPayload obj_drop;
    if (gui::AcceptRoomObjectDrop(&obj_drop)) {
      // Convert canvas mouse position to room tile coordinates
      auto [tile_x, tile_y] = DungeonRenderingHelpers::ScreenToRoomCoordinates(
          ImGui::GetMousePos(), canvas_.zero_point(), canvas_.global_scale());
      if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
        zelda3::RoomObject new_obj(static_cast<int16_t>(obj_drop.object_id),
                                   static_cast<uint8_t>(tile_x),
                                   static_cast<uint8_t>(tile_y), 0, 0);
        const size_t before = room.GetTileObjects().size();
        object_interaction_.entity_coordinator().tile_handler().PlaceObjectAt(
            room_id, new_obj, tile_x, tile_y);
        if (room.GetTileObjects().size() > before) {
          object_interaction_.SetSelectedObjects({before});
        }
      }
    }

    // Accept sprite drops (reposition from another room or sprite list)
    gui::SpriteDragPayload sprite_drop;
    if (gui::AcceptSpriteDrop(&sprite_drop)) {
      auto [tile_x, tile_y] = DungeonRenderingHelpers::ScreenToRoomCoordinates(
          ImGui::GetMousePos(), canvas_.zero_point(), canvas_.global_scale());
      // Sprites use 16-pixel units, tiles are 8-pixel
      int sprite_x = (tile_x * 8) / 16;
      int sprite_y = (tile_y * 8) / 16;
      if (sprite_x >= 0 && sprite_x < 32 && sprite_y >= 0 && sprite_y < 32) {
        // Use 5-arg constructor: (id, x, y, subtype, layer)
        zelda3::Sprite new_sprite(static_cast<uint8_t>(sprite_drop.sprite_id),
                                  static_cast<uint8_t>(sprite_x),
                                  static_cast<uint8_t>(sprite_y), 0, 0);
        if (auto* ctx = object_interaction_.entity_coordinator()
                            .sprite_handler()
                            .context()) {
          ctx->NotifyMutation(MutationDomain::kSprites);
        }
        room.GetSprites().push_back(new_sprite);
        room.MarkSpritesDirty();
        if (auto* ctx = object_interaction_.entity_coordinator()
                            .sprite_handler()
                            .context()) {
          ctx->NotifyInvalidateCache(MutationDomain::kSprites);
        }
      }
    }
  }

  // Draw optional overlays on top of background bitmap
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Draw the room layout first as the base layer

    // VISUALIZATION: Draw object position rectangles (for debugging)
    // This shows where objects are placed regardless of whether graphics render
    if (show_object_bounds_) {
      DrawObjectPositionOutlines(canvas_rt, room);
    }

    // Track collision overlay (custom collision tiles)
    if (show_track_collision_overlay_) {
      DungeonRenderingHelpers::DrawTrackCollisionOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), GetCollisionOverlayCache(room.id()),
          track_collision_config_, track_direction_map_enabled_,
          track_tile_order_, switch_tile_order_, show_track_collision_legend_);
    }

    if (show_custom_collision_overlay_) {
      DungeonRenderingHelpers::DrawCustomCollisionOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room);
    }

    if (show_water_fill_overlay_) {
      DungeonRenderingHelpers::DrawWaterFillOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room);
    }

    if (show_camera_quadrant_overlay_) {
      DungeonRenderingHelpers::DrawCameraQuadrantOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room);
    }

    if (show_minecart_sprite_overlay_) {
      DungeonRenderingHelpers::DrawMinecartSpriteOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room, minecart_sprite_ids_,
          track_collision_config_);
    }

    if (show_track_gap_overlay_) {
      DungeonRenderingHelpers::DrawTrackGapOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room, GetCollisionOverlayCache(room.id()));
    }

    if (show_track_route_overlay_) {
      DungeonRenderingHelpers::DrawTrackRouteOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), GetCollisionOverlayCache(room.id()));
    }

    // Custom Objects overlay: draw a translucent cyan rectangle + label for
    // each tile object that uses a custom draw routine (IDs 0x31/0x32).
    if (show_custom_objects_overlay_) {
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      const ImVec2 canvas_pos = canvas_.zero_point();
      const float scale = canvas_.global_scale();
      // Base the highlight on theme.info (semantically: "look here"). The
      // overlay reuses the accent hue at translucent/opaque alpha for fill
      // vs border so the pair stays visually linked under any theme.
      const ImVec4 info = gui::GetInfoColor();
      const ImU32 fill_color =
          ImGui::GetColorU32(ImVec4(info.x, info.y, info.z, 0.25f));
      const ImU32 border_color =
          ImGui::GetColorU32(ImVec4(info.x, info.y, info.z, 0.8f));
      // Text-background dark overlay stays hardcoded: it's a legibility plate
      // behind the label, not a themed element.
      const ImU32 text_bg_color = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f));

      // Custom draw routines are registered for object IDs 0x31 and 0x32
      // (Oracle of Secrets minecart track objects). Flag any object whose ID
      // falls in that range so the overlay is general but practically correct.
      auto is_custom = [](int id) {
        return id == 0x31 || id == 0x32;
      };

      for (const auto& obj : room.GetTileObjects()) {
        if (!is_custom(static_cast<int>(obj.id_))) {
          continue;
        }

        // Object positions are in tile units; canvas pixels = tile * 8 * scale.
        const float px = static_cast<float>(obj.x()) * 8.0f * scale;
        const float py = static_cast<float>(obj.y()) * 8.0f * scale;

        // Draw a 16x16-pixel (2-tile) highlight box — small but visible.
        const float box_w = 16.0f * scale;
        const float box_h = 16.0f * scale;
        const ImVec2 p0(canvas_pos.x + px, canvas_pos.y + py);
        const ImVec2 p1(p0.x + box_w, p0.y + box_h);

        draw_list->AddRectFilled(p0, p1, fill_color, 2.0f);
        draw_list->AddRect(p0, p1, border_color, 2.0f, 0, 1.5f);

        // Label: object ID and subtype
        char label[32];
        std::snprintf(label, sizeof(label), "0x%02X s%d",
                      static_cast<int>(obj.id_),
                      static_cast<int>(obj.size_ & 0x1F));
        const ImVec2 text_sz = ImGui::CalcTextSize(label);
        const ImVec2 tp(p0.x + 1.0f, p0.y - text_sz.y - 1.0f);
        draw_list->AddRectFilled(
            tp, ImVec2(tp.x + text_sz.x + 2.0f, tp.y + text_sz.y),
            text_bg_color, 2.0f);
        draw_list->AddText(tp, border_color, label);
      }
    }

    if (minecart_track_panel_) {
      const bool show_tracks = show_minecart_tracks_ ||
                               minecart_track_panel_->IsPickingCoordinates();
      const auto& tracks = minecart_track_panel_->GetTracks();
      if (show_tracks && !tracks.empty()) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = canvas_.zero_point();
        float scale = canvas_.global_scale();
        const auto& theme = AgentUI::GetTheme();
        const int active_track =
            minecart_track_panel_->IsPickingCoordinates()
                ? minecart_track_panel_->GetPickingTrackIndex()
                : -1;

        for (const auto& track : tracks) {
          auto local = dungeon_coords::CameraToLocalCoords(
              static_cast<uint16_t>(track.start_x),
              static_cast<uint16_t>(track.start_y));
          if (local.room_id != room_id) {
            continue;
          }

          ImVec4 marker_color = theme.selection_primary;
          if (track.id == active_track) {
            marker_color = theme.status_warning;
          }

          const float px = static_cast<float>(local.local_pixel_x) * scale;
          const float py = static_cast<float>(local.local_pixel_y) * scale;
          ImVec2 center(canvas_pos.x + px, canvas_pos.y + py);
          const float radius = 6.0f * scale;

          draw_list->AddCircleFilled(center, radius,
                                     ImGui::GetColorU32(marker_color));
          draw_list->AddCircle(center, radius + 2.0f,
                               ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f)), 0,
                               2.0f);

          std::string label = absl::StrFormat("T%d", track.id);
          draw_list->AddText(
              ImVec2(center.x + 8.0f * scale, center.y - 6.0f * scale),
              ImGui::GetColorU32(theme.text_primary), label.c_str());
        }
      }
    }
  }

  // Draw coordinate overlay when hovering over canvas
  if (show_coordinate_overlay_ && canvas_.IsMouseHovering()) {
    auto [tile_x, tile_y] = DungeonRenderingHelpers::ScreenToRoomCoordinates(
        ImGui::GetMousePos(), canvas_.zero_point(), canvas_.global_scale());

    // Only show if within bounds
    if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {

      // Calculate logical pixel coordinates
      int canvas_x = tile_x * 8;
      int canvas_y = tile_y * 8;

      // Calculate camera/world coordinates (for minecart tracks, sprites, etc.)
      auto [camera_x, camera_y] =
          dungeon_coords::TileToCameraCoords(room_id, tile_x, tile_y);

      // Calculate sprite coordinates (16-pixel units)
      int sprite_x = canvas_x / dungeon_coords::kSpriteTileSize;
      int sprite_y = canvas_y / dungeon_coords::kSpriteTileSize;

      // Draw coordinate HUD at mouse position
      ImVec2 mouse_pos = ImGui::GetMousePos();
      ImVec2 overlay_pos = ImVec2(mouse_pos.x + 15, mouse_pos.y + 15);

      gui::DrawCanvasHUD("##CoordHUD", overlay_pos, ImVec2(0, 0), [&]() {
        ImGui::Text("Tile: (%d, %d)", tile_x, tile_y);
        ImGui::Text("Pixel: (%d, %d)", canvas_x, canvas_y);
        ImGui::Text("Camera: ($%04X, $%04X)", camera_x, camera_y);
        ImGui::Text("Sprite: (%d, %d)", sprite_x, sprite_y);
      });
    }
  }

  // End canvas frame - this draws grid/overlay based on frame_opts
  gui::EndCanvas(canvas_, canvas_rt, frame_opts);

  // Pull generic view-control changes back out of the shared canvas state so
  // subsequent frames and dungeon-specific UI stay in sync.
  show_grid_ = canvas_.GetConfig().enable_grid;
  set_custom_grid_size(static_cast<int>(canvas_.GetConfig().grid_step));
}

void DungeonCanvasViewer::DisplayObjectInfo(const gui::CanvasRuntime& rt,
                                            const zelda3::RoomObject& object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay with hex ID and name
  std::string name = GetObjectName(object.id_);
  std::string info_text;
  if (object.id_ >= 0x100) {
    info_text =
        absl::StrFormat("0x%03X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  } else {
    info_text =
        absl::StrFormat("0x%02X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  }

  // Draw text at the object position using runtime-based helper
  gui::DrawText(rt, info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderSprites(const gui::CanvasRuntime& rt,
                                        const zelda3::Room& room) {
  // Skip if sprites are not visible
  if (!entity_visibility_.show_sprites) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();

  // Adaptive entity size: expand on touch devices for easier tapping
  const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
  const int entity_size = is_touch ? 24 : 16;

  // Render sprites as colored squares with sprite name/ID
  // NOTE: Sprite coordinates are in 16-pixel units (0-31 range = 512 pixels)
  // unlike object coordinates which are in 8-pixel tile units
  for (const auto& sprite : room.GetSprites()) {
    // Sprites use 16-pixel coordinate system
    int canvas_x = sprite.x() * 16;
    int canvas_y = sprite.y() * 16;

    if (canvas_x >= -entity_size && canvas_y >= -entity_size &&
        canvas_x < 512 + entity_size && canvas_y < 512 + entity_size) {
      ImVec4 sprite_color;

      // Color-code sprites based on layer
      if (sprite.layer() == 0) {
        sprite_color = theme.dungeon_sprite_layer0;  // Green for layer 0
      } else {
        sprite_color = theme.dungeon_sprite_layer1;  // Blue for layer 1
      }

      // Draw square with adaptive size for touch targets
      gui::DrawRect(rt, canvas_x, canvas_y, entity_size, entity_size,
                    sprite_color);

      // Draw sprite ID and name using unified ResourceLabelProvider
      std::string full_name = zelda3::GetSpriteLabel(sprite.id());
      std::string sprite_text;
      // Truncate long names for display
      if (full_name.length() > 12) {
        sprite_text = absl::StrFormat("%02X %s..", sprite.id(),
                                      full_name.substr(0, 8).c_str());
      } else {
        sprite_text =
            absl::StrFormat("%02X %s", sprite.id(), full_name.c_str());
      }

      gui::DrawText(rt, sprite_text, canvas_x, canvas_y);
    }
  }
}

void DungeonCanvasViewer::RenderPotItems(const gui::CanvasRuntime& rt,
                                         const zelda3::Room& room) {
  // Skip if pot items are not visible
  if (!entity_visibility_.show_pot_items) {
    return;
  }

  const auto& pot_items = room.GetPotItems();

  // If no pot items in this room, nothing to render
  if (pot_items.empty()) {
    return;
  }

  // Pot item names
  static const char* kPotItemNames[] = {
      "Nothing",        // 0
      "Green Rupee",    // 1
      "Rock",           // 2
      "Bee",            // 3
      "Health",         // 4
      "Bomb",           // 5
      "Heart",          // 6
      "Blue Rupee",     // 7
      "Key",            // 8
      "Arrow",          // 9
      "Bomb",           // 10
      "Heart",          // 11
      "Magic",          // 12
      "Full Magic",     // 13
      "Cucco",          // 14
      "Green Soldier",  // 15
      "Bush Stal",      // 16
      "Blue Soldier",   // 17
      "Landmine",       // 18
      "Heart",          // 19
      "Fairy",          // 20
      "Heart",          // 21
      "Nothing",        // 22
      "Hole",           // 23
      "Warp",           // 24
      "Staircase",      // 25
      "Bombable",       // 26
      "Switch"          // 27
  };
  constexpr size_t kPotItemNameCount =
      sizeof(kPotItemNames) / sizeof(kPotItemNames[0]);

  // Pot items now have their own position data from ROM
  // No need to match to objects - each item has exact pixel coordinates
  for (const auto& pot_item : pot_items) {
    // Get pixel coordinates from PotItem structure
    int pixel_x = pot_item.GetPixelX();
    int pixel_y = pot_item.GetPixelY();

    // Convert to canvas coordinates (already in pixels, just need offset)
    // Note: pot item coords are already in full room pixel space
    auto [canvas_x, canvas_y] =
        DungeonRenderingHelpers::RoomToCanvasCoordinates(pixel_x / 8,
                                                         pixel_y / 8);

    // Adaptive entity size for touch devices
    const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
    const int entity_size = is_touch ? 24 : 16;

    if (canvas_x >= -entity_size && canvas_y >= -entity_size &&
        canvas_x < 512 + entity_size && canvas_y < 512 + entity_size) {
      // Draw colored square
      const auto& theme = AgentUI::GetTheme();
      ImVec4 pot_item_color;
      if (pot_item.item == 0) {
        pot_item_color = theme.status_inactive;  // Muted color for Nothing
        pot_item_color.w = 0.4f;
      } else {
        pot_item_color = theme.item_color;  // Gold/Yellow for items
        pot_item_color.w = 0.75f;
      }

      gui::DrawRect(rt, canvas_x, canvas_y, entity_size, entity_size,
                    pot_item_color);

      // Get item name
      std::string item_name;
      if (pot_item.item < kPotItemNameCount) {
        item_name = kPotItemNames[pot_item.item];
      } else {
        item_name = absl::StrFormat("Unk%02X", pot_item.item);
      }

      // Draw label above the box
      std::string item_text =
          absl::StrFormat("%02X %s", pot_item.item, item_name.c_str());
      gui::DrawText(rt, item_text, canvas_x, canvas_y - 10);
    }
  }
}

void DungeonCanvasViewer::RenderEntityOverlay(const gui::CanvasRuntime& rt,
                                              const zelda3::Room& room) {
  // Render all entity overlays using runtime-based helpers
  RenderSprites(rt, room);
  RenderPotItems(rt, room);
}

void DungeonCanvasViewer::HandleTouchLongPressContextMenu(
    const gui::CanvasRuntime& rt, const zelda3::Room& room) {
  constexpr const char* kPopupId = "##TouchEntityContextMenu";
  const ImGuiIO& io = ImGui::GetIO();
  const bool touch_context_click =
      rt.hovered && io.MouseSource == ImGuiMouseSource_TouchScreen &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  const bool gesture_long_press = touch_handler_.WasLongPressed();
  const bool should_open_context = gesture_long_press || touch_context_click;

  // On long-press, hit-test entities at the gesture position and open popup.
  // iOS maps long-press to right-click; treat that as a touch context gesture.
  if (should_open_context) {
    ImVec2 gesture_pos = gesture_long_press
                             ? touch_handler_.GetGesturePosition()
                             : ImGui::GetMousePos();
    float scale = rt.scale > 0.0f ? rt.scale : 1.0f;

    // Convert screen position to room pixel coordinates
    float rel_x = (gesture_pos.x - rt.canvas_p0.x) / scale;
    float rel_y = (gesture_pos.y - rt.canvas_p0.y) / scale;

    // Adaptive hit-test size for touch devices
    const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
    const int hit_size = is_touch ? 24 : 16;

    // Hit-test sprites
    const auto& sprites = room.GetSprites();
    for (size_t idx = 0; idx < sprites.size(); ++idx) {
      int sprite_px = sprites[idx].x() * 16;
      int sprite_py = sprites[idx].y() * 16;
      if (rel_x >= sprite_px && rel_x < sprite_px + hit_size &&
          rel_y >= sprite_py && rel_y < sprite_py + hit_size) {
        object_interaction_.SelectEntity(EntityType::Sprite, idx);
        ImGui::OpenPopup(kPopupId);
        break;
      }
    }

    // Hit-test pot items
    if (!ImGui::IsPopupOpen(kPopupId)) {
      const auto& pot_items = room.GetPotItems();
      for (size_t idx = 0; idx < pot_items.size(); ++idx) {
        int item_px = pot_items[idx].GetPixelX();
        int item_py = pot_items[idx].GetPixelY();
        if (rel_x >= item_px && rel_x < item_px + hit_size &&
            rel_y >= item_py && rel_y < item_py + hit_size) {
          object_interaction_.SelectEntity(EntityType::Item, idx);
          ImGui::OpenPopup(kPopupId);
          break;
        }
      }
    }

    // Hit-test tile objects (variable-size entities)
    if (!ImGui::IsPopupOpen(kPopupId)) {
      const auto& objects = room.GetTileObjects();
      for (size_t idx = 0; idx < objects.size(); ++idx) {
        const auto& obj = objects[idx];
        int obj_px = obj.x() * 8;
        int obj_py = obj.y() * 8;
        auto [obj_w, obj_h] =
            zelda3::DimensionService::Get().GetPixelDimensions(obj);
        obj_w = std::max(obj_w, 8);
        obj_h = std::max(obj_h, 8);
        if (rel_x >= obj_px && rel_x < obj_px + obj_w && rel_y >= obj_py &&
            rel_y < obj_py + obj_h) {
          object_interaction_.SetSelectedObjects({idx});
          ImGui::OpenPopup(kPopupId);
          break;
        }
      }
    }
  }

  // Render the context popup
  if (ImGui::BeginPopup(kPopupId)) {
    // Show actions based on what's selected
    if (object_interaction_.HasEntitySelection()) {
      auto sel = object_interaction_.GetSelectedEntity();
      if (sel.type == EntityType::Sprite) {
        const auto& sprites = room.GetSprites();
        if (sel.index < sprites.size()) {
          std::string label = zelda3::GetSpriteLabel(sprites[sel.index].id());
          ImGui::TextDisabled("Sprite: %02X %s", sprites[sel.index].id(),
                              label.c_str());
          ImGui::Separator();
        }
        if (ImGui::MenuItem("Delete Sprite")) {
          object_interaction_.entity_coordinator().DeleteSelectedEntity();
        }
      } else if (sel.type == EntityType::Item) {
        ImGui::TextDisabled("Pot Item");
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Item")) {
          object_interaction_.entity_coordinator().DeleteSelectedEntity();
        }
      }
    } else if (object_interaction_.GetSelectionCount() > 0) {
      const auto indices = object_interaction_.GetSelectedObjectIndices();
      if (indices.size() == 1) {
        const auto& objects = room.GetTileObjects();
        if (indices[0] < objects.size()) {
          std::string name = GetObjectName(objects[indices[0]].id_);
          ImGui::TextDisabled("Object: %03X %s", objects[indices[0]].id_,
                              name.c_str());
          ImGui::Separator();
        }
      } else {
        ImGui::TextDisabled("%zu objects selected",
                            object_interaction_.GetSelectionCount());
        ImGui::Separator();
      }
      if (ImGui::MenuItem("Delete")) {
        object_interaction_.HandleDeleteSelected();
      }
      if (ImGui::MenuItem("Copy")) {
        object_interaction_.HandleCopySelected();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Send to Front")) {
        object_interaction_.SendSelectedToFront();
      }
      if (ImGui::MenuItem("Send to Back")) {
        object_interaction_.SendSelectedToBack();
      }
    }
    ImGui::EndPopup();
  }
}

// Room layout visualization

// Object visualization methods
void DungeonCanvasViewer::DrawObjectPositionOutlines(
    const gui::CanvasRuntime& rt, const zelda3::Room& room) {
  // Draw colored rectangles showing object positions
  // This helps visualize object placement even if graphics don't render
  // correctly

  const auto& theme = AgentUI::GetTheme();
  const auto& objects = room.GetTileObjects();

  for (const auto& obj : objects) {
    // Filter by object type (default to true if unknown type)
    bool show_this_type = true;  // Default to showing
    if (obj.id_ < 0x100) {
      show_this_type = object_outline_toggles_.show_type1_objects;
    } else if (obj.id_ >= 0x100 && obj.id_ < 0x200) {
      show_this_type = object_outline_toggles_.show_type2_objects;
    } else if (obj.id_ >= 0xF00) {
      show_this_type = object_outline_toggles_.show_type3_objects;
    }
    // else: unknown type, use default (true)

    // Filter by layer (default to true if unknown layer)
    bool show_this_layer = true;  // Default to showing
    if (obj.GetLayerValue() == 0) {
      show_this_layer = object_outline_toggles_.show_layer0_objects;
    } else if (obj.GetLayerValue() == 1) {
      show_this_layer = object_outline_toggles_.show_layer1_objects;
    } else if (obj.GetLayerValue() == 2) {
      show_this_layer = object_outline_toggles_.show_layer2_objects;
    }
    // else: unknown layer, use default (true)

    // Skip if filtered out
    if (!show_this_type || !show_this_layer) {
      continue;
    }

    // Use GetSelectionBoundsPixels which includes position offsets for objects
    // that extend in negative directions (diagonals, moving walls, etc.)
    auto [canvas_x, canvas_y, width, height] =
        zelda3::DimensionService::Get().GetSelectionBoundsPixels(obj);

    // IMPORTANT: Do NOT apply canvas scale here - DrawRect handles it
    // Clamp to reasonable sizes (in logical space)
    width = std::min(width, 512);
    height = std::min(height, 512);

    // Color-code by layer
    ImVec4 outline_color;
    if (obj.GetLayerValue() == 0) {
      outline_color = theme.dungeon_outline_layer0;  // Red for layer 0
    } else if (obj.GetLayerValue() == 1) {
      outline_color = theme.dungeon_outline_layer1;  // Green for layer 1
    } else {
      outline_color = theme.dungeon_outline_layer2;  // Blue for layer 2
    }

    // Draw outline rectangle using runtime-based helper
    gui::DrawRect(rt, canvas_x, canvas_y, width, height, outline_color);

    // Draw object ID label with hex ID, abbreviated name, and draw stream.
    std::string name = GetObjectName(obj.id_);
    // Truncate name to fit (approx 12 chars for small objects)
    if (name.length() > 12) {
      name = name.substr(0, 10) + "..";
    }
    std::string label;
    if (obj.id_ >= 0x100) {
      label = absl::StrFormat("0x%03X\n%s\n%s  [%dx%d]", obj.id_, name.c_str(),
                              GetObjectStreamLabel(obj.GetLayerValue()), width,
                              height);
    } else {
      label = absl::StrFormat("0x%02X\n%s\n%s  [%dx%d]", obj.id_, name.c_str(),
                              GetObjectStreamLabel(obj.GetLayerValue()), width,
                              height);
    }
    gui::DrawText(rt, label, canvas_x + 1, canvas_y + 1);
  }
}

const DungeonRenderingHelpers::CollisionOverlayCache&
DungeonCanvasViewer::GetCollisionOverlayCache(int room_id) {
  auto it = collision_overlay_cache_.find(room_id);
  if (it != collision_overlay_cache_.end()) {
    return it->second;
  }

  DungeonRenderingHelpers::CollisionOverlayCache cache;
  cache.entries.clear();

  if (!rom_ || !rom_->is_loaded()) {
    collision_overlay_cache_.emplace(room_id, cache);
    return collision_overlay_cache_.at(room_id);
  }

  auto map_or = zelda3::LoadCustomCollisionMap(rom_, room_id);
  if (!map_or.ok()) {
    collision_overlay_cache_.emplace(room_id, cache);
    return collision_overlay_cache_.at(room_id);
  }

  const auto& map = map_or.value();
  cache.has_data = map.has_data;
  if (cache.has_data && !track_collision_config_.IsEmpty()) {
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        const uint8_t tile = map.tiles[static_cast<size_t>(y * 64 + x)];
        if (tile < 256 && (track_collision_config_.track_tiles[tile] ||
                           track_collision_config_.stop_tiles[tile] ||
                           track_collision_config_.switch_tiles[tile])) {
          cache.entries.push_back(
              DungeonRenderingHelpers::CollisionOverlayEntry{
                  static_cast<uint8_t>(x), static_cast<uint8_t>(y), tile});
        }
      }
    }
  }

  collision_overlay_cache_.emplace(room_id, std::move(cache));
  return collision_overlay_cache_.at(room_id);
}

// Room graphics management methods
absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  LOG_DEBUG("[LoadAndRender]", "START room_id=%d", room_id);

  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Invalid room ID");
    return absl::InvalidArgumentError("Invalid room ID");
  }

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: ROM not loaded");
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!rooms_) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Room data not available");
    return absl::FailedPreconditionError("Room data not available");
  }

  auto& room = (*rooms_)[room_id];
  LOG_DEBUG("[LoadAndRender]", "Got room reference");

  // Load the room's palette with bounds checking
  if (!game_data_) {
    LOG_ERROR("[LoadAndRender]", "GameData not available");
    return absl::FailedPreconditionError("GameData not available");
  }
  const auto& dungeon_main = game_data_->palette_groups.dungeon_main;
  if (!dungeon_main.empty()) {
    // Use Room's canonical two-level palette resolver; it already clamps to
    // the dungeon_main group size.
    current_palette_group_id_ =
        static_cast<uint64_t>(room.ResolveDungeonPaletteId());

    auto full_palette = dungeon_main[current_palette_group_id_];
    ASSIGN_OR_RETURN(current_palette_group_,
                     gfx::CreatePaletteGroupFromLargePalette(full_palette, 16));
    LOG_DEBUG("[LoadAndRender]", "Palette loaded: group_id=%zu",
              current_palette_group_id_);
  }

  // Render the room graphics (self-contained - handles all palette application)
  LOG_DEBUG("[LoadAndRender]", "Calling room.RenderRoomGraphics()...");
  room.ReloadGraphics(current_entrance_blockset_);
  LOG_DEBUG("[LoadAndRender]",
            "RenderRoomGraphics() complete - room buffers self-contained");

  LOG_DEBUG("[LoadAndRender]", "SUCCESS");
  return absl::OkStatus();
}

gfx::Bitmap* DungeonCanvasViewer::PrepareRoomCompositeBitmap(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms || !rooms_ || !rom_ ||
      !rom_->is_loaded()) {
    return nullptr;
  }

  auto& room = (*rooms_)[room_id];
  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  if (!bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
    (void)LoadAndRenderRoomGraphics(room_id);
  }

  auto& layer_mgr = GetRoomLayerManager(room_id);
  layer_mgr.ApplyLayerMerging(room.layer_merging());
  layer_mgr.ApplyRoomEffect(room.effect());

  auto& composite = room.GetCompositeBitmap(layer_mgr);
  if (!composite.is_active() || composite.width() <= 0) {
    return nullptr;
  }

  if (!composite.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &composite);
    composite.set_modified(false);
  } else if (composite.modified()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &composite);
    composite.set_modified(false);
  }

  return &composite;
}

void DungeonCanvasViewer::DrawRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms || !rooms_)
    return;

  float scale = canvas_.global_scale();

  if (gfx::Bitmap* composite = PrepareRoomCompositeBitmap(room_id);
      composite && composite->texture()) {
    canvas_.DrawBitmap(*composite, 0, 0, scale, 255);
  }
}

void DungeonCanvasViewer::DrawMaskHighlights(const gui::CanvasRuntime& rt,
                                             const zelda3::Room& room) {
  // Draw semi-transparent blue overlay on BG2/Layer 1 objects when mask mode
  // is active. This helps identify which objects are the "overlay" content
  // (platforms, statues, stairs) that create transparency holes in BG1.
  const auto& objects = room.GetTileObjects();

  // Create ObjectDrawer for dimension calculation
  zelda3::ObjectDrawer drawer(const_cast<zelda3::Room&>(room).rom(), room.id(),
                              nullptr);

  // Mask highlight color: semi-transparent cyan/blue
  // DrawRect draws a filled rectangle with a black outline
  ImVec4 mask_color(0.2f, 0.6f, 1.0f, 0.4f);  // Light blue, 40% opacity

  for (const auto& obj : objects) {
    // Only highlight Layer 1 (BG2) objects - these are the mask/overlay objects
    if (obj.GetLayerValue() != 1) {
      continue;
    }

    // Convert object position to canvas coordinates
    auto [canvas_x, canvas_y] =
        DungeonRenderingHelpers::RoomToCanvasCoordinates(obj.x(), obj.y());

    // Calculate object dimensions via DimensionService
    auto [width, height] =
        zelda3::DimensionService::Get().GetPixelDimensions(obj);

    // Clamp to reasonable sizes
    width = std::min(width, 512);
    height = std::min(height, 512);

    // Draw filled rectangle with semi-transparent overlay (includes black outline)
    gui::DrawRect(rt, canvas_x, canvas_y, width, height, mask_color);
  }
}

void DungeonCanvasViewer::DrawRoomHeader(zelda3::Room& room, int room_id) {
  ImGui::Separator();
  if (header_read_only_)
    ImGui::BeginDisabled();

  constexpr ImGuiTableFlags kPropsTableFlags =
      ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;

  if (ImGui::BeginTable("##RoomPropsTable", 2, kPropsTableFlags)) {
    const float nav_col_width = (ImGui::GetFrameHeight() * 4.0f) +
                                (ImGui::GetStyle().ItemSpacing.x * 3.0f) +
                                (ImGui::GetStyle().FramePadding.x * 2.0f);
    ImGui::TableSetupColumn("NavCol", ImGuiTableColumnFlags_WidthFixed,
                            nav_col_width);
    ImGui::TableSetupColumn("PropsCol", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawRoomNavigation(room_id);
    ImGui::TableNextColumn();
    DrawRoomPropertyTable(room, room_id);

    if (!compact_header_mode_ || show_room_details_) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled(ICON_MD_SELECT_ALL " Select");
      ImGui::TableNextColumn();
      DrawLayerControls(room, room_id);
    }

    ImGui::EndTable();
  }

  if (header_read_only_)
    ImGui::EndDisabled();
}

void DungeonCanvasViewer::DrawRoomNavigation(int room_id) {
  if (!room_swap_callback_ && !room_navigation_callback_)
    return;

  const int col = room_id % kRoomMatrixCols;
  const int row = room_id / kRoomMatrixCols;

  auto room_if_valid = [](int candidate) -> std::optional<int> {
    if (candidate < 0 || candidate >= zelda3::kNumberOfRooms) {
      return std::nullopt;
    }
    return candidate;
  };

  const auto north = room_if_valid(row > 0 ? room_id - kRoomMatrixCols : -1);
  const auto south =
      room_if_valid(row < kRoomMatrixRows - 1 ? room_id + kRoomMatrixCols : -1);
  const auto west = room_if_valid(col > 0 ? room_id - 1 : -1);
  const auto east = room_if_valid(col < kRoomMatrixCols - 1 ? room_id + 1 : -1);

  auto make_tooltip = [](const std::optional<int>& target,
                         const char* direction) -> std::string {
    if (!target.has_value())
      return "";
    return absl::StrFormat("%s: [%03X] %s", direction, *target,
                           zelda3::GetRoomLabel(*target));
  };

  auto nav_button = [&](const char* id, ImGuiDir dir,
                        const std::optional<int>& target,
                        const std::string& tooltip) {
    const bool enabled = target.has_value();
    if (!enabled) {
      ImGui::BeginDisabled();
    }
    const bool pressed = ImGui::ArrowButton(id, dir);
    if (!enabled) {
      ImGui::EndDisabled();
    }
    if (enabled && ImGui::IsItemHovered() && !tooltip.empty()) {
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
    if (pressed && enabled) {
      if (room_swap_callback_) {
        room_swap_callback_(room_id, *target);
      } else if (room_navigation_callback_) {
        room_navigation_callback_(*target);
      }
    }
  };

  ImGui::PushID(room_id);
  ImGui::BeginGroup();
  nav_button("##RoomNavWest", ImGuiDir_Left, west, make_tooltip(west, "West"));
  ImGui::SameLine();
  nav_button("##RoomNavNorth", ImGuiDir_Up, north,
             make_tooltip(north, "North"));
  ImGui::SameLine();
  nav_button("##RoomNavSouth", ImGuiDir_Down, south,
             make_tooltip(south, "South"));
  ImGui::SameLine();
  nav_button("##RoomNavEast", ImGuiDir_Right, east, make_tooltip(east, "East"));
  ImGui::EndGroup();
  ImGui::PopID();
}

void DungeonCanvasViewer::DrawRoomPropertyTable(zelda3::Room& room,
                                                int room_id) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text(ICON_MD_TUNE " %03X", room_id);
  if (room.HasUnsavedChanges()) {
    ImGui::SameLine(0, 6);
    ImGui::TextColored(gui::ConvertColorToImVec4(
                           gui::ThemeManager::Get().GetCurrentTheme().warning),
                       ICON_MD_EDIT " Pending");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Room changes are pending in the editor. Apply Room writes them to "
          "the loaded ROM buffer.");
    }
  }
  ImGui::SameLine();

  if (pin_callback_) {
    if (gui::ThemedIconButton(is_pinned_ ? ICON_MD_PUSH_PIN : ICON_MD_PIN,
                              is_pinned_ ? "Unpin Room" : "Pin Room",
                              ImVec2(0, 0), is_pinned_)) {
      pin_callback_(!is_pinned_);
    }
    ImGui::SameLine();
  }

  if (gui::ThemedIconButton(
          show_room_details_ ? ICON_MD_EXPAND_LESS : ICON_MD_EXPAND_MORE,
          show_room_details_ ? "Hide Details" : "Show Details")) {
    show_room_details_ = !show_room_details_;
  }
  ImGui::SameLine();

  // Core properties with human-readable names
  auto hex_input = [&](const char* label, const char* icon, uint8_t* val,
                       uint8_t max, const char* tooltip) {
    ImGui::TextDisabled("%s", icon);
    ImGui::SameLine(0, 2);

    // Apply flash feedback to the background of the input
    const std::string anim_id = std::string(label) + "_Flash";
    const ImVec4 flash_color = gui::GetAnimator().AnimateColor(
        "##RoomProps", anim_id, ImVec4(0, 0, 0, 0), 8.0f);

    if (flash_color.w > 0.01f) {
      ImGui::PushStyleColor(ImGuiCol_FrameBg, flash_color);
    }

    auto res = gui::InputHexByteEx(label, val, max, 32.f, true);
    bool changed = res.ShouldApply();

    if (flash_color.w > 0.01f) {
      ImGui::PopStyleColor();
    }

    gui::ValueChangeFlash(changed, anim_id.c_str());

    if (changed) {
      return true;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", tooltip);
    return false;
  };

  uint8_t bs = room.blockset();
  if (hex_input("##BS", ICON_MD_VIEW_MODULE, &bs, 81, "Blockset")) {
    room.SetBlockset(bs);
    if (room.rom() && room.rom()->is_loaded())
      room.RenderRoomGraphics();
  }
  // Show dungeon name after blockset hex input
  ImGui::SameLine(0, 2);
  ImGui::TextDisabled("(%s)", DungeonRoomSelector::GetBlocksetGroupName(bs));
  ImGui::SameLine();

  uint8_t pal = room.palette();
  if (hex_input("##Pal", ICON_MD_PALETTE, &pal, 71, "Palette")) {
    room.SetPalette(pal);
    if (room.rom() && room.rom()->is_loaded())
      room.RenderRoomGraphics();
  }
  ImGui::SameLine();

  uint8_t lyr = room.layout_id();
  if (hex_input("##Lyr", ICON_MD_GRID_VIEW, &lyr, 7, "Layout")) {
    room.SetLayoutId(lyr);
    room.MarkLayoutDirty();
    if (room.rom() && room.rom()->is_loaded())
      room.RenderRoomGraphics();
  }
  ImGui::SameLine();

  uint8_t ss = room.spriteset();
  if (hex_input("##SS", ICON_MD_PEST_CONTROL, &ss, 143, "Spriteset")) {
    room.SetSpriteset(ss);
    if (room.rom() && room.rom()->is_loaded())
      room.RenderRoomGraphics();
  }

  if (show_room_details_) {
    // Show extended properties
    ImGui::TextDisabled("Floor: %d | Effect: %d | Tag1: %d | Tag2: %d",
                        room.floor1(), room.effect(), room.tag1(), room.tag2());
  }
}

void DungeonCanvasViewer::DrawCompactLayerToggles(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return;
  }

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const float compact_gap =
      std::max(2.0f, gui::LayoutHelpers::GetStandardSpacing() * 0.25f);
  const float compact_padding =
      std::clamp(gui::LayoutHelpers::GetButtonPadding(), 2.0f, 6.0f);

  gui::StyleVarGuard compact_style({
      {ImGuiStyleVar_FramePadding,
       ImVec2(compact_padding, compact_padding * 0.5f)},
      {ImGuiStyleVar_ItemSpacing, ImVec2(compact_gap, 0.0f)},
  });

  auto as_button_color = [](ImVec4 color, float alpha) {
    color.w = alpha;
    return color;
  };

  const ImVec4 inactive_color =
      as_button_color(gui::ConvertColorToImVec4(theme.frame_bg), 0.55f);
  const ImVec4 inactive_hover =
      as_button_color(gui::ConvertColorToImVec4(theme.frame_bg_hovered), 0.7f);
  const ImVec4 inactive_active =
      as_button_color(gui::ConvertColorToImVec4(theme.frame_bg_active), 0.85f);

  auto draw_toggle = [&](const char* label, bool enabled, ImVec4 active_color,
                         const char* tooltip, auto&& on_toggle) {
    const ImVec4 button = enabled ? active_color : inactive_color;
    const ImVec4 hovered =
        enabled ? as_button_color(
                      gui::ConvertColorToImVec4(theme.button_hovered), 0.95f)
                : inactive_hover;
    const ImVec4 pressed =
        enabled ? as_button_color(
                      gui::ConvertColorToImVec4(theme.button_active), 1.0f)
                : inactive_active;

    gui::StyleColorGuard button_colors({
        {ImGuiCol_Button, button},
        {ImGuiCol_ButtonHovered, hovered},
        {ImGuiCol_ButtonActive, pressed},
    });

    if (ImGui::SmallButton(label)) {
      on_toggle();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
  };

  const bool bg1_visible = IsBG1Visible(room_id);
  draw_toggle("BG1##LayerToggleBG1", bg1_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.info), 0.9f),
              "Toggle BG1 (main layer) visibility",
              [&]() { SetBG1Visible(room_id, !bg1_visible); });

  ImGui::SameLine();
  const bool bg2_visible = IsBG2Visible(room_id);
  draw_toggle("BG2##LayerToggleBG2", bg2_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.warning), 0.9f),
              "Toggle BG2 (overlay layer) visibility",
              [&]() { SetBG2Visible(room_id, !bg2_visible); });

  ImGui::SameLine();
  const bool sprites_visible = entity_visibility_.show_sprites;
  draw_toggle(ICON_MD_PEST_CONTROL "##LayerToggleSprites", sprites_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.success), 0.9f),
              "Toggle sprite visibility", [&]() {
                entity_visibility_.show_sprites =
                    !entity_visibility_.show_sprites;
              });

  ImGui::SameLine();
  draw_toggle(ICON_MD_GRID_ON "##LayerToggleGrid", show_grid_,
              as_button_color(gui::ConvertColorToImVec4(theme.secondary), 0.9f),
              "Toggle grid overlay", [&]() { show_grid_ = !show_grid_; });

  ImGui::SameLine();
  draw_toggle(
      ICON_MD_CROP_FREE "##LayerToggleBounds", show_object_bounds_,
      as_button_color(gui::ConvertColorToImVec4(theme.selection_primary), 0.9f),
      "Toggle object bounds overlay",
      [&]() { show_object_bounds_ = !show_object_bounds_; });

  ImGui::SameLine();
  const bool pots_visible = entity_visibility_.show_pot_items;
  draw_toggle(ICON_MD_INVENTORY_2 "##LayerTogglePots", pots_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.success), 0.9f),
              "Toggle pot item markers", [&]() {
                entity_visibility_.show_pot_items =
                    !entity_visibility_.show_pot_items;
              });

  ImGui::SameLine();
  draw_toggle(ICON_MD_FILTER_CENTER_FOCUS "##LayerToggleCollision",
              show_custom_collision_overlay_,
              as_button_color(gui::ConvertColorToImVec4(theme.warning), 0.9f),
              "Toggle custom collision overlay", [&]() {
                show_custom_collision_overlay_ =
                    !show_custom_collision_overlay_;
              });
}

void DungeonCanvasViewer::DrawLayerControls(zelda3::Room& /*room*/,
                                            int room_id) {
  auto& interaction = object_interaction_;

  interaction.SetLayersMerged(GetRoomLayerManager(room_id).AreLayersMerged());
  int current_filter = interaction.GetLayerFilter();

  auto radio = [&](const char* label, int filter) {
    if (ImGui::RadioButton(label, current_filter == filter)) {
      interaction.SetLayerFilter(filter);
    }
    ImGui::SameLine();
  };

  radio("All", ObjectSelection::kLayerAll);
  radio("L1", ObjectSelection::kLayer1);
  radio("L2", ObjectSelection::kLayer2);
  radio("L3", ObjectSelection::kLayer3);
}

}  // namespace yaze::editor
