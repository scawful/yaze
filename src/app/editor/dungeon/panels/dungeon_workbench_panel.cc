#include "app/editor/dungeon/panels/dungeon_workbench_panel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/panels/shortcut_legend_panel.h"
#include "app/editor/dungeon/widgets/dungeon_status_bar.h"
#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

// Object type category names based on ID range
const char* GetObjectCategory(int object_id) {
  if (object_id < 0x100)
    return "Standard";
  if (object_id < 0x200)
    return "Extended";
  if (object_id >= 0xF80)
    return "Special";
  return "Unknown";
}

// Derive a short dungeon-group label from a room's blockset value.
// Blockset 0-12 map to vanilla ALTTP dungeons; higher values are custom.
const char* GetBlocksetGroupName(uint8_t blockset) {
  static const char* kGroupNames[] = {
      "HC/Sewers",  // 0
      "Eastern",    // 1
      "Desert",     // 2
      "Hera",       // 3
      "A-Tower",    // 4
      "PoD",        // 5
      "Swamp",      // 6
      "Skull",      // 7
      "Thieves",    // 8
      "Ice",        // 9
      "Misery",     // 10
      "Turtle",     // 11
      "GT",         // 12
  };
  constexpr size_t kCount = sizeof(kGroupNames) / sizeof(kGroupNames[0]);
  return blockset < kCount ? kGroupNames[blockset] : "Custom";
}

const char* GetObjectStreamName(int layer_value) {
  switch (layer_value) {
    case 0:
      return "Primary (main pass)";
    case 1:
      return "BG2 overlay";
    case 2:
      return "BG1 overlay";
    default:
      return "Unknown";
  }
}

// Pot item names for the inspector
const char* GetPotItemName(uint8_t item) {
  static const char* kNames[] = {
      "Nothing",       "Green Rupee",  "Rock",         "Bee",
      "Heart (4)",     "Bomb (4)",     "Heart",        "Blue Rupee",
      "Key",           "Arrow (5)",    "Bomb (1)",     "Heart",
      "Magic (Small)", "Full Magic",   "Cucco",        "Green Soldier",
      "Bush Stal",     "Blue Soldier", "Landmine",     "Heart",
      "Fairy",         "Heart",        "Nothing (22)", "Hole",
      "Warp",          "Staircase",    "Bombable",     "Switch",
  };
  constexpr size_t kCount = sizeof(kNames) / sizeof(kNames[0]);
  return item < kCount ? kNames[item] : "Unknown";
}

float ClampWorkbenchPaneWidth(float desired_width, float min_width,
                              float max_width) {
  return std::clamp(desired_width, min_width, std::max(min_width, max_width));
}

float CalcWorkbenchIconButtonWidth(const char* icon, float button_height) {
  if (!icon || !*icon) {
    return button_height;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float text_w = ImGui::CalcTextSize(icon).x;
  const float padding = std::max(2.0f, style.FramePadding.x);
  const float width = std::ceil(text_w + (style.FramePadding.x * 2.0f) + padding);
  return std::max(button_height, width);
}

struct ResponsiveWorkbenchLayout {
  bool show_left = false;
  bool show_right = false;
  bool compact_left = false;
  bool compact_right = false;
};

ResponsiveWorkbenchLayout ResolveResponsiveWorkbenchLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, bool want_left, bool want_right) {
  ResponsiveWorkbenchLayout result;
  result.show_left = want_left;
  result.show_right = want_right;

  auto required_width = [&](bool left, bool right, bool compact_left,
                            bool compact_right) {
    float required = min_canvas_width;
    required += left ? (compact_left ? std::max(136.0f, min_sidebar_width * 0.72f)
                                     : min_sidebar_width)
                     : 0.0f;
    required += right ? (compact_right ? std::max(200.0f, min_sidebar_width + 32.0f)
                                       : min_sidebar_width)
                      : 0.0f;
    if (left) {
      required += splitter_width;
    }
    if (right) {
      required += splitter_width;
    }
    return required;
  };

  if (result.show_left &&
      total_width < required_width(result.show_left, result.show_right, false,
                                   false)) {
    result.compact_left = true;
  }
  if (result.show_right &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, false)) {
    result.compact_right = true;
  }
  if (result.show_right &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, result.compact_right)) {
    result.show_right = false;
    result.compact_right = false;
  }
  if (result.show_left &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, result.compact_right)) {
    result.show_left = false;
    result.compact_left = false;
  }

  return result;
}

void DrawVerticalSplitter(const char* id, float height, float* pane_width,
                          float min_width, float max_width,
                          bool resize_from_left_edge) {
  if (!pane_width) {
    return;
  }

  const float splitter_width = gui::UIConfig::kSplitterWidth;
  const ImVec2 splitter_pos = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(splitter_width, std::max(height, 1.0f)));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    *pane_width = ClampWorkbenchPaneWidth(*pane_width, min_width, max_width);
  }
  if (active) {
    const float delta = ImGui::GetIO().MouseDelta.x;
    const float proposed =
        resize_from_left_edge ? (*pane_width - delta) : (*pane_width + delta);
    *pane_width = ClampWorkbenchPaneWidth(proposed, min_width, max_width);
    ImGui::SetTooltip("Width: %.0f px", *pane_width);
  }

  ImVec4 splitter_color = gui::GetOutlineVec4();
  splitter_color.w = active ? 0.95f : (hovered ? 0.72f : 0.35f);
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(splitter_pos.x + splitter_width * 0.5f, splitter_pos.y),
      ImVec2(splitter_pos.x + splitter_width * 0.5f, splitter_pos.y + height),
      ImGui::GetColorU32(splitter_color), active ? 2.0f : 1.0f);
}

void DrawWorkbenchInspectorSectionHeader(const char* label) {
  ImGui::SeparatorText(label);
}

