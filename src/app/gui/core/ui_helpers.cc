#include "app/gui/core/ui_helpers.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

// ============================================================================
// Theme and Semantic Colors
// ============================================================================

ImVec4 GetThemeColor(ImGuiCol idx) { return ImGui::GetStyle().Colors[idx]; }

ImVec4 GetSuccessColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.success);
}

ImVec4 GetWarningColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.warning);
}

ImVec4 GetErrorColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.error);
}

ImVec4 GetInfoColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.info);
}

ImVec4 GetAccentColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.accent);
}

// Entity/Map marker colors (vibrant with good visibility)
ImVec4 GetEntranceColor() {
  // Bright yellow with strong visibility
  return ImVec4(1.0f, 0.9f, 0.0f, 0.85f);  // Yellow-gold, high visibility
}

ImVec4 GetExitColor() {
  // Bright cyan-white for contrast
  return ImVec4(0.9f, 1.0f, 1.0f, 0.85f);  // Cyan-white, high visibility
}

ImVec4 GetItemColor() {
  // Vibrant red for items
  return ImVec4(1.0f, 0.2f, 0.2f, 0.85f);  // Bright red, high visibility
}

ImVec4 GetSpriteColor() {
  // Bright magenta for sprites
  return ImVec4(1.0f, 0.3f, 1.0f, 0.85f);  // Bright magenta, high visibility
}

ImVec4 GetSelectedColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.accent);
}

ImVec4 GetLockedColor() {
  return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange for locked items
}

// Status colors
ImVec4 GetVanillaRomColor() {
  return GetWarningColor();  // Yellow from theme
}

ImVec4 GetCustomRomColor() {
  return GetSuccessColor();  // Green from theme
}

ImVec4 GetModifiedColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.accent);
}

// ============================================================================
// Layout Helpers
// ============================================================================

void BeginField(const char* label, float label_width) {
  ImGui::BeginGroup();
  if (label_width > 0.0f) {
    ImGui::Text("%s:", label);
    ImGui::SameLine(label_width);
  } else {
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
  }
  ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
}

void EndField() {
  ImGui::PopItemWidth();
  ImGui::EndGroup();
}

bool BeginPropertyTable(const char* id, int columns,
                        ImGuiTableFlags extra_flags) {
  ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | extra_flags;
  if (ImGui::BeginTable(id, columns, flags)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
  }
  return false;
}

void EndPropertyTable() { ImGui::EndTable(); }

void PropertyRow(const char* label, const char* value) {
  ImGui::TableNextColumn();
  ImGui::Text("%s", label);
  ImGui::TableNextColumn();
  ImGui::TextWrapped("%s", value);
}

void PropertyRow(const char* label, int value) {
  ImGui::TableNextColumn();
  ImGui::Text("%s", label);
  ImGui::TableNextColumn();
  ImGui::Text("%d", value);
}

void PropertyRowHex(const char* label, uint8_t value) {
  ImGui::TableNextColumn();
  ImGui::Text("%s", label);
  ImGui::TableNextColumn();
  ImGui::Text("0x%02X", value);
}

void PropertyRowHex(const char* label, uint16_t value) {
  ImGui::TableNextColumn();
  ImGui::Text("%s", label);
  ImGui::TableNextColumn();
  ImGui::Text("0x%04X", value);
}

void SectionHeader(const char* icon, const char* label, const ImVec4& color) {
  ImGui::TextColored(color, "%s %s", icon, label);
  ImGui::Separator();
}

// ============================================================================
// Common Widget Patterns
// ============================================================================

bool IconButton(const char* icon, const char* label, const ImVec2& size) {
  std::string button_text = std::string(icon) + " " + std::string(label);
  return ImGui::Button(button_text.c_str(), size);
}

bool ColoredButton(const char* label, ButtonType type, const ImVec2& size) {
  ImVec4 color;
  switch (type) {
    case ButtonType::Success:
      color = GetSuccessColor();
      break;
    case ButtonType::Warning:
      color = GetWarningColor();
      break;
    case ButtonType::Error:
      color = GetErrorColor();
      break;
    case ButtonType::Info:
      color = GetInfoColor();
      break;
    default:
      color = GetThemeColor(ImGuiCol_Button);
      break;
  }

  ImGui::PushStyleColor(ImGuiCol_Button, color);
  ImGui::PushStyleColor(
      ImGuiCol_ButtonHovered,
      ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, color.w));
  ImGui::PushStyleColor(
      ImGuiCol_ButtonActive,
      ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, color.w));

  bool result = ImGui::Button(label, size);

  ImGui::PopStyleColor(3);
  return result;
}

