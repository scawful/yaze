#define IMGUI_DEFINE_MATH_OPERATORS

#include <algorithm>
#include <functional>

#include "app/gui/app/editor_layout.h"

#include "absl/strings/str_format.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/automation/widget_measurement.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

// ============================================================================
// PanelWindow Static Variables (for duplicate rendering detection)
// ============================================================================
int PanelWindow::last_frame_count_ = 0;
std::vector<std::string> PanelWindow::panels_begun_this_frame_;
bool PanelWindow::duplicate_detected_ = false;
std::string PanelWindow::duplicate_panel_name_;

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
                      ((num_buttons - 1) * item_spacing) + (padding * 2);

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
  RomVersionBadge(
      version == 0xFF ? "Vanilla" : absl::StrFormat("v%d", version).c_str(),
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

bool Toolset::AddProperty(const char* icon, const char* label, uint8_t* value,
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

bool Toolset::AddProperty(const char* icon, const char* label, uint16_t* value,
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
  ImGui::SameLine(0, 2);         // Reduce spacing between icon and combo
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
    WidgetIdRegistry::Instance().RegisterWidget(button_path, "button",
                                                ImGui::GetItemID(), tooltip);
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
  if (p_open)
    *p_open = is_open;
  in_section_ = is_open;
  return is_open;
}

void Toolset::EndCollapsibleSection() {
  in_section_ = false;
}

void Toolset::AddV3StatusBadge(uint8_t version,
                               std::function<void()> on_settings) {
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
// PanelWindow Implementation
// ============================================================================

PanelWindow::PanelWindow(const char* title, const char* icon)
    : title_(title), icon_(icon ? icon : ""), default_size_(400, 300) {
  window_name_ = icon_.empty() ? title_ : icon_ + " " + title_;
}

PanelWindow::PanelWindow(const char* title, const char* icon, bool* p_open)
    : title_(title), icon_(icon ? icon : ""), default_size_(400, 300) {
  p_open_ = p_open;
  window_name_ = icon_.empty() ? title_ : icon_ + " " + title_;
}

void PanelWindow::SetDefaultSize(float width, float height) {
  default_size_ = ImVec2(width, height);
  default_size_set_ = true;
}

void PanelWindow::SetPosition(Position pos) {
  position_ = pos;
}

void PanelWindow::AddHeaderButton(const char* icon, const char* tooltip,
                                  std::function<void()> callback) {
  header_buttons_.push_back({icon, tooltip, callback});
}

bool PanelWindow::Begin(bool* p_open) {
  // Check visibility flag first - if provided and false, don't show the panel
  if (p_open && !*p_open) {
    imgui_begun_ = false;
    return false;
  }

  // === DEBUG: Track duplicate rendering ===
  int current_frame = ImGui::GetFrameCount();
  if (current_frame != last_frame_count_) {
    // New frame - reset tracking
    last_frame_count_ = current_frame;
    panels_begun_this_frame_.clear();
    duplicate_detected_ = false;
    duplicate_panel_name_.clear();
  }

  // Check if this panel was already begun this frame
  for (const auto& panel_name : panels_begun_this_frame_) {
    if (panel_name == window_name_) {
      duplicate_detected_ = true;
      duplicate_panel_name_ = window_name_;
      // Log the duplicate detection
      fprintf(stderr,
              "[PanelWindow] DUPLICATE DETECTED: '%s' Begin() called twice in "
              "frame %d\n",
              window_name_.c_str(), current_frame);
      break;
    }
  }
  panels_begun_this_frame_.push_back(window_name_);
  // === END DEBUG ===

  // Handle icon-collapsed state
  if (icon_collapsible_ && collapsed_to_icon_) {
    DrawFloatingIconButton();
    imgui_begun_ = false;
    return false;
  }

  ImGuiWindowFlags flags = ImGuiWindowFlags_None;

  // Apply headless mode
  if (headless_) {
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoCollapse;
  }

  // Control docking
  if (!docking_allowed_) {
    flags |= ImGuiWindowFlags_NoDocking;
  }

  // Prevent persisting window settings (position, size, docking state)
  if (!save_settings_) {
    flags |= ImGuiWindowFlags_NoSavedSettings;
  }

  // Set initial position based on position enum
  if (first_draw_) {
    float display_width = ImGui::GetIO().DisplaySize.x;
    float display_height = ImGui::GetIO().DisplaySize.y;
    ImVec2 initial_size = default_size_;
    ImVec2 start_offset = start_offset_;
    const bool touch_device = LayoutHelpers::IsTouchDevice();

    if (touch_device) {
      ImVec2 work_size = ImGui::GetMainViewport()
                             ? ImGui::GetMainViewport()->WorkSize
                             : ImVec2(display_width, display_height);
      ImVec2 min_size(work_size.x * 0.65f, work_size.y * 0.55f);
      ImVec2 max_size(work_size.x * 0.96f, work_size.y * 0.92f);
      ImVec2 desired = default_size_set_
                           ? ImVec2(default_size_.x * 1.2f,
                                    default_size_.y * 1.2f)
                           : ImVec2(work_size.x * 0.9f, work_size.y * 0.82f);
      initial_size.x = std::clamp(desired.x, min_size.x, max_size.x);
      initial_size.y = std::clamp(desired.y, min_size.y, max_size.y);

      if (!start_offset_set_) {
        const size_t hash = std::hash<std::string>{}(window_name_);
        const float step = 18.0f;
        start_offset.x += step * static_cast<float>(hash % 5);
        start_offset.y += step * static_cast<float>((hash / 5) % 5);
      }
    }

    switch (position_) {
      case Position::Right:
        ImGui::SetNextWindowPos(
            ImVec2(display_width - initial_size.x - 10.0f, 30.0f) +
                start_offset,
            ImGuiCond_FirstUseEver);
        break;
      case Position::Left:
        ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f) + start_offset,
                                ImGuiCond_FirstUseEver);
        break;
      case Position::Bottom:
        ImGui::SetNextWindowPos(
            ImVec2(10.0f, display_height - initial_size.y - 10.0f) +
                start_offset,
            ImGuiCond_FirstUseEver);
        break;
      case Position::Top:
        ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f) + start_offset,
                                ImGuiCond_FirstUseEver);
        break;
      case Position::Center:
        ImGui::SetNextWindowPos(
            ImVec2(display_width * 0.5f - initial_size.x * 0.5f,
                   display_height * 0.5f - initial_size.y * 0.5f) +
                start_offset,
            ImGuiCond_FirstUseEver);
        break;
      case Position::Floating:
      case Position::Free:
        ImGui::SetNextWindowPos(
            ImVec2(display_width * 0.5f - initial_size.x * 0.5f,
                   display_height * 0.3f) +
                start_offset,
            ImGuiCond_FirstUseEver);
        break;
    }

    ImGui::SetNextWindowSize(initial_size, ImGuiCond_FirstUseEver);
    first_draw_ = false;
  }

  // Create window title with icon
  std::string window_title = icon_.empty() ? title_ : icon_ + " " + title_;

  // Modern panel styling
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, GetThemeColor(ImGuiCol_TitleBg));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, GetAccentColor());

  // Use p_open parameter if provided, otherwise use stored p_open_
  bool* actual_p_open = p_open ? p_open : p_open_;

  // If closable is false, don't pass p_open (removes X button)
  bool visible = ImGui::Begin(window_title.c_str(),
                              closable_ ? actual_p_open : nullptr, flags);

  // Mark that ImGui::Begin() was called - End() must always be called now
  imgui_begun_ = true;

  // Draw custom header buttons if visible
  if (visible) {
    DrawHeaderButtons();
  }

  // Register panel window for test automation
  if (ImGui::GetCurrentWindow() && ImGui::GetCurrentWindow()->ID != 0) {
    std::string panel_path = absl::StrFormat("PanelWindow:%s", title_.c_str());
    WidgetIdRegistry::Instance().RegisterWidget(
        panel_path, "window", ImGui::GetCurrentWindow()->ID,
        absl::StrFormat("Editor panel: %s", title_.c_str()));
  }

  return visible;
}