bool BeginWorkbenchInspectorSection(const char* label, bool default_open) {
  gui::StyleVarGuard frame_padding_guard(
      ImGuiStyleVar_FramePadding,
      ImVec2(ImGui::GetStyle().FramePadding.x,
             std::max(5.0f, ImGui::GetStyle().FramePadding.y + 1.0f)));
  return ImGui::CollapsingHeader(
      label, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

}  // namespace

DungeonWorkbenchPanel::DungeonWorkbenchPanel(
    DungeonRoomSelector* room_selector, int* current_room_id,
    std::function<void(int)> on_room_selected,
    std::function<void(int, RoomSelectionIntent)> on_room_selected_with_intent,
    std::function<void(int)> on_save_room,
    std::function<DungeonCanvasViewer*()> get_viewer,
    std::function<DungeonCanvasViewer*()> get_compare_viewer,
    std::function<const std::deque<int>&()> get_recent_rooms,
    std::function<void(int)> forget_recent_room,
    std::function<void(const std::string&)> show_panel,
    std::function<void(bool)> set_workflow_mode, Rom* rom)
    : room_selector_(room_selector),
      current_room_id_(current_room_id),
      on_room_selected_(std::move(on_room_selected)),
      on_room_selected_with_intent_(std::move(on_room_selected_with_intent)),
      on_save_room_(std::move(on_save_room)),
      get_viewer_(std::move(get_viewer)),
      get_compare_viewer_(std::move(get_compare_viewer)),
      get_recent_rooms_(std::move(get_recent_rooms)),
      forget_recent_room_(std::move(forget_recent_room)),
      show_panel_(std::move(show_panel)),
      set_workflow_mode_(std::move(set_workflow_mode)),
      rom_(rom) {}

std::string DungeonWorkbenchPanel::GetId() const {
  return "dungeon.workbench";
}
std::string DungeonWorkbenchPanel::GetDisplayName() const {
  return "Dungeon Workbench";
}
std::string DungeonWorkbenchPanel::GetIcon() const {
  return ICON_MD_WORKSPACES;
}
std::string DungeonWorkbenchPanel::GetEditorCategory() const {
  return "Dungeon";
}
int DungeonWorkbenchPanel::GetPriority() const {
  return 10;
}

void DungeonWorkbenchPanel::SetRom(Rom* rom) {
  rom_ = rom;
  room_dungeon_cache_.clear();
  room_dungeon_cache_built_ = false;
}

void DungeonWorkbenchPanel::DrawSidebarHeader(float button_size) {
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(4.0f, ImGui::GetStyle().ItemSpacing.x * 0.75f),
             ImGui::GetStyle().ItemSpacing.y));

  const bool can_open_overview = static_cast<bool>(show_panel_);
  const float collapse_w =
      CalcWorkbenchIconButtonWidth(ICON_MD_CHEVRON_LEFT, button_size);
  const float matrix_w =
      CalcWorkbenchIconButtonWidth(ICON_MD_GRID_VIEW, button_size);
  const float map_w = CalcWorkbenchIconButtonWidth(ICON_MD_MAP, button_size);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float header_width = ImGui::GetContentRegionAvail().x;
  const float action_cluster_w = collapse_w + spacing;
  const float overview_buttons_w = matrix_w + spacing + map_w;
  const bool stack_header_actions = header_width < 232.0f;
  const bool stack_mode_switch = header_width < 190.0f;
  const bool show_both_overview_buttons =
      can_open_overview &&
      header_width > (180.0f + action_cluster_w + overview_buttons_w);
  const bool show_one_overview_button =
      !show_both_overview_buttons && can_open_overview &&
      header_width > (140.0f + action_cluster_w + matrix_w);

  ImGui::TextDisabled(ICON_MD_EXPLORE " Navigate");
  if (!stack_header_actions) {
    ImGui::SameLine();
    const float action_start_x =
        std::max(ImGui::GetCursorPosX(),
                 ImGui::GetWindowContentRegionMax().x - action_cluster_w -
                     (show_both_overview_buttons ? overview_buttons_w
                                                 : (show_one_overview_button
                                                        ? matrix_w + spacing
                                                        : 0.0f)));
    ImGui::SetCursorPosX(action_start_x);
  } else {
    ImGui::Spacing();
  }

  if (show_both_overview_buttons || show_one_overview_button) {
    if (ImGui::Button(ICON_MD_GRID_VIEW "##OpenRoomMatrix",
                      ImVec2(matrix_w, button_size))) {
      show_panel_("dungeon.room_matrix");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Open room matrix overview");
    }

    if (show_both_overview_buttons) {
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_MAP "##OpenDungeonMap",
                        ImVec2(map_w, button_size))) {
        show_panel_("dungeon.dungeon_map");
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open dungeon map overview");
      }
    }

    ImGui::SameLine();
  }
  if (ImGui::Button(ICON_MD_CHEVRON_LEFT "##CollapseRooms",
                    ImVec2(collapse_w, button_size))) {
    layout_state_.show_left_sidebar = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Collapse navigation");
  }

  ImGui::Spacing();
  const float mode_width = stack_mode_switch
                               ? -1.0f
                               : std::max(72.0f, (header_width - spacing) * 0.5f);
  if (ImGui::Selectable(ICON_MD_LIST " Rooms", sidebar_mode_ == SidebarMode::Rooms,
                        0, ImVec2(mode_width, 0.0f))) {
    sidebar_mode_ = SidebarMode::Rooms;
  }
  if (!stack_mode_switch) {
    ImGui::SameLine();
  }
  if (ImGui::Selectable(ICON_MD_DOOR_FRONT " Entrances",
                        sidebar_mode_ == SidebarMode::Entrances, 0,
                        ImVec2(mode_width, 0.0f))) {
    sidebar_mode_ = SidebarMode::Entrances;
  }
}

void DungeonWorkbenchPanel::DrawSidebarContent() {
  if (!room_selector_) {
    ImGui::TextDisabled("Room navigation unavailable");
    return;
  }

  ImGui::PushID("WorkbenchSidebarMode");
  switch (sidebar_mode_) {
    case SidebarMode::Rooms:
      room_selector_->DrawRoomSelector();
      break;
    case SidebarMode::Entrances:
      room_selector_->DrawEntranceSelector();
      break;
  }
  ImGui::PopID();
}

