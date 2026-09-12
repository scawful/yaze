#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/ui/workbench/dungeon_workbench_chrome.h"
#include "app/editor/dungeon/widgets/dungeon_room_nav_widget.h"
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

constexpr float kInlineCompareRoomIdToolbarWidth = 980.0f;
constexpr float kCompactCompareButtonWidth = 34.0f;
constexpr int kDungeonRoomCount = 0x128;
constexpr float kDenseToolbarButtonWidth = 84.0f;
constexpr float kMaxComparePickerWidth = 420.0f;
constexpr float kCompareSearchListHeight = 220.0f;
// Within-cluster gap: tighter for buttons that share a purpose (pane toggles,
// room write actions, right-cluster trio).
constexpr float kToolbarActionGap = 6.0f;
// Between-cluster gap: clear breathing room at conceptual boundaries
// (toggles → nav, room actions → canvas modes, canvas modes → compare).
constexpr float kToolbarClusterGap = 12.0f;

constexpr char kToolbarPopupIdViewOptions[] = "##WorkbenchViewOptions";
constexpr char kToolbarPopupIdCompareSearchList[] = "##CompareSearchList";
constexpr char kToolbarPopupIdCompareMenu[] = "##WorkbenchCompareMenu";
constexpr char kToolbarPopupIdRecentRooms[] = "##WorkbenchRecentRooms";
constexpr char kToolbarPopupIdOverflow[] = "##WorkbenchToolbarOverflow";
constexpr char kToolbarStartCompareLabel[] = ICON_MD_COMPARE_ARROWS;
constexpr char kToolbarRecentRoomsLabel[] = ICON_MD_HISTORY;
constexpr char kToolbarViewOptionsLabel[] = ICON_MD_VISIBILITY;
constexpr char kToolbarModeConnectedLabel[] = ICON_MD_VIEW_QUILT;
constexpr char kToolbarOverflowLabel[] = ICON_MD_MORE_HORIZ;
constexpr char kToolbarRoomSearchHint[] = "Type to filter rooms...";
constexpr char kToolbarComparePickerTooltip[] = "Pick a room to compare";
constexpr char kToolbarCompareRoomIdTooltip[] = "Compare room ID";
constexpr char kToolbarPanelWorkflowTooltip[] =
    "Switch to standalone panel workflow (Ctrl+Shift+W)";
constexpr char kToolbarNoCompareHistoryMessage[] =
    "Visit another room to seed compare history.";
constexpr char kToolbarNoCompareHistoryTooltip[] =
    "Visit another room first to seed compare history.";
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
  bool pane_toggles = true;
  bool recent_rooms = true;
  bool stitched_rooms = true;
  bool compare = false;
  bool apply_room = false;
  bool dungeon_map = false;
  bool view_options = false;
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

void DrawViewOptionsContents(DungeonCanvasViewer* viewer) {
  if (!viewer) {
    return;
  }

  // Canvas overlays — generic, common, expected on by default.
  ImGui::TextDisabled(tr("Canvas"));
  bool v = viewer->show_grid();
  if (ImGui::Checkbox(tr("Grid (8x8)"), &v)) {
    viewer->set_show_grid(v);
  }
  v = viewer->show_object_bounds();
  if (ImGui::Checkbox(tr("Object Bounds"), &v)) {
    viewer->set_show_object_bounds(v);
  }
  v = viewer->show_coordinate_overlay();
  if (ImGui::Checkbox(tr("Hover Coordinates"), &v)) {
    viewer->set_show_coordinate_overlay(v);
  }
  v = viewer->show_camera_quadrant_overlay();
  if (ImGui::Checkbox(tr("Camera Quadrants"), &v)) {
    viewer->set_show_camera_quadrant_overlay(v);
  }

  // Authoring overlays — specialized, infrequent. Grouped together so the
  // common four don't get visually drowned by Oracle/track-specific ones.
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextDisabled(tr("Authoring"));
  v = viewer->show_track_collision_overlay();
  if (ImGui::Checkbox(tr("Track Collision"), &v)) {
    viewer->set_show_track_collision_overlay(v);
  }
  v = viewer->show_custom_collision_overlay();
  if (ImGui::Checkbox(tr("Custom Collision"), &v)) {
    viewer->set_show_custom_collision_overlay(v);
  }
  v = viewer->show_water_fill_overlay();
  if (ImGui::Checkbox(tr("Water Fill (Oracle)"), &v)) {
    viewer->set_show_water_fill_overlay(v);
  }
  v = viewer->show_minecart_sprite_overlay();
  if (ImGui::Checkbox(tr("Minecart Pathing"), &v)) {
    viewer->set_show_minecart_sprite_overlay(v);
  }
  v = viewer->show_track_gap_overlay();
  if (ImGui::Checkbox(tr("Track Gaps"), &v)) {
    viewer->set_show_track_gap_overlay(v);
  }
  v = viewer->show_track_route_overlay();
  if (ImGui::Checkbox(tr("Track Routes"), &v)) {
    viewer->set_show_track_route_overlay(v);
  }
  v = viewer->show_custom_objects_overlay();
  if (ImGui::Checkbox(tr("Custom Objects (Oracle)"), &v)) {
    viewer->set_show_custom_objects_overlay(v);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        tr("Highlight custom-draw objects (IDs 0x31/0x32)\n"
           "with a cyan overlay showing position and subtype."));
  }
}

