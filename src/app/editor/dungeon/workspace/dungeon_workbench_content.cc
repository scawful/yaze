#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/ui/window/shortcut_legend_panel.h"
#include "app/editor/dungeon/ui/workbench/dungeon_workbench_chrome.h"
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

constexpr float kCompactLeftSidebarMinWidth = 224.0f;
constexpr float kCompactRightSidebarMinWidth = 272.0f;
constexpr float kCompactLeftSidebarScale = 0.72f;
constexpr float kCompactRightSidebarScale = 0.84f;

float GetCompactSidebarWidth(bool right_sidebar, float min_sidebar_width) {
  return std::max(
      right_sidebar ? kCompactRightSidebarMinWidth
                    : kCompactLeftSidebarMinWidth,
      min_sidebar_width * (right_sidebar ? kCompactRightSidebarScale
                                         : kCompactLeftSidebarScale));
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

bool DrawWorkbenchActionButton(const char* label, const ImVec2& size) {
  gui::StyleVarGuard align_guard(ImGuiStyleVar_ButtonTextAlign,
                                 ImVec2(0.08f, 0.5f));
  return ImGui::Button(label, size);
}

}  // namespace

DungeonWorkbenchResponsiveLayout ResolveDungeonWorkbenchResponsiveLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, bool want_left, bool want_right) {
  DungeonWorkbenchResponsiveLayout result;
  result.show_left = want_left;
  result.show_right = want_right;

  auto required_width = [&](bool left, bool right, bool compact_left,
                            bool compact_right) {
    float required = min_canvas_width;
    required +=
        left ? (compact_left ? GetCompactSidebarWidth(false, min_sidebar_width)
                             : min_sidebar_width)
             : 0.0f;
    required +=
        right ? (compact_right ? GetCompactSidebarWidth(true, min_sidebar_width)
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
      total_width <
          required_width(result.show_left, result.show_right, false, false)) {
    result.compact_left = true;
  }
  if (result.show_right &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, false)) {
    result.compact_right = true;
  }
  if (result.show_left &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, result.compact_right)) {
    result.show_left = false;
    result.compact_left = false;
  }
  if (result.show_right &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, result.compact_right)) {
    result.show_right = false;
    result.compact_right = false;
  }

  return result;
}

DungeonWorkbenchPaneLayout ResolveDungeonWorkbenchPaneLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, float stored_left_width, float stored_right_width,
    bool want_left, bool want_right) {
  DungeonWorkbenchPaneLayout layout;
  layout.responsive = ResolveDungeonWorkbenchResponsiveLayout(
      total_width, min_canvas_width, min_sidebar_width, splitter_width,
      want_left, want_right);

  const float compact_left_width =
      GetCompactSidebarWidth(false, min_sidebar_width);
  const float compact_right_width =
      GetCompactSidebarWidth(true, min_sidebar_width);
  layout.min_left_width =
      layout.responsive.compact_left ? compact_left_width : min_sidebar_width;
  layout.min_right_width =
      layout.responsive.compact_right ? compact_right_width : min_sidebar_width;

  float left_width = layout.responsive.show_left
                         ? (layout.responsive.compact_left ? compact_left_width
                                                           : stored_left_width)
                         : 0.0f;
  float right_width =
      layout.responsive.show_right
          ? (layout.responsive.compact_right ? compact_right_width
                                             : stored_right_width)
          : 0.0f;

  const float max_left_width =
      total_width - right_width - min_canvas_width -
      (layout.responsive.show_left ? splitter_width : 0.0f) -
      (layout.responsive.show_right ? splitter_width : 0.0f);
  const float max_right_width =
      total_width - left_width - min_canvas_width -
      (layout.responsive.show_left ? splitter_width : 0.0f) -
      (layout.responsive.show_right ? splitter_width : 0.0f);
  if (layout.responsive.show_left) {
    left_width = ClampWorkbenchPaneWidth(
        left_width, layout.min_left_width,
        std::max(layout.min_left_width, max_left_width));
  } else {
    left_width = 0.0f;
  }
  if (layout.responsive.show_right) {
    right_width = ClampWorkbenchPaneWidth(
        right_width, layout.min_right_width,
        std::max(layout.min_right_width, max_right_width));
  } else {
    right_width = 0.0f;
  }

  float center_width = total_width - left_width - right_width;
  if (layout.responsive.show_left) {
    center_width -= splitter_width;
  }
  if (layout.responsive.show_right) {
    center_width -= splitter_width;
  }
  if (center_width < min_canvas_width) {
    float deficit = min_canvas_width - center_width;
    if (layout.responsive.show_left) {
      const float shrink =
          std::min(deficit, left_width - layout.min_left_width);
      left_width -= shrink;
      deficit -= shrink;
    }
    if (deficit > 0.0f && layout.responsive.show_right) {
      const float shrink =
          std::min(deficit, right_width - layout.min_right_width);
      right_width -= shrink;
      deficit -= shrink;
    }
    center_width = std::max(
        1.0f, total_width - left_width - right_width -
                  (layout.responsive.show_left ? splitter_width : 0.0f) -
                  (layout.responsive.show_right ? splitter_width : 0.0f));
  }

  layout.left_width = left_width;
  layout.center_width = center_width;
  layout.right_width = right_width;
  return layout;
}

