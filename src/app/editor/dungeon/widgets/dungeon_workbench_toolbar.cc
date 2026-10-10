#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_overlay_controls.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/ui/workbench/dungeon_workbench_chrome.h"
#include "app/editor/dungeon/widgets/dungeon_room_nav_widget.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/canvas/item_context_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

constexpr float kEntranceWidth = 220.0f;
constexpr int kDungeonRoomCount = 0x128;
constexpr float kMaxComparePickerWidth = 420.0f;
constexpr float kCompareSearchListHeight = 220.0f;
// Within-cluster gap: tighter for buttons that share a purpose (pane toggles,
// room write actions, right-cluster trio).
constexpr float kToolbarActionGap = 6.0f;
// Between-cluster gap: clear breathing room at conceptual boundaries
// (toggles → nav, room actions → canvas modes, canvas modes → compare).
constexpr float kToolbarClusterGap = 12.0f;

constexpr char kToolbarPopupIdViewOptions[] = "##WorkbenchViewOptions";
constexpr char kToolbarPopupIdSaveContent[] = "##WorkbenchSaveContent";
constexpr char kToolbarPopupIdCompareSearchList[] = "##CompareSearchList";
constexpr char kToolbarPopupIdCompareMenu[] = "##WorkbenchCompareMenu";
constexpr char kToolbarPopupIdRecentRooms[] = "##WorkbenchRecentRooms";
constexpr char kToolbarPopupIdOverflow[] = "##WorkbenchToolbarOverflow";
constexpr char kToolbarStartCompareLabel[] = "Compare";
constexpr char kToolbarRecentRoomsLabel[] = ICON_MD_HISTORY;
constexpr char kToolbarViewOptionsLabel[] = "Display";
constexpr char kToolbarSaveContentLabel[] = "Save content";
constexpr char kToolbarModeRoomLabel[] = "Room";
constexpr char kToolbarModeConnectedLabel[] = "Overview";
constexpr char kToolbarOverflowLabel[] = ICON_MD_MORE_HORIZ;
constexpr char kToolbarRoomSearchHint[] = "Type to filter rooms...";
constexpr char kToolbarPanelWorkflowTooltip[] =
    "Switch to standalone panel workflow";
constexpr char kToolbarNoRecentRoomsTooltip[] =
    "Visit another room to build room history.";
struct ToolbarLayout {
  float width = 0.0f;
  float spacing = 0.0f;
  float button_size = 0.0f;
};

struct CompareDefaultResult {
  bool found = false;
  int room_id = -1;
};

struct ToolbarVisibility {
  bool layers = false;
  bool entrance = false;
  bool pane_toggles = true;
  bool recent_rooms = true;
  bool stitched_rooms = true;
  bool compare = false;
  bool apply_room = false;
  bool dungeon_map = false;
  bool view_options = false;
  bool tools = false;
  bool save_content = false;
  bool panel_mode = false;
  bool overflow = false;
};

ToolbarLayout ResolveToolbarLayout(float toolbar_width) {
  ToolbarLayout layout;
  layout.width = toolbar_width;
  layout.spacing = ImGui::GetStyle().ItemSpacing.x;
  layout.button_size =
      std::max(gui::UIConfig::kIconButtonSmall,
               gui::LayoutHelpers::GetStandardWidgetHeight() + 1.0f);
  return layout;
}

bool DrawToolbarActionButton(const char* id, const char* label,
                             const ImVec2& size, const char* tooltip,
                             bool active = false);

CompareDefaultResult PickDefaultCompareRoom(
    int current_room, int previous_room,
    const std::function<const std::deque<int>&()>& get_recent_rooms) {
  if (previous_room >= 0 && previous_room != current_room) {
    return {true, previous_room};
  }
  if (get_recent_rooms) {
    const auto& mru = get_recent_rooms();
    for (int rid : mru) {
      if (rid != current_room) {
        return {true, rid};
      }
    }
  }
  return {};
}