void DrawViewOptionsButton(DungeonCanvasViewer* viewer,
                           const ToolbarLayout& layout) {
  if (!viewer) {
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
    DrawViewOptionsContents(viewer);
    ImGui::EndPopup();
  }
}

void DrawCanvasModeSelector(DungeonWorkbenchLayoutState* layout,
                            const ToolbarLayout& toolbar_layout) {
  if (!layout) {
    return;
  }

  const float mode_height = toolbar_layout.button_size;
  const float connected_width =
      workbench::CalcIconButtonWidth(kToolbarModeConnectedLabel, mode_height);
  if (DrawToolbarActionButton("CanvasModeConnected", kToolbarModeConnectedLabel,
                              ImVec2(connected_width, mode_height),
                              layout->show_connected_canvas_view
                                  ? "Return to single-room canvas"
                                  : "Stitched Rooms: browse the current room "
                                    "and its neighbors as one canvas",
                              layout->show_connected_canvas_view)) {
    layout->show_connected_canvas_view = !layout->show_connected_canvas_view;
  }
}

bool DrawToolbarActionButton(const char* id, const char* label,
                             const ImVec2& size, const char* tooltip,
                             bool active) {
  ImGui::PushID(id);
  const bool pressed = gui::ToggleButton(label, active, size);
  ImGui::PopID();
  if (tooltip && *tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return pressed;
}

void DrawComparePicker(
    int current_room_id, int* compare_room_id,
    const std::function<const std::deque<int>&()>& get_recent_rooms,
    char* search_buf, size_t search_buf_size,
    const project::YazeProject* project) {
  if (!compare_room_id || *compare_room_id < 0) {
    return;
  }
  const bool can_search = search_buf != nullptr && search_buf_size > 1;
  const char* filter = can_search ? search_buf : "";

  char preview[128];
  const auto label =
      dungeon_project_labels::GetRoomLabel(project, *compare_room_id);
  snprintf(preview, sizeof(preview), "[%03X] %s", *compare_room_id,
           label.c_str());

  auto to_lower = [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  };
  auto icontains = [&](const std::string& haystack,
                       const char* needle) -> bool {
    if (!needle || *needle == '\0') {
      return true;
    }
    const size_t nlen = std::strlen(needle);
    for (size_t i = 0; i + nlen <= haystack.size(); ++i) {
      bool match = true;
      for (size_t j = 0; j < nlen; ++j) {
        if (to_lower(static_cast<unsigned char>(haystack[i + j])) !=
            to_lower(static_cast<unsigned char>(needle[j]))) {
          match = false;
          break;
        }
      }
      if (match)
        return true;
    }
    return false;
  };

  // Picker: MRU + searchable full list.
  ImGui::SetNextItemWidth(std::clamp(ImGui::GetContentRegionAvail().x, 120.0f,
                                     kMaxComparePickerWidth));
  if (ImGui::BeginCombo("##CompareRoomPicker", preview,
                        ImGuiComboFlags_HeightLarge)) {
    ImGui::TextDisabled(ICON_MD_HISTORY " Recent");
    if (get_recent_rooms) {
      const auto& mru = get_recent_rooms();
      for (int rid : mru) {
        if (rid == current_room_id) {
          continue;
        }
        char item[128];
        const auto rid_label =
            dungeon_project_labels::GetRoomLabel(project, rid);
        snprintf(item, sizeof(item), "[%03X] %s", rid, rid_label.c_str());
        const bool is_selected = (rid == *compare_room_id);
        if (ImGui::Selectable(item, is_selected)) {
          *compare_room_id = rid;
        }
      }
    }

    ImGui::Separator();
    ImGui::TextDisabled(ICON_MD_SEARCH " Search");
    ImGui::SetNextItemWidth(-1.0f);
    if (can_search) {
      ImGui::InputTextWithHint("##CompareSearch", kToolbarRoomSearchHint,
                               search_buf, search_buf_size);
    } else {
      ImGui::TextDisabled(tr("Search unavailable"));
    }

    ImGui::Spacing();
    std::vector<int> filtered_rooms;
    filtered_rooms.reserve(kDungeonRoomCount);
    for (int rid = 0; rid < kDungeonRoomCount; ++rid) {
      if (rid == current_room_id) {
        continue;
      }
      const auto rid_label = dungeon_project_labels::GetRoomLabel(project, rid);
      char hex_buf[8];
      snprintf(hex_buf, sizeof(hex_buf), "%03X", rid);
      if (!icontains(rid_label, filter) && !icontains(hex_buf, filter)) {
        continue;
      }
      filtered_rooms.push_back(rid);
    }

    ImGui::BeginChild(kToolbarPopupIdCompareSearchList,
                      ImVec2(0, kCompareSearchListHeight), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered_rooms.size()));
    while (clipper.Step()) {
      for (int idx = clipper.DisplayStart; idx < clipper.DisplayEnd; ++idx) {
        const int rid = filtered_rooms[idx];
        const auto rid_label =
            dungeon_project_labels::GetRoomLabel(project, rid);
        char item[128];
        snprintf(item, sizeof(item), "[%03X] %s", rid, rid_label.c_str());
        const bool is_selected = (rid == *compare_room_id);
        if (ImGui::Selectable(item, is_selected)) {
          *compare_room_id = rid;
        }
      }
    }
    ImGui::EndChild();

    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", kToolbarComparePickerTooltip);
  }
}