DungeonWorkbenchContent::DungeonWorkbenchContent(
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

std::string DungeonWorkbenchContent::GetId() const {
  return "dungeon.workbench";
}
std::string DungeonWorkbenchContent::GetDisplayName() const {
  return "Dungeon Workbench";
}
std::string DungeonWorkbenchContent::GetIcon() const {
  return ICON_MD_WORKSPACES;
}
std::string DungeonWorkbenchContent::GetEditorCategory() const {
  return "Dungeon";
}
int DungeonWorkbenchContent::GetPriority() const {
  return 10;
}

void DungeonWorkbenchContent::SetRom(Rom* rom) {
  rom_ = rom;
  room_dungeon_cache_.clear();
  room_dungeon_cache_built_ = false;
}

void DungeonWorkbenchContent::DrawSidebarPane(float width, float height,
                                              float button_size, bool compact) {
  const bool sidebar_open = ImGui::BeginChild("##DungeonWorkbenchSidebar",
                                              ImVec2(width, height), true);
  if (sidebar_open) {
    DrawSidebarHeader(button_size, compact);
    DrawSidebarContent();
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawSidebarHeader(float button_size,
                                                bool compact) {
  const bool can_open_overview = static_cast<bool>(show_panel_);
  const float collapse_w =
      workbench::CalcIconButtonWidth(ICON_MD_CHEVRON_LEFT, button_size);
  const float menu_w = can_open_overview ? workbench::CalcIconButtonWidth(
                                               ICON_MD_MORE_HORIZ, button_size)
                                         : 0.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float header_width = ImGui::GetContentRegionAvail().x;
  const bool stack_mode_switch = compact && header_width < 220.0f;
  const float action_cluster_w =
      collapse_w + (can_open_overview ? (spacing + menu_w) : 0.0f);

  workbench::DrawPaneHeader(
      "##DungeonWorkbenchSidebarHeader", ICON_MD_VIEW_SIDEBAR, "Browse",
      "Browse", nullptr, compact, action_cluster_w, [&]() {
        if (can_open_overview) {
          if (workbench::DrawHeaderIconAction("SidebarQuickActions",
                                              ICON_MD_FACT_CHECK, button_size,
                                              "Open room review tools", true)) {
            ImGui::OpenPopup("##WorkbenchSidebarQuickActions");
          }
          if (ImGui::BeginPopup("##WorkbenchSidebarQuickActions")) {
            if (ImGui::MenuItem(ICON_MD_GRID_VIEW " Room Matrix")) {
              show_panel_("dungeon.room_matrix");
            }
            if (ImGui::MenuItem(ICON_MD_MAP " Dungeon Map")) {
              show_panel_("dungeon.dungeon_map");
            }
            ImGui::EndPopup();
          }
          ImGui::SameLine();
        }
        if (workbench::DrawHeaderIconAction("CollapseRooms",
                                            ICON_MD_CHEVRON_LEFT, button_size,
                                            "Collapse navigation pane")) {
          layout_state_.show_left_sidebar = false;
        }
      });

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
  DrawSidebarModeTabs(stack_mode_switch, std::max(button_size, 26.0f));
  ImGui::Separator();
}

void DungeonWorkbenchContent::DrawSidebarModeTabs(bool stacked,
                                                  float segment_height) {
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float rooms_width =
      workbench::CalcIconButtonWidth("Rooms", segment_height);
  const float entrances_width =
      workbench::CalcIconButtonWidth("Entrances", segment_height);
  const bool stack = stacked || ImGui::GetContentRegionAvail().x <
                                    (rooms_width + entrances_width + spacing);

  const ImVec2 rooms_size(
      stack ? ImGui::GetContentRegionAvail().x : rooms_width, segment_height);
  if (gui::ToggleButton("Rooms", sidebar_mode_ == SidebarMode::Rooms,
                        rooms_size)) {
    sidebar_mode_ = SidebarMode::Rooms;
  }
  if (!stack) {
    ImGui::SameLine(0.0f, spacing);
  }
  const ImVec2 entrances_size(
      stack ? ImGui::GetContentRegionAvail().x : entrances_width,
      segment_height);
  if (gui::ToggleButton("Entrances", sidebar_mode_ == SidebarMode::Entrances,
                        entrances_size)) {
    sidebar_mode_ = SidebarMode::Entrances;
  }
}

void DungeonWorkbenchContent::DrawSidebarContent() {
  if (!room_selector_) {
    ImGui::TextDisabled("Room navigation unavailable");
    return;
  }

  ImGui::PushID("WorkbenchSidebarMode");
  switch (sidebar_mode_) {
    case SidebarMode::Rooms:
      room_selector_->DrawRoomBrowser();
      break;
    case SidebarMode::Entrances:
      room_selector_->DrawEntranceBrowser();
      break;
  }
  ImGui::PopID();
}

void DungeonWorkbenchContent::Draw(bool* p_open) {
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
  const float splitter_w = gui::UIConfig::kSplitterWidth;
  const float total_w = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
  // Relaxed from 320 px to 260 px so the Inspect pane can breathe at widths
  // where the previous floor forced a too-wide right rail. Room browser on the
  // left also benefits since its matrix cells scale down gracefully.
  const float min_sidebar_w =
      std::max(gui::UIConfig::kContentMinWidthSidebar, 260.0f);
  const float min_canvas_w = std::max(420.0f, min_sidebar_w + 96.0f);
  const DungeonWorkbenchPaneLayout pane_layout =
      ResolveDungeonWorkbenchPaneLayout(
          total_w, min_canvas_w, min_sidebar_w, splitter_w,
          layout_state_.left_width, layout_state_.right_width,
          layout_state_.show_left_sidebar, layout_state_.show_right_inspector);
  const bool show_left = pane_layout.responsive.show_left;
  const bool show_right = pane_layout.responsive.show_right;

  if (current_room_id_) {
    DungeonWorkbenchToolbarParams params;
    params.layout = &layout_state_;
    params.left_sidebar_visible = show_left;
    params.current_room_id = current_room_id_;
    params.previous_room_id = &previous_room_id_;
    params.split_view_enabled = &split_view_enabled_;
    params.compare_room_id = &compare_room_id_;
    params.primary_viewer = primary_viewer;
    params.compare_viewer = compare_viewer;
    params.on_room_selected = on_room_selected_;
    params.get_recent_rooms = get_recent_rooms_;
    params.set_workflow_mode = set_workflow_mode_;
    params.open_room_matrix = [this]() {
      if (show_panel_) {
        show_panel_("dungeon.room_matrix");
      }
    };
    params.compare_search_buf = compare_search_buf_;
    params.compare_search_buf_size = sizeof(compare_search_buf_);
    const bool request_panel_workflow = DungeonWorkbenchToolbar::Draw(params);
    if (request_panel_workflow && set_workflow_mode_) {
      // Defer panel visibility mutation until toolbar child/table scopes closed.
      set_workflow_mode_(false);
      return;
    }
  }

  const float btn = gui::LayoutHelpers::GetTouchSafeWidgetHeight();
  const float total_h = std::max(ImGui::GetContentRegionAvail().y, 1.0f);
  const float left_w = pane_layout.left_width;
  const float right_w = pane_layout.right_width;
  const float center_w = pane_layout.center_width;
  if (show_left && !pane_layout.responsive.compact_left) {
    layout_state_.left_width = left_w;
  }
  if (show_right && !pane_layout.responsive.compact_right) {
    layout_state_.right_width = right_w;
  }

  if (show_left) {
    DrawSidebarPane(left_w, total_h, btn, pane_layout.responsive.compact_left);
  }
  if (show_left) {
    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter(
        "##DungeonWorkbenchLeftSplitter", total_h, &layout_state_.left_width,
        pane_layout.min_left_width,
        total_w - right_w - min_canvas_w - (show_right ? splitter_w : 0.0f),
        false);
  }

  if (show_left) {
    ImGui::SameLine(0.0f, 0.0f);
  }
  DrawCanvasPane(center_w, total_h, primary_viewer, show_left);

  if (show_right) {
    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter(
        "##DungeonWorkbenchRightSplitter", total_h, &layout_state_.right_width,
        pane_layout.min_right_width,
        total_w - left_w - min_canvas_w - (show_left ? splitter_w : 0.0f),
        true);
    ImGui::SameLine(0.0f, 0.0f);
  }
  if (show_right) {
    DrawInspectorPane(right_w, total_h, btn,
                      pane_layout.responsive.compact_right, primary_viewer);
  }
}

void DungeonWorkbenchContent::DrawCanvasPane(
    float width, float height, DungeonCanvasViewer* primary_viewer,
    bool left_sidebar_visible) {
  const bool canvas_open = ImGui::BeginChild("##DungeonWorkbenchCanvas",
                                             ImVec2(width, height), false);
  if (canvas_open) {
    if (primary_viewer) {
      const bool show_recent_tabs =
          split_view_enabled_ || !left_sidebar_visible;
      if (show_recent_tabs) {
        DrawRecentRoomTabs();
      }
      if (!layout_state_.show_connected_canvas_view) {
        DrawSelectionShelf(*primary_viewer);
      }

      // Reserve a fixed strip at the bottom for the status bar so neither
      // single-room, split, nor connected-mode rendering can run past it and
      // clip against the outer window chrome. Inner scrolling (wheel zoom in
      // connected mode, canvas pan in single-room) happens inside this body
      // child — the outer ##DungeonWorkbenchCanvas never needs a scrollbar.
      const float status_bar_reserved =
          std::max(
              ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f,
              gui::UIConfig::kStatusBarHeight) +
          ImGui::GetStyle().ItemSpacing.y;

      const bool connected_mode = layout_state_.show_connected_canvas_view;
      ImGuiWindowFlags body_flags = 0;
      if (connected_mode) {
        // Body child owns the connected-mode scroll container; matrix no
        // longer nests its own BeginChild for scrolling.
        const ImVec2 content_size =
            primary_viewer->GetConnectedContentSize(*current_room_id_);
        const float scale = primary_viewer->ConnectedCanvasScale();
        ImGui::SetNextWindowContentSize(
            ImVec2(content_size.x * scale, content_size.y * scale));
        body_flags |= ImGuiWindowFlags_HorizontalScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse;
      }
      const bool body_open = ImGui::BeginChild(
          "##DungeonCanvasBody", ImVec2(0.0f, -status_bar_reserved), false,
          body_flags);
      if (body_open) {
        if (connected_mode) {
          split_view_enabled_ = false;
          if (primary_viewer->DrawConnectedRoomMatrix(*current_room_id_)
                  .has_value()) {
            layout_state_.show_connected_canvas_view = false;
          }
        } else if (split_view_enabled_) {
          DrawSplitView(*primary_viewer);
        } else {
          primary_viewer->DrawDungeonCanvas(*current_room_id_);
        }
      }
      ImGui::EndChild();

      const char* tool_mode = get_tool_mode_ ? get_tool_mode_() : "Select";
      bool room_dirty = false;
      if (auto* rooms = primary_viewer->rooms();
          rooms && current_room_id_ && *current_room_id_ >= 0) {
        if (const auto* room = rooms->GetIfMaterialized(*current_room_id_)) {
          room_dirty = room->HasUnsavedChanges();
        }
      }
      auto status =
          DungeonStatusBar::BuildState(*primary_viewer, tool_mode, room_dirty);
      status.workflow_mode =
          layout_state_.show_connected_canvas_view ? "Connected" : "Workbench";
      status.workflow_primary = true;
      if (can_undo_)
        status.can_undo = can_undo_();
      if (can_redo_)
        status.can_redo = can_redo_();
      if (undo_desc_) {
        static std::string s_undo_desc;
        s_undo_desc = undo_desc_();
        status.undo_desc = s_undo_desc.empty() ? nullptr : s_undo_desc.c_str();
      }
      if (redo_desc_) {
        static std::string s_redo_desc;
        s_redo_desc = redo_desc_();
        status.redo_desc = s_redo_desc.empty() ? nullptr : s_redo_desc.c_str();
      }
      if (undo_depth_)
        status.undo_depth = undo_depth_();
      status.on_undo = on_undo_;
      status.on_redo = on_redo_;
      DungeonStatusBar::Draw(status);
    } else {
      ImGui::TextDisabled("No active viewer");
    }
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawSelectionShelf(DungeonCanvasViewer& viewer) {
  auto& interaction = viewer.object_interaction();
  const size_t object_count = interaction.GetSelectionCount();
  const bool has_entity = interaction.HasEntitySelection();
  if (object_count == 0 && !has_entity) {
    return;
  }

  gui::StyleVarGuard frame_padding_guard(
      ImGuiStyleVar_FramePadding,
      ImVec2(std::max(4.0f, ImGui::GetStyle().FramePadding.x),
             std::max(2.0f, ImGui::GetStyle().FramePadding.y - 1.0f)));
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(4.0f, ImGui::GetStyle().ItemSpacing.x * 0.7f),
             std::max(2.0f, ImGui::GetStyle().ItemSpacing.y * 0.55f)));

  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  bool first_button = true;
  auto start_action = [&](const char* label) {
    const float button_width = ImGui::CalcTextSize(label).x +
                               (ImGui::GetStyle().FramePadding.x * 2.0f) + 8.0f;
    if (!first_button) {
      const float next_x = ImGui::GetItemRectMax().x + spacing + button_width;
      const float max_x =
          ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
      if (next_x <= max_x) {
        ImGui::SameLine(0.0f, spacing);
      }
    }
    first_button = false;
  };

  auto draw_action = [&](const char* label, bool enabled,
                         const auto& on_press) {
    start_action(label);
    if (!enabled) {
      ImGui::BeginDisabled();
    }
    if (DrawWorkbenchActionButton(label, ImVec2(0.0f, 0.0f)) && enabled) {
      on_press();
    }
    if (!enabled) {
      ImGui::EndDisabled();
    }
  };

  if (object_count > 0) {
    draw_action(ICON_MD_DELETE " Delete", true,
                [&]() { viewer.DeleteSelectedObjects(); });
    draw_action(ICON_MD_CLEAR " Clear", true,
                [&]() { interaction.ClearSelection(); });
    draw_action(ICON_MD_TUNE " Open Editor", static_cast<bool>(show_panel_),
                [&]() { show_panel_("dungeon.object_editor"); });
  } else if (has_entity) {
    const SelectedEntity selection = interaction.GetSelectedEntity();
    const char* panel_id = nullptr;
    const char* panel_label = nullptr;
    switch (selection.type) {
      case EntityType::Door:
        panel_id = "dungeon.door_editor";
        panel_label = ICON_MD_DOOR_FRONT " Door Editor";
        break;
      case EntityType::Sprite:
        panel_id = "dungeon.sprite_editor";
        panel_label = ICON_MD_PERSON " Sprite Panel";
        break;
      case EntityType::Item:
        panel_id = "dungeon.item_editor";
        panel_label = ICON_MD_INVENTORY " Item Panel";
        break;
      default:
        break;
    }

    draw_action(ICON_MD_DELETE " Delete Entity", true, [&]() {
      interaction.entity_coordinator().DeleteSelectedEntity();
      interaction.ClearEntitySelection();
    });
    if (panel_label != nullptr) {
      draw_action(panel_label, show_panel_ && panel_id != nullptr,
                  [&]() { show_panel_(panel_id); });
    }
  }

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

void DungeonWorkbenchContent::DrawInspectorPane(float width, float height,
                                                float button_size, bool compact,
                                                DungeonCanvasViewer* viewer) {
  const bool inspector_open = ImGui::BeginChild("##DungeonWorkbenchInspector",
                                                ImVec2(width, height), true);
  if (inspector_open) {
    DrawInspectorHeader(button_size, compact);
    if (viewer) {
      DrawInspector(*viewer);
    } else {
      ImGui::TextDisabled("No active viewer");
    }
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawInspectorHeader(float button_size,
                                                  bool compact) {
  const float collapse_w =
      workbench::CalcIconButtonWidth(ICON_MD_CHEVRON_RIGHT, button_size);

  workbench::DrawPaneHeader(
      "##DungeonWorkbenchInspectorHeader", ICON_MD_TUNE, "Inspect", "Inspect",
      nullptr, compact, collapse_w, [&]() {
        if (workbench::DrawHeaderIconAction("CollapseInspector",
                                            ICON_MD_CHEVRON_RIGHT, button_size,
                                            "Collapse inspector")) {
          layout_state_.show_right_inspector = false;
        }
      });

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
  DrawInspectorPrimarySelector(std::max(button_size, 26.0f));
  ImGui::Separator();
}

void DungeonWorkbenchContent::DrawRecentRoomTabs() {
  if (!get_recent_rooms_ || !current_room_id_ || !on_room_selected_) {
    return;
  }

  DungeonRoomStore* rooms = nullptr;
  if (auto* viewer = get_viewer_ ? get_viewer_() : nullptr) {
    rooms = viewer->rooms();
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
  const project::YazeProject* label_project =
      get_viewer_ && get_viewer_() ? get_viewer_()->project() : nullptr;

  if (gui::BeginThemedTabBar("##DungeonRecentRooms", kFlags)) {
    for (int room_id : recent_ids) {
      bool open = true;
      const ImGuiTabItemFlags tab_flags =
          (room_id == *current_room_id_) ? ImGuiTabItemFlags_SetSelected : 0;
      const auto room_name =
          dungeon_project_labels::GetRoomLabel(label_project, room_id);
      const bool room_dirty =
          rooms != nullptr && rooms->GetIfMaterialized(room_id) != nullptr &&
          rooms->GetIfMaterialized(room_id)->HasUnsavedChanges();
      char tab_label[64];
      if (room_name.empty() || room_name == "Unknown") {
        snprintf(tab_label, sizeof(tab_label), "%03X%s##recent_%03X", room_id,
                 room_dirty ? "*" : "", room_id);
      } else {
        snprintf(tab_label, sizeof(tab_label), "%03X%s %.12s##recent_%03X",
                 room_id, room_dirty ? "*" : "", room_name.c_str(), room_id);
      }
      const bool selected = ImGui::BeginTabItem(tab_label, &open, tab_flags);

      if (!open && forget_recent_room_) {
        to_forget.push_back(room_id);
      }

      if (ImGui::IsItemHovered()) {
        const auto label =
            dungeon_project_labels::GetRoomLabel(label_project, room_id);
        ImGui::SetTooltip("[%03X] %s%s", room_id, label.c_str(),
                          room_dirty ? "\nPending room changes" : "");
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

void DungeonWorkbenchContent::DrawSplitView(
    DungeonCanvasViewer& primary_viewer) {
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
  ImGui::AlignTextToFramePadding();
  const project::YazeProject* active_project = primary_viewer.project();
  ImGui::TextDisabled(
      ICON_MD_CROP_FREE " Active  [%03X] %s", *current_room_id_,
      dungeon_project_labels::GetRoomLabel(active_project, *current_room_id_)
          .c_str());
  ImGui::Separator();
  const bool split_active_open = gui::LayoutHelpers::BeginContentChild(
      "##SplitActive", ImVec2(0.0f, gui::UIConfig::kContentMinHeightCanvas));
  if (split_active_open) {
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
  }
  gui::LayoutHelpers::EndContentChild();

  // Compare pane
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  const project::YazeProject* compare_project =
      get_compare_viewer_ && get_compare_viewer_()
          ? get_compare_viewer_()->project()
          : active_project;
  ImGui::TextDisabled(
      ICON_MD_COMPARE_ARROWS " Compare [%03X] %s", compare_room_id_,
      dungeon_project_labels::GetRoomLabel(compare_project, compare_room_id_)
          .c_str());
  ImGui::Separator();
  const bool split_compare_open = gui::LayoutHelpers::BeginContentChild(
      "##SplitCompare", ImVec2(0.0f, gui::UIConfig::kContentMinHeightCanvas));
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

void DungeonWorkbenchContent::BuildRoomDungeonCache() {
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

void DungeonWorkbenchContent::DrawInspector(DungeonCanvasViewer& viewer) {
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(4.0f, ImGui::GetStyle().ItemSpacing.x * 0.75f),
             std::max(4.0f, ImGui::GetStyle().ItemSpacing.y * 0.7f)));
  DrawInspectorShelf(viewer);
}

void DungeonWorkbenchContent::DrawInspectorPrimarySelector(
    float segment_height) {
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float room_width =
      workbench::CalcIconButtonWidth("Room", segment_height);
  const float selection_width =
      workbench::CalcIconButtonWidth("Selection", segment_height);
  const bool stack = ImGui::GetContentRegionAvail().x <
                     (room_width + selection_width + spacing);
  const ImVec2 room_size(stack ? ImGui::GetContentRegionAvail().x : room_width,
                         segment_height);
  if (gui::ToggleButton("Room", inspector_focus_ == InspectorFocus::Room,
                        room_size)) {
    inspector_focus_ = InspectorFocus::Room;
  }
  if (!stack) {
    ImGui::SameLine(0.0f, spacing);
  }
  const ImVec2 selection_size(
      stack ? ImGui::GetContentRegionAvail().x : selection_width,
      segment_height);
  if (gui::ToggleButton("Selection",
                        inspector_focus_ == InspectorFocus::Selection,
                        selection_size)) {
    inspector_focus_ = InspectorFocus::Selection;
  }
}

void DungeonWorkbenchContent::DrawInspectorCompactSummary(
    DungeonCanvasViewer& viewer) {
  const int room_id = (viewer.current_room_id() >= 0)
                          ? viewer.current_room_id()
                          : (current_room_id_ ? *current_room_id_ : -1);
  const auto& interaction = viewer.object_interaction();
  const size_t selected_objects = interaction.GetSelectionCount();
  const bool has_entity = interaction.HasEntitySelection();

  ImGui::TextDisabled(ICON_MD_SUMMARIZE " Summary");
  if (room_id >= 0) {
    ImGui::Text("[%03X] %s", room_id,
                dungeon_project_labels::GetRoomLabel(viewer.project(), room_id)
                    .c_str());
  } else {
    ImGui::TextDisabled("No room selected");
  }

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
  if (selected_objects > 0 || has_entity) {
    ImGui::TextDisabled(ICON_MD_SELECT_ALL " Focus");
    if (has_entity) {
      ImGui::BulletText("Entity selected");
    }
    if (selected_objects > 0) {
      ImGui::BulletText("%zu object%s selected", selected_objects,
                        selected_objects == 1 ? "" : "s");
    }
    if (DrawWorkbenchActionButton(ICON_MD_OPEN_IN_FULL " Open Selection",
                                  ImVec2(-1, 0))) {
      inspector_focus_ = InspectorFocus::Selection;
    }
  } else {
    ImGui::TextDisabled("Nothing selected");
  }

  if (room_id >= 0) {
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    if (on_save_room_ &&
        DrawWorkbenchActionButton(ICON_MD_SAVE " Apply Room to ROM",
                                  ImVec2(-1, 0))) {
      on_save_room_(room_id);
    }
    if (DrawWorkbenchActionButton(ICON_MD_CASTLE " Room Details",
                                  ImVec2(-1, 0))) {
      inspector_focus_ = InspectorFocus::Room;
    }
  }

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
  if (BeginWorkbenchInspectorSection(ICON_MD_VISIBILITY " View", true)) {
    DrawInspectorShelfView(viewer);
  }

  if (show_panel_ &&
      BeginWorkbenchInspectorSection(ICON_MD_BUILD " Tools", false)) {
    DrawInspectorShelfTools(viewer);
  }
}

void DungeonWorkbenchContent::DrawInspectorShelf(DungeonCanvasViewer& viewer) {
  if (ImGui::GetContentRegionAvail().x < 240.0f) {
    DrawInspectorCompactSummary(viewer);
    return;
  }

  if (inspector_focus_ == InspectorFocus::Room) {
    DrawInspectorShelfRoom(viewer);
  } else {
    DrawInspectorShelfSelection(viewer);
  }

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
  if (BeginWorkbenchInspectorSection(ICON_MD_VISIBILITY " View Options",
                                     true)) {
    DrawInspectorShelfView(viewer);
  }
  if (BeginWorkbenchInspectorSection(ICON_MD_BUILD " Quick Tools", false)) {
    DrawInspectorShelfTools(viewer);
  }
}

void DungeonWorkbenchContent::DrawInspectorShelfRoom(
    DungeonCanvasViewer& viewer) {
  const auto& theme = AgentUI::GetTheme();

  int room_id = viewer.current_room_id();
  if (room_id < 0 && current_room_id_) {
    room_id = *current_room_id_;
  }

  const std::string room_label =
      (room_id >= 0)
          ? dungeon_project_labels::GetRoomLabel(viewer.project(), room_id)
          : std::string("None");

  // Room badge: hex ID + copy button (only for valid room IDs).
  DrawWorkbenchInspectorSectionHeader(ICON_MD_CASTLE " Room Summary");
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

  bool room_dirty = false;
  if (auto* rooms = viewer.rooms(); rooms && room_id >= 0) {
    if (const auto* room = rooms->GetIfMaterialized(room_id)) {
      room_dirty = room->HasUnsavedChanges();
    }
  }
  if (room_dirty) {
    ImGui::TextColored(theme.status_warning,
                       ICON_MD_EDIT " Pending room changes");
  } else if (room_id >= 0) {
    ImGui::TextDisabled(ICON_MD_CHECK " Room matches ROM buffer");
  }

  // Dungeon group context: prefer ROM entrance-based lookup (accurate for
  // custom Oracle dungeons); fall back to blockset-derived name.
  if (!room_dungeon_cache_built_ && rom_ && rom_->is_loaded()) {
    BuildRoomDungeonCache();
  }
  if (room_id >= 0) {
    std::string project_group_name =
        dungeon_project_labels::GetDungeonNameForRoom(viewer.project(),
                                                      room_id);
    const char* group_name =
        project_group_name.empty() ? nullptr : project_group_name.c_str();
    if (!group_name) {
      auto cache_it = room_dungeon_cache_.find(room_id);
      if (cache_it != room_dungeon_cache_.end() && !cache_it->second.empty()) {
        group_name = cache_it->second.c_str();
      }
    }
    if (!group_name) {
      auto* rooms = viewer.rooms();
      if (rooms && room_id < static_cast<int>(rooms->size())) {
        group_name = DungeonRoomSelector::GetBlocksetGroupName(
            (*rooms)[room_id].blockset());
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
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextDisabled(
      "Apply Room writes this room into the loaded ROM buffer. File > Save ROM "
      "writes the ROM file to disk.");
  ImGui::PopTextWrapPos();
  if (on_save_room_ && room_id >= 0) {
    if (DrawWorkbenchActionButton(ICON_MD_SAVE " Apply Room to ROM",
                                  ImVec2(-1, 0))) {
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
      if (DrawWorkbenchActionButton(ICON_MD_IMAGE " Graphics", ImVec2(-1, 0))) {
        show_panel_("dungeon.room_graphics");
      }
      ImGui::TableNextColumn();
      if (DrawWorkbenchActionButton(ICON_MD_SETTINGS " Settings",
                                    ImVec2(-1, 0))) {
        show_panel_("dungeon.settings");
      }
      ImGui::EndTable();
    }
  }

  // Core room properties (moved from canvas header).
  DrawWorkbenchInspectorSectionHeader(ICON_MD_TUNE " Room Properties");
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

void DungeonWorkbenchContent::DrawInspectorShelfSelection(
    DungeonCanvasViewer& viewer) {
  auto& interaction = viewer.object_interaction();
  const auto& theme = AgentUI::GetTheme();

  const int room_id = viewer.current_room_id();
  const size_t obj_count = interaction.GetSelectionCount();
  const bool has_entity = interaction.HasEntitySelection();

  if (!has_entity && obj_count == 0) {
    DrawWorkbenchInspectorSectionHeader(ICON_MD_INFO " Selection");
    ImGui::TextDisabled("Click an object or entity to inspect");
    return;
  }

  // ── Tile Object Selection ──
  if (obj_count > 0) {
    DrawWorkbenchInspectorSectionHeader(ICON_MD_WIDGETS " Object Selection");
    ImGui::TextColored(theme.text_primary, ICON_MD_WIDGETS " %zu object%s",
                       obj_count, obj_count == 1 ? "" : "s");
    if (obj_count == 1) {
      ImGui::SameLine();
      ImGui::TextDisabled("Focused selection");
    }

    const auto indices = interaction.GetSelectedObjectIndices();

    // Multi-object summary + inline bulk editor (O1)
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

      // Bulk editor: nudge/layer/stacking/size + destructive duplicate/delete.
      // Operations route through tile_handler / interaction, so every action
      // captures its own undo snapshot.
      auto& tile_handler = interaction.entity_coordinator().tile_handler();
      const std::vector<size_t> selection_copy(indices.begin(), indices.end());

      DrawWorkbenchInspectorSectionHeader(ICON_MD_OPEN_WITH " Bulk Edit");

      // Nudge grid (arrow buttons around a tile-delta drag).
      static int bulk_nudge_dx = 0;
      static int bulk_nudge_dy = 0;
      ImGui::TextDisabled("Nudge (tiles)");
      ImGui::PushButtonRepeat(true);
      if (ImGui::Button(ICON_MD_ARROW_UPWARD "##BulkNudgeUp"))
        tile_handler.MoveObjects(room_id, selection_copy, 0, -1);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ARROW_DOWNWARD "##BulkNudgeDown"))
        tile_handler.MoveObjects(room_id, selection_copy, 0, 1);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ARROW_BACK "##BulkNudgeLeft"))
        tile_handler.MoveObjects(room_id, selection_copy, -1, 0);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ARROW_FORWARD "##BulkNudgeRight"))
        tile_handler.MoveObjects(room_id, selection_copy, 1, 0);
      ImGui::PopButtonRepeat();

      ImGui::SetNextItemWidth(60);
      ImGui::DragInt("##BulkNudgeDx", &bulk_nudge_dx, 0.25f, -63, 63, "Δx:%d");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      ImGui::DragInt("##BulkNudgeDy", &bulk_nudge_dy, 0.25f, -63, 63, "Δy:%d");
      ImGui::SameLine();
      if (ImGui::Button("Apply##BulkNudgeApply") &&
          (bulk_nudge_dx != 0 || bulk_nudge_dy != 0)) {
        tile_handler.MoveObjects(room_id, selection_copy, bulk_nudge_dx,
                                 bulk_nudge_dy);
        bulk_nudge_dx = 0;
        bulk_nudge_dy = 0;
      }

      ImGui::Spacing();
      ImGui::TextDisabled("Layer (set all)");
      if (ImGui::SmallButton("Primary##BulkLayer0"))
        tile_handler.UpdateObjectsLayer(room_id, selection_copy, 0);
      ImGui::SameLine();
      if (ImGui::SmallButton("BG2##BulkLayer1"))
        tile_handler.UpdateObjectsLayer(room_id, selection_copy, 1);
      ImGui::SameLine();
      if (ImGui::SmallButton("BG1##BulkLayer2"))
        tile_handler.UpdateObjectsLayer(room_id, selection_copy, 2);

      ImGui::Spacing();
      ImGui::TextDisabled("Stacking");
      if (ImGui::SmallButton(ICON_MD_FLIP_TO_FRONT " Front##BulkFront"))
        tile_handler.SendToFront(room_id, selection_copy);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_FLIP_TO_BACK " Back##BulkBack"))
        tile_handler.SendToBack(room_id, selection_copy);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_KEYBOARD_ARROW_UP "##BulkFwd"))
        tile_handler.MoveForward(room_id, selection_copy);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_KEYBOARD_ARROW_DOWN "##BulkBwd"))
        tile_handler.MoveBackward(room_id, selection_copy);

      ImGui::Spacing();
      ImGui::TextDisabled("Size");
      if (ImGui::SmallButton(ICON_MD_REMOVE "##BulkSizeDec"))
        tile_handler.ResizeObjects(room_id, selection_copy, -1);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_ADD "##BulkSizeInc"))
        tile_handler.ResizeObjects(room_id, selection_copy, 1);

      ImGui::Spacing();
      if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Duplicate##BulkDup"))
        (void)tile_handler.DuplicateObjects(room_id, selection_copy, 1, 1);
      ImGui::SameLine();
      {
        gui::StyleColorGuard danger_colors({
            {ImGuiCol_Button, theme.status_error},
            {ImGuiCol_ButtonHovered,
             ImVec4(theme.status_error.x + 0.1f, theme.status_error.y + 0.05f,
                    theme.status_error.z + 0.05f, 1.0f)},
        });
        if (ImGui::SmallButton(ICON_MD_DELETE " Delete##BulkDel"))
          ImGui::OpenPopup("##BulkDeleteConfirm");
      }

      if (ImGui::BeginPopup("##BulkDeleteConfirm")) {
        ImGui::TextColored(
            theme.status_error, ICON_MD_WARNING " Delete %zu object%s?",
            selection_copy.size(), selection_copy.size() == 1 ? "" : "s");
        ImGui::Separator();
        if (ImGui::Button("Cancel##BulkDelCancel")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        {
          gui::StyleColorGuard confirm_colors({
              {ImGuiCol_Button, theme.status_error},
          });
          if (ImGui::Button(ICON_MD_DELETE " Confirm##BulkDelConfirm")) {
            tile_handler.DeleteObjects(room_id, selection_copy);
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::EndPopup();
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

    switch (sel.type) {
      case EntityType::Door: {
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
  }
}

void DungeonWorkbenchContent::DrawInspectorShelfView(
    DungeonCanvasViewer& viewer) {
  DrawWorkbenchInspectorSectionHeader(ICON_MD_VISIBILITY " Overlay Toggles");
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

void DungeonWorkbenchContent::DrawInspectorShelfTools(
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
  if (DrawWorkbenchActionButton(ICON_MD_CATEGORY " Selector", ImVec2(-1, 0))) {
    show_panel_("dungeon.object_selector");
  }
  ImGui::TableNextColumn();
  if (DrawWorkbenchActionButton(ICON_MD_TUNE " Object Editor", ImVec2(-1, 0))) {
    show_panel_("dungeon.object_editor");
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (DrawWorkbenchActionButton(ICON_MD_PERSON " Sprites", ImVec2(-1, 0))) {
    show_panel_("dungeon.sprite_editor");
  }
  ImGui::TableNextColumn();
  if (DrawWorkbenchActionButton(ICON_MD_INVENTORY " Items", ImVec2(-1, 0))) {
    show_panel_("dungeon.item_editor");
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (DrawWorkbenchActionButton(ICON_MD_SETTINGS " Settings", ImVec2(-1, 0))) {
    show_panel_("dungeon.settings");
  }
  ImGui::TableNextColumn();
  ImGui::Dummy(ImVec2(0.0f, 0.0f));

  ImGui::EndTable();

  DrawWorkbenchInspectorSectionHeader(ICON_MD_TRAVEL_EXPLORE " Review");
  if (ImGui::BeginTable("##WorkbenchReviewGrid", 2, kFlags)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (DrawWorkbenchActionButton(ICON_MD_GRID_VIEW " Matrix", ImVec2(-1, 0))) {
      show_panel_("dungeon.room_matrix");
    }
    ImGui::TableNextColumn();
    if (DrawWorkbenchActionButton(ICON_MD_MAP " Dungeon Map", ImVec2(-1, 0))) {
      show_panel_("dungeon.dungeon_map");
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (DrawWorkbenchActionButton(ICON_MD_DOOR_FRONT " Entrances",
                                  ImVec2(-1, 0))) {
      show_panel_("dungeon.entrance_list");
    }
    ImGui::TableNextColumn();
    if (DrawWorkbenchActionButton(ICON_MD_PALETTE " Palette", ImVec2(-1, 0))) {
      show_panel_("dungeon.palette_editor");
    }
    ImGui::EndTable();
  }

  DrawWorkbenchInspectorSectionHeader(ICON_MD_KEYBOARD " Reference");
  if (DrawWorkbenchActionButton(ICON_MD_KEYBOARD " Keyboard Shortcuts",
                                ImVec2(-1, 0))) {
    show_shortcut_legend_ = true;
  }
  ShortcutLegendPanel::Draw(&show_shortcut_legend_);
}

}  // namespace yaze::editor
