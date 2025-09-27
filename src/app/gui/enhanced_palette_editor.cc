#include "enhanced_palette_editor.h"

#include <algorithm>
#include <map>
#include "app/core/window.h"
#include "app/gui/color.h"
#include "util/log.h"

namespace yaze {
namespace gui {

using core::Renderer;

void EnhancedPaletteEditor::Initialize(Rom* rom) {
  rom_ = rom;
  rom_palettes_loaded_ = false;
  if (rom_) {
    LoadROMPalettes();
  }
}

void EnhancedPaletteEditor::ShowPaletteEditor(gfx::SnesPalette& palette, const std::string& title) {
  if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enhanced Palette Editor");
    ImGui::Separator();
    
    // Palette grid editor
    DrawPaletteGrid(palette);
    
    ImGui::Separator();
    
    // Analysis and tools
    if (ImGui::CollapsingHeader("Palette Analysis")) {
      DrawPaletteAnalysis(palette);
    }
    
    if (ImGui::CollapsingHeader("ROM Palette Manager") && rom_) {
      DrawROMPaletteSelector();
      
      if (ImGui::Button("Apply ROM Palette") && !rom_palette_groups_.empty()) {
        if (current_group_index_ < static_cast<int>(rom_palette_groups_.size())) {
          palette = rom_palette_groups_[current_group_index_];
        }
      }
    }
    
    ImGui::Separator();
    
    // Action buttons
    if (ImGui::Button("Save Backup")) {
      SavePaletteBackup(palette);
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Backup")) {
      RestorePaletteBackup(palette);
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    
    ImGui::EndPopup();
  }
}

void EnhancedPaletteEditor::ShowROMPaletteManager() {
  if (!show_rom_manager_) return;
  
  if (ImGui::Begin("ROM Palette Manager", &show_rom_manager_)) {
    if (!rom_) {
      ImGui::Text("No ROM loaded");
      ImGui::End();
      return;
    }
    
    if (!rom_palettes_loaded_) {
      LoadROMPalettes();
    }
    
    DrawROMPaletteSelector();
    
    if (current_group_index_ < static_cast<int>(rom_palette_groups_.size())) {
      ImGui::Separator();
      ImGui::Text("Preview of %s:", palette_group_names_[current_group_index_].c_str());
      
      const auto& preview_palette = rom_palette_groups_[current_group_index_];
      DrawPaletteGrid(const_cast<gfx::SnesPalette&>(preview_palette));
      
      DrawPaletteAnalysis(preview_palette);
    }
  }
  ImGui::End();
}

void EnhancedPaletteEditor::ShowColorAnalysis(const gfx::Bitmap& bitmap, const std::string& title) {
  if (!show_color_analysis_) return;
  
  if (ImGui::Begin(title.c_str(), &show_color_analysis_)) {
    ImGui::Text("Bitmap Color Analysis");
    ImGui::Separator();
    
    if (!bitmap.is_active()) {
      ImGui::Text("Bitmap is not active");
      ImGui::End();
      return;
    }
    
    // Analyze pixel distribution
    std::map<uint8_t, int> pixel_counts;
    const auto& data = bitmap.vector();
    
    for (uint8_t pixel : data) {
      uint8_t palette_index = pixel & 0x0F; // 4-bit palette index
      pixel_counts[palette_index]++;
    }
    
    ImGui::Text("Bitmap Size: %d x %d (%zu pixels)", 
               bitmap.width(), bitmap.height(), data.size());
    
    ImGui::Separator();
    ImGui::Text("Pixel Distribution:");
    
    // Show distribution as bars
    int total_pixels = static_cast<int>(data.size());
    for (const auto& [index, count] : pixel_counts) {
      float percentage = (static_cast<float>(count) / total_pixels) * 100.0f;
      ImGui::Text("Index %d: %d pixels (%.1f%%)", index, count, percentage);
      
      // Progress bar visualization
      ImGui::SameLine();
      ImGui::ProgressBar(percentage / 100.0f, ImVec2(100, 0));
      
      // Color swatch if palette is available
      if (index < static_cast<int>(bitmap.palette().size())) {
        ImGui::SameLine();
        auto color = bitmap.palette()[index];
        ImVec4 display_color = color.rgb();
        ImGui::ColorButton(("##color" + std::to_string(index)).c_str(), 
                          display_color, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("SNES Color: 0x%04X\nRGB: (%d, %d, %d)", 
                           color.snes(),
                           static_cast<int>(display_color.x * 255),
                           static_cast<int>(display_color.y * 255),
                           static_cast<int>(display_color.z * 255));
        }
      }
    }
  }
  ImGui::End();
}

bool EnhancedPaletteEditor::ApplyROMPalette(gfx::Bitmap* bitmap, int group_index, int palette_index) {
  if (!bitmap || !rom_palettes_loaded_ || 
      group_index < 0 || group_index >= static_cast<int>(rom_palette_groups_.size())) {
    return false;
  }
  
  try {
    const auto& selected_palette = rom_palette_groups_[group_index];
    
    // Save current palette as backup
    SavePaletteBackup(bitmap->palette());
    
    // Apply new palette
    if (palette_index >= 0 && palette_index < 8) {
      bitmap->SetPaletteWithTransparent(selected_palette, palette_index);
    } else {
      bitmap->SetPalette(selected_palette);
    }
    
    Renderer::Get().UpdateBitmap(bitmap);
    
    current_group_index_ = group_index;
    current_palette_index_ = palette_index;
    
    util::logf("Applied ROM palette: %s (index %d)", 
               palette_group_names_[group_index].c_str(), palette_index);
    return true;
    
  } catch (const std::exception& e) {
    util::logf("Failed to apply ROM palette: %s", e.what());
    return false;
  }
}

const gfx::SnesPalette* EnhancedPaletteEditor::GetSelectedROMPalette() const {
  if (!rom_palettes_loaded_ || current_group_index_ < 0 || 
      current_group_index_ >= static_cast<int>(rom_palette_groups_.size())) {
    return nullptr;
  }
  
  return &rom_palette_groups_[current_group_index_];
}

void EnhancedPaletteEditor::SavePaletteBackup(const gfx::SnesPalette& palette) {
  backup_palette_ = palette;
}

bool EnhancedPaletteEditor::RestorePaletteBackup(gfx::SnesPalette& palette) {
  if (backup_palette_.size() == 0) {
    return false;
  }
  
  palette = backup_palette_;
  return true;
}

void EnhancedPaletteEditor::DrawPaletteGrid(gfx::SnesPalette& palette, int cols) {
  for (int i = 0; i < static_cast<int>(palette.size()); i++) {
    if (i % cols != 0) ImGui::SameLine();
    
    auto color = palette[i];
    ImVec4 display_color = color.rgb();
    
    ImGui::PushID(i);
    
    // Color button with editing capability
    if (ImGui::ColorButton("##color", display_color, 
                          ImGuiColorEditFlags_NoTooltip, ImVec2(30, 30))) {
      editing_color_index_ = i;
      temp_color_ = display_color;
    }
    
    // Context menu for individual colors
    if (ImGui::BeginPopupContextItem()) {
      ImGui::Text("Color %d (0x%04X)", i, color.snes());
      ImGui::Separator();
      
      if (ImGui::MenuItem("Edit Color")) {
        editing_color_index_ = i;
        temp_color_ = display_color;
      }
      
      if (ImGui::MenuItem("Copy Color")) {
        // Could implement color clipboard here
      }
      
      if (ImGui::MenuItem("Reset to Black")) {
        palette[i] = gfx::SnesColor(0);
      }
      
      ImGui::EndPopup();
    }
    
    // Tooltip with detailed info
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Color %d\nSNES: 0x%04X\nRGB: (%d, %d, %d)\nClick to edit", 
                       i, color.snes(),
                       static_cast<int>(display_color.x * 255),
                       static_cast<int>(display_color.y * 255),
                       static_cast<int>(display_color.z * 255));
    }
    