void DrawActiveCompareRoomControls(const DungeonWorkbenchToolbarParams& p,
                                   bool show_direct_room_id) {
  ImGui::TextDisabled(tr("Compare Room"));
  DrawComparePicker(*p.current_room_id, p.compare_room_id, p.get_recent_rooms,
                    p.compare_search_buf, p.compare_search_buf_size,
                    p.primary_viewer ? p.primary_viewer->project() : nullptr);
  if (!show_direct_room_id) {
    return;
  }

  uint16_t cmp = static_cast<uint16_t>(
      std::clamp(*p.compare_room_id, 0, kDungeonRoomCount - 1));
  if (auto res = gui::InputHexWordEx("##CompareRoomId", &cmp,
                                     kDenseToolbarButtonWidth + 6.0f, true);
      res.ShouldApply()) {
    *p.compare_room_id = std::clamp<int>(cmp, 0, kDungeonRoomCount - 1);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", kToolbarCompareRoomIdTooltip);
  }
}

void DrawCompareMenu(const DungeonWorkbenchToolbarParams& p,
                     const ToolbarLayout& layout) {
  if (!p.layout || !p.current_room_id || !p.compare_room_id ||
      !p.split_view_enabled) {
    return;
  }

  const CompareDefaultResult def = PickDefaultCompareRoom(
      *p.current_room_id, p.previous_room_id ? *p.previous_room_id : -1,
      p.get_recent_rooms);

  const bool compare_active = *p.split_view_enabled;
  const char* tooltip = compare_active ? "Compare settings" : "Start compare";
  if (DrawToolbarActionButton(
          "CompareMenuButton", kToolbarStartCompareLabel,
          ImVec2(kCompactCompareButtonWidth, layout.button_size), tooltip,
          compare_active)) {
    ImGui::OpenPopup(kToolbarPopupIdCompareMenu);
  }

  if (!ImGui::BeginPopup(kToolbarPopupIdCompareMenu)) {
    return;
  }

  if (!compare_active) {
    if (!def.found) {
      ImGui::TextDisabled(tr("No recent room to compare"));
      ImGui::Separator();
    }
    if (!def.found) {
      ImGui::BeginDisabled();
    }
    if (ImGui::MenuItem(tr("Start Compare"))) {
      p.layout->show_connected_canvas_view = false;
      *p.split_view_enabled = true;
      *p.compare_room_id = def.room_id;
    }
    if (!def.found) {
      ImGui::EndDisabled();
    }
  } else {
    DrawActiveCompareRoomControls(
        p, layout.width >= kInlineCompareRoomIdToolbarWidth);

    ImGui::Separator();
    if (ImGui::MenuItem(tr("Swap Rooms"))) {
      const int old_current = *p.current_room_id;
      const int old_compare = *p.compare_room_id;
      *p.compare_room_id = old_current;
      if (p.on_room_selected) {
        p.on_room_selected(old_compare);
      } else {
        *p.current_room_id = old_compare;
      }
    }
    if (ImGui::MenuItem(tr("Sync View"), nullptr, p.layout->sync_split_view)) {
      p.layout->sync_split_view = !p.layout->sync_split_view;
    }
    if (ImGui::MenuItem(tr("End Compare"))) {
      *p.split_view_enabled = false;
    }
  }

  ImGui::EndPopup();
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

    if (ImGui::BeginPopupContextItem("##RecentRoomActions")) {
      if (ImGui::MenuItem(ICON_MD_COMPARE_ARROWS " Compare")) {
        if (p.layout) {
          p.layout->show_connected_canvas_view = false;
        }
        if (p.split_view_enabled) {
          *p.split_view_enabled = true;
        }
        if (p.compare_room_id) {
          *p.compare_room_id = room_id;
        }
      }
      if (p.on_open_room_panel &&
          ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open as Panel")) {
        p.on_open_room_panel(room_id);
      }
      if (p.forget_recent_room) {
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_CLOSE " Remove from Recent")) {
          to_forget.push_back(room_id);
        }
      }
      ImGui::EndPopup();
    }
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
  if (visibility.panel_mode && p.set_workflow_mode) {
    append(layout.button_size);
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
      4.0f * ImGui::GetFrameHeight() + 3.0f * ImGui::GetStyle().ItemSpacing.x;
  append_left(navigation_width, kToolbarClusterGap);

  if (visibility.recent_rooms) {
    append_left(workbench::CalcIconButtonWidth(kToolbarRecentRoomsLabel,
                                               layout.button_size),
                kToolbarActionGap);
  }
  if (visibility.apply_room) {
    append_left(layout.button_size, kToolbarActionGap);
  }
  if (visibility.dungeon_map) {
    append_left(layout.button_size, kToolbarActionGap);
  }
  if (visibility.stitched_rooms) {
    append_left(workbench::CalcIconButtonWidth(kToolbarModeConnectedLabel,
                                               layout.button_size),
                kToolbarClusterGap);
  }
  if (visibility.compare) {
    append_left(kCompactCompareButtonWidth, kToolbarClusterGap);
  }

  const float right_width = ToolbarRightActionsWidth(p, layout, visibility);
  return left_width +
         (right_width > 0.0f ? kToolbarActionGap + right_width : 0.0f);
}

