#include "app/gui/layout_helpers.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "app/gui/theme_manager.h"
#include "app/gui/color.h"

namespace yaze {
namespace gui {

// Core sizing functions
float LayoutHelpers::GetStandardWidgetHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.widget_height_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetStandardSpacing() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 0.5f * theme.spacing_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetToolbarHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.toolbar_height_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetPanelPadding() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 0.5f * theme.panel_padding_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetStandardInputWidth() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 8.0f * theme.input_width_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetButtonPadding() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * 0.3f * theme.button_padding_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetTableRowHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.table_row_height_multiplier * theme.compact_factor;
}

float LayoutHelpers::GetCanvasToolbarHeight() {
  const auto& theme = GetTheme();
  return GetBaseFontSize() * theme.canvas_toolbar_multiplier * theme.compact_factor;
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
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ConvertColorToImVec4(theme.table_header_bg));
  ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ConvertColorToImVec4(theme.table_border_strong));
  ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ConvertColorToImVec4(theme.table_border_light));
  ImGui::PushStyleColor(ImGuiCol_TableRowBg, ConvertColorToImVec4(theme.table_row_bg));
  ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ConvertColorToImVec4(theme.table_row_bg_alt));

  // Set row height if not overridden by caller
  if (!(flags & ImGuiTableFlags_NoHostExtendY)) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                       ImVec2(ImGui::GetStyle().CellPadding.x, GetTableRowHeight() * 0.25f));
  }

  return ImGui::BeginTable(str_id, columns, flags, outer_size, inner_width);
}

void LayoutHelpers::BeginCanvasPanel(const char* label, ImVec2* canvas_size) {
  const auto& theme = GetTheme();

  // Apply theme to canvas container
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.editor_background));
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
                                        size_t buf_size, ImGuiInputTextFlags flags) {
  ImGui::SetNextItemWidth(GetStandardInputWidth());
  return ImGui::InputText(label, buf, buf_size, flags);
}

bool LayoutHelpers::AutoSizedInputInt(const char* label, int* v, int step,
                                      int step_fast, ImGuiInputTextFlags flags) {
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
  float input_width = GetStandardInputWidth() * 0.5f; // Hex inputs are smaller

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
  float input_width = GetStandardInputWidth() * 0.6f; // Hex word slightly wider

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

bool LayoutHelpers::InputToolbarField(const char* label, char* buf, size_t buf_size) {
  // Compact input field for toolbars
  float compact_width = GetStandardInputWidth() * 0.8f * GetTheme().compact_factor;
  ImGui::SetNextItemWidth(compact_width);

  return ImGui::InputText(label, buf, buf_size,
                         ImGuiInputTextFlags_AutoSelectAll);
}

// Toolbar helpers
void LayoutHelpers::BeginToolbar(const char* label) {
  const auto& theme = GetTheme();
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.menu_bar_bg));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                     ImVec2(GetButtonPadding(), GetButtonPadding()));
  ImGui::BeginChild(label, ImVec2(0, GetToolbarHeight()), true,
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
  const auto& theme = GetTheme();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                     ImVec2(GetButtonPadding(), GetButtonPadding()));
  bool result = ImGui::Button(label, size);
  ImGui::PopStyleVar(1);
  return result;
}

// Common layout patterns
void LayoutHelpers::PropertyRow(const char* label, std::function<void()> widget_callback) {
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

} // namespace gui
} // namespace yaze
