#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/gui/editor_layout.h"

#include "absl/strings/str_format.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/ui_helpers.h"
#include "app/gui/widgets/widget_measurement.h"
#include "app/gui/widgets/widget_id_registry.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

// ============================================================================
// Toolset Implementation
// ============================================================================

void Toolset::Begin() {
  // Ultra-compact toolbar with minimal padding
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
  
  // Don't use BeginGroup - it causes stretching. Just use direct layout.
  in_toolbar_ = true;
  button_count_ = 0;
  current_line_width_ = 0.0f;
  
}

void Toolset::End() {
  // End the current line
  ImGui::NewLine();
  
  ImGui::PopStyleVar(3);
  ImGui::Separator();
  in_toolbar_ = false;
  current_line_width_ = 0.0f;
}

void Toolset::BeginModeGroup() {
  // Compact inline mode buttons without child window to avoid scroll issues
  // Just use a simple colored background rect
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.17f, 0.5f));
  
  // Use a frameless child with exact button height to avoid scrolling
  const float button_size = 28.0f;  // Smaller buttons to match toolbar height
  const float padding = 4.0f;
  const int num_buttons = 2;
  const float item_spacing = ImGui::GetStyle().ItemSpacing.x;
  
  float total_width = (num_buttons * button_size) + 
                      ((num_buttons - 1) * item_spacing) + 
                      (padding * 2);
  
  ImGui::BeginChild("##ModeGroup", ImVec2(total_width, button_size + padding), 
                   ImGuiChildFlags_AlwaysUseWindowPadding,
                   ImGuiWindowFlags_NoScrollbar);
  
  // Store for button sizing
  mode_group_button_size_ = button_size;
}

bool Toolset::ModeButton(const char* icon, bool selected, const char* tooltip) {
  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, GetAccentColor());
  }
  
  // Use smaller buttons that fit the toolbar height
  float size = mode_group_button_size_ > 0 ? mode_group_button_size_ : 28.0f;
  bool clicked = ImGui::Button(icon, ImVec2(size, size));
  
  if (selected) {
    ImGui::PopStyleColor();
  }
  
  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  
  ImGui::SameLine();
  button_count_++;
  
  return clicked;
}

void Toolset::EndModeGroup() {
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::SameLine();
  AddSeparator();
}

void Toolset::AddSeparator() {
  // Use a proper separator that doesn't stretch
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();
}

void Toolset::AddRomBadge(uint8_t version, std::function<void()> on_upgrade) {
  RomVersionBadge(version == 0xFF ? "Vanilla" : 
                 absl::StrFormat("v%d", version).c_str(), 
                 version == 0xFF);
  
  if (on_upgrade && (version == 0xFF || version < 3)) {
    ImGui::SameLine(0, 2);  // Tighter spacing
    if (ImGui::SmallButton(ICON_MD_UPGRADE)) {
      on_upgrade();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Upgrade to ZSCustomOverworld v3");
    }
  }
  
  ImGui::SameLine();
}

bool Toolset::AddProperty(const char* icon, const char* label,
                          uint8_t* value,
                          std::function<void()> on_change) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(55);
  
  bool changed = InputHexByte(label, value);
  if (changed && on_change) {
    on_change();
  }
  
  ImGui::SameLine();
  return changed;
}

bool Toolset::AddProperty(const char* icon, const char* label,
                          uint16_t* value,
                          std::function<void()> on_change) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(70);
  
  bool changed = InputHexWord(label, value);
  if (changed && on_change) {
    on_change();
  }
  
  ImGui::SameLine();
  return changed;
}

bool Toolset::AddCombo(const char* icon, int* current,
                       const char* const items[], int count) {
  ImGui::Text("%s", icon);
  ImGui::SameLine(0, 2);  // Reduce spacing between icon and combo
  ImGui::SetNextItemWidth(100);  // Slightly narrower for better fit
  
  bool changed = ImGui::Combo("##combo", current, items, count);
  ImGui::SameLine();
  
  return changed;
}

bool Toolset::AddToggle(const char* icon, bool* state, const char* tooltip) {
  bool result = ToggleIconButton(icon, icon, state, tooltip);
  ImGui::SameLine();
  return result;
}

bool Toolset::AddAction(const char* icon, const char* tooltip) {
  bool clicked = ImGui::SmallButton(icon);
  
  // Register for test automation
  if (ImGui::GetItemID() != 0 && tooltip) {
    std::string button_path = absl::StrFormat("ToolbarAction:%s", tooltip);
    WidgetIdRegistry::Instance().RegisterWidget(
        button_path, "button", ImGui::GetItemID(), tooltip);
  }
  
  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  
  ImGui::SameLine();
  return clicked;
}

