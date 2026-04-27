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
#include "app/editor/dungeon/dungeon_selection_snapshot.h"
#include "app/editor/dungeon/ui/window/custom_collision_panel.h"
#include "app/editor/dungeon/ui/window/dungeon_map_panel.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/ui/window/room_tag_editor_panel.h"
#include "app/editor/dungeon/ui/window/shortcut_legend_panel.h"
#include "app/editor/dungeon/ui/window/water_fill_panel.h"
#include "app/editor/dungeon/ui/workbench/dungeon_workbench_chrome.h"
#include "app/editor/dungeon/widgets/dungeon_status_bar.h"
#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_layout.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/features.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_layer_manager.h"
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
      return "Layer 1 (Primary)";
    case 1:
      return "Layer 2 (BG2 overlay)";
    case 2:
      return "Layer 3 (BG1 overlay)";
    default:
      return "Unknown";
  }
}

const char* GetBg2ModeName(int value) {
  static constexpr const char* kNames[] = {
      "Off",      "Parallax", "Dark",        "On top",   "Translucent",
      "Addition", "Normal",   "Transparent", "Dark room"};
  constexpr int kNameCount = sizeof(kNames) / sizeof(kNames[0]);
  return (value >= 0 && value < kNameCount) ? kNames[value] : "Unknown";
}

const char* GetCollisionName(int value) {
  static constexpr const char* kNames[] = {"One", "Both", "Both + Scroll",
                                           "Moving Floor", "Moving Water"};
  constexpr int kNameCount = sizeof(kNames) / sizeof(kNames[0]);
  return (value >= 0 && value < kNameCount) ? kNames[value] : "Unknown";
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
    std::function<void()> on_save_all_rooms,
    std::function<DungeonCanvasViewer*()> get_viewer,
    std::function<DungeonCanvasViewer*()> get_compare_viewer,
    std::function<const std::deque<int>&()> get_recent_rooms,
    std::function<void(int)> forget_recent_room,
    std::function<void(bool)> set_workflow_mode, Rom* rom)
    : room_selector_(room_selector),
      current_room_id_(current_room_id),
      on_room_selected_(std::move(on_room_selected)),
      on_room_selected_with_intent_(std::move(on_room_selected_with_intent)),
      on_save_room_(std::move(on_save_room)),
      on_save_all_rooms_(std::move(on_save_all_rooms)),
      get_viewer_(std::move(get_viewer)),
      get_compare_viewer_(std::move(get_compare_viewer)),
      get_recent_rooms_(std::move(get_recent_rooms)),
      forget_recent_room_(std::move(forget_recent_room)),
      set_workflow_mode_(std::move(set_workflow_mode)),
      rom_(rom) {}

DungeonWorkbenchContent::~DungeonWorkbenchContent() = default;

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

void DungeonWorkbenchContent::SetEmbeddedToolPanels(
    RoomTagEditorPanel* room_tags, CustomCollisionPanel* custom_collision,
    WaterFillPanel* water_fill, MinecartTrackEditorPanel* minecart_tracks) {
  room_tag_panel_ = room_tags;
  custom_collision_panel_ = custom_collision;
  water_fill_panel_ = water_fill;
  minecart_track_panel_ = minecart_tracks;
}

void DungeonWorkbenchContent::SetEmbeddedEditorPanels(
    WindowContent* object_selector, WindowContent* door_editor,
    WindowContent* sprite_editor, WindowContent* item_editor,
    WindowContent* room_graphics, WindowContent* palette_editor) {
  object_selector_content_ = object_selector;
  door_editor_content_ = door_editor;
  sprite_editor_content_ = sprite_editor;
  item_editor_content_ = item_editor;
  room_graphics_content_ = room_graphics;
  palette_editor_content_ = palette_editor;
}

void DungeonWorkbenchContent::FocusRoomInspector() {
  layout_state_.show_right_inspector = true;
  inspector_mode_ = InspectorMode::Room;
}

void DungeonWorkbenchContent::FocusSelectionInspector() {
  layout_state_.show_right_inspector = true;
  inspector_mode_ = InspectorMode::Selection;
}

void DungeonWorkbenchContent::FocusEntranceBrowser() {
  layout_state_.show_left_sidebar = true;
  sidebar_mode_ = SidebarMode::Entrances;
}

void DungeonWorkbenchContent::ShowConnectedGraph() {
  layout_state_.show_connected_canvas_view = true;
  split_view_enabled_ = false;
}

void DungeonWorkbenchContent::RequestDungeonMapPopup() {
  open_dungeon_map_popup_ = true;
}

void DungeonWorkbenchContent::OpenObjectSelectorTool() {
  OpenTool(WorkbenchTool::ObjectSelector);
}

void DungeonWorkbenchContent::OpenDoorTool() {
  OpenTool(WorkbenchTool::DoorEditor);
}

void DungeonWorkbenchContent::OpenSpriteTool() {
  OpenTool(WorkbenchTool::SpriteEditor);
}

void DungeonWorkbenchContent::OpenItemTool() {
  OpenTool(WorkbenchTool::ItemEditor);
}

void DungeonWorkbenchContent::OpenRoomGraphicsTool() {
  OpenTool(WorkbenchTool::RoomGraphics);
}

void DungeonWorkbenchContent::OpenPaletteTool() {
  OpenTool(WorkbenchTool::Palette);
}

void DungeonWorkbenchContent::OpenRoomTagsTool() {
  OpenTool(WorkbenchTool::RoomTags);
}

void DungeonWorkbenchContent::OpenCustomCollisionTool() {
  OpenTool(WorkbenchTool::CustomCollision);
}

void DungeonWorkbenchContent::OpenWaterFillTool() {
  OpenTool(WorkbenchTool::WaterFill);
}

void DungeonWorkbenchContent::OpenMinecartTool() {
  OpenTool(WorkbenchTool::MinecartTracks);
}

bool DungeonWorkbenchContent::IsToolDrawerActiveForTesting() const {
  return inspector_mode_ == InspectorMode::Tools;
}

const char* DungeonWorkbenchContent::GetInspectorModeIdForTesting() const {
  switch (inspector_mode_) {
    case InspectorMode::Room:
      return "room";
    case InspectorMode::Selection:
      return "selection";
    case InspectorMode::Tools:
      return "tools";
  }
  return "unknown";
}