void DungeonWorkbenchPanel::Draw(bool* p_open) {
  (void)p_open;
  const auto& theme = AgentUI::GetTheme();

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextDisabled(ICON_MD_INFO " Load a ROM to edit dungeon rooms.");
    return;
  }
  if (!room_selector_ || !current_room_id_ || !get_viewer_) {
    ImGui::TextColored(theme.text_error_red, "Dungeon Workbench not wired");
    return;
  }

  DungeonCanvasViewer* primary_viewer = get_viewer_ ? get_viewer_() : nullptr;
  DungeonCanvasViewer* compare_viewer =
      get_compare_viewer_ ? get_compare_viewer_() : nullptr;

  if (current_room_id_) {
    DungeonWorkbenchToolbarParams params;
    params.layout = &layout_state_;
    params.current_room_id = current_room_id_;
    params.previous_room_id = &previous_room_id_;
    params.split_view_enabled = &split_view_enabled_;
    params.compare_room_id = &compare_room_id_;
    params.primary_viewer = primary_viewer;
    params.compare_viewer = compare_viewer;
    params.on_room_selected = on_room_selected_;
    params.get_recent_rooms = get_recent_rooms_;
    params.set_workflow_mode = set_workflow_mode_;
    params.compare_search_buf = compare_search_buf_;
    params.compare_search_buf_size = sizeof(compare_search_buf_);
    const bool request_panel_workflow = DungeonWorkbenchToolbar::Draw(params);
    if (request_panel_workflow && set_workflow_mode_) {
      // Defer panel visibility mutation until toolbar child/table scopes closed.
      set_workflow_mode_(false);
      return;
    }
    ImGui::Spacing();
  }

  const float btn = gui::LayoutHelpers::GetTouchSafeWidgetHeight();
  const float splitter_w = gui::UIConfig::kSplitterWidth;
  const float total_w = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
  const float total_h = std::max(ImGui::GetContentRegionAvail().y, 1.0f);
  const float min_sidebar_w = gui::UIConfig::kContentMinWidthSidebar;
  const float min_canvas_w = std::max(320.0f, min_sidebar_w + 120.0f);
  const ResponsiveWorkbenchLayout responsive = ResolveResponsiveWorkbenchLayout(
      total_w, min_canvas_w, min_sidebar_w, splitter_w,
      layout_state_.show_left_sidebar, layout_state_.show_right_inspector);
  const bool show_left = responsive.show_left;
  const bool show_right = responsive.show_right;
  const float compact_left_w = std::max(136.0f, min_sidebar_w * 0.72f);
  const float compact_right_w = std::max(200.0f, min_sidebar_w + 32.0f);
  const float active_left_min_w =
      responsive.compact_left ? compact_left_w : min_sidebar_w;
  const float active_right_min_w =
      responsive.compact_right ? compact_right_w : min_sidebar_w;

  float left_w = show_left
                     ? (responsive.compact_left ? compact_left_w
                                                : layout_state_.left_width)
                     : 0.0f;
  float right_w = show_right
                      ? (responsive.compact_right ? compact_right_w
                                                 : layout_state_.right_width)
                      : 0.0f;
  const float max_left_w =
      total_w - right_w - min_canvas_w - (show_left ? splitter_w : 0.0f) -
      (show_right ? splitter_w : 0.0f);
  const float max_right_w =
      total_w - left_w - min_canvas_w - (show_left ? splitter_w : 0.0f) -
      (show_right ? splitter_w : 0.0f);
  if (show_left) {
    left_w =
        ClampWorkbenchPaneWidth(left_w, active_left_min_w,
                                std::max(active_left_min_w, max_left_w));
    if (!responsive.compact_left) {
      layout_state_.left_width = left_w;
    }
  } else {
    left_w = 0.0f;
  }
  if (show_right) {
    right_w = ClampWorkbenchPaneWidth(right_w, active_right_min_w,
                                      std::max(active_right_min_w, max_right_w));
    if (!responsive.compact_right) {
      layout_state_.right_width = right_w;
    }
  } else {
    right_w = 0.0f;
  }

  float center_w = total_w - left_w - right_w;
  if (show_left) {
    center_w -= splitter_w;
  }
  if (show_right) {
    center_w -= splitter_w;
  }
  if (center_w < min_canvas_w) {
    float deficit = min_canvas_w - center_w;
    if (show_right) {
      const float shrink =
          std::min(deficit, right_w - active_right_min_w);
      right_w -= shrink;
      if (!responsive.compact_right) {
        layout_state_.right_width = right_w;
      }
      deficit -= shrink;
    }
    if (deficit > 0.0f && show_left) {
      const float shrink =
          std::min(deficit, left_w - active_left_min_w);
      left_w -= shrink;
      if (!responsive.compact_left) {
        layout_state_.left_width = left_w;
      }
      deficit -= shrink;
    }
    center_w = std::max(1.0f, total_w - left_w - right_w -
                                  (show_left ? splitter_w : 0.0f) -
                                  (show_right ? splitter_w : 0.0f));
  }

  // Sidebar: room navigation (list + filter)
  if (show_left) {
    const bool sidebar_open =
        ImGui::BeginChild("##DungeonWorkbenchSidebar", ImVec2(left_w, total_h),
                          true);
    if (sidebar_open) {
      DrawSidebarHeader(btn);
      ImGui::Separator();
      DrawSidebarContent();
    }
    ImGui::EndChild();
  }
  if (show_left) {
    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##DungeonWorkbenchLeftSplitter", total_h,
                         &layout_state_.left_width, min_sidebar_w,
                         total_w - right_w - min_canvas_w -
                             (show_right ? splitter_w : 0.0f),
                         false);
  }

  // Canvas: main room view (minimum height so canvas never collapses)
  if (show_left) {
    ImGui::SameLine(0.0f, 0.0f);
  }
  const bool canvas_open =
      ImGui::BeginChild("##DungeonWorkbenchCanvas", ImVec2(center_w, total_h),
                        false);
  if (canvas_open) {
    if (primary_viewer) {
      DrawRecentRoomTabs();
      if (split_view_enabled_) {
        DrawSplitView(*primary_viewer);
      } else {
        primary_viewer->DrawDungeonCanvas(*current_room_id_);
      }

      // Status bar at the bottom of the canvas area
      {
        const char* tool_mode = get_tool_mode_ ? get_tool_mode_() : "Select";
        auto status =
            DungeonStatusBar::BuildState(*primary_viewer, tool_mode, false);
        status.workflow_mode = "Workbench";
        status.workflow_primary = true;
        if (can_undo_)
          status.can_undo = can_undo_();
        if (can_redo_)
          status.can_redo = can_redo_();
        if (undo_desc_) {
          static std::string s_undo_desc;
          s_undo_desc = undo_desc_();
          status.undo_desc =
              s_undo_desc.empty() ? nullptr : s_undo_desc.c_str();
        }
        if (redo_desc_) {
          static std::string s_redo_desc;
          s_redo_desc = redo_desc_();
          status.redo_desc =
              s_redo_desc.empty() ? nullptr : s_redo_desc.c_str();
        }
        if (undo_depth_)
          status.undo_depth = undo_depth_();
        status.on_undo = on_undo_;
        status.on_redo = on_redo_;
        DungeonStatusBar::Draw(status);
      }
    } else {
      ImGui::TextDisabled("No active viewer");
    }
  }
  ImGui::EndChild();

  // Inspector: room, selection, and quick-tool shelves.
  if (show_right) {
    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##DungeonWorkbenchRightSplitter", total_h,
                         &layout_state_.right_width, min_sidebar_w,
                         total_w - left_w - min_canvas_w -
                             (show_left ? splitter_w : 0.0f),
                         true);
    ImGui::SameLine(0.0f, 0.0f);
  }
  if (show_right) {
    const bool inspector_open =
        ImGui::BeginChild("##DungeonWorkbenchInspector",
                          ImVec2(right_w, total_h), true);
    if (inspector_open) {
      // Header with collapse button
      const float collapse_w =
          CalcWorkbenchIconButtonWidth(ICON_MD_CHEVRON_RIGHT, btn);
      ImGui::TextDisabled("%s",
                          responsive.compact_right ? ICON_MD_TUNE " Inspect"
                                                   : ICON_MD_TUNE " Inspector");
      ImGui::SameLine();
      ImGui::SetCursorPosX(std::max(
          ImGui::GetCursorPosX(),
          ImGui::GetWindowContentRegionMax().x - collapse_w));
      if (ImGui::Button(ICON_MD_CHEVRON_RIGHT "##CollapseInspector",
                        ImVec2(collapse_w, btn))) {
        layout_state_.show_right_inspector = false;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Collapse inspector");
      }

      ImGui::Separator();
      if (primary_viewer) {
        DrawInspector(*primary_viewer);
      } else {
        ImGui::TextDisabled("No active viewer");
      }
    }
    ImGui::EndChild();
  }
}