bool IconToggleButton(const char* id, const char* icon_on, const char* icon_off,
                      bool* value, float btn_size, const char* tooltip_on,
                      const char* tooltip_off) {
  if (!value) {
    return false;
  }

  const float btn = btn_size;
  const float btn_w =
      workbench::CalcIconToggleButtonWidth(icon_on, icon_off, btn);
  const bool active = *value;

  const ImVec4 col_btn = ImGui::GetStyleColorVec4(ImGuiCol_Button);
  const ImVec4 col_active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

  ImGui::PushID(id);
  gui::StyleColorGuard btn_guard(ImGuiCol_Button,
                                 active ? col_active : col_btn);
  const bool pressed =
      ImGui::Button(active ? icon_on : icon_off, ImVec2(btn_w, btn));

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", active ? tooltip_on : tooltip_off);
  }
  if (pressed) {
    *value = !*value;
  }
  ImGui::PopID();
  return pressed;
}

void DrawViewOptionsContents(const DungeonWorkbenchToolbarParams& p) {
  if (!p.primary_viewer) {
    return;
  }

  DrawDungeonOverlayControls(*p.primary_viewer);
}

void DrawViewOptionsButton(const DungeonWorkbenchToolbarParams& p,
                           const ToolbarLayout& layout) {
  if (!p.primary_viewer) {
    return;
  }

  const float button_width = workbench::CalcIconButtonWidth(
      kToolbarViewOptionsLabel, layout.button_size);
  if (DrawToolbarActionButton("ViewOptionsButton", kToolbarViewOptionsLabel,
                              ImVec2(button_width, layout.button_size),
                              "Canvas view options")) {
    ImGui::OpenPopup(kToolbarPopupIdViewOptions);
  }

  if (ImGui::BeginPopup(kToolbarPopupIdViewOptions)) {
    DrawViewOptionsContents(p);
    ImGui::EndPopup();
  }
}

void DrawSaveContentButton(const DungeonWorkbenchToolbarParams& p,
                           const ToolbarLayout& layout) {
  const float button_width = workbench::CalcIconButtonWidth(
      kToolbarSaveContentLabel, layout.button_size);
  if (DrawToolbarActionButton("SaveContentButton", kToolbarSaveContentLabel,
                              ImVec2(button_width, layout.button_size),
                              "Choose which edited room content is saved")) {
    ImGui::OpenPopup(kToolbarPopupIdSaveContent);
  }
  if (ImGui::BeginPopup(kToolbarPopupIdSaveContent)) {
    p.draw_save_content();
    ImGui::EndPopup();
  }
}

float CanvasModeSelectorWidth(const ToolbarLayout& layout) {
  return workbench::CalcIconButtonWidth(kToolbarModeRoomLabel,
                                        layout.button_size) +
         kToolbarActionGap +
         workbench::CalcIconButtonWidth(kToolbarModeConnectedLabel,
                                        layout.button_size);
}

void DrawCanvasModeSelector(const DungeonWorkbenchToolbarParams& p,
                            const ToolbarLayout& toolbar_layout) {
  const float height = toolbar_layout.button_size;
  if (DrawToolbarActionButton(
          "CanvasModeRoom", kToolbarModeRoomLabel,
          ImVec2(workbench::CalcIconButtonWidth(kToolbarModeRoomLabel, height),
                 height),
          "Edit the current room",
          !p.layout->show_connected_canvas_view && !*p.split_view_enabled)) {
    p.layout->show_connected_canvas_view = false;
    *p.split_view_enabled = false;
  }
  ImGui::SameLine(0.0f, kToolbarActionGap);
  if (DrawToolbarActionButton(
          "CanvasModeConnected", kToolbarModeConnectedLabel,
          ImVec2(workbench::CalcIconButtonWidth(kToolbarModeConnectedLabel,
                                                height),
                 height),
          "Dungeon overview: click a room preview to edit it",
          p.layout->show_connected_canvas_view)) {
    p.layout->show_connected_canvas_view = true;
    *p.split_view_enabled = false;
  }
}