void PanelWindow::End() {
  // Only call ImGui::End() and pop styles if ImGui::Begin() was called
  if (imgui_begun_) {
    // Check if window was focused this frame
    focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    imgui_begun_ = false;
  }
}

void PanelWindow::Focus() {
  // Set window focus using ImGui's focus system
  ImGui::SetWindowFocus(window_name_.c_str());
  focused_ = true;
}

void PanelWindow::DrawFloatingIconButton() {
  // Draw a small floating button with the icon
  ImGui::SetNextWindowPos(saved_icon_pos_, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(50, 50));

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

  std::string icon_window_name = window_name_ + "##IconCollapsed";

  if (ImGui::Begin(icon_window_name.c_str(), nullptr, flags)) {
    // Draw icon button
    if (ImGui::Button(icon_.c_str(), ImVec2(40, 40))) {
      collapsed_to_icon_ = false;  // Expand back to full window
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Expand %s", title_.c_str());
    }

    // Allow dragging the icon
    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
      saved_icon_pos_.x += mouse_delta.x;
      saved_icon_pos_.y += mouse_delta.y;
    }
  }
  ImGui::End();
}

void PanelWindow::DrawHeaderButtons() {
  // Note: Drawing buttons in docked window title bars is problematic with ImGui's
  // docking system. The pin functionality is better managed through the Activity Bar
  // sidebar where each panel entry can have a pin toggle. This avoids layout issues
  // with docked windows and provides a cleaner UI.
  //
  // For now, pin state is tracked internally but the button is not rendered.
  // Right-click context menu in Activity Bar can be used for pinning.

  // Skip drawing header buttons in content area - they interfere with docking
  // and take up vertical space. The pin state is still tracked and used by
  // PanelManager for category filtering.
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

void EditorLayout::RegisterPanel(PanelWindow* panel) {
  panels_.push_back(panel);
}

}  // namespace gui
}  // namespace yaze
