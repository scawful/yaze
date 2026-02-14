#include "app/gui/widgets/themed_widgets.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/gfx/types/snes_color.h"  // For SnesColor

namespace yaze {
namespace gui {

bool RippleButton(const char* label, const ImVec2& size,
                  const ImVec4& ripple_color, const char* panel_id,
                  const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  // Track click state for ripple animation
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::string("ripple_") + label;
  static std::unordered_map<std::string, float> click_times;
  static std::unordered_map<std::string, ImVec2> click_positions;

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ConvertColorToImVec4(theme.button_active));

  bool clicked = ImGui::Button(label, size);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 rect_min = ImGui::GetItemRectMin();
  ImVec2 rect_max = ImGui::GetItemRectMax();
  ImVec2 rect_center = ImVec2((rect_min.x + rect_max.x) * 0.5f,
                              (rect_min.y + rect_max.y) * 0.5f);
  float max_radius =
      std::max(rect_max.x - rect_min.x, rect_max.y - rect_min.y) * 0.7f;

  // Trigger ripple on click
  if (clicked) {
    click_times[anim_key] = 0.0f;
    click_positions[anim_key] = ImGui::GetIO().MousePos;
  }

  // Animate ripple
  auto time_iter = click_times.find(anim_key);
  if (time_iter != click_times.end()) {
    float& ripple_time = time_iter->second;
    ripple_time += ImGui::GetIO().DeltaTime * 3.0f;  // Speed of ripple

    if (ripple_time < 1.0f) {
      // Ripple expanding
      float eased_t = Animator::EaseOutCubic(ripple_time);
      float radius = eased_t * max_radius;
      float alpha = ripple_color.w * (1.0f - eased_t);  // Fade out

      ImVec2 ripple_center = click_positions[anim_key];
      ImU32 color = ImGui::GetColorU32(
          ImVec4(ripple_color.x, ripple_color.y, ripple_color.z, alpha));

      // Clip to button bounds
      draw_list->PushClipRect(rect_min, rect_max, true);
      draw_list->AddCircleFilled(ripple_center, radius, color, 32);
      draw_list->PopClipRect();
    } else {
      // Animation complete, remove tracking
      click_times.erase(anim_key);
      click_positions.erase(anim_key);
    }
  }

  // Hover effect
  const bool hovered = ImGui::IsItemHovered();
  float hover_t = GetAnimator().Animate(panel_key, anim_key + "_hover",
                                        hovered ? 1.0f : 0.0f, 8.0f);
  if (hover_t > 0.001f) {
    ImVec4 overlay = ConvertColorToImVec4(theme.button_hovered);
    overlay.w *= hover_t * 0.2f;
    draw_list->AddRectFilled(rect_min, rect_max, ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  ImGui::PopStyleColor(3);
  return clicked;
}

bool BouncyButton(const char* label, const ImVec2& size, const char* panel_id,
                  const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::string("bouncy_") + label;

  // Track press state for bounce animation
  static std::unordered_map<std::string, float> bounce_times;

  // Determine current scale based on animation state
  float target_scale = 1.0f;
  bool is_pressed = ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

  auto bounce_iter = bounce_times.find(anim_key);
  float current_scale = 1.0f;

  if (bounce_iter != bounce_times.end()) {
    float& bounce_time = bounce_iter->second;
    bounce_time += ImGui::GetIO().DeltaTime * 8.0f;

    if (bounce_time < 1.0f) {
      // Bounce back with elastic easing
      current_scale = 0.92f + 0.08f * Animator::EaseOutBack(bounce_time);
    } else {
      bounce_times.erase(anim_key);
      current_scale = 1.0f;
    }
  } else if (is_pressed) {
    target_scale = 0.92f;  // Pressed scale
  }

  // Animate scale
  float scale = GetAnimator().Animate(panel_key, anim_key + "_scale",
                                      target_scale, 15.0f);
  if (bounce_iter != bounce_times.end()) {
    scale = current_scale;  // Override with bounce animation
  }

  // Calculate scaled size
  ImVec2 actual_size = size;
  if (actual_size.x == 0 && actual_size.y == 0) {
    actual_size = ImGui::CalcTextSize(label);
    actual_size.x += ImGui::GetStyle().FramePadding.x * 2;
    actual_size.y += ImGui::GetStyle().FramePadding.y * 2;
  }

  ImVec2 scaled_size = ImVec2(actual_size.x * scale, actual_size.y * scale);
  ImVec2 offset = ImVec2((actual_size.x - scaled_size.x) * 0.5f,
                         (actual_size.y - scaled_size.y) * 0.5f);

  // Position adjustment for centering scaled button
  ImVec2 cursor_pos = ImGui::GetCursorPos();
  ImGui::SetCursorPos(ImVec2(cursor_pos.x + offset.x, cursor_pos.y + offset.y));

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ConvertColorToImVec4(theme.button_active));

  bool clicked = ImGui::Button(label, scaled_size);

  // Trigger bounce on release
  if (ImGui::IsItemDeactivated() && ImGui::IsItemHovered()) {
    bounce_times[anim_key] = 0.0f;
  }

  ImGui::PopStyleColor(3);

  // Restore cursor position for proper layout
  ImGui::SetCursorPos(ImVec2(cursor_pos.x + actual_size.x + ImGui::GetStyle().ItemSpacing.x,
                             cursor_pos.y));

  return clicked;
}

bool ThemedIconButton(const char* icon, const char* tooltip,
                      const ImVec2& size, bool is_active,
                      bool is_disabled, const char* panel_id,
                      const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImVec4 bg_color = is_active ? ConvertColorToImVec4(theme.button_active)
                              : ConvertColorToImVec4(theme.button);
  ImVec4 text_color = is_disabled ? ConvertColorToImVec4(theme.text_disabled)
                                  : (is_active ? ConvertColorToImVec4(theme.text_primary)
                                               : ConvertColorToImVec4(theme.text_secondary));

  ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleColor(ImGuiCol_Text, text_color);

  bool clicked = ImGui::Button(icon, size);

  // Animated hover effect
  const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        hovered ? 1.0f : 0.0f, 8.0f);