void DungeonWorkbenchPanel::DrawRecentRoomTabs() {
  if (!get_recent_rooms_ || !current_room_id_ || !on_room_selected_) {
    return;
  }

  const auto& recent = get_recent_rooms_();
  if (recent.empty()) {
    return;
  }
  // Copy IDs up-front so we can safely mutate the underlying MRU list (close
  // tabs) without invalidating iterators mid-loop.
  std::vector<int> recent_ids(recent.begin(), recent.end());
  std::vector<int> to_forget;

  constexpr ImGuiTabBarFlags kFlags = ImGuiTabBarFlags_AutoSelectNewTabs |
                                      ImGuiTabBarFlags_FittingPolicyScroll |
                                      ImGuiTabBarFlags_TabListPopupButton;

  // Adaptive frame padding: larger tabs on touch/iPad for easier tapping
  const ImVec2 frame_pad = ImGui::GetStyle().FramePadding;
  const bool is_touch = gui::LayoutHelpers::IsTouchDevice();
  const float extra_y = is_touch ? 6.0f : 1.0f;
  const float extra_x = is_touch ? 4.0f : 0.0f;
  gui::StyleVarGuard pad_guard(
      ImGuiStyleVar_FramePadding,
      ImVec2(frame_pad.x + extra_x, frame_pad.y + extra_y));

  if (gui::BeginThemedTabBar("##DungeonRecentRooms", kFlags)) {
    for (int room_id : recent_ids) {
      bool open = true;
      const ImGuiTabItemFlags tab_flags =
          (room_id == *current_room_id_) ? ImGuiTabItemFlags_SetSelected : 0;
      const auto room_name = zelda3::GetRoomLabel(room_id);
      char tab_label[64];
      if (room_name.empty() || room_name == "Unknown") {
        snprintf(tab_label, sizeof(tab_label), "%03X##recent_%03X", room_id,
                 room_id);
      } else {
        snprintf(tab_label, sizeof(tab_label), "%03X %.12s##recent_%03X",
                 room_id, room_name.c_str(), room_id);
      }
      const bool selected = ImGui::BeginTabItem(tab_label, &open, tab_flags);

      if (!open && forget_recent_room_) {
        to_forget.push_back(room_id);
      }

      if (ImGui::IsItemHovered()) {
        const auto label = zelda3::GetRoomLabel(room_id);
        ImGui::SetTooltip("[%03X] %s", room_id, label.c_str());
      }

      if (ImGui::IsItemActivated() && room_id != *current_room_id_) {
        on_room_selected_(room_id);
      }

      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(ICON_MD_COMPARE_ARROWS " Compare")) {
          split_view_enabled_ = true;
          compare_room_id_ = room_id;
        }
        if (on_room_selected_with_intent_ &&
            ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open as Panel")) {
          on_room_selected_with_intent_(room_id,
                                        RoomSelectionIntent::kOpenStandalone);
        }
        if (forget_recent_room_ && ImGui::MenuItem(ICON_MD_CLOSE " Close")) {
          to_forget.push_back(room_id);
        }
        ImGui::EndPopup();
      }

      if (selected) {
        ImGui::EndTabItem();
      }
    }

    gui::EndThemedTabBar();
  }

  if (!to_forget.empty() && forget_recent_room_) {
    for (int rid : to_forget) {
      forget_recent_room_(rid);
    }
  }
}

void DungeonWorkbenchPanel::DrawSplitView(DungeonCanvasViewer& primary_viewer) {
  if (!current_room_id_ || !split_view_enabled_ || compare_room_id_ < 0) {
    if (split_view_enabled_) {
      split_view_enabled_ = false;
    }
    return;
  }

  // Choose a sensible default compare room (most-recent non-current).
  if (compare_room_id_ < 0 || compare_room_id_ == *current_room_id_) {
    if (get_recent_rooms_) {
      for (int rid : get_recent_rooms_()) {
        if (rid != *current_room_id_) {
          compare_room_id_ = rid;
          break;
        }
      }
    }
  }

  if (compare_room_id_ < 0) {
    // Nothing to compare yet.
    split_view_enabled_ = false;
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  constexpr ImGuiTableFlags kSplitFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_NoPadOuterX |
      ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_BordersInnerV;

  if (!ImGui::BeginTable("##DungeonWorkbenchSplit", 2, kSplitFlags)) {
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Compare", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();

  // Active pane (minimum height so canvas never collapses)
  ImGui::TableNextColumn();
  const bool split_active_open = gui::LayoutHelpers::BeginContentChild(
      "##SplitActive", ImVec2(0.0f, gui::UIConfig::kContentMinHeightCanvas),
      false);
  if (split_active_open) {
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
  }
  gui::LayoutHelpers::EndContentChild();

  // Compare pane
  ImGui::TableNextColumn();
  const bool split_compare_open = gui::LayoutHelpers::BeginContentChild(
      "##SplitCompare", ImVec2(0.0f, gui::UIConfig::kContentMinHeightCanvas),
      false);
  if (split_compare_open) {
    if (auto* compare_viewer =
            get_compare_viewer_ ? get_compare_viewer_() : nullptr) {
      if (layout_state_.sync_split_view) {
        compare_viewer->canvas().ApplyScaleSnapshot(
            primary_viewer.canvas().GetConfig());
      }
      compare_viewer->DrawDungeonCanvas(compare_room_id_);
    } else {
      ImGui::TextDisabled("No compare viewer");
    }
  }
  gui::LayoutHelpers::EndContentChild();

  ImGui::EndTable();
}

void DungeonWorkbenchPanel::BuildRoomDungeonCache() {
  room_dungeon_cache_.clear();
  room_dungeon_cache_built_ = true;  // Always set, even if ROM missing.
  if (!rom_ || !rom_->is_loaded())
    return;

  // Short dungeon names for display in the inspector badge.
  // Indices 0-13 = vanilla ALTTP dungeons; higher indices = custom/Oracle.
  static const char* const kShortNames[] = {
      "Sewers", "HC",    "Eastern", "Desert", "A-Tower", "Swamp",  "PoD",
      "Misery", "Skull", "Ice",     "Hera",   "Thieves", "Turtle", "GT",
  };
  constexpr int kVanillaCount =
      static_cast<int>(sizeof(kShortNames) / sizeof(kShortNames[0]));

  auto AddRoom = [&](int room_id, int dungeon_id) {
    if (room_id < 0)
      return;
    if (room_dungeon_cache_.contains(room_id))
      return;  // Entrance wins over spawn.
    if (dungeon_id >= 0 && dungeon_id < kVanillaCount) {
      room_dungeon_cache_[room_id] = kShortNames[dungeon_id];
    } else {
      char buf[16];
      snprintf(buf, sizeof(buf), "Dungeon %02X", dungeon_id);
      room_dungeon_cache_[room_id] = buf;
    }
  };

  // Standard entrances (0x00–0x83) — authoritative dungeon assignment.
  for (int i = 0; i < 0x84; ++i) {
    zelda3::RoomEntrance ent(rom_, static_cast<uint8_t>(i), false);
    int did = ent.dungeon_id_;
    if (did >= 0 && did < kVanillaCount) {
      room_dungeon_cache_[ent.room_] = kShortNames[did];
    } else {
      char buf[16];
      snprintf(buf, sizeof(buf), "Dungeon %02X", did);
      room_dungeon_cache_[ent.room_] = buf;
    }
  }

  // Spawn points (0x00–0x13) — fill in rooms not covered by entrances.
  for (int i = 0; i < 0x14; ++i) {
    zelda3::RoomEntrance ent(rom_, static_cast<uint8_t>(i), true);
    AddRoom(static_cast<int>(ent.room_), static_cast<int>(ent.dungeon_id_));
  }
}
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(6.0f, ImGui::GetStyle().ItemSpacing.x * 0.9f),
             std::max(6.0f, ImGui::GetStyle().ItemSpacing.y * 0.95f)));
  DrawInspectorPrimarySelector();