    ImGui::PopID();
  }
  
  // Color editor popup
  if (editing_color_index_ >= 0) {
    ImGui::OpenPopup("Edit Color");
    
    if (ImGui::BeginPopupModal("Edit Color", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Editing Color %d", editing_color_index_);
      
      if (ImGui::ColorEdit4("Color", &temp_color_.x, 
                           ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayRGB)) {
        // Update the palette color in real-time
        auto new_snes_color = gfx::SnesColor(temp_color_);
        palette[editing_color_index_] = new_snes_color;
      }
      
      if (ImGui::Button("Apply")) {
        editing_color_index_ = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        editing_color_index_ = -1;
        ImGui::CloseCurrentPopup();
      }
      
      ImGui::EndPopup();
    }
  }
}

void EnhancedPaletteEditor::DrawROMPaletteSelector() {
  if (!rom_palettes_loaded_) {
    LoadROMPalettes();
  }
  
  if (rom_palette_groups_.empty()) {
    ImGui::Text("No ROM palettes available");
    return;
  }
  
  // Group selector
  ImGui::Text("Palette Group:");
  if (ImGui::Combo("##PaletteGroup", &current_group_index_, 
                   [](void* data, int idx, const char** out_text) -> bool {
                     auto* names = static_cast<std::vector<std::string>*>(data);
                     if (idx < 0 || idx >= static_cast<int>(names->size())) return false;
                     *out_text = (*names)[idx].c_str();
                     return true;
                   }, &palette_group_names_, static_cast<int>(palette_group_names_.size()))) {
    // Group changed - could trigger preview update
  }
  
  // Palette index selector
  ImGui::Text("Palette Index:");
  ImGui::SliderInt("##PaletteIndex", &current_palette_index_, 0, 7, "%d");
  
  // Quick palette preview
  if (current_group_index_ < static_cast<int>(rom_palette_groups_.size())) {
    ImGui::Text("Preview:");
    const auto& preview_palette = rom_palette_groups_[current_group_index_];
    
    // Show just first 8 colors in a row
    for (int i = 0; i < 8 && i < static_cast<int>(preview_palette.size()); i++) {
      if (i > 0) ImGui::SameLine();
      auto color = preview_palette[i];
      ImVec4 display_color = color.rgb();
      ImGui::ColorButton(("##preview" + std::to_string(i)).c_str(), 
                        display_color, ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
    }
  }
}

void EnhancedPaletteEditor::DrawColorEditControls(gfx::SnesColor& color, int color_index) {
  ImVec4 rgba = color.rgb();
  
  ImGui::PushID(color_index);
  
  if (ImGui::ColorEdit4("##color_edit", &rgba.x, 
                       ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayRGB)) {
    color = gfx::SnesColor(rgba);
  }
  
  // SNES-specific controls
  ImGui::Text("SNES Color: 0x%04X", color.snes());
  
  // Individual RGB component sliders (0-31 for SNES)
  int r = (color.snes() & 0x1F);
  int g = (color.snes() >> 5) & 0x1F;
  int b = (color.snes() >> 10) & 0x1F;
  
  if (ImGui::SliderInt("Red", &r, 0, 31)) {
    uint16_t new_color = (color.snes() & 0xFFE0) | (r & 0x1F);
    color = gfx::SnesColor(new_color);
  }
  
  if (ImGui::SliderInt("Green", &g, 0, 31)) {
    uint16_t new_color = (color.snes() & 0xFC1F) | ((g & 0x1F) << 5);
    color = gfx::SnesColor(new_color);
  }
  
  if (ImGui::SliderInt("Blue", &b, 0, 31)) {
    uint16_t new_color = (color.snes() & 0x83FF) | ((b & 0x1F) << 10);
    color = gfx::SnesColor(new_color);
  }
  
  ImGui::PopID();
}

void EnhancedPaletteEditor::DrawPaletteAnalysis(const gfx::SnesPalette& palette) {
  ImGui::Text("Palette Information:");
  ImGui::Text("Size: %zu colors", palette.size());
  
  // Color distribution analysis
  std::map<uint16_t, int> color_frequency;
  for (int i = 0; i < static_cast<int>(palette.size()); i++) {
    color_frequency[palette[i].snes()]++;
  }
  
  ImGui::Text("Unique Colors: %zu", color_frequency.size());
  
  if (color_frequency.size() < palette.size()) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Warning: Duplicate colors detected!");
    
    if (ImGui::TreeNode("Duplicate Colors")) {
      for (const auto& [snes_color, count] : color_frequency) {
        if (count > 1) {
          ImVec4 display_color = gfx::SnesColor(snes_color).rgb();
          ImGui::ColorButton(("##dup" + std::to_string(snes_color)).c_str(), 
                            display_color, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
          ImGui::SameLine();
          ImGui::Text("0x%04X appears %d times", snes_color, count);
        }
      }
      ImGui::TreePop();
    }
  }
  
  // Brightness analysis
  float total_brightness = 0.0f;
  float min_brightness = 1.0f;
  float max_brightness = 0.0f;
  
  for (int i = 0; i < static_cast<int>(palette.size()); i++) {
    ImVec4 color = palette[i].rgb();
    float brightness = (color.x + color.y + color.z) / 3.0f;
    total_brightness += brightness;
    min_brightness = std::min(min_brightness, brightness);
    max_brightness = std::max(max_brightness, brightness);
  }
  
  float avg_brightness = total_brightness / palette.size();
  
  ImGui::Separator();
  ImGui::Text("Brightness Analysis:");
  ImGui::Text("Average: %.2f", avg_brightness);
  ImGui::Text("Range: %.2f - %.2f", min_brightness, max_brightness);
  
  // Show brightness as progress bar
  ImGui::Text("Brightness Distribution:");
  ImGui::ProgressBar(avg_brightness, ImVec2(-1, 0), "Avg");
}

void EnhancedPaletteEditor::LoadROMPalettes() {
  if (!rom_ || rom_palettes_loaded_) return;
  
  try {
    const auto& palette_groups = rom_->palette_group();
    rom_palette_groups_.clear();
    palette_group_names_.clear();
    
    // Load all available palette groups
    if (palette_groups.overworld_main.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.overworld_main[0]);
      palette_group_names_.push_back("Overworld Main");
    }
    if (palette_groups.overworld_aux.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.overworld_aux[0]);
      palette_group_names_.push_back("Overworld Aux");
    }
    if (palette_groups.overworld_animated.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.overworld_animated[0]);
      palette_group_names_.push_back("Overworld Animated");
    }
    if (palette_groups.dungeon_main.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.dungeon_main[0]);
      palette_group_names_.push_back("Dungeon Main");
    }
    if (palette_groups.global_sprites.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.global_sprites[0]);
      palette_group_names_.push_back("Global Sprites");
    }
    if (palette_groups.armors.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.armors[0]);
      palette_group_names_.push_back("Armor");
    }
    if (palette_groups.swords.size() > 0) {
      rom_palette_groups_.push_back(palette_groups.swords[0]);
      palette_group_names_.push_back("Swords");
    }
    
    rom_palettes_loaded_ = true;
    util::logf("Enhanced Palette Editor: Loaded %zu ROM palette groups", rom_palette_groups_.size());
    
  } catch (const std::exception& e) {
    util::logf("Enhanced Palette Editor: Failed to load ROM palettes: %s", e.what());
  }
}

} // namespace gui
} // namespace yaze
