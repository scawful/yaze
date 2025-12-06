#include "app/gui/widgets/themed_widgets.h"

#include "app/gui/core/theme_manager.h"
#include "app/gui/core/icons.h"
#include "app/gfx/types/snes_color.h" // For SnesColor

namespace yaze {
namespace gui {

bool ThemedIconButton(const char* icon, const char* tooltip,
                      const ImVec2& size, bool is_active, 
                      bool is_disabled) {
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

  ImGui::PopStyleColor(4);

  if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return clicked;
}

bool TransparentIconButton(const char* icon, const ImVec2& size,
                           const char* tooltip, bool is_active,
                           const ImVec4& active_color) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  // Transparent background
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.header_hovered));
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
    text_color = ConvertColorToImVec4(theme.text_secondary);
  }
  ImGui::PushStyleColor(ImGuiCol_Text, text_color);

  bool clicked = ImGui::Button(icon, size);

  ImGui::PopStyleColor(4);

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return clicked;
}

bool ThemedButton(const char* label, const ImVec2& size) {
  // Standard button uses ImGui style colors which are already set by ThemeManager::ApplyTheme
  return ImGui::Button(label, size);
}

bool PrimaryButton(const char* label, const ImVec2& size) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.primary));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered)); // Should ideally be a lighter primary
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.text_primary)); // OnPrimary

  bool clicked = ImGui::Button(label, size);

  ImGui::PopStyleColor(4);
  return clicked;
}

bool DangerButton(const char* label, const ImVec2& size) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.error));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.text_primary));

  bool clicked = ImGui::Button(label, size);

  ImGui::PopStyleColor(4);
  return clicked;
}

void SectionHeader(const char* label) {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.primary));
  ImGui::Text("%s", label);
  ImGui::PopStyleColor();
  ImGui::Separator();
}

// Stub for PaletteColorButton since it requires SnesColor which might need more includes
// For now, we assume it was defined elsewhere or we need to implement it if we overwrote it.
// Based on palette_group_card.cc usage, it seems it was expected.
// I'll implement a basic version.
bool PaletteColorButton(const char* id, const gfx::SnesColor& color, 
                        bool is_selected, bool is_modified, 
                        const ImVec2& size) {
    ImVec4 col = ConvertSnesColorToImVec4(color);
    bool clicked = ImGui::ColorButton(id, col, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker, size);
    
    if (is_selected) {
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 
            IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);
    }
    if (is_modified) {
        // Draw a small dot or indicator
    }
    return clicked;
}

void PanelHeader(const char* title, const char* icon, bool* p_open) {
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

    if (TransparentIconButton(ICON_MD_CLOSE, ImVec2(button_size, button_size), "Close")) {
      *p_open = false;
    }
  }

  // Move cursor past header
  ImGui::SetCursorPosY(header_height + 8.0f);
}

}  // namespace gui
}  // namespace yaze