void DungeonWorkbenchPanel::DrawInspector(DungeonCanvasViewer& viewer) {
  DrawInspectorShelf(viewer);
}

void DungeonWorkbenchPanel::DrawInspectorPrimarySelector() {
  const float width = ImGui::GetContentRegionAvail().x;
  const bool stack = width < 260.0f;
  const float button_width = stack ? -1.0f : (width - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  const bool room_active = inspector_focus_ == InspectorFocus::Room;
  const bool selection_active = inspector_focus_ == InspectorFocus::Selection;

  if (room_active) {
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  }
  if (ImGui::Button(ICON_MD_CASTLE " Room", ImVec2(button_width, 0.0f))) {
    inspector_focus_ = InspectorFocus::Room;
  }
  if (room_active) {
    ImGui::PopStyleColor();
  }
  if (!stack) {
    ImGui::SameLine();
  }
  if (selection_active) {
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  }
  if (ImGui::Button(ICON_MD_SELECT_ALL " Selection", ImVec2(button_width, 0.0f))) {
    inspector_focus_ = InspectorFocus::Selection;
  }
  if (selection_active) {
    ImGui::PopStyleColor();
  }
}

void DungeonWorkbenchPanel::DrawInspectorCompactSummary(
    DungeonCanvasViewer& viewer) {
  const int room_id =
      (viewer.current_room_id() >= 0) ? viewer.current_room_id()
                                      : (current_room_id_ ? *current_room_id_ : -1);
  const auto& interaction = viewer.object_interaction();
  const size_t selected_objects = interaction.GetSelectionCount();
  const bool has_entity = interaction.HasEntitySelection();

  ImGui::TextDisabled(ICON_MD_SUMMARIZE " Summary");
  if (room_id >= 0) {
    ImGui::Text("[%03X] %s", room_id, zelda3::GetRoomLabel(room_id).c_str());
  } else {
    ImGui::TextDisabled("No room selected");
  }

  ImGui::Spacing();
  if (selected_objects > 0 || has_entity) {
    ImGui::TextDisabled(ICON_MD_SELECT_ALL " Focus");
    if (has_entity) {
      ImGui::BulletText("Entity selected");
    }
    if (selected_objects > 0) {
      ImGui::BulletText("%zu object%s selected", selected_objects,
                        selected_objects == 1 ? "" : "s");
    }
    if (ImGui::Button(ICON_MD_OPEN_IN_FULL " Open Selection",
                      ImVec2(-1, 0))) {
      inspector_focus_ = InspectorFocus::Selection;
    }
  } else {
    ImGui::TextDisabled("Nothing selected");
  }

  if (room_id >= 0) {
    ImGui::Spacing();
    if (on_save_room_ && ImGui::Button(ICON_MD_SAVE " Save Room", ImVec2(-1, 0))) {
      on_save_room_(room_id);
    }
    if (ImGui::Button(ICON_MD_CASTLE " Room Details", ImVec2(-1, 0))) {
      inspector_focus_ = InspectorFocus::Room;
    }
  }

  ImGui::Spacing();
  if (BeginWorkbenchInspectorSection(ICON_MD_VISIBILITY " View", true)) {
    DrawInspectorShelfView(viewer);
  }

  if (show_panel_ && BeginWorkbenchInspectorSection(ICON_MD_BUILD " Tools", false)) {
    DrawInspectorShelfTools(viewer);
  }
}

void DungeonWorkbenchPanel::DrawInspectorShelf(DungeonCanvasViewer& viewer) {
  if (ImGui::GetContentRegionAvail().x < 240.0f) {
    DrawInspectorCompactSummary(viewer);
    return;
  }

  ImGui::Spacing();
  if (inspector_focus_ == InspectorFocus::Room) {
    DrawInspectorShelfRoom(viewer);
  } else {
    DrawInspectorShelfSelection(viewer);
  }

  ImGui::Spacing();
  if (BeginWorkbenchInspectorSection(ICON_MD_VISIBILITY " View Options", true)) {
    DrawInspectorShelfView(viewer);
  }
  if (BeginWorkbenchInspectorSection(ICON_MD_BUILD " Quick Tools", false)) {
    DrawInspectorShelfTools(viewer);
  }
}

void DungeonWorkbenchPanel::DrawInspectorShelfRoom(
    DungeonCanvasViewer& viewer) {
  const auto& theme = AgentUI::GetTheme();

  int room_id = viewer.current_room_id();
  if (room_id < 0 && current_room_id_) {
    room_id = *current_room_id_;
  }

  const std::string room_label =
      (room_id >= 0) ? zelda3::GetRoomLabel(room_id) : std::string("None");
  DrawWorkbenchInspectorSectionHeader(ICON_MD_CASTLE " Room Summary");

  // Room badge: hex ID + copy button (only for valid room IDs).
  if (room_id >= 0) {
    ImGui::Text("Room: 0x%03X (%d)", room_id, room_id);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY "##CopyRoomId")) {
      char buf[16];
      snprintf(buf, sizeof(buf), "0x%03X", room_id);
      ImGui::SetClipboardText(buf);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Copy room ID (0x%03X) to clipboard", room_id);
    }
  } else {
    ImGui::TextUnformatted("Room: None");
  }

  // Dungeon group context: prefer ROM entrance-based lookup (accurate for
  // custom Oracle dungeons); fall back to blockset-derived name.
  if (!room_dungeon_cache_built_ && rom_ && rom_->is_loaded()) {
    BuildRoomDungeonCache();
  }
  if (room_id >= 0) {
    const char* group_name = nullptr;
    {
      auto cache_it = room_dungeon_cache_.find(room_id);
      if (cache_it != room_dungeon_cache_.end() && !cache_it->second.empty()) {
        group_name = cache_it->second.c_str();
      }
    }
    if (!group_name) {
      auto* rooms = viewer.rooms();
      if (rooms && room_id < static_cast<int>(rooms->size())) {
        group_name = GetBlocksetGroupName((*rooms)[room_id].blockset());
      }
    }
    if (group_name) {
      ImGui::TextDisabled(ICON_MD_CASTLE " %s – %s", group_name,
                          room_label.c_str());
    } else {
      ImGui::TextDisabled("%s", room_label.c_str());
    }
  } else {
    ImGui::TextDisabled("%s", room_label.c_str());
  }

  // Quick actions.
  DrawWorkbenchInspectorSectionHeader(ICON_MD_SAVE " Room Actions");
  if (on_save_room_ && room_id >= 0) {
    if (ImGui::Button(ICON_MD_SAVE " Save Room", ImVec2(-1, 0))) {
      on_save_room_(room_id);
    }
  }

  if (show_panel_) {
    ImGui::TextDisabled(ICON_MD_OPEN_IN_NEW " Open Panels");
    constexpr ImGuiTableFlags kPanelFlags =
        ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##WorkbenchRoomPanels", 2, kPanelFlags)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Button(ICON_MD_IMAGE " Graphics", ImVec2(-1, 0))) {
        show_panel_("dungeon.room_graphics");
      }
      ImGui::TableNextColumn();
      if (ImGui::Button(ICON_MD_SETTINGS " Settings", ImVec2(-1, 0))) {
        show_panel_("dungeon.settings");
      }
      ImGui::EndTable();
    }
  }
  DrawWorkbenchInspectorSectionHeader(ICON_MD_TUNE " Room Properties");

  // Core room properties (moved from canvas header).
  if (auto* rooms = viewer.rooms();
      rooms && room_id >= 0 && room_id < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[room_id];

    uint8_t blockset_val = room.blockset();
    uint8_t palette_val = room.palette();
    uint8_t layout_val = room.layout_id();
    uint8_t spriteset_val = room.spriteset();

    constexpr float kHexW = 92.0f;

    constexpr ImGuiTableFlags kPropsFlags = ImGuiTableFlags_BordersInnerV |
                                            ImGuiTableFlags_RowBg |
                                            ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##WorkbenchRoomProps", 2, kPropsFlags)) {
      ImGui::TableSetupColumn("Prop", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

      gui::LayoutHelpers::PropertyRow("Blockset", [&]() {
        if (auto res = gui::InputHexByteEx("##Blockset", &blockset_val, 81,
                                           kHexW, true);
            res.ShouldApply()) {
          room.SetBlockset(blockset_val);
          if (room.rom() && room.rom()->is_loaded()) {
            room.RenderRoomGraphics();
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Blockset (0-51)");
        }
      });
      gui::LayoutHelpers::PropertyRow("Palette", [&]() {
        if (auto res =
                gui::InputHexByteEx("##Palette", &palette_val, 71, kHexW, true);
            res.ShouldApply()) {
          room.SetPalette(palette_val);
          if (room.rom() && room.rom()->is_loaded()) {
            room.RenderRoomGraphics();
          }
          // Re-run editor sync so palette group + dependent panels update.
          if (on_room_selected_) {
            on_room_selected_(room_id);
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Palette (0-47)");
        }
      });
      gui::LayoutHelpers::PropertyRow("Layout", [&]() {
        if (auto res =
                gui::InputHexByteEx("##Layout", &layout_val, 7, kHexW, true);
            res.ShouldApply()) {
          room.SetLayoutId(layout_val);
          room.MarkLayoutDirty();
          if (room.rom() && room.rom()->is_loaded()) {
            room.RenderRoomGraphics();
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Layout (0-7)");
        }
      });
      gui::LayoutHelpers::PropertyRow("Spriteset", [&]() {
        if (auto res = gui::InputHexByteEx("##Spriteset", &spriteset_val, 143,
                                           kHexW, true);
            res.ShouldApply()) {
          room.SetSpriteset(spriteset_val);
          if (room.rom() && room.rom()->is_loaded()) {
            room.RenderRoomGraphics();
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Spriteset (0-8F)");
        }
      });

      ImGui::EndTable();
    }
  } else {
    ImGui::TextDisabled("Room properties unavailable");
  }

  DrawWorkbenchInspectorSectionHeader(ICON_MD_BUILD " Editing Status");
  auto& interaction = viewer.object_interaction();
  const bool placing = interaction.mode_manager().IsPlacementActive();
  if (placing) {
    ImGui::TextColored(theme.text_info, "Placement active");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CLOSE " Cancel")) {
      interaction.mode_manager().CancelCurrentMode();
    }
  }
}

void DungeonWorkbenchPanel::DrawInspectorShelfSelection(
    DungeonCanvasViewer& viewer) {
  auto& interaction = viewer.object_interaction();
  const auto& theme = AgentUI::GetTheme();

  const int room_id = viewer.current_room_id();
  const size_t obj_count = interaction.GetSelectionCount();
  const bool has_entity = interaction.HasEntitySelection();

  if (!has_entity && obj_count == 0) {
    DrawWorkbenchInspectorSectionHeader(ICON_MD_INFO " Selection");
    ImGui::TextDisabled("Click an object or entity to inspect");
    if (show_panel_) {
      DrawWorkbenchInspectorSectionHeader(ICON_MD_BUILD " Jump Into Editing");
      constexpr ImGuiTableFlags kEmptyStateFlags =
          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
      if (ImGui::BeginTable("##SelectionEmptyStateActions", 2,
                            kEmptyStateFlags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_MD_WIDGETS " Objects", ImVec2(-1, 0))) {
          show_panel_("dungeon.object_editor");
        }
        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_MD_PERSON " Sprites", ImVec2(-1, 0))) {
          show_panel_("dungeon.sprite_editor");
        }
        ImGui::EndTable();
      }
    }
    return;
  }

    DrawWorkbenchInspectorSectionHeader(ICON_MD_WIDGETS " Object Selection");
  // ── Tile Object Selection ──
  if (obj_count > 0) {
    ImGui::TextColored(theme.text_primary, ICON_MD_WIDGETS " %zu object%s",
                       obj_count, obj_count == 1 ? "" : "s");
    if (obj_count == 1) {
      ImGui::SameLine();
      ImGui::TextDisabled("Focused selection");
    }

    constexpr ImGuiTableFlags kActionFlags =
        ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##SelectionObjectActions", 3, kActionFlags)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Button(ICON_MD_DELETE " Delete", ImVec2(-1, 0))) {
        viewer.DeleteSelectedObjects();
      }
      ImGui::TableNextColumn();
      if (ImGui::Button(ICON_MD_CLEAR " Clear", ImVec2(-1, 0))) {
        interaction.ClearSelection();
      }
      ImGui::TableNextColumn();
      if (show_panel_) {
        if (ImGui::Button(ICON_MD_WIDGETS " Open Editor", ImVec2(-1, 0))) {
          show_panel_("dungeon.object_editor");
        }
      } else {
        ImGui::BeginDisabled();
        ImGui::Button(ICON_MD_WIDGETS " Open Editor", ImVec2(-1, 0));
        ImGui::EndDisabled();
      }
      ImGui::EndTable();
    }

    const auto indices = interaction.GetSelectedObjectIndices();

    // Multi-object summary
    if (indices.size() > 1 && room_id >= 0 && viewer.rooms()) {
      auto& room = (*viewer.rooms())[room_id];
      auto& objects = room.GetTileObjects();
      DrawWorkbenchInspectorSectionHeader(ICON_MD_SUMMARIZE " Multi-selection");
      for (size_t i = 0; i < indices.size() && i < 8; ++i) {
        size_t idx = indices[i];
        if (idx < objects.size()) {
          auto& obj = objects[idx];
          std::string name = zelda3::GetObjectName(obj.id_);
          ImGui::BulletText("0x%03X %s", obj.id_, name.c_str());
        }
      }
      if (indices.size() > 8) {
        ImGui::TextDisabled("  ... and %zu more", indices.size() - 8);
      }
    }

    // Single-object detailed inspector
    if (indices.size() == 1 && room_id >= 0 && viewer.rooms()) {
      auto& room = (*viewer.rooms())[room_id];
      auto& objects = room.GetTileObjects();
      const size_t idx = indices.front();
      if (idx < objects.size()) {
        auto& obj = objects[idx];
        const std::string obj_name = zelda3::GetObjectName(obj.id_);
        const int subtype = zelda3::GetObjectSubtype(obj.id_);

        // Name + category header
        DrawWorkbenchInspectorSectionHeader(ICON_MD_CATEGORY " Focused Object");
        ImGui::TextColored(theme.text_primary, "%s", obj_name.c_str());
        ImGui::TextDisabled("%s (Type %d)  #%zu in list",
                            GetObjectCategory(obj.id_), subtype, idx);

        // Property table
        constexpr ImGuiTableFlags kPropsFlags = ImGuiTableFlags_BordersInnerV |
                                                ImGuiTableFlags_RowBg |
                                                ImGuiTableFlags_NoPadOuterX;
        if (ImGui::BeginTable("##SelObjProps", 2, kPropsFlags)) {
          ImGui::TableSetupColumn("Prop", ImGuiTableColumnFlags_WidthFixed,
                                  56.0f);
          ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

          // ID
          gui::LayoutHelpers::PropertyRow("ID", [&]() {
            uint16_t obj_id = static_cast<uint16_t>(obj.id_ & 0x0FFF);
            if (auto res =
                    gui::InputHexWordEx("##SelObjId", &obj_id, 80.0f, true);
                res.ShouldApply()) {
              obj_id &= 0x0FFF;
              interaction.SetObjectId(idx, static_cast<int16_t>(obj_id));
            }
          });

          // Position
          gui::LayoutHelpers::PropertyRow("Pos", [&]() {
            int pos_x = obj.x_;
            int pos_y = obj.y_;
            ImGui::SetNextItemWidth(60);
            bool x_changed =
                ImGui::DragInt("##SelObjX", &pos_x, 0.1f, 0, 63, "X:%d");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            bool y_changed =
                ImGui::DragInt("##SelObjY", &pos_y, 0.1f, 0, 63, "Y:%d");
            if (x_changed || y_changed) {
              int delta_x = pos_x - obj.x_;
              int delta_y = pos_y - obj.y_;
              interaction.entity_coordinator().tile_handler().MoveObjects(
                  room_id, {idx}, delta_x, delta_y);
            }
          });

          // Size
          gui::LayoutHelpers::PropertyRow("Size", [&]() {
            uint8_t size = obj.size_ & 0x0F;
            if (auto res = gui::InputHexByteEx("##SelObjSize", &size, 0x0F,
                                               60.0f, true);
                res.ShouldApply()) {
              interaction.SetObjectSize(idx, size);
            }
          });

          // Stream / layer routing
          gui::LayoutHelpers::PropertyRow("Stream", [&]() {
            int layer = static_cast<int>(obj.GetLayerValue());
            const char* layer_names[] = {"Primary (main pass)", "BG2 overlay",
                                         "BG1 overlay"};
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##SelObjLayer", &layer, layer_names,
                             IM_ARRAYSIZE(layer_names))) {
              layer = std::clamp(layer, 0, 2);
              interaction.SetObjectLayer(
                  idx, static_cast<zelda3::RoomObject::LayerType>(layer));
            }
          });
          gui::LayoutHelpers::PropertyRow("Route", [&]() {
            ImGui::TextDisabled("%s", GetObjectStreamName(obj.GetLayerValue()));
          });

          // Pixel coords (read-only info)
          gui::LayoutHelpers::PropertyRow("Pixel", [&]() {
            ImGui::TextDisabled("(%d, %d)", obj.x_ * 8, obj.y_ * 8);
          });

          ImGui::EndTable();
        }
      }
    }
  }

  // ── Entity Selection (Doors, Sprites, Items) ──
  if (has_entity && room_id >= 0 && viewer.rooms()) {
    const auto sel = interaction.GetSelectedEntity();
    auto& room = (*viewer.rooms())[room_id];
    DrawWorkbenchInspectorSectionHeader(ICON_MD_SELECT_ALL " Entity Selection");

    const char* entity_panel_id = nullptr;
    const char* entity_action_label = nullptr;

    switch (sel.type) {
      case EntityType::Door: {
        entity_panel_id = "dungeon.entrance_properties";
        entity_action_label = ICON_MD_TUNE " Entrance Panel";
        const auto& doors = room.GetDoors();
        if (sel.index < doors.size()) {
          const auto& door = doors[sel.index];
          std::string type_name(zelda3::GetDoorTypeName(door.type));
          std::string dir_name(zelda3::GetDoorDirectionName(door.direction));

          ImGui::TextColored(theme.text_primary, ICON_MD_DOOR_FRONT " %s",
                             type_name.c_str());
          ImGui::TextDisabled("Direction: %s  Position: 0x%02X",
                              dir_name.c_str(), door.position);

          auto [tile_x, tile_y] = door.GetTileCoords();
          auto [pixel_x, pixel_y] = door.GetPixelCoords();
          ImGui::TextDisabled("Tile: (%d, %d)  Pixel: (%d, %d)", tile_x, tile_y,
                              pixel_x, pixel_y);
        }
        break;
      }
      case EntityType::Sprite: {
        entity_panel_id = "dungeon.sprite_editor";
        entity_action_label = ICON_MD_PERSON " Sprite Panel";
        const auto& sprites = room.GetSprites();
        if (sel.index < sprites.size()) {
          const auto& sprite = sprites[sel.index];
          std::string sprite_name = zelda3::GetSpriteLabel(sprite.id());

          ImGui::TextColored(theme.text_primary, ICON_MD_PERSON " %s",
                             sprite_name.c_str());
          ImGui::TextDisabled("ID: 0x%02X  Subtype: %d  Layer: %d", sprite.id(),
                              sprite.subtype(), sprite.layer());
          ImGui::TextDisabled("Pos: (%d, %d)  Pixel: (%d, %d)", sprite.x(),
                              sprite.y(), sprite.x() * 16, sprite.y() * 16);

          // Overlord check
          if (sprite.subtype() == 0x07 && sprite.id() >= 0x01 &&
              sprite.id() <= 0x1A) {
            std::string overlord_name = zelda3::GetOverlordLabel(sprite.id());
            ImGui::TextColored(theme.text_warning_yellow,
                               ICON_MD_STAR " Overlord: %s",
                               overlord_name.c_str());
          }
        }
        break;
      }
      case EntityType::Item: {
        entity_panel_id = "dungeon.item_editor";
        entity_action_label = ICON_MD_INVENTORY " Item Panel";
        const auto& items = room.GetPotItems();
        if (sel.index < items.size()) {
          const auto& pot_item = items[sel.index];
          const char* item_name = GetPotItemName(pot_item.item);

          ImGui::TextColored(theme.text_primary, ICON_MD_INVENTORY_2 " %s",
                             item_name);
          ImGui::TextDisabled("Item ID: 0x%02X  Raw Pos: 0x%04X", pot_item.item,
                              pot_item.position);
          ImGui::TextDisabled("Pixel: (%d, %d)  Tile: (%d, %d)",
                              pot_item.GetPixelX(), pot_item.GetPixelY(),
                              pot_item.GetTileX(), pot_item.GetTileY());
        }
        break;
      }
      default:
        break;
    }

    if (ImGui::BeginTable(
            "##EntityActions", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Button(ICON_MD_DELETE " Delete Entity", ImVec2(-1, 0))) {
        interaction.entity_coordinator().DeleteSelectedEntity();
        interaction.ClearEntitySelection();
      }
      ImGui::TableNextColumn();
      if (show_panel_ && entity_panel_id && entity_action_label) {
        if (ImGui::Button(entity_action_label, ImVec2(-1, 0))) {
          show_panel_(entity_panel_id);
        }
      } else {
        ImGui::BeginDisabled();
        ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Panel", ImVec2(-1, 0));
        ImGui::EndDisabled();
      }
      ImGui::EndTable();
    }
  }
}

  DrawWorkbenchInspectorSectionHeader(ICON_MD_VISIBILITY " Overlay Toggles");
