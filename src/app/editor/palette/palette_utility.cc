#include "palette_utility.h"

#include "absl/strings/str_format.h"
#include "app/editor/palette/palette_editor.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/icons.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
namespace palette_utility {

bool DrawPaletteJumpButton(const char* label, const std::string& group_name,
                           int palette_index, PaletteEditor* editor) {
  bool clicked = ImGui::SmallButton(
      absl::StrFormat("%s %s", ICON_MD_PALETTE, label).c_str());
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Jump to palette editor:\n%s - Palette %d", 
                     group_name.c_str(), palette_index);
  }
  
  if (clicked && editor) {
    editor->JumpToPalette(group_name, palette_index);
  }
  
  return clicked;
}

bool DrawInlineColorEdit(const char* label, gfx::SnesColor* color,
                         const std::string& group_name, int palette_index,
                         int color_index, PaletteEditor* editor) {
  ImGui::PushID(label);
  
  // Draw color button
  ImVec4 col = gui::ConvertSnesColorToImVec4(*color);
  bool changed = ImGui::ColorEdit4(label, &col.x, 
                                   ImGuiColorEditFlags_NoInputs | 
                                   ImGuiColorEditFlags_NoLabel);
  
  if (changed) {
    *color = gui::ConvertImVec4ToSnesColor(col);
  }
  
  // Draw jump button
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW)) {
    if (editor) {
      editor->JumpToPalette(group_name, palette_index);
    }
  }
  ImGui::PopStyleColor();
  
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Jump to Palette Editor");
    ImGui::TextDisabled("%s - Palette %d, Color %d", 
                       group_name.c_str(), palette_index, color_index);
    DrawColorInfoTooltip(*color);
    ImGui::EndTooltip();
  }
  
  ImGui::PopID();
  return changed;
}

bool DrawPaletteIdSelector(const char* label, int* palette_id,
                           const std::string& group_name,
                           PaletteEditor* editor) {
  ImGui::PushID(label);
  
  // Draw combo box
  bool changed = ImGui::InputInt(label, palette_id);
  
  // Clamp to valid range (0-255 typically)
  if (*palette_id < 0) *palette_id = 0;
  if (*palette_id > 255) *palette_id = 255;
  
  // Draw jump button
  ImGui::SameLine();
  if (DrawPaletteJumpButton("Jump", group_name, *palette_id, editor)) {
    // Button clicked, editor will handle jump
  }
  
  ImGui::PopID();
  return changed;
}

void DrawColorInfoTooltip(const gfx::SnesColor& color) {
  auto rgb = color.rgb();
  ImGui::Separator();
  ImGui::Text("RGB: (%d, %d, %d)",
              static_cast<int>(rgb.x),
              static_cast<int>(rgb.y),
              static_cast<int>(rgb.z));
  ImGui::Text("SNES: $%04X", color.snes());
  ImGui::Text("Hex: #%02X%02X%02X",
              static_cast<int>(rgb.x),
              static_cast<int>(rgb.y),
              static_cast<int>(rgb.z));
}

void DrawPalettePreview(const std::string& group_name, int palette_index,
                       Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    ImGui::TextDisabled("(ROM not loaded)");
    return;
  }
  
  auto* group = rom->mutable_palette_group()->get_group(group_name);
  if (!group || palette_index >= group->size()) {
    ImGui::TextDisabled("(Palette not found)");
    return;
  }
  
  auto palette = group->palette(palette_index);
  
  // Draw colors in a row
  int preview_size = std::min(8, static_cast<int>(palette.size()));
  for (int i = 0; i < preview_size; i++) {
    if (i > 0) ImGui::SameLine();
    
    ImGui::PushID(i);
    ImVec4 col = gui::ConvertSnesColorToImVec4(palette[i]);
    ImGui::ColorButton("##preview", col, 
                      ImGuiColorEditFlags_NoAlpha | 
                      ImGuiColorEditFlags_NoPicker |
                      ImGuiColorEditFlags_NoTooltip,
                      ImVec2(16, 16));
    
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Color %d", i);
      DrawColorInfoTooltip(palette[i]);
      ImGui::EndTooltip();
    }
    
    ImGui::PopID();
  }
}

}  // namespace palette_utility
}  // namespace editor
}  // namespace yaze

