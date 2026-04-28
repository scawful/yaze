#ifndef YAZE_APP_EDITOR_DUNGEON_UI_WORKBENCH_DUNGEON_WORKBENCH_CHROME_H
#define YAZE_APP_EDITOR_DUNGEON_UI_WORKBENCH_DUNGEON_WORKBENCH_CHROME_H

#include <algorithm>
#include <cmath>
#include <functional>
#include <span>

#include "app/gui/core/color.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze::editor::workbench {

struct SegmentSpec {
  const char* label = "";
  const char* tooltip = nullptr;
};

inline float CalcIconButtonWidth(const char* icon, float button_height) {
  if (!icon || !*icon) {
    return button_height;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float text_width = ImGui::CalcTextSize(icon).x;
  const float padding = std::max(2.0f, style.FramePadding.x);
  const float needed_width =
      std::ceil(text_width + (style.FramePadding.x * 2.0f) + padding);
  return std::max(button_height, needed_width);
}

inline float CalcIconToggleButtonWidth(const char* icon_on,
                                       const char* icon_off,
                                       float button_height) {
  return std::max(CalcIconButtonWidth(icon_on, button_height),
                  CalcIconButtonWidth(icon_off, button_height));
}

inline float CalcPaneSegmentHeight(float button_size) {
  return std::max(button_size + 2.0f,
                  gui::LayoutHelpers::GetTouchSafeWidgetHeight() + 2.0f);
}

inline bool ShouldStackSegments(float available_width, int item_count,
                                float min_item_width) {
  if (item_count <= 1) {
    return false;
  }
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float required_width =
      (min_item_width * static_cast<float>(item_count)) +
      (spacing * static_cast<float>(item_count - 1));
  return available_width < required_width;
}

inline void DrawPaneTitle(const char* icon, const char* title,
                          const char* compact_title, bool compact,
                          const char* subtitle = nullptr) {
  ImGui::BeginGroup();
  if (icon && *icon) {
    gui::ColoredText(icon, gui::GetPrimaryVec4());
    ImGui::SameLine(0.0f, 4.0f);
  }

  ImGui::BeginGroup();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(
      (compact && compact_title && *compact_title) ? compact_title : title);
  if (subtitle && *subtitle) {
    ImGui::TextDisabled("%s", subtitle);
  }
  ImGui::EndGroup();
  ImGui::EndGroup();
}

inline void DrawSectionLabel(const char* icon, const char* label,
                             const char* tooltip = nullptr) {
  ImGui::BeginGroup();
  if (icon && *icon) {
    gui::ColoredText(icon, gui::GetPrimaryVec4());
    ImGui::SameLine(0.0f, 6.0f);
  }
  ImGui::TextUnformatted(label);
  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  ImGui::EndGroup();
}

inline bool DrawHeaderIconAction(const char* id, const char* icon,
                                 float button_size, const char* tooltip,
                                 bool active = false) {
  ImGui::PushID(id);
  const bool pressed = gui::TransparentIconButton(
      icon, ImVec2(button_size, button_size), tooltip, active,
      ImVec4(0, 0, 0, 0), "dungeon_workbench", id);
  ImGui::PopID();
  return pressed;
}

inline void DrawPaneHeader(const char* table_id, const char* icon,
                           const char* title, const char* compact_title,
                           const char* subtitle, bool compact,
                           float action_width,
                           const std::function<void()>& draw_actions) {
  // Pane header rows use action-cluster SameLine(0, spacing) calls for the
  // right column; the table itself owns title-vs-actions layout. So the
  // header guard only needs gentle vertical breathing room — don't shrink
  // ItemSpacing.x below the theme default.
  gui::StyleVarGuard frame_padding_guard(
      ImGuiStyleVar_FramePadding,
      ImVec2(std::max(4.0f, ImGui::GetStyle().FramePadding.x),
             std::max(3.0f, ImGui::GetStyle().FramePadding.y)));
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(ImGui::GetStyle().ItemSpacing.x, 4.0f),
             std::max(3.0f, ImGui::GetStyle().ItemSpacing.y - 1.0f)));

  constexpr ImGuiTableFlags kPaneHeaderTableFlags =
      ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoPadInnerX |
      ImGuiTableFlags_NoPadOuterX;

  if (!ImGui::BeginTable(table_id, 2, kPaneHeaderTableFlags)) {
    return;
  }

  ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                          action_width + 4.0f);
  ImGui::TableNextRow();

  ImGui::TableNextColumn();
  DrawPaneTitle(icon, title, compact_title, compact, subtitle);

  ImGui::TableNextColumn();
  if (draw_actions) {
    draw_actions();
  }
  ImGui::EndTable();
}

inline void DrawSegmentedControls(const char* id,
                                  std::span<const SegmentSpec> segments,
                                  int selected_index, float height,
                                  float min_item_width,
                                  const std::function<void(int)>& on_select,
                                  bool force_stack = false) {
  if (segments.empty()) {
    return;
  }

  const float available_width =
      std::max(ImGui::GetContentRegionAvail().x, 1.0f);
  const bool stack =
      force_stack ||
      ShouldStackSegments(available_width, static_cast<int>(segments.size()),
                          min_item_width);
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float inline_width =
      stack ? -1.0f
            : std::max(min_item_width,
                       (available_width -
                        (spacing * static_cast<float>(segments.size() - 1))) /
                           static_cast<float>(segments.size()));

  gui::StyleVarGuard text_align_guard(ImGuiStyleVar_ButtonTextAlign,
                                      ImVec2(0.5f, 0.5f));

  ImGui::PushID(id);
  for (size_t i = 0; i < segments.size(); ++i) {
    const bool selected = selected_index == static_cast<int>(i);
    if (gui::ToggleButton(segments[i].label, selected,
                          ImVec2(inline_width, height))) {
      if (on_select) {
        on_select(static_cast<int>(i));
      }
    }
    if (segments[i].tooltip && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", segments[i].tooltip);
    }
    if (!stack && i + 1 < segments.size()) {
      ImGui::SameLine(0.0f, spacing);
    }
  }
  ImGui::PopID();
}

}  // namespace yaze::editor::workbench

#endif  // YAZE_APP_EDITOR_DUNGEON_UI_WORKBENCH_DUNGEON_WORKBENCH_CHROME_H