bool ToggleIconButton(const char* icon_on, const char* icon_off, bool* state,
                      const char* tooltip) {
  const char* icon = *state ? icon_on : icon_off;
  ImVec4 color = *state ? GetSuccessColor() : GetThemeColor(ImGuiCol_Button);

  ImGui::PushStyleColor(ImGuiCol_Button, color);
  bool result = ImGui::SmallButton(icon);
  ImGui::PopStyleColor();

  if (result) *state = !*state;

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return result;
}

void HelpMarker(const char* desc) {
  ImGui::TextDisabled(ICON_MD_HELP_OUTLINE);
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void SeparatorText(const char* label) { ImGui::SeparatorText(label); }

void StatusBadge(const char* text, ButtonType type) {
  ImVec4 color;
  switch (type) {
    case ButtonType::Success:
      color = GetSuccessColor();
      break;
    case ButtonType::Warning:
      color = GetWarningColor();
      break;
    case ButtonType::Error:
      color = GetErrorColor();
      break;
    case ButtonType::Info:
      color = GetInfoColor();
      break;
    default:
      color = GetThemeColor(ImGuiCol_Text);
      break;
  }

  ImGui::PushStyleColor(ImGuiCol_Button, color);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 3));
  ImGui::SmallButton(text);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

// ============================================================================
// Editor-Specific Patterns
// ============================================================================

void BeginToolset(const char* id) {
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 2));
  ImGui::BeginTable(id, 32, ImGuiTableFlags_SizingFixedFit);
}

void EndToolset() {
  ImGui::EndTable();
  ImGui::PopStyleVar();
}

void ToolsetButton(const char* icon, bool selected, const char* tooltip,
                   std::function<void()> on_click) {
  ImGui::TableNextColumn();

  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, GetAccentColor());
  }

  if (ImGui::Button(icon)) {
    if (on_click) on_click();
  }

  if (selected) {
    ImGui::PopStyleColor();
  }

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
}

void BeginCanvasContainer(const char* id, bool scrollable) {
  ImGuiWindowFlags flags = scrollable ? ImGuiWindowFlags_AlwaysVerticalScrollbar
                                      : ImGuiWindowFlags_None;
  ImGui::BeginChild(id, ImVec2(0, 0), true, flags);
}

void EndCanvasContainer() { ImGui::EndChild(); }

bool EditorTabItem(const char* icon, const char* label, bool* p_open) {
  char tab_label[256];
  snprintf(tab_label, sizeof(tab_label), "%s %s", icon, label);
  return ImGui::BeginTabItem(tab_label, p_open);
}

bool ConfirmationDialog(const char* id, const char* title, const char* message,
                        const char* confirm_text, const char* cancel_text) {
  bool confirmed = false;

  if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s", title);
    ImGui::Separator();
    ImGui::TextWrapped("%s", message);
    ImGui::Separator();

    if (ColoredButton(confirm_text, ButtonType::Warning, ImVec2(120, 0))) {
      confirmed = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(cancel_text, ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return confirmed;
}

// ============================================================================
// Visual Indicators
// ============================================================================

void StatusIndicator(const char* label, bool active, const char* tooltip) {
  ImVec4 color =
      active ? GetSuccessColor() : GetThemeColor(ImGuiCol_TextDisabled);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  float radius = 5.0f;

  pos.x += radius + 3;
  pos.y += ImGui::GetTextLineHeight() * 0.5f;

  draw_list->AddCircleFilled(pos, radius, ImGui::GetColorU32(color));

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2 + 8);
  ImGui::Text("%s", label);

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
}

void RomVersionBadge(const char* version, bool is_vanilla) {
  ImVec4 color = is_vanilla ? GetWarningColor() : GetSuccessColor();
  const char* icon = is_vanilla ? ICON_MD_INFO : ICON_MD_CHECK_CIRCLE;

  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::Text("%s %s", icon, version);
  ImGui::PopStyleColor();
}

void LockIndicator(bool locked, const char* label) {
  if (locked) {
    ImGui::TextColored(GetLockedColor(), ICON_MD_LOCK " %s (Locked)", label);
  } else {
    ImGui::Text(ICON_MD_LOCK_OPEN " %s", label);
  }
}

// ============================================================================
// Spacing and Alignment
// ============================================================================

void VerticalSpacing(float pixels) { ImGui::Dummy(ImVec2(0, pixels)); }

void HorizontalSpacing(float pixels) {
  ImGui::Dummy(ImVec2(pixels, 0));
  ImGui::SameLine();
}

void CenterText(const char* text) {
  float text_width = ImGui::CalcTextSize(text).x;
  ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - text_width) * 0.5f);
  ImGui::Text("%s", text);
}