bool Toolset::BeginCollapsibleSection(const char* label, bool* p_open) {
  ImGui::NewLine();  // Start on new line
  bool is_open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_None);
  if (p_open) *p_open = is_open;
  in_section_ = is_open;
  return is_open;
}

void Toolset::EndCollapsibleSection() {
  in_section_ = false;
}

void Toolset::AddV3StatusBadge(uint8_t version, std::function<void()> on_settings) {
  if (version >= 3 && version != 0xFF) {
    StatusBadge("v3 Active", ButtonType::Success);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_TUNE " Settings") && on_settings) {
      on_settings();
    }
  } else {
    StatusBadge("v3 Available", ButtonType::Default);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_UPGRADE " Upgrade")) {
      ImGui::OpenPopup("UpgradeROMVersion");
    }
  }
  ImGui::SameLine();
}

bool Toolset::AddUsageStatsButton(const char* tooltip) {
  bool clicked = ImGui::SmallButton(ICON_MD_ANALYTICS " Usage");
  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  ImGui::SameLine();
  return clicked;
}

// ============================================================================
// EditorCard Implementation
// ============================================================================

EditorCard::EditorCard(const char* title, const char* icon)
    : title_(title), icon_(icon ? icon : ""), default_size_(400, 300) {
  window_name_ = icon_.empty() ? title_ : icon_ + " " + title_;
}

EditorCard::EditorCard(const char* title, const char* icon, bool* p_open)
    : title_(title), icon_(icon ? icon : ""), default_size_(400, 300) {
  p_open_ = p_open;
  window_name_ = icon_.empty() ? title_ : icon_ + " " + title_;
}

void EditorCard::SetDefaultSize(float width, float height) {
  default_size_ = ImVec2(width, height);
}

void EditorCard::SetPosition(Position pos) {
  position_ = pos;
}

bool EditorCard::Begin(bool* p_open) {
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;
  
  // Set initial position based on position enum
  if (first_draw_) {
    float display_width = ImGui::GetIO().DisplaySize.x;
    float display_height = ImGui::GetIO().DisplaySize.y;
    
    switch (position_) {
      case Position::Right:
        ImGui::SetNextWindowPos(ImVec2(display_width - default_size_.x - 10, 30),
                               ImGuiCond_FirstUseEver);
        break;
      case Position::Left:
        ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
        break;
      case Position::Bottom:
        ImGui::SetNextWindowPos(
          ImVec2(10, display_height - default_size_.y - 10),
          ImGuiCond_FirstUseEver);
        break;
      case Position::Floating:
      case Position::Free:
        ImGui::SetNextWindowPos(
          ImVec2(display_width * 0.5f - default_size_.x * 0.5f, 
                 display_height * 0.3f),
          ImGuiCond_FirstUseEver);
        break;
    }
    
    ImGui::SetNextWindowSize(default_size_, ImGuiCond_FirstUseEver);
    first_draw_ = false;
  }
  
  // Create window title with icon
  std::string window_title = icon_.empty() ? title_ : icon_ + " " + title_;
  
  // Modern card styling
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, GetThemeColor(ImGuiCol_TitleBg));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, GetAccentColor());
  
  bool visible = ImGui::Begin(window_title.c_str(), p_open, flags);
  
  // Register card window for test automation
  if (ImGui::GetCurrentWindow() && ImGui::GetCurrentWindow()->ID != 0) {
    std::string card_path = absl::StrFormat("EditorCard:%s", title_.c_str());
    WidgetIdRegistry::Instance().RegisterWidget(
        card_path, "window", ImGui::GetCurrentWindow()->ID, 
        absl::StrFormat("Editor card: %s", title_.c_str()));
  }
  
  return visible;
}

void EditorCard::End() {
  // Check if window was focused this frame
  focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
  
  ImGui::End();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(2);
}

void EditorCard::Focus() {
  // Set window focus using ImGui's focus system
  ImGui::SetWindowFocus(window_name_.c_str());
  focused_ = true;
}

// ============================================================================
// EditorLayout Implementation
// ============================================================================

void EditorLayout::Begin() {
  toolbar_.Begin();
  in_layout_ = true;
}

void EditorLayout::End() {
  toolbar_.End();
  in_layout_ = false;
}

void EditorLayout::BeginMainCanvas() {
  // Main canvas takes remaining space
  ImGui::BeginChild("##MainCanvas", ImVec2(0, 0), false);
}

void EditorLayout::EndMainCanvas() {
  ImGui::EndChild();
}

void EditorLayout::RegisterCard(EditorCard* card) {
  cards_.push_back(card);
}

}  // namespace gui
}  // namespace yaze

