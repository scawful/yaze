#include "app/gui/core/layout_helpers.h"

#include <algorithm>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IOS == 1
#include "app/platform/ios/ios_platform_state.h"
#endif

namespace yaze {
namespace gui {

// Core sizing functions
float LayoutHelpers::GetStandardWidgetHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.widget_height_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetStandardSpacing() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 0.5f * theme.spacing_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetToolbarHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.toolbar_height_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetPanelPadding() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 0.5f * theme.panel_padding_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetStandardInputWidth() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 8.0f * theme.input_width_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetButtonPadding() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 0.3f * theme.button_padding_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetTableRowHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.table_row_height_multiplier *
         theme.compact_factor;
}

float LayoutHelpers::GetCanvasToolbarHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.canvas_toolbar_multiplier *
         theme.compact_factor;
}

LayoutHelpers::SafeAreaInsets LayoutHelpers::GetSafeAreaInsets() {
  SafeAreaInsets insets{};
#if defined(__APPLE__) && TARGET_OS_IOS == 1
  const auto ios_insets = ::yaze::platform::ios::GetSafeAreaInsets();
  insets.left = ios_insets.left;
  insets.right = ios_insets.right;
  insets.top = ios_insets.top;
  insets.bottom = ios_insets.bottom;
#else
  const ImVec2 pad = ImGui::GetStyle().DisplaySafeAreaPadding;
  insets.left = pad.x;
  insets.right = pad.x;
  insets.top = pad.y;
  insets.bottom = pad.y;
#endif
  return insets;
}

float LayoutHelpers::GetTopInset() {
  float top = GetSafeAreaInsets().top;
#if defined(__APPLE__) && TARGET_OS_IOS == 1
  top = std::max(top, ::yaze::platform::ios::GetOverlayTopInset());
#endif
  return top;
}

bool LayoutHelpers::IsTouchDevice() {
  ImGuiIO& io = ImGui::GetIO();
  return (io.ConfigFlags & ImGuiConfigFlags_IsTouchScreen) != 0;
}

float LayoutHelpers::GetMinTouchTarget() {
  return IsTouchDevice() ? 44.0f : 0.0f;
}

float LayoutHelpers::GetTouchSafeWidgetHeight() {
  return std::max(GetStandardWidgetHeight(), GetMinTouchTarget());
}

LayoutHelpers::WindowClampResult LayoutHelpers::ClampWindowToRect(
    const ImVec2& pos, const ImVec2& size, const ImVec2& rect_pos,
    const ImVec2& rect_size, float min_visible) {
  WindowClampResult result{pos, false};

  if (rect_size.x <= 0.0f || rect_size.y <= 0.0f) {
    return result;
  }

  const float max_visible_x = std::max(1.0f, size.x - 1.0f);
  const float max_visible_y = std::max(1.0f, size.y - 1.0f);
  float min_visible_x = std::min(min_visible, max_visible_x);
  float min_visible_y = std::min(min_visible, max_visible_y);

  // Avoid impossible constraints on tiny viewports.
  min_visible_x = std::min(min_visible_x, rect_size.x * 0.5f);
  min_visible_y = std::min(min_visible_y, rect_size.y * 0.5f);

  const float min_x = rect_pos.x + min_visible_x - size.x;
  const float max_x = rect_pos.x + rect_size.x - min_visible_x;
  const float min_y = rect_pos.y + min_visible_y - size.y;
  const float max_y = rect_pos.y + rect_size.y - min_visible_y;

  result.pos.x = std::clamp(pos.x, min_x, max_x);
  result.pos.y = std::clamp(pos.y, min_y, max_y);
  result.clamped =
      (result.pos.x != pos.x) || (result.pos.y != pos.y);
  return result;
}

// Layout utilities
void LayoutHelpers::BeginPaddedPanel(const char* label, float padding) {
  if (padding < 0.0f) {
    padding = GetPanelPadding();
  }
  ImGui::BeginChild(label, ImVec2(0, 0), true);
  ImGui::Dummy(ImVec2(padding, padding));
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::Dummy(ImVec2(0, padding));
}

