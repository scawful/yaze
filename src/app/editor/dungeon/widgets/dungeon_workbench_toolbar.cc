#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"

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
constexpr float kToolbarActionGap = 4.0f;

constexpr char kToolbarPopupIdViewOptions[] = "##WorkbenchViewOptions";
constexpr char kToolbarPopupIdCompareSearchList[] = "##CompareSearchList";
constexpr char kToolbarPopupIdCompareMenu[] = "##WorkbenchCompareMenu";
constexpr char kToolbarStartCompareLabel[] = ICON_MD_COMPARE_ARROWS;
constexpr char kToolbarViewOptionsLabel[] = ICON_MD_VISIBILITY;
constexpr char kToolbarModeConnectedLabel[] = "Connected";
constexpr char kToolbarRoomReviewLabel[] = ICON_MD_FACT_CHECK;
constexpr char kToolbarRoomSearchHint[] = "Type to filter rooms...";
constexpr char kToolbarComparePickerTooltip[] = "Pick a room to compare";
constexpr char kToolbarCompareRoomIdTooltip[] = "Compare room ID";
constexpr char kToolbarPanelWorkflowTooltip[] =
    "Switch to standalone panel workflow (Ctrl+Shift+W)";
constexpr char kToolbarRoomMatrixTooltip[] = "Open room review tools";
constexpr char kToolbarNoCompareHistoryMessage[] =
    "Visit another room to seed compare history.";
constexpr char kToolbarNoCompareHistoryTooltip[] =
    "Visit another room first to seed compare history.";
struct ToolbarLayout {
  float width = 0.0f;
  float spacing = 0.0f;
  float button_size = 0.0f;
};

struct CompareDefaultResult {
  bool found = false;
  int room_id = -1;
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
    bool v = viewer->show_grid();
    if (ImGui::Checkbox("Grid", &v)) {
      viewer->set_show_grid(v);
    }
    v = viewer->show_object_bounds();
    if (ImGui::Checkbox("Object Bounds", &v)) {
      viewer->set_show_object_bounds(v);
    }
    v = viewer->show_coordinate_overlay();
    if (ImGui::Checkbox("Hover Coordinates", &v)) {
      viewer->set_show_coordinate_overlay(v);
    }
    v = viewer->show_camera_quadrant_overlay();
    if (ImGui::Checkbox("Camera Quadrants", &v)) {
      viewer->set_show_camera_quadrant_overlay(v);
    }
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
                                  ? "Return to room editing canvas"
                                  : "Browse rooms through the connection graph",
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
      ImGui::TextDisabled("Search unavailable");
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
      ImGui::TextDisabled("No recent room to compare");
      ImGui::Separator();
    }
    if (!def.found) {
      ImGui::BeginDisabled();
    }
    if (ImGui::MenuItem("Start Compare")) {
      p.layout->show_connected_canvas_view = false;
      *p.split_view_enabled = true;
      *p.compare_room_id = def.room_id;
    }
    if (!def.found) {
      ImGui::EndDisabled();
    }
  } else {
    ImGui::TextDisabled("Compare Room");
    DrawComparePicker(*p.current_room_id, p.compare_room_id, p.get_recent_rooms,
                      p.compare_search_buf, p.compare_search_buf_size,
                      p.primary_viewer ? p.primary_viewer->project() : nullptr);
    if (layout.width >= kInlineCompareRoomIdToolbarWidth) {
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

    ImGui::Separator();
    if (ImGui::MenuItem("Swap Rooms")) {
      const int old_current = *p.current_room_id;
      const int old_compare = *p.compare_room_id;
      *p.compare_room_id = old_current;
      if (p.on_room_selected) {
        p.on_room_selected(old_compare);
      } else {
        *p.current_room_id = old_compare;
      }
    }
    if (ImGui::MenuItem("Sync View", nullptr, p.layout->sync_split_view)) {
      p.layout->sync_split_view = !p.layout->sync_split_view;
    }
    if (ImGui::MenuItem("End Compare")) {
      *p.split_view_enabled = false;
    }
  }

  ImGui::EndPopup();
}

}  // namespace