const char* DungeonWorkbenchContent::GetActiveToolIdForTesting() const {
  return GetWorkbenchToolId(active_tool_);
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
  const bool can_open_overview = true;
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
                                              ICON_MD_MORE_HORIZ, button_size,
                                              "Open room review tools", true)) {
            ImGui::OpenPopup("##WorkbenchSidebarQuickActions");
          }
          if (ImGui::BeginPopup("##WorkbenchSidebarQuickActions")) {
            if (ImGui::MenuItem(ICON_MD_VIEW_QUILT " Stitched Rooms")) {
              ShowConnectedGraph();
            }
            if (ImGui::MenuItem(ICON_MD_MAP " Dungeon Map")) {
              RequestDungeonMapPopup();
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
  // Compact icon-only segmented selector. Each button is square (size = height)
  // so the cluster takes ~70px instead of stretching to ~170px with labels.
  // Tooltips carry the full label.
  (void)stacked;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const ImVec2 button_size(segment_height, segment_height);

  if (gui::ToggleButton(ICON_MD_VIEW_LIST "##NavRooms",
                        sidebar_mode_ == SidebarMode::Rooms, button_size)) {
    sidebar_mode_ = SidebarMode::Rooms;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Rooms");
  }
  ImGui::SameLine(0.0f, spacing);
  if (gui::ToggleButton(ICON_MD_DOOR_FRONT "##NavEntrances",
                        sidebar_mode_ == SidebarMode::Entrances, button_size)) {
    sidebar_mode_ = SidebarMode::Entrances;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Entrances");
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
      ShowConnectedGraph();
    };
    params.on_save_room = on_save_room_;
    params.on_request_dungeon_map = [this]() {
      RequestDungeonMapPopup();
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

  // Mirror toggle: when inspector_on_left_, swap render order so the inspector
  // visually sits on the left of the canvas (ZScream-style). Width state is
  // preserved (right_width is always the inspector's, left_width always the
  // sidebar's) so splitter drags still affect the correct pane regardless of
  // which side it's on.
  if (inspector_on_left_) {
    if (show_right) {
      DrawInspectorPane(right_w, total_h, btn,
                        pane_layout.responsive.compact_right, primary_viewer);
      ImGui::SameLine(0.0f, 0.0f);
      if (DrawDungeonWorkbenchVerticalSplitter(
              "##DungeonWorkbenchLeftSplitter", total_h,
              &layout_state_.right_width, pane_layout.min_right_width,
              total_w - left_w - min_canvas_w - (show_left ? splitter_w : 0.0f),
              false)) {
        layout_state_.show_right_inspector = false;
      }
      ImGui::SameLine(0.0f, 0.0f);
    }
    DrawCanvasPane(center_w, total_h, primary_viewer, show_left);
    if (show_left) {
      ImGui::SameLine(0.0f, 0.0f);
      if (DrawDungeonWorkbenchVerticalSplitter(
              "##DungeonWorkbenchRightSplitter", total_h,
              &layout_state_.left_width, pane_layout.min_left_width,
              total_w - right_w - min_canvas_w -
                  (show_right ? splitter_w : 0.0f),
              true)) {
        layout_state_.show_left_sidebar = false;
      }
      ImGui::SameLine(0.0f, 0.0f);
      DrawSidebarPane(left_w, total_h, btn,
                      pane_layout.responsive.compact_left);
    }
  } else {
    if (show_left) {
      DrawSidebarPane(left_w, total_h, btn,
                      pane_layout.responsive.compact_left);
    }
    if (show_left) {
      ImGui::SameLine(0.0f, 0.0f);
      if (DrawDungeonWorkbenchVerticalSplitter(
              "##DungeonWorkbenchLeftSplitter", total_h,
              &layout_state_.left_width, pane_layout.min_left_width,
              total_w - right_w - min_canvas_w -
                  (show_right ? splitter_w : 0.0f),
              false)) {
        layout_state_.show_left_sidebar = false;
      }
    }

    if (show_left) {
      ImGui::SameLine(0.0f, 0.0f);
    }
    DrawCanvasPane(center_w, total_h, primary_viewer, show_left);

    if (show_right) {
      ImGui::SameLine(0.0f, 0.0f);
      if (DrawDungeonWorkbenchVerticalSplitter(
              "##DungeonWorkbenchRightSplitter", total_h,
              &layout_state_.right_width, pane_layout.min_right_width,
              total_w - left_w - min_canvas_w - (show_left ? splitter_w : 0.0f),
              true)) {
        layout_state_.show_right_inspector = false;
      }
      ImGui::SameLine(0.0f, 0.0f);
    }
    if (show_right) {
      DrawInspectorPane(right_w, total_h, btn,
                        pane_layout.responsive.compact_right, primary_viewer);
    }
  }
  if (primary_viewer) {
    DrawDungeonMapPopup(*primary_viewer);
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

      const char* tool_mode =
          primary_viewer->object_interaction().mode_manager().GetModeName();
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
  const DungeonSelectionSnapshot snapshot = BuildDungeonSelectionSnapshot(
      interaction, viewer.rooms(), viewer.current_room_id());
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
    if (workbench::DrawActionButton(label, ImVec2(0.0f, 0.0f)) && enabled) {
      on_press();
    }
    if (!enabled) {
      ImGui::EndDisabled();
    }
  };

  ImGui::TextColored(AgentUI::GetTheme().text_secondary_gray,
                     ICON_MD_SELECT_ALL " %s",
                     GetDungeonSelectionSummaryText(snapshot).c_str());

  const SelectedEntity selection = interaction.GetSelectedEntity();
  const char* local_tool_label = nullptr;
  WorkbenchTool local_tool = WorkbenchTool::None;
  if (object_count == 0 && has_entity) {
    switch (selection.type) {
      case EntityType::Door:
        local_tool = WorkbenchTool::DoorEditor;
        local_tool_label = ICON_MD_DOOR_FRONT " Door Tools";
        break;
      case EntityType::Sprite:
        local_tool = WorkbenchTool::SpriteEditor;
        local_tool_label = ICON_MD_PERSON " Sprite Tools";
        break;
      case EntityType::Item:
        local_tool = WorkbenchTool::ItemEditor;
        local_tool_label = ICON_MD_INVENTORY " Item Tools";
        break;
      default:
        break;
    }
  }

  const bool can_copy_selection = snapshot.object_count > 0 ||
                                  snapshot.sprite_count > 0 ||
                                  snapshot.item_count > 0;
  draw_action(ICON_MD_CONTENT_COPY " Copy", can_copy_selection,
              [&]() { interaction.HandleCopySelected(); });
  draw_action(ICON_MD_CONTENT_PASTE " Paste", interaction.HasClipboardData(),
              [&]() { interaction.HandlePasteObjects(); });
  draw_action(ICON_MD_DELETE " Delete", true,
              [&]() { interaction.HandleDeleteSelected(); });
  draw_action(ICON_MD_CLEAR " Clear", true, [&]() {
    interaction.ClearSelection();
    interaction.ClearEntitySelection();
  });
  draw_action(ICON_MD_TUNE " Inspector", true,
              [&]() { FocusSelectionInspector(); });
  if (local_tool_label != nullptr) {
    draw_action(local_tool_label, true, [&]() { OpenTool(local_tool); });
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
      DrawInspector(*viewer, compact);
    } else {
      ImGui::TextDisabled("No active viewer");
    }
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawInspectorHeader(float button_size,
                                                  bool compact) {
  // Chevron flips with mirror state so the collapse arrow always points
  // toward the inspector's exit edge (off-right when on right, off-left when
  // on left).
  const char* collapse_icon =
      inspector_on_left_ ? ICON_MD_CHEVRON_LEFT : ICON_MD_CHEVRON_RIGHT;
  const float collapse_w =
      workbench::CalcIconButtonWidth(collapse_icon, button_size);
  const float swap_w =
      workbench::CalcIconButtonWidth(ICON_MD_SWAP_HORIZ, button_size);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float action_cluster_w = swap_w + spacing + collapse_w;

  workbench::DrawPaneHeader(
      "##DungeonWorkbenchInspectorHeader", ICON_MD_TUNE, "Inspect", "Inspect",
      nullptr, compact, action_cluster_w, [&]() {
        if (workbench::DrawHeaderIconAction(
                "SwapInspectorSide", ICON_MD_SWAP_HORIZ, button_size,
                inspector_on_left_ ? "Move inspector to the right side"
                                   : "Move inspector to the left side "
                                     "(ZScream layout)")) {
          inspector_on_left_ = !inspector_on_left_;
          if (on_inspector_side_changed_) {
            on_inspector_side_changed_(inspector_on_left_);
          }
        }
        ImGui::SameLine(0.0f, spacing);
        if (workbench::DrawHeaderIconAction("CollapseInspector", collapse_icon,
                                            button_size,
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

void DungeonWorkbenchContent::DrawInspector(DungeonCanvasViewer& viewer,
                                            bool compact) {
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(4.0f, ImGui::GetStyle().ItemSpacing.x * 0.75f),
             std::max(4.0f, ImGui::GetStyle().ItemSpacing.y * 0.7f)));
  DrawInspectorShelf(viewer, compact);
}

void DungeonWorkbenchContent::SetAllSaveFlags(bool value) {
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = value;
  flags.kSaveSprites = value;
  flags.kSaveRoomHeaders = value;
  flags.kSaveChests = value;
  flags.kSavePotItems = value;
  flags.kSavePalettes = value;
  flags.kSaveCollision = value;
  flags.kSaveWaterFillZones = value;
  flags.kSaveBlocks = value;
  flags.kSaveTorches = value;
  flags.kSavePits = value;
}

void DungeonWorkbenchContent::DrawApplyScopeControls(int room_id) {
  auto& flags = core::FeatureFlags::get().dungeon;
  bool use_workbench = flags.kUseWorkbench;
  if (ImGui::Checkbox("Single-window Workbench", &use_workbench)) {
    flags.kUseWorkbench = use_workbench;
    if (set_workflow_mode_) {
      set_workflow_mode_(use_workbench);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Keep Dungeon editing in the integrated Workbench instead of separate "
        "high-level room panels.");
  }

  ImGui::Separator();
  ImGui::TextDisabled("Data written by Apply Room / Apply Loaded Rooms");
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (ImGui::BeginTable("##WorkbenchApplyScopeFlags", 2, kFlags)) {
    auto draw_checkbox = [](const char* label, bool* value) {
      ImGui::TableNextColumn();
      ImGui::Checkbox(label, value);
    };
    ImGui::TableNextRow();
    draw_checkbox("Room Objects", &flags.kSaveObjects);
    draw_checkbox("Sprites", &flags.kSaveSprites);
    ImGui::TableNextRow();
    draw_checkbox("Room Headers", &flags.kSaveRoomHeaders);
    draw_checkbox("Chests", &flags.kSaveChests);
    ImGui::TableNextRow();
    draw_checkbox("Pot Items", &flags.kSavePotItems);
    draw_checkbox("Palettes", &flags.kSavePalettes);
    ImGui::TableNextRow();
    draw_checkbox("Collision Maps", &flags.kSaveCollision);
    draw_checkbox("Water Fill", &flags.kSaveWaterFillZones);
    ImGui::TableNextRow();
    draw_checkbox("Blocks", &flags.kSaveBlocks);
    draw_checkbox("Torches", &flags.kSaveTorches);
    ImGui::TableNextRow();
    draw_checkbox("Pits", &flags.kSavePits);
    ImGui::TableNextColumn();
    ImGui::EndTable();
  }

  if (ImGui::SmallButton("Select All##WorkbenchApplyScope")) {
    SetAllSaveFlags(true);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Select None##WorkbenchApplyScope")) {
    SetAllSaveFlags(false);
  }

  ImGui::Separator();
  if (on_save_room_ && room_id >= 0 &&
      workbench::DrawActionButton(ICON_MD_SAVE " Apply Current Room",
                                  ImVec2(-1, 0))) {
    on_save_room_(room_id);
  }
  if (on_save_all_rooms_ &&
      workbench::DrawActionButton(ICON_MD_SAVE_ALT " Apply Loaded Rooms",
                                  ImVec2(-1, 0))) {
    on_save_all_rooms_();
  }
}

void DungeonWorkbenchContent::DrawLayerCompositingControls(
    DungeonCanvasViewer& viewer, int room_id) {
  if (room_id < 0) {
    ImGui::TextDisabled("No active room");
    return;
  }

  auto& layer_manager = viewer.GetRoomLayerManager(room_id);
  auto draw_blend_combo = [&](const char* combo_id,
                              zelda3::LayerType layer_type) {
    zelda3::LayerBlendMode current_mode =
        layer_manager.GetLayerBlendMode(layer_type);
    const char* current_name =
        zelda3::RoomLayerManager::GetBlendModeName(current_mode);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(combo_id, current_name)) {
      for (int mode_int = 0; mode_int <= 4; ++mode_int) {
        const auto mode = static_cast<zelda3::LayerBlendMode>(mode_int);
        const char* mode_name =
            zelda3::RoomLayerManager::GetBlendModeName(mode);
        const bool selected = current_mode == mode;
        if (ImGui::Selectable(mode_name, selected)) {
          layer_manager.SetLayerBlendMode(layer_type, mode);
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
  };

  struct BlendRow {
    const char* label;
    const char* combo_id;
    zelda3::LayerType layer_type;
  };
  static constexpr BlendRow kBlendRows[] = {
      {"BG1 Layout", "##WorkbenchBlendBG1Layout",
       zelda3::LayerType::BG1_Layout},
      {"BG1 Objects", "##WorkbenchBlendBG1Objects",
       zelda3::LayerType::BG1_Objects},
      {"BG2 Layout", "##WorkbenchBlendBG2Layout",
       zelda3::LayerType::BG2_Layout},
      {"BG2 Objects", "##WorkbenchBlendBG2Objects",
       zelda3::LayerType::BG2_Objects},
  };

  constexpr ImGuiTableFlags kBlendTableFlags =
      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX;
  if (ImGui::BeginTable("##WorkbenchLayerBlend", 2, kBlendTableFlags)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 92.0f);
    ImGui::TableSetupColumn("##combo", ImGuiTableColumnFlags_WidthStretch);
    for (const auto& row : kBlendRows) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(row.label);
      ImGui::TableNextColumn();
      draw_blend_combo(row.combo_id, row.layer_type);
    }
    ImGui::EndTable();
  }

  if (workbench::DrawActionButton(ICON_MD_REFRESH " Reset Layer Blend",
                                  ImVec2(-1, 0))) {
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG1_Layout,
                                    zelda3::LayerBlendMode::Normal);
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG1_Objects,
                                    zelda3::LayerBlendMode::Normal);
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG2_Layout,
                                    zelda3::LayerBlendMode::Normal);
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                                    zelda3::LayerBlendMode::Normal);
  }
}

DungeonMapPanel* DungeonWorkbenchContent::GetEmbeddedDungeonMap(
    DungeonCanvasViewer& viewer) {
  if (!room_selector_ || !current_room_id_) {
    return nullptr;
  }
  if (!embedded_dungeon_map_) {
    embedded_dungeon_map_ = std::make_unique<DungeonMapPanel>(
        current_room_id_, &room_selector_->mutable_active_rooms(),
        on_room_selected_, viewer.rooms());
    embedded_dungeon_map_->SetRoomIntentCallback(on_room_selected_with_intent_);
    if (*current_room_id_ >= 0) {
      embedded_dungeon_map_->AddRoom(*current_room_id_);
    }
  }
  embedded_dungeon_map_->SetRooms(viewer.rooms());
  if (const auto* project = viewer.project()) {
    embedded_dungeon_map_->SetHackManifest(&project->hack_manifest);
  } else {
    embedded_dungeon_map_->SetHackManifest(nullptr);
  }
  return embedded_dungeon_map_.get();
}

void DungeonWorkbenchContent::DrawDungeonMapPopup(DungeonCanvasViewer& viewer) {
  constexpr const char* kPopupId = "Dungeon Map##WorkbenchDungeonMapPopup";
  if (open_dungeon_map_popup_) {
    ImGui::SetNextWindowSize(ImVec2(660.0f, 520.0f), ImGuiCond_Appearing);
    ImGui::OpenPopup(kPopupId);
    open_dungeon_map_popup_ = false;
  }

  bool popup_open = true;
  if (ImGui::BeginPopupModal(kPopupId, &popup_open,
                             ImGuiWindowFlags_NoSavedSettings)) {
    if (!popup_open) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }
    if (auto* map = GetEmbeddedDungeonMap(viewer)) {
      const ImVec2 map_size =
          ImVec2(std::max(360.0f, ImGui::GetContentRegionAvail().x),
                 std::max(260.0f, ImGui::GetContentRegionAvail().y - 34.0f));
      if (ImGui::BeginChild("##WorkbenchDungeonMapBody", map_size, false)) {
        map->Draw(nullptr);
      }
      ImGui::EndChild();
    } else {
      ImGui::TextDisabled("Dungeon map unavailable");
    }

    if (workbench::DrawActionButton(ICON_MD_CLOSE " Close", ImVec2(-1, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void DungeonWorkbenchContent::OpenTool(WorkbenchTool tool) {
  if (tool == WorkbenchTool::None) {
    return;
  }
  active_tool_ = tool;
  inspector_mode_ = InspectorMode::Tools;
  layout_state_.show_right_inspector = true;
}

bool DungeonWorkbenchContent::IsWorkbenchToolAvailable(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return room_tag_panel_ != nullptr;
    case WorkbenchTool::CustomCollision:
      return custom_collision_panel_ != nullptr;
    case WorkbenchTool::WaterFill:
      return water_fill_panel_ != nullptr;
    case WorkbenchTool::MinecartTracks:
      return minecart_track_panel_ != nullptr;
    case WorkbenchTool::ObjectSelector:
      return object_selector_content_ != nullptr;
    case WorkbenchTool::DoorEditor:
      return door_editor_content_ != nullptr;
    case WorkbenchTool::SpriteEditor:
      return sprite_editor_content_ != nullptr;
    case WorkbenchTool::ItemEditor:
      return item_editor_content_ != nullptr;
    case WorkbenchTool::RoomGraphics:
      return room_graphics_content_ != nullptr;
    case WorkbenchTool::Palette:
      return palette_editor_content_ != nullptr;
    case WorkbenchTool::None:
      return false;
  }
  return false;
}

const char* DungeonWorkbenchContent::GetWorkbenchToolId(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return "room_tags";
    case WorkbenchTool::CustomCollision:
      return "custom_collision";
    case WorkbenchTool::WaterFill:
      return "water_fill";
    case WorkbenchTool::MinecartTracks:
      return "minecart";
    case WorkbenchTool::ObjectSelector:
      return "object_selector";
    case WorkbenchTool::DoorEditor:
      return "door";
    case WorkbenchTool::SpriteEditor:
      return "sprite";
    case WorkbenchTool::ItemEditor:
      return "item";
    case WorkbenchTool::RoomGraphics:
      return "room_graphics";
    case WorkbenchTool::Palette:
      return "palette";
    case WorkbenchTool::None:
      return "none";
  }
  return "unknown";
}

const char* DungeonWorkbenchContent::GetWorkbenchToolTitle(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return ICON_MD_LABEL " Room Tags";
    case WorkbenchTool::CustomCollision:
      return ICON_MD_GRID_ON " Custom Collision";
    case WorkbenchTool::WaterFill:
      return ICON_MD_WATER_DROP " Water Fill";
    case WorkbenchTool::MinecartTracks:
      return ICON_MD_TRAIN " Minecart Tracks";
    case WorkbenchTool::ObjectSelector:
      return ICON_MD_CATEGORY " Object Selector";
    case WorkbenchTool::DoorEditor:
      return ICON_MD_DOOR_FRONT " Door Tools";
    case WorkbenchTool::SpriteEditor:
      return ICON_MD_PERSON " Sprite Tools";
    case WorkbenchTool::ItemEditor:
      return ICON_MD_INVENTORY " Item Tools";
    case WorkbenchTool::RoomGraphics:
      return ICON_MD_IMAGE " Room Graphics";
    case WorkbenchTool::Palette:
      return ICON_MD_PALETTE " Palette";
    case WorkbenchTool::None:
      return ICON_MD_BUILD " Tool";
  }
  return ICON_MD_BUILD " Tool";
}

const char* DungeonWorkbenchContent::GetWorkbenchToolIcon(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return ICON_MD_LABEL;
    case WorkbenchTool::CustomCollision:
      return ICON_MD_GRID_ON;
    case WorkbenchTool::WaterFill:
      return ICON_MD_WATER_DROP;
    case WorkbenchTool::MinecartTracks:
      return ICON_MD_TRAIN;
    case WorkbenchTool::ObjectSelector:
      return ICON_MD_CATEGORY;
    case WorkbenchTool::DoorEditor:
      return ICON_MD_DOOR_FRONT;
    case WorkbenchTool::SpriteEditor:
      return ICON_MD_PERSON;
    case WorkbenchTool::ItemEditor:
      return ICON_MD_INVENTORY;
    case WorkbenchTool::RoomGraphics:
      return ICON_MD_IMAGE;
    case WorkbenchTool::Palette:
      return ICON_MD_PALETTE;
    case WorkbenchTool::None:
      return ICON_MD_BUILD;
  }
  return ICON_MD_BUILD;
}

const char* DungeonWorkbenchContent::GetWorkbenchToolShortLabel(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return "Room Tags";
    case WorkbenchTool::CustomCollision:
      return "Custom Collision";
    case WorkbenchTool::WaterFill:
      return "Water Fill";
    case WorkbenchTool::MinecartTracks:
      return "Minecart Tracks";
    case WorkbenchTool::ObjectSelector:
      return "Object Selector";
    case WorkbenchTool::DoorEditor:
      return "Door Tools";
    case WorkbenchTool::SpriteEditor:
      return "Sprite Tools";
    case WorkbenchTool::ItemEditor:
      return "Item Tools";
    case WorkbenchTool::RoomGraphics:
      return "Room Graphics";
    case WorkbenchTool::Palette:
      return "Palette";
    case WorkbenchTool::None:
      return "Tool";
  }
  return "Tool";
}

const char* DungeonWorkbenchContent::GetWorkbenchToolUnavailableMessage(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return "Room tag tools are not available.";
    case WorkbenchTool::CustomCollision:
      return "Custom collision tools are not available.";
    case WorkbenchTool::WaterFill:
      return "Water fill tools are not available.";
    case WorkbenchTool::MinecartTracks:
      return "Minecart track tools are not available.";
    case WorkbenchTool::ObjectSelector:
      return "Object selector is not available.";
    case WorkbenchTool::DoorEditor:
      return "Door tools are not available.";
    case WorkbenchTool::SpriteEditor:
      return "Sprite tools are not available.";
    case WorkbenchTool::ItemEditor:
      return "Item tools are not available.";
    case WorkbenchTool::RoomGraphics:
      return "Room graphics tools are not available.";
    case WorkbenchTool::Palette:
      return "Palette tools are not available.";
    case WorkbenchTool::None:
      return "No Workbench tool selected.";
  }
  return "Tool is not available.";
}