void LayoutHelpers::EndPaddedPanel() {
  ImGui::Dummy(ImVec2(0, GetPanelPadding()));
  ImGui::EndGroup();
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(GetPanelPadding(), 0));
  ImGui::EndChild();
}

bool LayoutHelpers::BeginTableWithTheming(const char* str_id, int columns,
                                          ImGuiTableFlags flags,
                                          const ImVec2& outer_size,
                                          float inner_width) {
  const auto& theme = GetTheme();

  // Apply theme colors to table
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,
                        ConvertColorToImVec4(theme.table_header_bg));
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong,
                        ConvertColorToImVec4(theme.table_border_strong));
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight,
                        ConvertColorToImVec4(theme.table_border_light));
  ImGui::PushStyleColor(ImGuiCol_TableRowBg,
                        ConvertColorToImVec4(theme.table_row_bg));
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,
                        ConvertColorToImVec4(theme.table_row_bg_alt));

  // Set row height if not overridden by caller
  if (!(flags & ImGuiTableFlags_NoHostExtendY)) {
    ImGui::PushStyleVar(
        ImGuiStyleVar_CellPadding,
        ImVec2(ImGui::GetStyle().CellPadding.x, GetTableRowHeight() * 0.25f));
  }

  return ImGui::BeginTable(str_id, columns, flags, outer_size, inner_width);
}

void LayoutHelpers::EndTableWithTheming() {
  ImGui::EndTable();
  // Pop style colors (5 colors pushed in BeginTableWithTheming)
  ImGui::PopStyleColor(5);
  // Pop style var if it was pushed (CellPadding)
  ImGui::PopStyleVar(1);
}

void LayoutHelpers::BeginCanvasPanel(const char* label, ImVec2* canvas_size) {
  const auto& theme = GetTheme();

  // Apply theme to canvas container
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ConvertColorToImVec4(theme.editor_background));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

  if (canvas_size) {
    ImGui::BeginChild(label, *canvas_size, true);
  } else {
    ImGui::BeginChild(label, ImVec2(0, 0), true);
  }
}

void LayoutHelpers::EndCanvasPanel() {
  ImGui::EndChild();
  ImGui::PopStyleVar(1);
  ImGui::PopStyleColor(1);
}

// Input field helpers
bool LayoutHelpers::AutoSizedInputField(const char* label, char* buf,
                                        size_t buf_size,
                                        ImGuiInputTextFlags flags) {
  ImGui::SetNextItemWidth(GetStandardInputWidth());
  return ImGui::InputText(label, buf, buf_size, flags);
}

bool LayoutHelpers::AutoSizedInputInt(const char* label, int* v, int step,
                                      int step_fast,
                                      ImGuiInputTextFlags flags) {
  ImGui::SetNextItemWidth(GetStandardInputWidth());
  return ImGui::InputInt(label, v, step, step_fast, flags);
}

bool LayoutHelpers::AutoSizedInputFloat(const char* label, float* v, float step,
                                        float step_fast, const char* format,
                                        ImGuiInputTextFlags flags) {
  ImGui::SetNextItemWidth(GetStandardInputWidth());
  return ImGui::InputFloat(label, v, step, step_fast, format, flags);
}

// Input preset functions for common patterns
bool LayoutHelpers::InputHexRow(const char* label, uint8_t* data) {
  const auto& theme = GetTheme();
  float input_width = GetStandardInputWidth() * 0.5f;  // Hex inputs are smaller

  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::SameLine();

  // Use theme-aware input width for hex byte (2 chars + controls)
  ImGui::SetNextItemWidth(input_width);

  char buf[8];
  snprintf(buf, sizeof(buf), "%02X", *data);

  bool changed = ImGui::InputText(
      ("##" + std::string(label)).c_str(), buf, sizeof(buf),
      ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);

  if (changed) {
    unsigned int temp;
    if (sscanf(buf, "%X", &temp) == 1) {
      *data = static_cast<uint8_t>(temp & 0xFF);
    }
  }

  return changed;
}