void RightAlign(float width) {
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       ImGui::GetContentRegionAvail().x - width);
}

// ============================================================================
// Animation Helpers
// ============================================================================

float GetPulseAlpha(float speed) {
  return 0.5f +
         0.5f * sinf(static_cast<float>(ImGui::GetTime()) * speed * 2.0f);
}

float GetFadeIn(float duration) {
  static double start_time = 0.0;
  double current_time = ImGui::GetTime();

  if (start_time == 0.0) {
    start_time = current_time;
  }

  float elapsed = static_cast<float>(current_time - start_time);
  float alpha = ImClamp(elapsed / duration, 0.0f, 1.0f);

  // Reset after complete
  if (alpha >= 1.0f) {
    start_time = 0.0;
  }

  return alpha;
}

void PushPulseEffect(float speed) {
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                      ImGui::GetStyle().Alpha * GetPulseAlpha(speed));
}

void PopPulseEffect() { ImGui::PopStyleVar(); }

void LoadingSpinner(const char* label, float radius) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  pos.x += radius + 4;
  pos.y += radius + 4;

  const float rotation = static_cast<float>(ImGui::GetTime()) * 3.0f;
  const int segments = 16;
  const float thickness = 3.0f;

  const float start_angle = rotation;
  const float end_angle = rotation + IM_PI * 1.5f;

  draw_list->PathArcTo(pos, radius, start_angle, end_angle, segments);
  draw_list->PathStroke(ImGui::GetColorU32(GetAccentColor()), 0, thickness);

  if (label) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + radius * 2 + 8);
    ImGui::Text("%s", label);
  } else {
    ImGui::Dummy(ImVec2(radius * 2 + 8, radius * 2 + 8));
  }
}

// ============================================================================
// Responsive Layout Helpers
// ============================================================================

float GetResponsiveWidth(float min_width, float max_width, float ratio) {
  float available = ImGui::GetContentRegionAvail().x;
  float target = available * ratio;

  if (target < min_width) return min_width;
  if (target > max_width) return max_width;
  return target;
}

void SetupResponsiveColumns(int count, float min_col_width) {
  float available = ImGui::GetContentRegionAvail().x;
  float col_width = available / count;

  if (col_width < min_col_width) {
    col_width = min_col_width;
  }

  for (int i = 0; i < count; ++i) {
    ImGui::TableSetupColumn("##col", ImGuiTableColumnFlags_WidthFixed,
                            col_width);
  }
}

static int g_two_col_table_active = 0;

void BeginTwoColumns(const char* id, float split_ratio) {
  ImGuiTableFlags flags = ImGuiTableFlags_Resizable |
                          ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable(id, 2, flags)) {
    float available = ImGui::GetContentRegionAvail().x;
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_None,
                            available * split_ratio);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_None,
                            available * (1.0f - split_ratio));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    g_two_col_table_active++;
  }
}

void SwitchColumn() { ImGui::TableNextColumn(); }

void EndTwoColumns() {
  ImGui::EndTable();
  g_two_col_table_active--;
}

// ============================================================================
// Input Helpers
// ============================================================================

bool LabeledInputHex(const char* label, uint8_t* value) {
  BeginField(label);
  ImGui::PushItemWidth(60);
  bool changed =
      ImGui::InputScalar("##hex", ImGuiDataType_U8, value, nullptr, nullptr,
                         "%02X", ImGuiInputTextFlags_CharsHexadecimal);
  ImGui::PopItemWidth();
  EndField();
  return changed;
}

bool LabeledInputHex(const char* label, uint16_t* value) {
  BeginField(label);
  ImGui::PushItemWidth(80);
  bool changed =
      ImGui::InputScalar("##hex", ImGuiDataType_U16, value, nullptr, nullptr,
                         "%04X", ImGuiInputTextFlags_CharsHexadecimal);
  ImGui::PopItemWidth();
  EndField();
  return changed;
}

bool IconCombo(const char* icon, const char* label, int* current,
               const char* const items[], int count) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  return ImGui::Combo(label, current, items, count);
}

}  // namespace gui
}  // namespace yaze