void DungeonWorkbenchContent::DrawWorkbenchTool(DungeonCanvasViewer& viewer,
                                                WorkbenchTool tool) {
  const int room_id = viewer.current_room_id();
  auto draw_window_content = [](WindowContent* content,
                                const char* unavailable_message) {
    if (!content) {
      ImGui::TextDisabled("%s", unavailable_message);
      return;
    }
    content->Draw(nullptr);
  };

  ImGui::PushID(GetWorkbenchToolId(tool));
  switch (tool) {
    case WorkbenchTool::RoomTags:
      if (!room_tag_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      room_tag_panel_->SetCurrentRoomId(room_id);
      room_tag_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::CustomCollision:
      if (!custom_collision_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      custom_collision_panel_->SetCanvasViewer(&viewer);
      custom_collision_panel_->SetInteraction(&viewer.object_interaction());
      custom_collision_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::WaterFill:
      if (!water_fill_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      water_fill_panel_->SetCanvasViewer(&viewer);
      water_fill_panel_->SetInteraction(&viewer.object_interaction());
      water_fill_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::MinecartTracks:
      if (!minecart_track_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      minecart_track_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::ObjectSelector:
      draw_window_content(object_selector_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::DoorEditor:
      draw_window_content(door_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::SpriteEditor:
      draw_window_content(sprite_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::ItemEditor:
      draw_window_content(item_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::RoomGraphics:
      draw_window_content(room_graphics_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::Palette:
      draw_window_content(palette_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::None:
      ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
      break;
  }
  ImGui::PopID();
}

void DungeonWorkbenchContent::DrawInspectorToolStrip() {
  // Two-row icon strip lets users swap tools in one click without scrolling
  // past the active body. Row 1 holds entity/selection tools; row 2 holds
  // room-data tools. Active tool is highlighted via gui::ToggleButton accent.
  static constexpr WorkbenchTool kStrip[2][5] = {
      {WorkbenchTool::ObjectSelector, WorkbenchTool::DoorEditor,
       WorkbenchTool::SpriteEditor, WorkbenchTool::ItemEditor,
       WorkbenchTool::Palette},
      {WorkbenchTool::RoomGraphics, WorkbenchTool::RoomTags,
       WorkbenchTool::CustomCollision, WorkbenchTool::WaterFill,
       WorkbenchTool::MinecartTracks},
  };

  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (!ImGui::BeginTable("##WorkbenchToolStrip", 5, kFlags)) {
    return;
  }
  for (int row = 0; row < 2; ++row) {
    ImGui::TableNextRow();
    for (int col = 0; col < 5; ++col) {
      ImGui::TableNextColumn();
      const WorkbenchTool tool = kStrip[row][col];
      const bool enabled = IsWorkbenchToolAvailable(tool);
      const bool active = active_tool_ == tool;
      char btn_id[48];
      std::snprintf(btn_id, sizeof(btn_id), "%s##StripTool_%s",
                    GetWorkbenchToolIcon(tool), GetWorkbenchToolId(tool));
      if (!enabled) {
        ImGui::BeginDisabled();
      }
      if (gui::ToggleButton(btn_id, active, ImVec2(-1, 0)) && enabled) {
        OpenTool(tool);
      }
      if (!enabled) {
        ImGui::EndDisabled();
      }
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (enabled) {
          ImGui::SetTooltip("%s", GetWorkbenchToolShortLabel(tool));
        } else {
          ImGui::SetTooltip("%s\n(%s)", GetWorkbenchToolShortLabel(tool),
                            GetWorkbenchToolUnavailableMessage(tool));
        }
      }
    }
  }
  ImGui::EndTable();
}

void DungeonWorkbenchContent::DrawInspectorToolDrawer(
    DungeonCanvasViewer& viewer) {
  // Quick-switch strip first: users can hop between tools without leaving the
  // drawer. The inspector primary segmented selector handles Room/Selection
  // returns, so no in-drawer back button is needed.
  DrawInspectorToolStrip();

  ImGui::Dummy(ImVec2(0.0f, 2.0f));
  workbench::DrawInspectorSectionHeader(GetWorkbenchToolTitle(active_tool_));

  const bool available = IsWorkbenchToolAvailable(active_tool_);
  if (!available) {
    ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(active_tool_));
    return;
  }

  // The strip + section header take ~70-90px depending on font scale. Anything
  // remaining in the inspector belongs to the tool body, with a small floor so
  // narrow inspectors still leave room for at least a few rows of controls.
  const float available_h = std::max(180.0f, ImGui::GetContentRegionAvail().y);
  const bool body_open =
      ImGui::BeginChild("##WorkbenchToolDrawerBody", ImVec2(0.0f, available_h),
                        true, ImGuiWindowFlags_HorizontalScrollbar);
  if (body_open) {
    DrawWorkbenchTool(viewer, active_tool_);
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawInspectorPrimarySelector(
    float segment_height) {
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float room_width =
      workbench::CalcIconButtonWidth("Room", segment_height);
  const float selection_width =
      workbench::CalcIconButtonWidth("Selection", segment_height);
  const float tools_width =
      workbench::CalcIconButtonWidth("Tools", segment_height);
  const float available_width = ImGui::GetContentRegionAvail().x;
  const bool stack = available_width < (room_width + selection_width +
                                        tools_width + spacing * 2.0f);
  auto draw_mode = [&](const char* label, float width, InspectorMode mode) {
    const ImVec2 size(stack ? ImGui::GetContentRegionAvail().x : width,
                      segment_height);
    if (gui::ToggleButton(label, inspector_mode_ == mode, size)) {
      inspector_mode_ = mode;
    }
    if (!stack) {
      ImGui::SameLine(0.0f, spacing);
    }
  };

  draw_mode("Room", room_width, InspectorMode::Room);
  draw_mode("Selection", selection_width, InspectorMode::Selection);
  const ImVec2 tools_size(
      stack ? ImGui::GetContentRegionAvail().x : tools_width, segment_height);
  if (gui::ToggleButton("Tools", inspector_mode_ == InspectorMode::Tools,
                        tools_size)) {
    inspector_mode_ = InspectorMode::Tools;
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
    if (workbench::DrawActionButton(ICON_MD_OPEN_IN_FULL " Open Selection",
                                    ImVec2(-1, 0))) {
      inspector_mode_ = InspectorMode::Selection;
    }
  } else {
    ImGui::TextDisabled("Nothing selected");
  }

  // Apply Room, View overlays, and Tools quick-grid all live elsewhere now:
  // Apply Room is on the canvas toolbar, the overlay checkboxes live in the
  // toolbar's View Options popup, and Tools have their own inspector primary
  // mode (the segmented selector at the inspector header). Compact summary
  // stays focused on what's selected.
}

void DungeonWorkbenchContent::DrawInspectorShelf(DungeonCanvasViewer& viewer,
                                                 bool compact) {
  const auto& interaction = viewer.object_interaction();
  const bool has_selection =
      interaction.GetSelectionCount() > 0 || interaction.HasEntitySelection();
  if (has_selection && !inspector_selection_was_active_ &&
      inspector_mode_ != InspectorMode::Tools) {
    inspector_mode_ = InspectorMode::Selection;
  }
  inspector_selection_was_active_ = has_selection;

  // Use the resolved pane layout instead of GetContentRegionAvail().x. The
  // latter changes when a vertical scrollbar appears, which can make the
  // inspector alternate between full and compact content every frame.
  if (compact) {
    DrawInspectorCompactSummary(viewer);
    return;
  }

  switch (inspector_mode_) {
    case InspectorMode::Room:
      DrawInspectorShelfRoom(viewer);
      break;
    case InspectorMode::Selection:
      DrawInspectorShelfSelection(viewer);
      break;
    case InspectorMode::Tools:
      DrawInspectorToolDrawer(viewer);
      return;
  }

  // Stitched Rooms, View Options, and Tools collapsibles intentionally
  // removed: the canvas toolbar exposes the Stitched Rooms toggle directly,
  // the View Options popup off the toolbar's eye icon owns all 11 overlay
  // checkboxes, and the inspector primary segmented selector at the header
  // already routes between Room / Selection / Tools modes.
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
  workbench::DrawInspectorSectionHeader(ICON_MD_CASTLE " Room Summary");
  if (room_id >= 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Room");
    ImGui::SameLine();
    uint16_t requested_room_id = static_cast<uint16_t>(room_id);
    ImGui::SetNextItemWidth(74.0f);
    const bool can_jump_room = static_cast<bool>(on_room_selected_);
    if (!can_jump_room) {
      ImGui::BeginDisabled();
    }
    if (auto res = gui::InputHexWordEx("##WorkbenchRoomId", &requested_room_id,
                                       74.0f, true);
        res.ShouldApply()) {
      const int target_room_id = std::clamp(static_cast<int>(requested_room_id),
                                            0, zelda3::kNumberOfRooms - 1);
      if (target_room_id != room_id && on_room_selected_) {
        if (current_room_id_) {
          *current_room_id_ = target_room_id;
        }
        on_room_selected_(target_room_id);
      }
    }
    if (!can_jump_room) {
      ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered()) {
      const int preview_room_id = std::clamp(
          static_cast<int>(requested_room_id), 0, zelda3::kNumberOfRooms - 1);
      ImGui::SetTooltip("Open room 0x%03X", preview_room_id);
    }
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

  // Apply Room and Dungeon Map have moved to the canvas toolbar (Save and
  // Map icons). Apply Scope and Layer Compositing remain here as
  // collapsibles since they're rarely-touched per-room batch settings.
  if (workbench::BeginInspectorSection(ICON_MD_SAVE_ALT " Apply Scope",
                                       false)) {
    DrawApplyScopeControls(room_id);
  }

  if (workbench::BeginInspectorSection(ICON_MD_LAYERS " Layer Compositing",
                                       false)) {
    DrawLayerCompositingControls(viewer, room_id);
  }

  // ZScream-style compact room header: keep the raw ROM fields together so
  // experienced dungeon authors can scan and edit them without panel hopping.
  workbench::DrawInspectorSectionHeader(ICON_MD_TUNE " Room Header");
  if (auto* rooms = viewer.rooms();
      rooms && room_id >= 0 && room_id < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[room_id];

    uint8_t layout_val = room.layout_id();
    uint8_t blockset_val = room.blockset();
    uint8_t floor1_val = room.floor1();
    uint8_t floor2_val = room.floor2();
    uint8_t palette_val = room.palette();
    uint8_t spriteset_val = room.spriteset();
    uint16_t message_val = room.message_id();
    uint8_t bg2_val = static_cast<uint8_t>(room.bg2());
    uint8_t effect_val = static_cast<uint8_t>(room.effect());
    uint8_t collision_val = static_cast<uint8_t>(room.collision());
    uint8_t tag1_val = static_cast<uint8_t>(room.tag1());
    uint8_t tag2_val = static_cast<uint8_t>(room.tag2());

    auto render_room_graphics = [&]() {
      if (room.rom() && room.rom()->is_loaded()) {
        room.RenderRoomGraphics();
      }
    };

    constexpr float kHexW = 54.0f;
    constexpr ImGuiTableFlags kHeaderFlags = ImGuiTableFlags_BordersInnerV |
                                             ImGuiTableFlags_RowBg |
                                             ImGuiTableFlags_NoPadOuterX;
    auto draw_label = [](const char* label) {
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(label);
    };
    auto draw_byte_field = [&](const char* label, const char* id, uint8_t value,
                               uint8_t max_value, const std::string& tooltip,
                               auto apply) {
      ImGui::TableNextColumn();
      draw_label(label);
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(kHexW);
      uint8_t edit_value = value;
      if (auto res =
              gui::InputHexByteEx(id, &edit_value, max_value, kHexW, true);
          res.ShouldApply()) {
        apply(edit_value);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip.c_str());
      }
    };
    auto draw_word_field = [&](const char* label, const char* id,
                               uint16_t value, uint16_t max_value,
                               const std::string& tooltip, auto apply) {
      ImGui::TableNextColumn();
      draw_label(label);
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(kHexW + 12.0f);
      uint16_t edit_value = value;
      if (auto res = gui::InputHexWordEx(id, &edit_value, kHexW + 12.0f, true);
          res.ShouldApply()) {
        apply(std::min<uint16_t>(edit_value, max_value));
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip.c_str());
      }
    };

    if (ImGui::BeginTable("##WorkbenchRoomHeader", 4, kHeaderFlags)) {
      ImGui::TableSetupColumn("L1", ImGuiTableColumnFlags_WidthFixed, 44.0f);
      ImGui::TableSetupColumn("V1", ImGuiTableColumnFlags_WidthFixed, 66.0f);
      ImGui::TableSetupColumn("L2", ImGuiTableColumnFlags_WidthFixed, 44.0f);
      ImGui::TableSetupColumn("V2", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      draw_byte_field("Lay", "##RoomHeaderLayout", layout_val, 0x07,
                      "Layout (0-7)", [&](uint8_t value) {
                        room.SetLayoutId(value);
                        room.MarkLayoutDirty();
                        render_room_graphics();
                      });
      draw_byte_field("Blk", "##RoomHeaderBlockset", blockset_val, 0x51,
                      "Blockset (0-51)", [&](uint8_t value) {
                        room.SetBlockset(value);
                        render_room_graphics();
                      });

      ImGui::TableNextRow();
      draw_byte_field("F1", "##RoomHeaderFloor1", floor1_val, 0x0F,
                      "BG1 floor graphics (0-F)", [&](uint8_t value) {
                        room.set_floor1(value);
                        render_room_graphics();
                      });
      draw_byte_field("F2", "##RoomHeaderFloor2", floor2_val, 0x0F,
                      "BG2 floor graphics (0-F)", [&](uint8_t value) {
                        room.set_floor2(value);
                        render_room_graphics();
                      });

      ImGui::TableNextRow();
      draw_byte_field("Pal", "##RoomHeaderPalette", palette_val, 0x47,
                      "Palette set (0-47)", [&](uint8_t value) {
                        room.SetPalette(value);
                        render_room_graphics();
                        if (on_room_selected_) {
                          on_room_selected_(room_id);
                        }
                      });
      draw_byte_field("Spr", "##RoomHeaderSpriteset", spriteset_val, 0x8F,
                      "Sprite graphics set (0-8F)", [&](uint8_t value) {
                        room.SetSpriteset(value);
                        render_room_graphics();
                      });

      ImGui::TableNextRow();
      draw_word_field("Msg", "##RoomHeaderMessage", message_val, 0x0FFF,
                      "Dungeon message ID (0-FFF)",
                      [&](uint16_t value) { room.SetMessageId(value); });
      draw_byte_field("BG2", "##RoomHeaderBg2", bg2_val, 0x08,
                      std::string("BG2 mode: ") + GetBg2ModeName(bg2_val),
                      [&](uint8_t value) {
                        room.SetBg2(static_cast<background2>(value));
                        render_room_graphics();
                      });

      ImGui::TableNextRow();
      const char* effect_name =
          effect_val < 8 ? zelda3::RoomEffect[effect_val].c_str() : "Unknown";
      draw_byte_field("FX", "##RoomHeaderEffect", effect_val, 0x07,
                      std::string("Effect: ") + effect_name,
                      [&](uint8_t value) {
                        room.SetEffect(static_cast<zelda3::EffectKey>(value));
                        render_room_graphics();
                      });
      draw_byte_field(
          "Coll", "##RoomHeaderCollision", collision_val, 0x04,
          std::string("Collision: ") + GetCollisionName(collision_val),
          [&](uint8_t value) {
            room.SetCollision(static_cast<zelda3::CollisionKey>(value));
          });

      ImGui::TableNextRow();
      draw_byte_field("Tag1", "##RoomHeaderTag1", tag1_val, 0x40,
                      std::string("Tag1: ") + zelda3::GetRoomTagLabel(tag1_val),
                      [&](uint8_t value) {
                        room.SetTag1(static_cast<zelda3::TagKey>(value));
                        render_room_graphics();
                      });
      draw_byte_field("Tag2", "##RoomHeaderTag2", tag2_val, 0x40,
                      std::string("Tag2: ") + zelda3::GetRoomTagLabel(tag2_val),
                      [&](uint8_t value) {
                        room.SetTag2(static_cast<zelda3::TagKey>(value));
                        render_room_graphics();
                      });

      ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    workbench::DrawInspectorSectionHeader(ICON_MD_ALT_ROUTE " Destinations");
    if (ImGui::BeginTable("##WorkbenchRoomDestinations", 4, kHeaderFlags)) {
      ImGui::TableSetupColumn("L1", ImGuiTableColumnFlags_WidthFixed, 44.0f);
      ImGui::TableSetupColumn("V1", ImGuiTableColumnFlags_WidthFixed, 66.0f);
      ImGui::TableSetupColumn("L2", ImGuiTableColumnFlags_WidthFixed, 44.0f);
      ImGui::TableSetupColumn("V2", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      draw_byte_field("Pit", "##RoomHeaderPit", room.holewarp(), 0xFF,
                      "Pit/holewarp destination room",
                      [&](uint8_t value) { room.SetHolewarp(value); });
      draw_byte_field("St1", "##RoomHeaderStair1", room.staircase_room(0), 0xFF,
                      "Stair destination slot 1",
                      [&](uint8_t value) { room.SetStaircaseRoom(0, value); });

      ImGui::TableNextRow();
      draw_byte_field("St2", "##RoomHeaderStair2", room.staircase_room(1), 0xFF,
                      "Stair destination slot 2",
                      [&](uint8_t value) { room.SetStaircaseRoom(1, value); });
      draw_byte_field("St3", "##RoomHeaderStair3", room.staircase_room(2), 0xFF,
                      "Stair destination slot 3",
                      [&](uint8_t value) { room.SetStaircaseRoom(2, value); });

      ImGui::TableNextRow();
      draw_byte_field("St4", "##RoomHeaderStair4", room.staircase_room(3), 0xFF,
                      "Stair destination slot 4",
                      [&](uint8_t value) { room.SetStaircaseRoom(3, value); });
      draw_byte_field("P1", "##RoomHeaderStairPlane1", room.staircase_plane(0),
                      0x03, "Stair 1 target layer/plane (0-3)",
                      [&](uint8_t value) { room.SetStaircasePlane(0, value); });

      ImGui::TableNextRow();
      draw_byte_field("P2", "##RoomHeaderStairPlane2", room.staircase_plane(1),
                      0x03, "Stair 2 target layer/plane (0-3)",
                      [&](uint8_t value) { room.SetStaircasePlane(1, value); });
      draw_byte_field("P3", "##RoomHeaderStairPlane3", room.staircase_plane(2),
                      0x03, "Stair 3 target layer/plane (0-3)",
                      [&](uint8_t value) { room.SetStaircasePlane(2, value); });

      ImGui::TableNextRow();
      draw_byte_field("P4", "##RoomHeaderStairPlane4", room.staircase_plane(3),
                      0x03, "Stair 4 target layer/plane (0-3)",
                      [&](uint8_t value) { room.SetStaircasePlane(3, value); });
      ImGui::TableNextColumn();
      ImGui::TableNextColumn();

      ImGui::EndTable();
    }
  } else {
    ImGui::TextDisabled("Room header unavailable");
  }

  workbench::DrawInspectorSectionHeader(ICON_MD_BUILD " Editing Status");
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
    workbench::DrawInspectorSectionHeader(ICON_MD_INFO " Selection");
    ImGui::TextDisabled("Click an object or entity to inspect");
    return;
  }

  // ── Tile Object Selection ──
  if (obj_count > 0) {
    workbench::DrawInspectorSectionHeader(ICON_MD_WIDGETS " Object Selection");
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
      workbench::DrawInspectorSectionHeader(ICON_MD_SUMMARIZE
                                            " Multi-selection");
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

      workbench::DrawInspectorSectionHeader(ICON_MD_OPEN_WITH " Bulk Edit");

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
        workbench::DrawInspectorSectionHeader(ICON_MD_CATEGORY
                                              " Focused Object");
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

          // ZScream names this as Layer 1/2/3; the tooltip keeps the ROM stream
          // mapping visible without making the control harder to scan.
          gui::LayoutHelpers::PropertyRow("Layer", [&]() {
            int layer = static_cast<int>(obj.GetLayerValue());
            const char* layer_names[] = {"Layer 1 (Primary)",
                                         "Layer 2 (BG2 overlay)",
                                         "Layer 3 (BG1 overlay)"};
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##SelObjLayer", &layer, layer_names,
                             IM_ARRAYSIZE(layer_names))) {
              layer = std::clamp(layer, 0, 2);
              interaction.SetObjectLayer(
                  idx, static_cast<zelda3::RoomObject::LayerType>(layer));
            }
          });
          gui::LayoutHelpers::PropertyRow("Z/Route", [&]() {
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
    workbench::DrawInspectorSectionHeader(ICON_MD_SELECT_ALL
                                          " Entity Selection");

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

void DungeonWorkbenchContent::DrawInspectorShelfTools(
    DungeonCanvasViewer& viewer) {
  (void)viewer;
  workbench::DrawInspectorSectionHeader(ICON_MD_EDIT_NOTE " Edit");
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (!ImGui::BeginTable("##WorkbenchToolsGrid", 2, kFlags)) {
    return;
  }

  auto draw_tool_button = [&](const char* label, WorkbenchTool tool,
                              bool enabled = true) {
    if (!enabled) {
      ImGui::BeginDisabled();
    }
    const bool active =
        inspector_mode_ == InspectorMode::Tools && active_tool_ == tool;
    if (gui::ToggleButton(label, active, ImVec2(-1, 0)) && enabled) {
      OpenTool(tool);
    }
    if (!enabled) {
      ImGui::EndDisabled();
    }
  };

  // Edit row holds entity tools only. Mode switches (Selection / Room Details)
  // moved out — the inspector primary segmented selector at the inspector
  // header is the canonical mode switch.
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  draw_tool_button(ICON_MD_CATEGORY " Selector", WorkbenchTool::ObjectSelector,
                   object_selector_content_ != nullptr);
  ImGui::TableNextColumn();
  draw_tool_button(ICON_MD_DOOR_FRONT " Doors", WorkbenchTool::DoorEditor,
                   door_editor_content_ != nullptr);

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  draw_tool_button(ICON_MD_PERSON " Sprites", WorkbenchTool::SpriteEditor,
                   sprite_editor_content_ != nullptr);
  ImGui::TableNextColumn();
  draw_tool_button(ICON_MD_INVENTORY " Items", WorkbenchTool::ItemEditor,
                   item_editor_content_ != nullptr);

  ImGui::EndTable();

  workbench::DrawInspectorSectionHeader(ICON_MD_BUILD " Room Tools");
  if (ImGui::BeginTable("##WorkbenchRoomToolsGrid", 2, kFlags)) {
    auto room_tool_button = [&](const char* label, WorkbenchTool tool,
                                bool enabled) {
      ImGui::TableNextColumn();
      if (!enabled) {
        ImGui::BeginDisabled();
      }
      const bool active =
          inspector_mode_ == InspectorMode::Tools && active_tool_ == tool;
      if (gui::ToggleButton(label, active, ImVec2(-1, 0)) && enabled) {
        OpenTool(tool);
      }
      if (!enabled) {
        ImGui::EndDisabled();
      }
    };

    ImGui::TableNextRow();
    room_tool_button(ICON_MD_LABEL " Room Tags", WorkbenchTool::RoomTags,
                     room_tag_panel_ != nullptr);
    room_tool_button(ICON_MD_GRID_ON " Collision",
                     WorkbenchTool::CustomCollision,
                     custom_collision_panel_ != nullptr);
    ImGui::TableNextRow();
    room_tool_button(ICON_MD_WATER_DROP " Water Fill", WorkbenchTool::WaterFill,
                     water_fill_panel_ != nullptr);
    room_tool_button(ICON_MD_TRAIN " Minecart", WorkbenchTool::MinecartTracks,
                     minecart_track_panel_ != nullptr);
    ImGui::EndTable();
  }

  workbench::DrawInspectorSectionHeader(ICON_MD_TRAVEL_EXPLORE " Review");
  if (ImGui::BeginTable("##WorkbenchReviewGrid", 2, kFlags)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (workbench::DrawActionButton(ICON_MD_GRID_VIEW " Matrix",
                                    ImVec2(-1, 0))) {
      ShowConnectedGraph();
    }
    ImGui::TableNextColumn();
    if (workbench::DrawActionButton(ICON_MD_MAP " Dungeon Map",
                                    ImVec2(-1, 0))) {
      RequestDungeonMapPopup();
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (workbench::DrawActionButton(ICON_MD_DOOR_FRONT " Entrances",
                                    ImVec2(-1, 0))) {
      FocusEntranceBrowser();
    }
    ImGui::TableNextColumn();
    draw_tool_button(ICON_MD_PALETTE " Palette", WorkbenchTool::Palette,
                     palette_editor_content_ != nullptr);
    ImGui::EndTable();
  }

  workbench::DrawInspectorSectionHeader(ICON_MD_KEYBOARD " Reference");
  if (workbench::DrawActionButton(ICON_MD_KEYBOARD " Keyboard Shortcuts",
                                  ImVec2(-1, 0))) {
    show_shortcut_legend_ = true;
  }
  ShortcutLegendPanel::Draw(&show_shortcut_legend_);
}

}  // namespace yaze::editor
