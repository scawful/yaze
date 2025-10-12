#include "app/gui/themed_widgets.h"

namespace yaze {
namespace gui {
namespace themed {

// ============================================================================
// Buttons
// ============================================================================

bool Button(const char* label, const ImVec2& size) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));

  bool result = ImGui::Button(label, size);

  ImGui::PopStyleColor(3);
  return result;
}

bool IconButton(const char* icon, const char* tooltip) {
  bool result = Button(icon, ImVec2(LayoutHelpers::GetStandardWidgetHeight(),
                                     LayoutHelpers::GetStandardWidgetHeight()));
  if (tooltip && ImGui::IsItemHovered()) {
    BeginTooltip();
    ImGui::Text("%s", tooltip);
    EndTooltip();
  }
  return result;
}

bool PrimaryButton(const char* label, const ImVec2& size) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.accent));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                       ImVec4(theme.accent.red * 1.2f, theme.accent.green * 1.2f,
                             theme.accent.blue * 1.2f, theme.accent.alpha));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                       ImVec4(theme.accent.red * 0.8f, theme.accent.green * 0.8f,
                             theme.accent.blue * 0.8f, theme.accent.alpha));

  bool result = ImGui::Button(label, size);

  ImGui::PopStyleColor(3);
  return result;
}

bool DangerButton(const char* label, const ImVec2& size) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.error));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                       ImVec4(theme.error.red * 1.2f, theme.error.green * 1.2f,
                             theme.error.blue * 1.2f, theme.error.alpha));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                       ImVec4(theme.error.red * 0.8f, theme.error.green * 0.8f,
                             theme.error.blue * 0.8f, theme.error.alpha));

  bool result = ImGui::Button(label, size);

  ImGui::PopStyleColor(3);
  return result;
}

// ============================================================================
// Headers & Sections
// ============================================================================

void Header(const char* label) {
  LayoutHelpers::SectionHeader(label);
}

bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_Header, ConvertColorToImVec4(theme.header));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ConvertColorToImVec4(theme.header_hovered));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ConvertColorToImVec4(theme.header_active));

  bool result = ImGui::CollapsingHeader(label, flags);

  ImGui::PopStyleColor(3);
  return result;
}

// ============================================================================
// Cards & Panels
// ============================================================================

void Card(const char* label, std::function<void()> content, const ImVec2& size) {
  BeginPanel(label, size);
  content();
  EndPanel();
}

void BeginPanel(const char* label, const ImVec2& size) {
  const auto& theme = GetTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.surface));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.window_rounding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                     ImVec2(LayoutHelpers::GetPanelPadding(),
                           LayoutHelpers::GetPanelPadding()));

  ImGui::BeginChild(label, size, true);
}

void EndPanel() {
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(1);
}

// ============================================================================
// Inputs
// ============================================================================

bool InputText(const char* label, char* buf, size_t buf_size,
               ImGuiInputTextFlags flags) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ConvertColorToImVec4(theme.frame_bg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ConvertColorToImVec4(theme.frame_bg_hovered));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ConvertColorToImVec4(theme.frame_bg_active));

  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  bool result = ImGui::InputText(label, buf, buf_size, flags);

  ImGui::PopStyleColor(3);
  return result;
}

bool InputInt(const char* label, int* v, int step, int step_fast,
              ImGuiInputTextFlags flags) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ConvertColorToImVec4(theme.frame_bg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ConvertColorToImVec4(theme.frame_bg_hovered));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ConvertColorToImVec4(theme.frame_bg_active));

  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  bool result = ImGui::InputInt(label, v, step, step_fast, flags);

  ImGui::PopStyleColor(3);
  return result;
}

bool InputFloat(const char* label, float* v, float step, float step_fast,
                const char* format, ImGuiInputTextFlags flags) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ConvertColorToImVec4(theme.frame_bg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ConvertColorToImVec4(theme.frame_bg_hovered));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ConvertColorToImVec4(theme.frame_bg_active));

  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  bool result = ImGui::InputFloat(label, v, step, step_fast, format, flags);

  ImGui::PopStyleColor(3);
  return result;
}

bool Checkbox(const char* label, bool* v) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_CheckMark, ConvertColorToImVec4(theme.check_mark));

  bool result = ImGui::Checkbox(label, v);

  ImGui::PopStyleColor(1);
  return result;
}

bool Combo(const char* label, int* current_item, const char* const items[],
           int items_count, int popup_max_height_in_items) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ConvertColorToImVec4(theme.frame_bg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ConvertColorToImVec4(theme.frame_bg_hovered));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ConvertColorToImVec4(theme.frame_bg_active));

  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  bool result = ImGui::Combo(label, current_item, items, items_count,
                             popup_max_height_in_items);

  ImGui::PopStyleColor(3);
  return result;
}

// ============================================================================
// Tables
// ============================================================================

bool BeginTable(const char* str_id, int columns, ImGuiTableFlags flags,
                const ImVec2& outer_size, float inner_width) {
  return LayoutHelpers::BeginTableWithTheming(str_id, columns, flags, outer_size, inner_width);
}

void EndTable() {
  LayoutHelpers::EndTable();
}

// ============================================================================
// Tooltips & Help
// ============================================================================

void HelpMarker(const char* desc) {
  LayoutHelpers::HelpMarker(desc);
}

void BeginTooltip() {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ConvertColorToImVec4(theme.popup_bg));
  ImGui::BeginTooltip();
}

void EndTooltip() {
  ImGui::EndTooltip();
  ImGui::PopStyleColor(1);
}

// ============================================================================
// Status & Feedback
// ============================================================================

void StatusText(const char* text, StatusType type) {
  const auto& theme = GetTheme();
  ImVec4 color;

  switch (type) {
    case StatusType::kSuccess:
      color = ConvertColorToImVec4(theme.success);
      break;
    case StatusType::kWarning:
      color = ConvertColorToImVec4(theme.warning);
      break;
    case StatusType::kError:
      color = ConvertColorToImVec4(theme.error);
      break;
    case StatusType::kInfo:
      color = ConvertColorToImVec4(theme.info);
      break;
  }

  ImGui::TextColored(color, "%s", text);
}

void ProgressBar(float fraction, const ImVec2& size, const char* overlay) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ConvertColorToImVec4(theme.accent));

  ImGui::ProgressBar(fraction, size, overlay);

  ImGui::PopStyleColor(1);
}

// ============================================================================
// Utility
// ============================================================================

void PushWidgetColors() {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ConvertColorToImVec4(theme.frame_bg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ConvertColorToImVec4(theme.frame_bg_hovered));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ConvertColorToImVec4(theme.frame_bg_active));
  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
}

void PopWidgetColors() {
  ImGui::PopStyleColor(6);
}

}  // namespace themed
}  // namespace gui
}  // namespace yaze