  if (hover_t > 0.001f && !is_disabled) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec4 overlay = ConvertColorToImVec4(theme.button_hovered);
    overlay.w *= hover_t * 0.3f;  // Subtle overlay
    draw_list->AddRectFilled(rect_min, rect_max,
                             ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  ImGui::PopStyleColor(4);

  if (tooltip && hovered) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return clicked;
}

bool TransparentIconButton(const char* icon, const ImVec2& size,
                           const char* tooltip, bool is_active,
                           const ImVec4& active_color, const char* panel_id,
                           const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  // Transparent background
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.header_active));

  // Text color based on state
  // If active and custom color provided (alpha > 0), use that; otherwise use theme.primary
  ImVec4 text_color;
  if (is_active) {
    if (active_color.w > 0.0f) {
      text_color = active_color;  // Use category-specific color
    } else {
      text_color = ConvertColorToImVec4(theme.primary);  // Default to theme primary
    }
  } else {
    if (active_color.w > 0.0f) {
      text_color = active_color;
      text_color.w = std::min(text_color.w, 0.7f);
    } else {
      text_color = ConvertColorToImVec4(theme.text_primary);
      text_color.w *= 0.7f;
    }
  }
  ImGui::PushStyleColor(ImGuiCol_Text, text_color);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImDrawListSplitter splitter;
  splitter.Split(draw_list, 2);
  splitter.SetCurrentChannel(draw_list, 1);

  bool clicked = ImGui::Button(icon, size);

  const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        (hovered || is_active) ? 1.0f : 0.0f,
                                        10.0f);

  if (hover_t > 0.001f) {
    splitter.SetCurrentChannel(draw_list, 0);
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec4 overlay =
        is_active ? (active_color.w > 0.0f
                         ? active_color
                         : ConvertColorToImVec4(theme.header_active))
                  : ConvertColorToImVec4(theme.header_hovered);
    const float base_alpha = is_active ? 0.28f : 0.18f;
    overlay.w *= (base_alpha * hover_t);
    draw_list->AddRectFilled(rect_min, rect_max,
                             ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  splitter.Merge(draw_list);

  ImGui::PopStyleColor(4);

  if (tooltip && hovered) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return clicked;
}

bool ThemedButton(const char* label, const ImVec2& size, const char* panel_id,
                  const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  bool clicked = ImGui::Button(label, size);

  // Animated hover effect
  const bool hovered = ImGui::IsItemHovered();
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        hovered ? 1.0f : 0.0f, 8.0f);

  if (hover_t > 0.001f) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec4 overlay = ConvertColorToImVec4(theme.button_hovered);
    overlay.w *= hover_t * 0.25f;
    draw_list->AddRectFilled(rect_min, rect_max,
                             ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  return clicked;
}

bool PrimaryButton(const char* label, const ImVec2& size, const char* panel_id,
                   const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.primary));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.text_primary));

  bool clicked = ImGui::Button(label, size);

  // Animated hover effect with brighter highlight for primary
  const bool hovered = ImGui::IsItemHovered();
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        hovered ? 1.0f : 0.0f, 8.0f);

  if (hover_t > 0.001f) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec4 overlay = ImVec4(1.0f, 1.0f, 1.0f, hover_t * 0.15f);  // White highlight
    draw_list->AddRectFilled(rect_min, rect_max,
                             ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  ImGui::PopStyleColor(4);
  return clicked;
}

bool DangerButton(const char* label, const ImVec2& size, const char* panel_id,
                  const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.error));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.text_primary));

  bool clicked = ImGui::Button(label, size);

  // Animated hover effect with darker red tint for danger
  const bool hovered = ImGui::IsItemHovered();
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        hovered ? 1.0f : 0.0f, 8.0f);

  if (hover_t > 0.001f) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec4 overlay = ImVec4(0.0f, 0.0f, 0.0f, hover_t * 0.15f);  // Dark overlay
    draw_list->AddRectFilled(rect_min, rect_max,
                             ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  ImGui::PopStyleColor(4);
  return clicked;
}