ToolbarVisibility ResolveToolbarVisibility(
    const DungeonWorkbenchToolbarParams& p, const ToolbarLayout& layout) {
  ToolbarVisibility visibility;
  visibility.compare = true;
  visibility.apply_room =
      p.on_save_room && p.current_room_id && *p.current_room_id >= 0;
  visibility.dungeon_map = p.on_request_dungeon_map != nullptr;
  visibility.view_options = p.primary_viewer != nullptr;
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

  // Collapse infrequent or duplicated actions first. Pane toggles go last;
  // at ultra-compact widths NESW navigation remains the only inline cluster.
  if (collapse(visibility.dungeon_map))
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
  if (collapse(visibility.stitched_rooms))
    return visibility;
  collapse(visibility.pane_toggles);
  return visibility;
}

void DrawOverflowCompareActions(const DungeonWorkbenchToolbarParams& p) {
  const CompareDefaultResult def = PickDefaultCompareRoom(
      *p.current_room_id, p.previous_room_id ? *p.previous_room_id : -1,
      p.get_recent_rooms);
  const bool compare_active = *p.split_view_enabled;

  if (!compare_active) {
    if (ImGui::MenuItem(ICON_MD_COMPARE_ARROWS " Start Compare", nullptr, false,
                        def.found)) {
      p.layout->show_connected_canvas_view = false;
      *p.split_view_enabled = true;
      *p.compare_room_id = def.room_id;
    }
    if (!def.found &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("%s", kToolbarNoCompareHistoryTooltip);
    }
    return;
  }

  DrawActiveCompareRoomControls(p, /*show_direct_room_id=*/true);
  ImGui::Separator();
  if (ImGui::MenuItem(tr("Swap Rooms"))) {
    const int old_current = *p.current_room_id;
    const int old_compare = *p.compare_room_id;
    *p.compare_room_id = old_current;
    if (p.on_room_selected) {
      p.on_room_selected(old_compare);
    } else {
      *p.current_room_id = old_compare;
    }
  }
  if (ImGui::MenuItem(tr("Sync View"), nullptr, p.layout->sync_split_view)) {
    p.layout->sync_split_view = !p.layout->sync_split_view;
  }
  if (ImGui::MenuItem(tr("End Compare"))) {
    *p.split_view_enabled = false;
  }
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
    if (ImGui::MenuItem(ICON_MD_VIEW_QUILT " Stitched Rooms", nullptr,
                        p.layout->show_connected_canvas_view)) {
      p.layout->show_connected_canvas_view =
          !p.layout->show_connected_canvas_view;
    }
  }
  if (!visibility.compare) {
    DrawOverflowCompareActions(p);
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
    if (ImGui::BeginMenu(ICON_MD_VISIBILITY " Canvas View")) {
      DrawViewOptionsContents(p.primary_viewer);
      ImGui::EndMenu();
    }
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

  if (visibility.stitched_rooms) {
    ImGui::SameLine(0.0f, kToolbarClusterGap);
    DrawCanvasModeSelector(p.layout, layout);
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
      DrawViewOptionsButton(p.primary_viewer, layout);
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
