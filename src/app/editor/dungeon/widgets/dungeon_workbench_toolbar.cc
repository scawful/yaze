#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/widgets/dungeon_room_nav_widget.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

constexpr float kTightCompareStackThreshold = 520.0f;

class ScopedWorkbenchToolbar {
 public:
  explicit ScopedWorkbenchToolbar(const char* label) {
    context_ = ImGui::GetCurrentContext();
    if (context_ != nullptr) {
      style_stack_before_ = context_->StyleVarStack.Size;
      color_stack_before_ = context_->ColorStack.Size;
      window_stack_before_ = context_->CurrentWindowStack.Size;
    }

    const auto& theme = gui::LayoutHelpers::GetTheme();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          gui::ConvertColorToImVec4(theme.menu_bar_bg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(gui::LayoutHelpers::GetButtonPadding(),
                               gui::LayoutHelpers::GetButtonPadding()));

    // Keep toolbar controls unclipped at higher DPI and on touch displays.
    const float min_height =
        (gui::LayoutHelpers::GetTouchSafeWidgetHeight() + 6.0f) +
        (gui::LayoutHelpers::GetButtonPadding() * 2.0f) + 2.0f;
    const float height =
        std::max(gui::LayoutHelpers::GetToolbarHeight(), min_height);
    ImGui::BeginChild(
        label, ImVec2(0, height), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    began_child_ = true;
  }

  ~ScopedWorkbenchToolbar() {
    ImGuiContext* ctx =
        context_ != nullptr ? context_ : ImGui::GetCurrentContext();
    const bool has_child_window =
        ctx != nullptr && ctx->CurrentWindow != nullptr &&
        ctx->CurrentWindowStack.Size > window_stack_before_ &&
        ((ctx->CurrentWindow->Flags & ImGuiWindowFlags_ChildWindow) != 0);
    if (began_child_ && has_child_window) {
      ImGui::EndChild();
    }
    if (ctx != nullptr && ctx->StyleVarStack.Size > style_stack_before_) {
      ImGui::PopStyleVar(1);
    }
    if (ctx != nullptr && ctx->ColorStack.Size > color_stack_before_) {
      ImGui::PopStyleColor(1);
    }
  }

 private:
  ImGuiContext* context_ = nullptr;
  int style_stack_before_ = 0;
  int color_stack_before_ = 0;
  int window_stack_before_ = 0;
  bool began_child_ = false;
};

float CalcIconButtonWidth(const char* icon, float btn_height) {
  if (!icon || !*icon) {
    return btn_height;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  // ImGui buttons include horizontal frame padding, so a strict square (w==h)
  // can clip wider glyphs. Size to content, but never smaller than btn_height.
  const float text_w = ImGui::CalcTextSize(icon).x;
  const float fudge = std::max(2.0f, style.FramePadding.x);
  const float needed_w =
      std::ceil(text_w + (style.FramePadding.x * 2.0f) + fudge);
  return std::max(btn_height, needed_w);
}

float CalcIconToggleButtonWidth(const char* icon_on, const char* icon_off,
                                float btn_height) {
  return std::max(CalcIconButtonWidth(icon_on, btn_height),
                  CalcIconButtonWidth(icon_off, btn_height));
}

struct CompareDefaultResult {
  bool found = false;
  int room_id = -1;
};

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
  const float btn_w = CalcIconToggleButtonWidth(icon_on, icon_off, btn);
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

bool SquareIconButton(const char* id, const char* icon, float btn_size,
                      const char* tooltip) {
  const float btn = btn_size;
  const float btn_w = CalcIconButtonWidth(icon, btn);
  ImGui::PushID(id);
  const bool pressed = ImGui::Button(icon, ImVec2(btn_w, btn));
  ImGui::PopID();
  if (ImGui::IsItemHovered() && tooltip && *tooltip) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return pressed;
}

void DrawComparePicker(
    int current_room_id, int* compare_room_id,
    const std::function<const std::deque<int>&()>& get_recent_rooms,
    char* search_buf, size_t search_buf_size) {
  if (!compare_room_id || *compare_room_id < 0) {
    return;
  }
  const bool can_search = search_buf != nullptr && search_buf_size > 1;
  const char* filter = can_search ? search_buf : "";

  char preview[128];
  const auto label = zelda3::GetRoomLabel(*compare_room_id);
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
  ImGui::SetNextItemWidth(
      std::clamp(ImGui::GetContentRegionAvail().x, 180.0f, 420.0f));
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
        const auto rid_label = zelda3::GetRoomLabel(rid);
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
      ImGui::InputTextWithHint("##CompareSearch", "Type to filter rooms...",
                               search_buf, search_buf_size);
    } else {
      ImGui::TextDisabled("Search unavailable");
    }

    ImGui::Spacing();
    ImGui::BeginChild("##CompareSearchList", ImVec2(0, 220), true);
    ImGuiListClipper clipper;
    clipper.Begin(0x128);
    while (clipper.Step()) {
      for (int rid = clipper.DisplayStart; rid < clipper.DisplayEnd; ++rid) {
        if (rid == current_room_id) {
          continue;
        }
        const auto rid_label = zelda3::GetRoomLabel(rid);
        char hex_buf[8];
        snprintf(hex_buf, sizeof(hex_buf), "%03X", rid);
        if (!icontains(rid_label, filter) && !icontains(hex_buf, filter)) {
          continue;
        }
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
    ImGui::SetTooltip("Pick a room to compare");
  }
}

}  // namespace

bool DungeonWorkbenchToolbar::Draw(const DungeonWorkbenchToolbarParams& p) {
  if (!p.layout || !p.current_room_id || !p.split_view_enabled ||
      !p.compare_room_id) {
    ImGui::TextDisabled("Workbench toolbar not wired");
    return false;
  }

  // Keep this scope self-contained so toolbar teardown cannot pop unrelated
  // ImGui stack entries if upstream layout state changes mid-frame.
  ScopedWorkbenchToolbar toolbar_scope("##DungeonWorkbenchToolbar");
  bool request_panel_mode = false;

  const float btn =
      std::max(gui::LayoutHelpers::GetTouchSafeWidgetHeight() + 2.0f,
               gui::LayoutHelpers::GetStandardWidgetHeight() + 6.0f);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;

  {
    // Scope style-var overrides so they are unwound before the toolbar child
    // window closes. ImGui asserts if a child ends with leaked style vars.
    const ImVec2 frame_pad = ImGui::GetStyle().FramePadding;
    gui::StyleVarGuard frame_pad_guard(
        ImGuiStyleVar_FramePadding,
        ImVec2(frame_pad.x, std::max(frame_pad.y, 4.0f)));

    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_NoBordersInBody |
                                       ImGuiTableFlags_NoPadInnerX |
                                       ImGuiTableFlags_NoPadOuterX;
    if (ImGui::BeginTable("##DungeonWorkbenchToolbarTable", 3, kFlags)) {
      const float w_grid =
          CalcIconToggleButtonWidth(ICON_MD_GRID_ON, ICON_MD_GRID_OFF, btn);
      const float w_bounds = CalcIconButtonWidth(ICON_MD_CROP_SQUARE, btn);
      const float w_coords = CalcIconButtonWidth(ICON_MD_MY_LOCATION, btn);
      const float w_camera = CalcIconButtonWidth(ICON_MD_GRID_VIEW, btn);
      const float right_cluster_w =
          w_grid + w_bounds + w_coords + w_camera + (spacing * 3.0f);
      const float right_w = right_cluster_w + 6.0f;  // Avoid edge clipping.
      ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Middle", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthFixed,
                              right_w);
      ImGui::TableNextRow();

      // Left cluster: sidebar toggles, nav, room label.
      ImGui::TableNextColumn();
      (void)IconToggleButton("RoomsToggle", ICON_MD_LIST, ICON_MD_LIST,
                             &p.layout->show_left_sidebar, btn,
                             "Hide room browser", "Show room browser");
      ImGui::SameLine();
      (void)IconToggleButton("InspectorToggle", ICON_MD_TUNE, ICON_MD_TUNE,
                             &p.layout->show_right_inspector, btn,
                             "Hide inspector", "Show inspector");
      if (p.set_workflow_mode) {
        ImGui::SameLine();
        if (SquareIconButton("##PanelMode", ICON_MD_VIEW_QUILT, btn,
                             "Switch to standalone panel workflow")) {
          request_panel_mode = true;
        }
      }
      ImGui::SameLine();

      const int rid = *p.current_room_id;
      DungeonRoomNavWidget::Draw("Nav", rid, p.on_room_selected);
      ImGui::SameLine();

      const auto room_label = zelda3::GetRoomLabel(rid);
      char title[192];
      snprintf(title, sizeof(title), "[%03X] %s", rid, room_label.c_str());
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(title);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", title);
      }

      // Middle cluster: compare controls.
      ImGui::TableNextColumn();
      ImGui::BeginGroup();
      if (!*p.split_view_enabled) {
        if (SquareIconButton("##EnableSplit", ICON_MD_COMPARE_ARROWS, btn,
                             "Enable split view (compare)")) {
          const CompareDefaultResult def = PickDefaultCompareRoom(
              *p.current_room_id, p.previous_room_id ? *p.previous_room_id : -1,
              p.get_recent_rooms);
          if (def.found) {
            *p.split_view_enabled = true;
            *p.compare_room_id = def.room_id;
          }
        }
      } else {
        // Compare icon label.
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled(ICON_MD_COMPARE_ARROWS);
        ImGui::SameLine();

        const float avail = ImGui::GetContentRegionAvail().x;
        const bool stacked = avail < kTightCompareStackThreshold;
        if (stacked) {
          ImGui::NewLine();
        }

        DrawComparePicker(*p.current_room_id, p.compare_room_id,
                          p.get_recent_rooms, p.compare_search_buf,
                          p.compare_search_buf_size);

        ImGui::SameLine();
        uint16_t cmp =
            static_cast<uint16_t>(std::clamp(*p.compare_room_id, 0, 0x127));
        if (auto res =
                gui::InputHexWordEx("##CompareRoomId", &cmp, 70.0f, true);
            res.ShouldApply()) {
          *p.compare_room_id = std::clamp<int>(cmp, 0, 0x127);
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Compare room ID");
        }

        ImGui::SameLine();
        if (SquareIconButton("##SwapRooms", ICON_MD_SWAP_HORIZ, btn,
                             "Swap active and compare rooms")) {
          const int old_current = *p.current_room_id;
          const int old_compare = *p.compare_room_id;
          *p.compare_room_id = old_current;
          if (p.on_room_selected) {
            p.on_room_selected(old_compare);
          } else {
            *p.current_room_id = old_compare;
          }
        }

        ImGui::SameLine();
        if (IconToggleButton("##SyncView", ICON_MD_LINK, ICON_MD_LINK_OFF,
                             &p.layout->sync_split_view, btn,
                             "Unsync compare view",
                             "Sync compare view to active")) {
          // toggle handled inside IconToggleButton
        }

        ImGui::SameLine();
        if (SquareIconButton("##CloseSplit", ICON_MD_CLOSE, btn,
                             "Disable split view")) {
          *p.split_view_enabled = false;
        }
      }
      ImGui::EndGroup();

      // Right cluster: view toggles (grid/bounds/coords/camera).
      ImGui::TableNextColumn();
      if (p.primary_viewer) {
        // Right-align by manually moving cursor to the end of the cell.
        const float total_w = right_cluster_w;
        const float start_x =
            ImGui::GetCursorPosX() +
            std::max(0.0f, ImGui::GetContentRegionAvail().x - total_w);
        ImGui::SetCursorPosX(start_x);

        bool v = p.primary_viewer->show_grid();
        if (SquareIconButton("##GridToggle",
                             v ? ICON_MD_GRID_ON : ICON_MD_GRID_OFF, btn,
                             v ? "Hide grid" : "Show grid")) {
          p.primary_viewer->set_show_grid(!v);
        }
        ImGui::SameLine();

        v = p.primary_viewer->show_object_bounds();
        if (SquareIconButton("##BoundsToggle", ICON_MD_CROP_SQUARE, btn,
                             v ? "Hide object bounds" : "Show object bounds")) {
          p.primary_viewer->set_show_object_bounds(!v);
        }
        ImGui::SameLine();

        v = p.primary_viewer->show_coordinate_overlay();
        if (SquareIconButton(
                "##CoordsToggle", ICON_MD_MY_LOCATION, btn,
                v ? "Hide hover coordinates" : "Show hover coordinates")) {
          p.primary_viewer->set_show_coordinate_overlay(!v);
        }
        ImGui::SameLine();

        v = p.primary_viewer->show_camera_quadrant_overlay();
        if (SquareIconButton(
                "##CameraToggle", ICON_MD_GRID_VIEW, btn,
                v ? "Hide camera quadrants" : "Show camera quadrants")) {
          p.primary_viewer->set_show_camera_quadrant_overlay(!v);
        }
      }

      ImGui::EndTable();
    }
  }

  return request_panel_mode;
}

}  // namespace yaze::editor