bool SuccessButton(const char* label, const ImVec2& size, const char* panel_id,
                   const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.success));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleColor(ImGuiCol_Text,
                        ConvertColorToImVec4(theme.text_primary));

  bool clicked = ImGui::Button(label, size);

  const bool hovered = ImGui::IsItemHovered();
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        hovered ? 1.0f : 0.0f, 8.0f);

  if (hover_t > 0.001f) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 rect_min = ImGui::GetItemRectMin();
    ImVec2 rect_max = ImGui::GetItemRectMax();
    ImVec4 overlay = ImVec4(1.0f, 1.0f, 1.0f, hover_t * 0.15f);
    draw_list->AddRectFilled(rect_min, rect_max,
                             ImGui::GetColorU32(overlay),
                             ImGui::GetStyle().FrameRounding);
  }

  ImGui::PopStyleColor(4);
  return clicked;
}

bool ToolbarIconButton(const char* icon, const char* tooltip,
                       bool is_active) {
  return ThemedIconButton(icon, tooltip, IconSize::Toolbar(), is_active);
}

void SectionHeader(const char* label) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.primary));
  ImGui::Text("%s", label);
  ImGui::PopStyleColor();
  ImGui::Separator();
}

bool PaletteColorButton(const char* id, const gfx::SnesColor& color,
                        bool is_selected, bool is_modified,
                        const ImVec2& size, const char* panel_id,
                        const char* anim_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  ImVec4 col = ConvertSnesColorToImVec4(color);
  bool clicked = ImGui::ColorButton(
      id, col, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker, size);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 rect_min = ImGui::GetItemRectMin();
  ImVec2 rect_max = ImGui::GetItemRectMax();

  // Animated hover effect
  const bool hovered = ImGui::IsItemHovered();
  const char* panel_key = panel_id ? panel_id : "global";
  std::string anim_key = anim_id ? anim_id : std::to_string(ImGui::GetItemID());
  float hover_t = GetAnimator().Animate(panel_key, anim_key,
                                        hovered ? 1.0f : 0.0f, 10.0f);

  if (hover_t > 0.001f) {
    ImVec4 overlay = ImVec4(1.0f, 1.0f, 1.0f, hover_t * 0.25f);
    draw_list->AddRectFilled(rect_min, rect_max, ImGui::GetColorU32(overlay));
  }

  // Selection border (animated)
  if (is_selected) {
    float select_t = GetAnimator().Animate(panel_key, anim_key + "_sel", 1.0f, 8.0f);
    ImU32 border_color = IM_COL32(255, 255, 255, static_cast<int>(255 * select_t));
    draw_list->AddRect(rect_min, rect_max, border_color, 0.0f, 0, 2.0f);
  }

  // Modified indicator (small colored dot in corner)
  if (is_modified) {
    ImVec4 mod_color = ConvertColorToImVec4(theme.warning);
    float dot_radius = 4.0f;
    ImVec2 dot_center = ImVec2(rect_max.x - dot_radius - 2,
                               rect_min.y + dot_radius + 2);
    draw_list->AddCircleFilled(dot_center, dot_radius,
                               ImGui::GetColorU32(mod_color));
  }

  return clicked;
}

void PanelHeader(const char* title, const char* icon, bool* p_open,
                 const char* panel_id) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  const float header_height = 44.0f;
  const float padding = 12.0f;

  // Header background
  ImVec2 header_min = ImGui::GetCursorScreenPos();
  ImVec2 header_max = ImVec2(header_min.x + ImGui::GetWindowWidth(),
                             header_min.y + header_height);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(header_min, header_max,
                           ImGui::GetColorU32(ConvertColorToImVec4(theme.header)));

  // Bottom border
  draw_list->AddLine(ImVec2(header_min.x, header_max.y),
                     ImVec2(header_max.x, header_max.y),
                     ImGui::GetColorU32(ConvertColorToImVec4(theme.border)), 1.0f);

  // Content positioning
  ImGui::SetCursorPosX(padding);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (header_height - ImGui::GetTextLineHeight()) * 0.5f);

  // Icon
  if (icon) {
    ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.primary));
    ImGui::Text("%s", icon);
    ImGui::PopStyleColor();
    ImGui::SameLine();
  }

  // Title
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.text_primary));
  ImGui::Text("%s", title);
  ImGui::PopStyleColor();

  // Close button
  if (p_open) {
    const float button_size = 28.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - button_size - padding);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);

    if (TransparentIconButton(ICON_MD_CLOSE, ImVec2(button_size, button_size),
                              "Close", false, ImVec4(0, 0, 0, 0), panel_id,
                              "close")) {
      *p_open = false;
    }
  }

  // Move cursor past header
  ImGui::SetCursorPosY(header_height + 8.0f);
}

void ThemedTooltip(const char* text) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_PopupBg, ConvertColorToImVec4(theme.popup_bg));
  ImGui::PushStyleColor(ImGuiCol_Border, ConvertColorToImVec4(theme.border));
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.text_primary));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

  if (ImGui::BeginTooltip()) {
    ImGui::Text("%s", text);
    ImGui::EndTooltip();
  }

  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(3);
}

}  // namespace gui
}  // namespace yaze