void DungeonWorkbenchPanel::DrawInspectorShelfView(
    DungeonCanvasViewer& viewer) {
  bool val = viewer.show_grid();
  if (ImGui::Checkbox("Grid (8x8)", &val))
    viewer.set_show_grid(val);

  val = viewer.show_object_bounds();
  if (ImGui::Checkbox("Object Bounds", &val)) {
    viewer.set_show_object_bounds(val);
  }

  val = viewer.show_coordinate_overlay();
  if (ImGui::Checkbox("Hover Coordinates", &val)) {
    viewer.set_show_coordinate_overlay(val);
  }

  val = viewer.show_camera_quadrant_overlay();
  if (ImGui::Checkbox("Camera Quadrants", &val)) {
    viewer.set_show_camera_quadrant_overlay(val);
  }

  val = viewer.show_track_collision_overlay();
  if (ImGui::Checkbox("Track Collision", &val)) {
    viewer.set_show_track_collision_overlay(val);
  }

  val = viewer.show_custom_collision_overlay();
  if (ImGui::Checkbox("Custom Collision", &val)) {
    viewer.set_show_custom_collision_overlay(val);
  }

  val = viewer.show_water_fill_overlay();
  if (ImGui::Checkbox("Water Fill (Oracle)", &val)) {
    viewer.set_show_water_fill_overlay(val);
  }

  val = viewer.show_minecart_sprite_overlay();
  if (ImGui::Checkbox("Minecart Pathing", &val)) {
    viewer.set_show_minecart_sprite_overlay(val);
  }

  val = viewer.show_track_gap_overlay();
  if (ImGui::Checkbox("Track Gaps", &val)) {
    viewer.set_show_track_gap_overlay(val);
  }

  val = viewer.show_track_route_overlay();
  if (ImGui::Checkbox("Track Routes", &val)) {
    viewer.set_show_track_route_overlay(val);
  }

  val = viewer.show_custom_objects_overlay();
  if (ImGui::Checkbox("Custom Objects (Oracle)", &val)) {
    viewer.set_show_custom_objects_overlay(val);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Highlight custom-draw objects (IDs 0x31/0x32)\n"
        "with a cyan overlay showing position and subtype.");
  }
}