bool DrawToolbarActionButton(const char* id, const char* label,
                             const ImVec2& size, const char* tooltip,
                             bool active) {
  ImGui::PushID(id);
  const bool pressed = gui::ToggleButton(label, active, size);
  {
    gui::AutoWidgetScope scope("Dungeon/WorkbenchToolbar");
    gui::AutoRegisterLastItem("button", id, tooltip ? tooltip : label);
  }
  ImGui::PopID();
  if (tooltip && *tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return pressed;
}

// One searchable room list is used by the main action and narrow overflow.
// Selecting a room both starts comparison and chooses its target.
void DrawCompareContents(const DungeonWorkbenchToolbarParams& p) {
  ImGui::TextUnformatted(tr("Compare with room"));
  uint16_t room =
      static_cast<uint16_t>(std::clamp(*p.compare_room_id, 0, 0x127));
  if (auto result = gui::InputHexWordEx("Room ID", &room, 90.0f, true);
      result.ShouldApply() && room < kDungeonRoomCount &&
      room != *p.current_room_id) {
    *p.compare_room_id = room;
    *p.split_view_enabled = true;
    p.layout->show_connected_canvas_view = false;
  }
  if (*p.split_view_enabled) {
    ImGui::SameLine();
    if (ImGui::Button(tr("End compare"))) {
      *p.split_view_enabled = false;
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::Button(tr("Swap rooms"))) {
      const int target = *p.compare_room_id;
      *p.compare_room_id = *p.current_room_id;
      if (p.on_room_selected)
        p.on_room_selected(target);
      else
        *p.current_room_id = target;
    }
    ImGui::SameLine();
    ImGui::Checkbox(tr("Sync view"), &p.layout->sync_split_view);
  }
  const bool search_available =
      p.compare_search_buf && p.compare_search_buf_size > 1;
  ImGui::SetNextItemWidth(-1);
  if (search_available)
    ImGui::InputTextWithHint("##CompareSearch", kToolbarRoomSearchHint,
                             p.compare_search_buf, p.compare_search_buf_size);
  std::string filter = search_available ? p.compare_search_buf : "";
  std::transform(filter.begin(), filter.end(), filter.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  ImGui::BeginChild(kToolbarPopupIdCompareSearchList,
                    ImVec2(0, kCompareSearchListHeight), false);
  for (int id = 0; id < kDungeonRoomCount; ++id) {
    if (id == *p.current_room_id)
      continue;
    const std::string label = absl::StrFormat(
        "[%03X] %s", id,
        dungeon_project_labels::GetRoomLabel(
            p.primary_viewer ? p.primary_viewer->project() : nullptr, id));
    std::string lower = label;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower.find(filter) == std::string::npos)
      continue;
    ImGui::PushID(id);
    if (ImGui::Selectable(label.c_str(),
                          *p.split_view_enabled && *p.compare_room_id == id)) {
      *p.compare_room_id = id;
      *p.split_view_enabled = true;
      p.layout->show_connected_canvas_view = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void DrawComparePopup(const DungeonWorkbenchToolbarParams& p) {
  ImGui::SetNextWindowSize(ImVec2(kMaxComparePickerWidth, 0),
                           ImGuiCond_Appearing);
  if (ImGui::BeginPopup(kToolbarPopupIdCompareMenu)) {
    DrawCompareContents(p);
    ImGui::EndPopup();
  }
}

void DrawCompareMenu(const DungeonWorkbenchToolbarParams& p,
                     const ToolbarLayout& layout) {
  if (DrawToolbarActionButton(
          "CompareMenuButton", kToolbarStartCompareLabel,
          ImVec2(workbench::CalcIconButtonWidth(kToolbarStartCompareLabel,
                                                layout.button_size),
                 layout.button_size),
          *p.split_view_enabled ? "Choose comparison room or end comparison"
                                : "Compare rooms",
          *p.split_view_enabled)) {
    const auto previous = PickDefaultCompareRoom(
        *p.current_room_id, p.previous_room_id ? *p.previous_room_id : -1,
        p.get_recent_rooms);
    if (!*p.split_view_enabled && previous.found) {
      *p.compare_room_id = previous.room_id;
      *p.split_view_enabled = true;
      p.layout->show_connected_canvas_view = false;
    } else {
      ImGui::OpenPopup(kToolbarPopupIdCompareMenu);
    }
  }
  DrawComparePopup(p);
}

void DrawRecentRoomsContents(const DungeonWorkbenchToolbarParams& p,
                             bool show_heading) {
  if (show_heading) {
    ImGui::TextDisabled(tr("Recent Rooms"));
    ImGui::Separator();
  }
  DungeonRoomStore* rooms =
      p.primary_viewer ? p.primary_viewer->rooms() : nullptr;
  const project::YazeProject* project =
      p.primary_viewer ? p.primary_viewer->project() : nullptr;
  std::vector<int> recent_ids;
  if (p.get_recent_rooms) {
    const auto& recent = p.get_recent_rooms();
    recent_ids.assign(recent.begin(), recent.end());
  }
  std::vector<int> to_forget;

  for (int room_id : recent_ids) {
    ImGui::PushID(room_id);
    const bool is_current = p.current_room_id && room_id == *p.current_room_id;
    const bool room_dirty =
        rooms != nullptr && rooms->GetIfMaterialized(room_id) != nullptr &&
        rooms->GetIfMaterialized(room_id)->HasUnsavedChanges();
    const auto room_name =
        dungeon_project_labels::GetRoomLabel(project, room_id);
    char item_label[160];
    snprintf(item_label, sizeof(item_label), "[%03X]%s %s##RecentRoom", room_id,
             room_dirty ? "*" : "", room_name.c_str());

    if (ImGui::Selectable(item_label, is_current) && !is_current) {
      if (p.on_room_selected) {
        p.on_room_selected(room_id);
      } else if (p.current_room_id) {
        *p.current_room_id = room_id;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click to open; right-click for actions%s",
                        room_dirty ? "\nPending room changes" : "");
    }

    gui::ItemContextMenu("##RecentRoomActions", [&, room_id]() {
      std::vector<gui::MenuItemSpec> items;
      if (p.on_open_room_panel) {
        items.emplace_back("Open as Panel", ICON_MD_OPEN_IN_NEW,
                           [&p, room_id]() { p.on_open_room_panel(room_id); });
      }
      items.emplace_back("Compare", ICON_MD_COMPARE_ARROWS, [&p, room_id]() {
        if (p.layout) {
          p.layout->show_connected_canvas_view = false;
        }
        if (p.split_view_enabled) {
          *p.split_view_enabled = true;
        }
        if (p.compare_room_id) {
          *p.compare_room_id = room_id;
        }
      });
      items.back().separator_after = true;
      items.push_back(gui::CopyToClipboardItem(
          "Copy Room ID", absl::StrFormat("0x%03X", room_id)));
      if (p.forget_recent_room) {
        items.back().separator_after = true;
        items.emplace_back(
            "Remove from Recent", ICON_MD_CLOSE,
            [&to_forget, room_id]() { to_forget.push_back(room_id); });
      }
      return items;
    });
    ImGui::PopID();
  }

  for (int room_id : to_forget) {
    p.forget_recent_room(room_id);
  }
}

void DrawRecentRoomsMenu(const DungeonWorkbenchToolbarParams& p,
                         const ToolbarLayout& layout) {
  const bool has_history = p.get_recent_rooms && !p.get_recent_rooms().empty();
  const float button_width = workbench::CalcIconButtonWidth(
      kToolbarRecentRoomsLabel, layout.button_size);

  if (!has_history) {
    ImGui::BeginDisabled();
  }
  const bool open_history = DrawToolbarActionButton(
      "RecentRoomsButton", kToolbarRecentRoomsLabel,
      ImVec2(button_width, layout.button_size),
      has_history ? "Recent rooms" : kToolbarNoRecentRoomsTooltip);
  if (!has_history) {
    ImGui::EndDisabled();
  }
  if (open_history) {
    ImGui::OpenPopup(kToolbarPopupIdRecentRooms);
  }

  if (!ImGui::BeginPopup(kToolbarPopupIdRecentRooms)) {
    return;
  }
  DrawRecentRoomsContents(p, /*show_heading=*/true);
  ImGui::EndPopup();
}

float ToolbarRightActionsWidth(const DungeonWorkbenchToolbarParams& p,
                               const ToolbarLayout& layout,
                               const ToolbarVisibility& visibility) {
  float width = 0.0f;
  auto append = [&](float item_width) {
    if (width > 0.0f) {
      width += kToolbarActionGap;
    }
    width += item_width;
  };

  if (visibility.view_options && p.primary_viewer) {
    append(workbench::CalcIconButtonWidth(kToolbarViewOptionsLabel,
                                          layout.button_size));
  }
  if (visibility.tools) {
    append(workbench::CalcIconButtonWidth("Tools", layout.button_size));
  }
  if (visibility.save_content) {
    append(workbench::CalcIconButtonWidth(kToolbarSaveContentLabel,
                                          layout.button_size));
  }
  if (visibility.panel_mode && p.set_workflow_mode) {
    append(
        workbench::CalcIconButtonWidth(ICON_MD_VIEW_QUILT, layout.button_size));
  }
  if (visibility.overflow) {
    append(workbench::CalcIconButtonWidth(kToolbarOverflowLabel,
                                          layout.button_size));
  }
  return width;
}

float ToolbarEstimatedWidth(const DungeonWorkbenchToolbarParams& p,
                            const ToolbarLayout& layout,
                            const ToolbarVisibility& visibility) {
  float left_width = 0.0f;
  auto append_left = [&](float item_width, float gap) {
    if (left_width > 0.0f) {
      left_width += gap;
    }
    left_width += item_width;
  };

  if (visibility.pane_toggles) {
    append_left(workbench::CalcIconToggleButtonWidth(ICON_MD_LIST, ICON_MD_LIST,
                                                     layout.button_size),
                0.0f);
    append_left(workbench::CalcIconToggleButtonWidth(ICON_MD_TUNE, ICON_MD_TUNE,
                                                     layout.button_size),
                kToolbarActionGap);
  }

  const float navigation_width =
      4.0f * ImGui::GetFrameHeight() + 4.0f * ImGui::GetStyle().ItemSpacing.x +
      ImGui::CalcTextSize("00000").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  append_left(navigation_width, kToolbarClusterGap);
  if (visibility.stitched_rooms) {
    append_left(CanvasModeSelectorWidth(layout), kToolbarClusterGap);
  }

  if (visibility.entrance)
    append_left(kEntranceWidth, kToolbarClusterGap);
  if (visibility.layers)
    append_left(DungeonLayerControlsWidth(), kToolbarClusterGap);

  if (visibility.recent_rooms) {
    append_left(workbench::CalcIconButtonWidth(kToolbarRecentRoomsLabel,
                                               layout.button_size),
                kToolbarActionGap);
  }
  if (visibility.apply_room) {
    append_left(
        workbench::CalcIconButtonWidth(ICON_MD_SAVE, layout.button_size),
        kToolbarActionGap);
  }
  if (visibility.dungeon_map) {
    append_left(workbench::CalcIconButtonWidth(ICON_MD_MAP, layout.button_size),
                kToolbarActionGap);
  }
  if (visibility.compare) {
    append_left(workbench::CalcIconButtonWidth(kToolbarStartCompareLabel,
                                               layout.button_size),
                kToolbarClusterGap);
  }

  const float right_width = ToolbarRightActionsWidth(p, layout, visibility);
  return left_width +
         (right_width > 0.0f ? kToolbarActionGap + right_width : 0.0f);
}

ToolbarVisibility ResolveToolbarVisibility(
    const DungeonWorkbenchToolbarParams& p, const ToolbarLayout& layout) {
  ToolbarVisibility visibility;
  visibility.layers = p.primary_viewer != nullptr;
  visibility.entrance = p.draw_entrance_context != nullptr;
  visibility.compare = true;
  visibility.apply_room =
      p.on_save_room && p.current_room_id && *p.current_room_id >= 0;
  visibility.dungeon_map = p.on_request_dungeon_map != nullptr;
  visibility.view_options = p.primary_viewer != nullptr;
  visibility.tools = p.draw_tools != nullptr;
  visibility.save_content = p.draw_save_content != nullptr;
  visibility.panel_mode = p.set_workflow_mode != nullptr;

  constexpr float kSafetyMargin = 4.0f;
  auto fits = [&]() {
    return ToolbarEstimatedWidth(p, layout, visibility) + kSafetyMargin <=
           layout.width;
  };
  if (fits()) {
    return visibility;
  }

  auto collapse = [&](bool& item) {
    if (!item) {
      return false;
    }
    item = false;
    visibility.overflow = true;
    return fits();
  };

  // Keep the explicit view choice beside room navigation at laptop widths.
  // Entrance/layer controls remain available in the overflow on narrow panes.
  if (collapse(visibility.dungeon_map))
    return visibility;
  if (collapse(visibility.save_content))
    return visibility;
  if (collapse(visibility.apply_room))
    return visibility;
  if (collapse(visibility.panel_mode))
    return visibility;
  if (collapse(visibility.recent_rooms))
    return visibility;
  if (collapse(visibility.compare))
    return visibility;
  if (collapse(visibility.view_options))
    return visibility;
  if (collapse(visibility.entrance))
    return visibility;
  if (collapse(visibility.layers))
    return visibility;
  if (collapse(visibility.tools))
    return visibility;
  if (collapse(visibility.pane_toggles))
    return visibility;
  collapse(visibility.stitched_rooms);
  return visibility;
}

bool DrawToolbarOverflowMenu(const DungeonWorkbenchToolbarParams& p,
                             const ToolbarLayout& layout,
                             const ToolbarVisibility& visibility) {
  const float button_width =
      workbench::CalcIconButtonWidth(kToolbarOverflowLabel, layout.button_size);
  if (DrawToolbarActionButton("ToolbarOverflow", kToolbarOverflowLabel,
                              ImVec2(button_width, layout.button_size),
                              "More dungeon actions")) {
    ImGui::OpenPopup(kToolbarPopupIdOverflow);
  }

  bool request_panel_mode = false;
  if (!ImGui::BeginPopup(kToolbarPopupIdOverflow)) {
    return request_panel_mode;
  }

  if (!visibility.entrance && p.draw_entrance_context) {
    p.draw_entrance_context(kEntranceWidth);
    ImGui::Separator();
  }
  if (!visibility.layers && p.primary_viewer) {
    DrawDungeonLayerControls(*p.primary_viewer, *p.current_room_id,
                             layout.button_size);
    ImGui::Separator();
  }

  if (!visibility.pane_toggles) {
    if (ImGui::MenuItem(ICON_MD_LIST " Room Browser", nullptr,
                        p.layout->show_left_sidebar)) {
      p.layout->show_left_sidebar = !p.layout->show_left_sidebar;
    }
    if (ImGui::MenuItem(ICON_MD_TUNE " Inspector", nullptr,
                        p.layout->show_right_inspector)) {
      p.layout->show_right_inspector = !p.layout->show_right_inspector;
    }
    ImGui::Separator();
  }

  if (!visibility.recent_rooms) {
    const bool has_history =
        p.get_recent_rooms && !p.get_recent_rooms().empty();
    if (ImGui::BeginMenu(ICON_MD_HISTORY " Recent Rooms", has_history)) {
      DrawRecentRoomsContents(p, /*show_heading=*/false);
      ImGui::EndMenu();
    }
    if (!has_history &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("%s", kToolbarNoRecentRoomsTooltip);
    }
  }
  if (!visibility.stitched_rooms) {
    if (ImGui::MenuItem(
            "Room view", nullptr,
            !p.layout->show_connected_canvas_view && !*p.split_view_enabled)) {
      p.layout->show_connected_canvas_view = false;
      *p.split_view_enabled = false;
    }
    if (ImGui::MenuItem("Dungeon overview", nullptr,
                        p.layout->show_connected_canvas_view)) {
      p.layout->show_connected_canvas_view = true;
      *p.split_view_enabled = false;
    }
  }
  if (!visibility.compare) {
    DrawCompareContents(p);
  }

  const bool has_hidden_room_action =
      (!visibility.apply_room && p.on_save_room && p.current_room_id &&
       *p.current_room_id >= 0) ||
      (!visibility.dungeon_map && p.on_request_dungeon_map);
  if (has_hidden_room_action) {
    ImGui::Separator();
  }
  if (!visibility.apply_room && p.on_save_room && p.current_room_id &&
      *p.current_room_id >= 0 && ImGui::MenuItem(ICON_MD_SAVE " Apply Room")) {
    p.on_save_room(*p.current_room_id);
  }
  if (!visibility.dungeon_map && p.on_request_dungeon_map &&
      ImGui::MenuItem(ICON_MD_MAP " Dungeon Map")) {
    p.on_request_dungeon_map();
  }

  if (!visibility.view_options && p.primary_viewer) {
    ImGui::Separator();
    if (ImGui::BeginMenu("Display")) {
      DrawViewOptionsContents(p);
      ImGui::EndMenu();
    }
  }
  if (!visibility.tools && p.draw_tools) {
    p.draw_tools();
  }
  if (!visibility.save_content && p.draw_save_content &&
      ImGui::BeginMenu(kToolbarSaveContentLabel)) {
    p.draw_save_content();
    ImGui::EndMenu();
  }
  if (!visibility.panel_mode && p.set_workflow_mode) {
    if (ImGui::MenuItem(ICON_MD_VIEW_QUILT " Window Workflow")) {
      request_panel_mode = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", kToolbarPanelWorkflowTooltip);
    }
  }

  ImGui::EndPopup();
  return request_panel_mode;
}

}  // namespace

bool DungeonWorkbenchToolbar::ShouldShowInlineRoomNav(float toolbar_width) {
  (void)toolbar_width;
  return true;
}

bool DungeonWorkbenchToolbar::Draw(const DungeonWorkbenchToolbarParams& p) {
  if (!p.layout || !p.current_room_id || !p.split_view_enabled ||
      !p.compare_room_id) {
    ImGui::TextDisabled(tr("Workbench toolbar not wired"));
    return false;
  }

  const ToolbarLayout layout =
      ResolveToolbarLayout(std::max(ImGui::GetContentRegionAvail().x, 1.0f));
  bool request_panel_mode = false;

  // Gentle compaction only; kToolbarActionGap/ClusterGap own inter-button
  // spacing, so don't shrink ItemSpacing.x below the theme default here.
  const ImVec2 frame_pad = ImGui::GetStyle().FramePadding;
  gui::StyleVarGuard frame_pad_guard(
      ImGuiStyleVar_FramePadding, ImVec2(std::max(4.0f, frame_pad.x - 1.0f),
                                         std::max(3.0f, frame_pad.y - 1.0f)));
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(layout.spacing, 4.0f),
             std::max(3.0f, ImGui::GetStyle().ItemSpacing.y - 1.0f)));
  const ToolbarVisibility visibility = ResolveToolbarVisibility(p, layout);
  const float right_actions_width =
      ToolbarRightActionsWidth(p, layout, visibility);

  // Connected toolbar controls: render as a floating canvas overlay (the
  // viewer's built-in fallback) instead of inline. Inline rendering races
  // against the right cluster because the controls are variable-width and
  // only present when stitched mode is active. Overlay mode anchors them to
  // the canvas viewport corner where they belong.
  if (p.primary_viewer) {
    p.primary_viewer->SetConnectedControlsInline(false);
  }

  // ── Render row ───────────────────────────────────────────────────
  if (visibility.pane_toggles) {
    (void)IconToggleButton("RoomsToggle", ICON_MD_LIST, ICON_MD_LIST,
                           &p.layout->show_left_sidebar, layout.button_size,
                           "Hide room browser", "Show room browser");
    ImGui::SameLine(0.0f, kToolbarActionGap);
    (void)IconToggleButton("InspectorToggle", ICON_MD_TUNE, ICON_MD_TUNE,
                           &p.layout->show_right_inspector, layout.button_size,
                           "Hide inspector", "Show inspector");
    ImGui::SameLine(0.0f, kToolbarClusterGap);
  }
  DungeonRoomNavWidget::Draw("WorkbenchNav", *p.current_room_id,
                             p.on_room_selected);
  if (visibility.stitched_rooms) {
    ImGui::SameLine(0.0f, kToolbarClusterGap);
    DrawCanvasModeSelector(p, layout);
  }

  if (visibility.entrance) {
    ImGui::SameLine(0.0f, kToolbarClusterGap);
    p.draw_entrance_context(kEntranceWidth);
  }
  if (visibility.layers) {
    ImGui::SameLine(0.0f, kToolbarClusterGap);
    DrawDungeonLayerControls(*p.primary_viewer, *p.current_room_id,
                             layout.button_size);
  }

  if (visibility.recent_rooms) {
    ImGui::SameLine(0.0f, kToolbarActionGap);
    DrawRecentRoomsMenu(p, layout);
  }

  if (visibility.apply_room) {
    ImGui::SameLine(0.0f, kToolbarActionGap);
    if (workbench::DrawHeaderIconAction(
            "ApplyRoomToolbar", ICON_MD_SAVE, layout.button_size,
            "Apply this room into the loaded ROM buffer "
            "(File > Save ROM persists to disk)")) {
      p.on_save_room(*p.current_room_id);
    }
  }
  if (visibility.dungeon_map) {
    ImGui::SameLine(0.0f, kToolbarActionGap);
    if (workbench::DrawHeaderIconAction("DungeonMapToolbar", ICON_MD_MAP,
                                        layout.button_size,
                                        "Open the Dungeon Map popup")) {
      p.on_request_dungeon_map();
    }
  }

  if (visibility.compare) {
    ImGui::SameLine(0.0f, kToolbarClusterGap);
    DrawCompareMenu(p, layout);
  }

  if (right_actions_width > 0.0f) {
    const float last_left_edge =
        ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;
    const float right_start =
        std::max(last_left_edge + kToolbarActionGap,
                 ImGui::GetWindowContentRegionMax().x - right_actions_width);
    ImGui::SameLine(right_start, 0.0f);

    bool drew_right_action = false;
    if (visibility.view_options) {
      DrawViewOptionsButton(p, layout);
      drew_right_action = true;
    }
    if (visibility.tools) {
      if (drew_right_action) {
        ImGui::SameLine(0.0f, kToolbarActionGap);
      }
      p.draw_tools();
      drew_right_action = true;
    }
    if (visibility.save_content) {
      if (drew_right_action) {
        ImGui::SameLine(0.0f, kToolbarActionGap);
      }
      DrawSaveContentButton(p, layout);
      drew_right_action = true;
    }
    if (visibility.panel_mode) {
      if (drew_right_action) {
        ImGui::SameLine(0.0f, kToolbarActionGap);
      }
      if (workbench::DrawHeaderIconAction("PanelMode", ICON_MD_VIEW_QUILT,
                                          layout.button_size,
                                          kToolbarPanelWorkflowTooltip)) {
        request_panel_mode = true;
      }
      drew_right_action = true;
    }
    if (visibility.overflow) {
      if (drew_right_action) {
        ImGui::SameLine(0.0f, kToolbarActionGap);
      }
      request_panel_mode |= DrawToolbarOverflowMenu(p, layout, visibility);
    }
  }
  return request_panel_mode;
}

}  // namespace yaze::editor