bool LayoutHelpers::InputHexRow(const char* label, uint16_t* data) {
  const auto& theme = GetTheme();
  float input_width =
      GetStandardInputWidth() * 0.6f;  // Hex word slightly wider

  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::SameLine();

  // Use theme-aware input width for hex word (4 chars + controls)
  ImGui::SetNextItemWidth(input_width);

  char buf[8];
  snprintf(buf, sizeof(buf), "%04X", *data);

  bool changed = ImGui::InputText(
      ("##" + std::string(label)).c_str(), buf, sizeof(buf),
      ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);

  if (changed) {
    unsigned int temp;
    if (sscanf(buf, "%X", &temp) == 1) {
      *data = static_cast<uint16_t>(temp & 0xFFFF);
    }
  }

  return changed;
}

void LayoutHelpers::BeginPropertyGrid(const char* label) {
  const auto& theme = GetTheme();

  // Create a 2-column table for property editing
  if (ImGui::BeginTable(label, 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    // Setup columns: label column (30%) and value column (70%)
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                            GetStandardInputWidth() * 1.5f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
  }
}

void LayoutHelpers::EndPropertyGrid() {
  ImGui::EndTable();
}

bool LayoutHelpers::InputToolbarField(const char* label, char* buf,
                                      size_t buf_size) {
  // Compact input field for toolbars
  float compact_width =
      GetStandardInputWidth() * 0.8f * GetTheme().compact_factor;
  ImGui::SetNextItemWidth(compact_width);

  return ImGui::InputText(label, buf, buf_size,
                          ImGuiInputTextFlags_AutoSelectAll);
}

// Toolbar helpers
void LayoutHelpers::BeginToolbar(const char* label) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ConvertColorToImVec4(theme.menu_bar_bg));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(GetButtonPadding(), GetButtonPadding()));

  // Ensure the toolbar is tall enough to fit standard widgets without clipping,
  // especially at higher DPI or with larger icon glyphs. Use touch-safe height
  // so iOS toolbars meet the 44px Apple HIG minimum.
  const float min_height =
      GetTouchSafeWidgetHeight() + (GetButtonPadding() * 2.0f) + 1.0f;
  const float height = std::max(GetToolbarHeight(), min_height);

  ImGui::BeginChild(
      label, ImVec2(0, height), true,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void LayoutHelpers::EndToolbar() {
  ImGui::EndChild();
  ImGui::PopStyleVar(1);
  ImGui::PopStyleColor(1);
}

void LayoutHelpers::ToolbarSeparator() {
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(GetStandardSpacing(), 0));
  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(GetStandardSpacing(), 0));
  ImGui::SameLine();
}

bool LayoutHelpers::ToolbarButton(const char* label, const ImVec2& size) {
  const float min_touch = GetMinTouchTarget();
  ImVec2 effective_size = size;
  if (min_touch > 0.0f) {
    effective_size.x = std::max(effective_size.x, min_touch);
    effective_size.y = std::max(effective_size.y, min_touch);
  }
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(GetButtonPadding(), GetButtonPadding()));
  bool result = ImGui::Button(label, effective_size);
  ImGui::PopStyleVar(1);
  return result;
}

// Common layout patterns
void LayoutHelpers::PropertyRow(const char* label,
                                std::function<void()> widget_callback) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::TableSetColumnIndex(1);
  ImGui::SetNextItemWidth(-1);
  widget_callback();
}

void LayoutHelpers::SectionHeader(const char* label) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_Text, ConvertColorToImVec4(theme.accent));
  ImGui::SeparatorText(label);
  ImGui::PopStyleColor(1);
}

void LayoutHelpers::HelpMarker(const char* desc) {
  ImGui::TextDisabled("(?)");
  if (ImGui::BeginItemTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

}  // namespace gui
}  // namespace yaze