void DungeonWorkbenchPanel::DrawInspectorShelfTools(
    DungeonCanvasViewer& /*viewer*/) {
  if (!show_panel_) {
    ImGui::TextDisabled("No panel launcher available");
    return;
  }

  DrawWorkbenchInspectorSectionHeader(ICON_MD_EDIT_NOTE " Edit");
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (!ImGui::BeginTable("##WorkbenchToolsGrid", 2, kFlags)) {
    return;
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_WIDGETS " Objects", ImVec2(-1, 0))) {
    show_panel_("dungeon.object_editor");
  }
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_PERSON " Sprites", ImVec2(-1, 0))) {
    show_panel_("dungeon.sprite_editor");
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_INVENTORY " Items", ImVec2(-1, 0))) {
    show_panel_("dungeon.item_editor");
  }
  ImGui::TableNextColumn();
  if (ImGui::Button(ICON_MD_SETTINGS " Settings", ImVec2(-1, 0))) {
    show_panel_("dungeon.settings");
  }

  ImGui::EndTable();

  DrawWorkbenchInspectorSectionHeader(ICON_MD_TRAVEL_EXPLORE " Review");
  if (ImGui::BeginTable("##WorkbenchReviewGrid", 2, kFlags)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_GRID_VIEW " Matrix", ImVec2(-1, 0))) {
      show_panel_("dungeon.room_matrix");
    }
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_MAP " Dungeon Map", ImVec2(-1, 0))) {
      show_panel_("dungeon.dungeon_map");
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_DOOR_FRONT " Entrances", ImVec2(-1, 0))) {
      show_panel_("dungeon.entrance_list");
    }
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_PALETTE " Palette", ImVec2(-1, 0))) {
      show_panel_("dungeon.palette_editor");
    }
    ImGui::EndTable();
  }

  DrawWorkbenchInspectorSectionHeader(ICON_MD_KEYBOARD " Reference");
  if (ImGui::Button(ICON_MD_KEYBOARD " Keyboard Shortcuts", ImVec2(-1, 0))) {
    show_shortcut_legend_ = true;
  }
  ShortcutLegendPanel::Draw(&show_shortcut_legend_);
}

}  // namespace yaze::editor