bool DungeonWorkbenchToolbar::ShouldShowInlineRoomNav(float toolbar_width) {
  (void)toolbar_width;
  return true;
}

bool DungeonWorkbenchToolbar::Draw(const DungeonWorkbenchToolbarParams& p) {
  if (!p.layout || !p.current_room_id || !p.split_view_enabled ||
      !p.compare_room_id) {
    ImGui::TextDisabled("Workbench toolbar not wired");
    return false;
  }

  const ToolbarLayout layout =
      ResolveToolbarLayout(std::max(ImGui::GetContentRegionAvail().x, 1.0f));
  bool request_panel_mode = false;

  const ImVec2 frame_pad = ImGui::GetStyle().FramePadding;
  gui::StyleVarGuard frame_pad_guard(
      ImGuiStyleVar_FramePadding, ImVec2(std::max(4.0f, frame_pad.x - 1.0f),
                                         std::max(2.0f, frame_pad.y - 2.0f)));
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(3.0f, layout.spacing * 0.55f),
             std::max(2.0f, ImGui::GetStyle().ItemSpacing.y - 2.0f)));
  const float right_icon_width =
      workbench::CalcIconButtonWidth(ICON_MD_VISIBILITY, layout.button_size);
  float right_actions_width = right_icon_width;
  if (p.set_workflow_mode) {
    right_actions_width +=
        kToolbarActionGap +
        workbench::CalcIconButtonWidth(ICON_MD_VIEW_QUILT, layout.button_size);
  }
  if (p.open_room_matrix) {
    right_actions_width +=
        kToolbarActionGap + workbench::CalcIconButtonWidth(
                                kToolbarRoomReviewLabel, layout.button_size);
  }

  (void)IconToggleButton("RoomsToggle", ICON_MD_LIST, ICON_MD_LIST,
                         &p.layout->show_left_sidebar, layout.button_size,
                         "Hide room browser", "Show room browser");
  ImGui::SameLine(0.0f, kToolbarActionGap);
  (void)IconToggleButton("InspectorToggle", ICON_MD_TUNE, ICON_MD_TUNE,
                         &p.layout->show_right_inspector, layout.button_size,
                         "Hide inspector", "Show inspector");

  ImGui::SameLine(0.0f, kToolbarActionGap);
  DungeonRoomNavWidget::Draw("WorkbenchNav", *p.current_room_id,
                             p.on_room_selected);

  ImGui::SameLine(0.0f, kToolbarActionGap);
  DrawCanvasModeSelector(p.layout, layout);

  if (p.primary_viewer) {
    p.primary_viewer->SetConnectedControlsInline(
        p.layout->show_connected_canvas_view);
  }
  if (p.layout->show_connected_canvas_view && p.primary_viewer &&
      p.current_room_id) {
    ImGui::SameLine(0.0f, kToolbarActionGap);
    p.primary_viewer->DrawConnectedToolbarControls(*p.current_room_id);
  }

  ImGui::SameLine(0.0f, kToolbarActionGap);
  DrawCompareMenu(p, layout);

  const float right_start =
      std::max(ImGui::GetCursorPosX() + kToolbarActionGap,
               ImGui::GetWindowContentRegionMax().x - right_actions_width);
  ImGui::SameLine(right_start, 0.0f);
  DrawViewOptionsButton(p.primary_viewer, layout);
  if (p.set_workflow_mode) {
    ImGui::SameLine(0.0f, kToolbarActionGap);
    if (workbench::DrawHeaderIconAction("PanelMode", ICON_MD_VIEW_QUILT,
                                        layout.button_size,
                                        kToolbarPanelWorkflowTooltip)) {
      request_panel_mode = true;
    }
  }
  if (p.open_room_matrix) {
    ImGui::SameLine(0.0f, kToolbarActionGap);
    if (workbench::DrawHeaderIconAction("RoomMatrix", kToolbarRoomReviewLabel,
                                        layout.button_size,
                                        kToolbarRoomMatrixTooltip)) {
      p.open_room_matrix();
    }
  }

  return request_panel_mode;
}

}  // namespace yaze::editor
