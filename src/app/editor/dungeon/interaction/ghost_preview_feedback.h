#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_GHOST_PREVIEW_FEEDBACK_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_GHOST_PREVIEW_FEEDBACK_H_

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "app/editor/agent/agent_ui_theme.h"
#include "imgui/imgui.h"

namespace yaze::editor {

enum class PlacementCapacityState {
  kNormal = 0,
  kNearLimit,
  kAtLimit,
};

inline PlacementCapacityState GetPlacementCapacityState(size_t current_count,
                                                        size_t max_count) {
  if (max_count == 0) {
    return PlacementCapacityState::kNormal;
  }
  if (current_count >= max_count) {
    return PlacementCapacityState::kAtLimit;
  }
  if (current_count + 1 == max_count) {
    return PlacementCapacityState::kNearLimit;
  }
  return PlacementCapacityState::kNormal;
}

inline ImVec4 GetPlacementAccentColor(const AgentUITheme& theme,
                                      PlacementCapacityState state,
                                      const ImVec4& normal_color) {
  switch (state) {
    case PlacementCapacityState::kAtLimit:
      return theme.status_error;
    case PlacementCapacityState::kNearLimit:
      return theme.status_warning;
    case PlacementCapacityState::kNormal:
    default:
      return normal_color;
  }
}

inline std::string_view GetPlacementCapacityStatusText(
    PlacementCapacityState state) {
  switch (state) {
    case PlacementCapacityState::kAtLimit:
      return "ROOM FULL";
    case PlacementCapacityState::kNearLimit:
      return "LAST SLOT";
    case PlacementCapacityState::kNormal:
    default:
      return {};
  }
}

inline std::string_view GetPlacementCapacityTooltipSuffix(
    PlacementCapacityState state) {
  switch (state) {
    case PlacementCapacityState::kAtLimit:
      return "Placement blocked";
    case PlacementCapacityState::kNearLimit:
      return "Last available slot";
    case PlacementCapacityState::kNormal:
    default:
      return {};
  }
}

inline void DrawPlacementCapacityBadge(ImDrawList* draw_list,
                                       const ImVec2& badge_min,
                                       const AgentUITheme& theme,
                                       PlacementCapacityState state,
                                       std::string_view primary_text) {
  if (state == PlacementCapacityState::kNormal || primary_text.empty()) {
    return;
  }

  const ImVec4 accent =
      GetPlacementAccentColor(theme, state, theme.text_primary);
  const std::string_view status_text = GetPlacementCapacityStatusText(state);
  const ImVec2 primary_size = ImGui::CalcTextSize(primary_text.data());
  const ImVec2 status_size = ImGui::CalcTextSize(status_text.data());
  const float width = std::max(primary_size.x, status_size.x) + 14.0f;
  const float height = primary_size.y + status_size.y + 14.0f;
  const ImVec2 badge_max(badge_min.x + width, badge_min.y + height);

  draw_list->AddRectFilled(
      badge_min, badge_max,
      ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.18f)), 5.0f);
  draw_list->AddRect(
      badge_min, badge_max,
      ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.92f)), 5.0f, 0,
      2.0f);
  draw_list->AddText(ImVec2(badge_min.x + 7.0f, badge_min.y + 4.0f),
                     ImGui::GetColorU32(theme.text_primary),
                     primary_text.data());
  draw_list->AddText(
      ImVec2(badge_min.x + 7.0f, badge_min.y + 8.0f + primary_size.y),
      ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 1.0f)),
      status_text.data());
}

inline ImVec4 GetPlacementSummaryColor(const AgentUITheme& theme,
                                       size_t current_count, size_t max_count,
                                       const ImVec4& normal_color) {
  return GetPlacementAccentColor(
      theme, GetPlacementCapacityState(current_count, max_count), normal_color);
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_GHOST_PREVIEW_FEEDBACK_H_
