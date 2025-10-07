#include "palette_editor_widget.h"

#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

void PaletteEditorWidget::Initialize(Rom* rom) {
  rom_ = rom;
  current_palette_id_ = 0;
  selected_color_index_ = -1;
}

void PaletteEditorWidget::Draw() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ROM not loaded");
    return;
  }
  
  ImGui::BeginGroup();
  
  // Palette selector dropdown
  DrawPaletteSelector();
  
  ImGui::Separator();
  
  // Color grid display
  DrawColorGrid();
  
  ImGui::Separator();
  
  // Color picker for selected color
  if (selected_color_index_ >= 0) {
    DrawColorPicker();
  } else {
    ImGui::TextDisabled("Select a color to edit");
  }
  
  ImGui::EndGroup();
}

void PaletteEditorWidget::DrawPaletteSelector() {
  auto& dungeon_pal_group = rom_->mutable_palette_group()->dungeon_main;
  int num_palettes = dungeon_pal_group.size();
  
  ImGui::Text("Dungeon Palette:");
  ImGui::SameLine();
  
  if (ImGui::BeginCombo("##PaletteSelect", 
                        absl::StrFormat("Palette %d", current_palette_id_).c_str())) {
    for (int i = 0; i < num_palettes; i++) {
      bool is_selected = (current_palette_id_ == i);
      if (ImGui::Selectable(absl::StrFormat("Palette %d", i).c_str(), is_selected)) {
        current_palette_id_ = i;
        selected_color_index_ = -1;  // Reset color selection
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

void PaletteEditorWidget::DrawColorGrid() {
  auto& dungeon_pal_group = rom_->mutable_palette_group()->dungeon_main;
  
  if (current_palette_id_ < 0 || current_palette_id_ >= (int)dungeon_pal_group.size()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid palette ID");
    return;
  }
  
  auto palette = dungeon_pal_group[current_palette_id_];
  int num_colors = palette.size();
  
  ImGui::Text("Colors (%d):", num_colors);
  
  // Draw color grid (15 colors per row for good layout)
  const int colors_per_row = 15;
  const float color_button_size = 24.0f;
  
  for (int i = 0; i < num_colors; i++) {
    ImGui::PushID(i);
    
    // Get color as RGB (0-255)
    auto color = palette[i];
    ImVec4 col(color.rgb().x / 255.0f, 
               color.rgb().y / 255.0f,
               color.rgb().z / 255.0f,
               1.0f);
    
    // Color button
    bool is_selected = (i == selected_color_index_);
    if (is_selected) {
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 0, 1));  // Yellow border
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    }
    
    if (ImGui::ColorButton(absl::StrFormat("##color%d", i).c_str(), col,
                          ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker,
                          ImVec2(color_button_size, color_button_size))) {
      selected_color_index_ = i;
      editing_color_ = col;
    }
    
    if (is_selected) {
      ImGui::PopStyleVar();
      ImGui::PopStyleColor();
    }
    
    // Tooltip showing color index and SNES value
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Color %d\nSNES: 0x%04X\nRGB: (%d, %d, %d)", 
                       i, color.snes(),
                       (int)color.rgb().x, (int)color.rgb().y, (int)color.rgb().z);
    }
    
    // Layout: 15 per row
    if ((i + 1) % colors_per_row != 0 && i < num_colors - 1) {
      ImGui::SameLine();
    }
    
    ImGui::PopID();
  }
}

void PaletteEditorWidget::DrawColorPicker() {
  ImGui::SeparatorText(absl::StrFormat("Edit Color %d", selected_color_index_).c_str());
  
  auto& dungeon_pal_group = rom_->mutable_palette_group()->dungeon_main;
  auto palette = dungeon_pal_group[current_palette_id_];  // Get copy, not reference
  auto original_color = palette[selected_color_index_];
  
  // Color picker
  if (ImGui::ColorEdit3("Color", &editing_color_.x, 
                       ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel)) {
    // Convert ImGui color (0-1) to SNES color (0-31 per channel)
    int r = static_cast<int>(editing_color_.x * 31.0f);
    int g = static_cast<int>(editing_color_.y * 31.0f);
    int b = static_cast<int>(editing_color_.z * 31.0f);
    
    // Create SNES color (15-bit BGR555 format)
    uint16_t snes_color = (b << 10) | (g << 5) | r;
    
    // Update palette in ROM (need to write back through the group)
    palette[selected_color_index_] = gfx::SnesColor(snes_color);
    dungeon_pal_group[current_palette_id_] = palette;  // Write back
    
    // Notify that palette changed
    if (on_palette_changed_) {
      on_palette_changed_(current_palette_id_);
    }
  }
  
  // Show RGB values
  ImGui::Text("RGB (0-255): (%d, %d, %d)",
             (int)(editing_color_.x * 255),
             (int)(editing_color_.y * 255),
             (int)(editing_color_.z * 255));
  
  // Show SNES BGR555 value
  ImGui::Text("SNES BGR555: 0x%04X", original_color.snes());
  
  // Reset button
  if (ImGui::Button("Reset to Original")) {
    editing_color_ = ImVec4(original_color.rgb().x / 255.0f,
                           original_color.rgb().y / 255.0f,
                           original_color.rgb().z / 255.0f,
                           1.0f);
  }
}

}  // namespace gui
}  // namespace yaze

