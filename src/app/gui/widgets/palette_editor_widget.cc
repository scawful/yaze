#include "app/gui/widgets/palette_editor_widget.h"

#include <algorithm>
#include <map>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/color.h"
#include "util/log.h"

namespace yaze {
namespace gui {

// Merged implementation from PaletteWidget and PaletteEditorWidget

void PaletteEditorWidget::Initialize(Rom *rom) {
  rom_ = rom;
  rom_palettes_loaded_ = false;
  if (rom_) {
    LoadROMPalettes();
  }
  current_palette_id_ = 0;
  selected_color_index_ = -1;
}

// --- Embedded Draw Method (from simple editor) ---
void PaletteEditorWidget::Draw() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ROM not loaded");
    return;
  }

  ImGui::BeginGroup();

  DrawPaletteSelector();
  ImGui::Separator();

  auto &dungeon_pal_group = rom_->mutable_palette_group()->dungeon_main;
  if (current_palette_id_ >= 0 &&
      current_palette_id_ < (int)dungeon_pal_group.size()) {
    auto palette = dungeon_pal_group[current_palette_id_];
    DrawPaletteGrid(palette, 15);
    dungeon_pal_group[current_palette_id_] = palette;
  }

  ImGui::Separator();

  if (selected_color_index_ >= 0) {
    DrawColorPicker();
  } else {
    ImGui::TextDisabled("Select a color to edit");
  }

  ImGui::EndGroup();
}

void PaletteEditorWidget::DrawPaletteSelector() {
  auto &dungeon_pal_group = rom_->mutable_palette_group()->dungeon_main;
  int num_palettes = dungeon_pal_group.size();

  ImGui::Text("Dungeon Palette:");
  ImGui::SameLine();

  if (ImGui::BeginCombo(
          "##PaletteSelect",
          absl::StrFormat("Palette %d", current_palette_id_).c_str())) {
    for (int i = 0; i < num_palettes; i++) {
      bool is_selected = (current_palette_id_ == i);
      if (ImGui::Selectable(absl::StrFormat("Palette %d", i).c_str(),
                            is_selected)) {
        current_palette_id_ = i;
        selected_color_index_ = -1;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

void PaletteEditorWidget::DrawColorPicker() {
  ImGui::SeparatorText(
      absl::StrFormat("Edit Color %d", selected_color_index_).c_str());

  auto &dungeon_pal_group = rom_->mutable_palette_group()->dungeon_main;
  auto palette = dungeon_pal_group[current_palette_id_];
  auto original_color = palette[selected_color_index_];

  if (ImGui::ColorEdit3("Color", &editing_color_.x,
                       ImGuiColorEditFlags_NoAlpha |
                           ImGuiColorEditFlags_PickerHueWheel)) {
    int r = static_cast<int>(editing_color_.x * 31.0f);
    int g = static_cast<int>(editing_color_.y * 31.0f);
    int b = static_cast<int>(editing_color_.z * 31.0f);
    uint16_t snes_color = (b << 10) | (g << 5) | r;

    palette[selected_color_index_] = gfx::SnesColor(snes_color);
    dungeon_pal_group[current_palette_id_] = palette;

    if (on_palette_changed_) {
      on_palette_changed_(current_palette_id_);
    }
  }

  ImGui::Text("RGB (0-255): (%d, %d, %d)", (int)(editing_color_.x * 255),
              (int)(editing_color_.y * 255), (int)(editing_color_.z * 255));
  ImGui::Text("SNES BGR555: 0x%04X", original_color.snes());

  if (ImGui::Button("Reset to Original")) {
    editing_color_ = ImVec4(original_color.rgb().x / 255.0f,
                            original_color.rgb().y / 255.0f,
                            original_color.rgb().z / 255.0f, 1.0f);
  }
}

// --- Modal/Popup Methods (from feature-rich widget) ---

void PaletteEditorWidget::ShowPaletteEditor(gfx::SnesPalette &palette,
                                            const std::string &title) {
  if (ImGui::BeginPopupModal(title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enhanced Palette Editor");
    ImGui::Separator();

    DrawPaletteGrid(palette);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Palette Analysis")) {
      DrawPaletteAnalysis(palette);
    }

    if (ImGui::CollapsingHeader("ROM Palette Manager") && rom_) {
      DrawROMPaletteSelector();

      if (ImGui::Button("Apply ROM Palette") && !rom_palette_groups_.empty()) {
        if (current_group_index_ <
            static_cast<int>(rom_palette_groups_.size())) {
          palette = rom_palette_groups_[current_group_index_];
        }
      }
    }

    ImGui::Separator();

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

void PaletteEditorWidget::ShowROMPaletteManager() {
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
      ImGui::Text("Preview of %s:",
                  palette_group_names_[current_group_index_].c_str());

      const auto &preview_palette = rom_palette_groups_[current_group_index_];
      DrawPaletteGrid(const_cast<gfx::SnesPalette &>(preview_palette));
      DrawPaletteAnalysis(preview_palette);
    }
  }
  ImGui::End();
}

void PaletteEditorWidget::ShowColorAnalysis(const gfx::Bitmap &bitmap,
                                            const std::string &title) {
  if (!show_color_analysis_) return;

  if (ImGui::Begin(title.c_str(), &show_color_analysis_)) {
    ImGui::Text("Bitmap Color Analysis");
    ImGui::Separator();

    if (!bitmap.is_active()) {
      ImGui::Text("Bitmap is not active");
      ImGui::End();
      return;
    }

    std::map<uint8_t, int> pixel_counts;
    const auto &data = bitmap.vector();

    for (uint8_t pixel : data) {
      uint8_t palette_index = pixel & 0x0F;
      pixel_counts[palette_index]++;
    }

    ImGui::Text("Bitmap Size: %d x %d (%zu pixels)", bitmap.width(),
                bitmap.height(), data.size());

    ImGui::Separator();
    ImGui::Text("Pixel Distribution:");

    int total_pixels = static_cast<int>(data.size());
    for (const auto &[index, count] : pixel_counts) {
      float percentage = (static_cast<float>(count) / total_pixels) * 100.0f;
      ImGui::Text("Index %d: %d pixels (%.1f%%)", index, count, percentage);

      ImGui::SameLine();
      ImGui::ProgressBar(percentage / 100.0f, ImVec2(100, 0));

      if (index < static_cast<int>(bitmap.palette().size())) {
        ImGui::SameLine();
        auto color = bitmap.palette()[index];
        ImVec4 display_color = color.rgb();
        ImGui::ColorButton(("##color" + std::to_string(index)).c_str(),
                           display_color, ImGuiColorEditFlags_NoTooltip,
                           ImVec2(20, 20));
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

bool PaletteEditorWidget::ApplyROMPalette(gfx::Bitmap *bitmap, int group_index,
                                          int palette_index) {
  if (!bitmap || !rom_palettes_loaded_ || group_index < 0 ||
      group_index >= static_cast<int>(rom_palette_groups_.size())) {
    return false;
  }

  try {
    const auto &selected_palette = rom_palette_groups_[group_index];
    SavePaletteBackup(bitmap->palette());

    if (palette_index >= 0 && palette_index < 8) {
      bitmap->SetPaletteWithTransparent(selected_palette, palette_index);
    } else {
      bitmap->SetPalette(selected_palette);
    }

    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, bitmap);

    current_group_index_ = group_index;
    current_palette_index_ = palette_index;
    return true;
  } catch (const std::exception &e) {
    return false;
  }
}

const gfx::SnesPalette *PaletteEditorWidget::GetSelectedROMPalette() const {
  if (!rom_palettes_loaded_ || current_group_index_ < 0 ||
      current_group_index_ >= static_cast<int>(rom_palette_groups_.size())) {
    return nullptr;
  }
  return &rom_palette_groups_[current_group_index_];
}

void PaletteEditorWidget::SavePaletteBackup(const gfx::SnesPalette &palette) {
  backup_palette_ = palette;
}

bool PaletteEditorWidget::RestorePaletteBackup(gfx::SnesPalette &palette) {
  if (backup_palette_.size() == 0) {
    return false;
  }
  palette = backup_palette_;
  return true;
}

// Unified grid drawing function
void PaletteEditorWidget::DrawPaletteGrid(gfx::SnesPalette &palette, int cols) {
  for (int i = 0; i < static_cast<int>(palette.size()); i++) {
    if (i % cols != 0) ImGui::SameLine();

    auto color = palette[i];
    ImVec4 display_color = color.rgb();

    ImGui::PushID(i);
    if (ImGui::ColorButton("##color", display_color,
                           ImGuiColorEditFlags_NoTooltip, ImVec2(30, 30))) {
      editing_color_index_ = i;
      selected_color_index_ = i;
      temp_color_ = display_color;
      editing_color_ = display_color;
    }

    if (ImGui::BeginPopupContextItem()) {
      ImGui::Text("Color %d (0x%04X)", i, color.snes());
      ImGui::Separator();
      if (ImGui::MenuItem("Edit Color")) {
        editing_color_index_ = i;
        selected_color_index_ = i;
        temp_color_ = display_color;
        editing_color_ = display_color;
      }
      if (ImGui::MenuItem("Reset to Black")) {
        palette[i] = gfx::SnesColor(0);
      }
      ImGui::EndPopup();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Color %d\nSNES: 0x%04X\nRGB: (%d, %d, %d)", i,
                        color.snes(), static_cast<int>(display_color.x * 255),
                        static_cast<int>(display_color.y * 255),
                        static_cast<int>(display_color.z * 255));
    }

    ImGui::PopID();
  }

  if (editing_color_index_ >= 0) {
    ImGui::OpenPopup("Edit Color");
    if (ImGui::BeginPopupModal("Edit Color", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Editing Color %d", editing_color_index_);
      if (ImGui::ColorEdit4("Color", &temp_color_.x,
                           ImGuiColorEditFlags_NoAlpha |
                               ImGuiColorEditFlags_DisplayRGB)) {
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

void PaletteEditorWidget::DrawROMPaletteSelector() {
  if (!rom_palettes_loaded_) {
    LoadROMPalettes();
  }

  if (rom_palette_groups_.empty()) {
    ImGui::Text("No ROM palettes available");
    return;
  }

  ImGui::Text("Palette Group:");
  if (ImGui::Combo(
          "##PaletteGroup", &current_group_index_,
          [](void *data, int idx, const char **out_text) -> bool {
            auto *names = static_cast<std::vector<std::string> *>(data);
            if (idx < 0 || idx >= static_cast<int>(names->size())) return false;
            *out_text = (*names)[idx].c_str();
            return true;
          },
          &palette_group_names_,
          static_cast<int>(palette_group_names_.size()))) {
  }

  ImGui::Text("Palette Index:");
  ImGui::SliderInt("##PaletteIndex", &current_palette_index_, 0, 7, "%d");

  if (current_group_index_ < static_cast<int>(rom_palette_groups_.size())) {
    ImGui::Text("Preview:");
    const auto &preview_palette = rom_palette_groups_[current_group_index_];
    for (int i = 0; i < 8 && i < static_cast<int>(preview_palette.size());
         i++) {
      if (i > 0) ImGui::SameLine();
      auto color = preview_palette[i];
      ImVec4 display_color = color.rgb();
      ImGui::ColorButton(("##preview" + std::to_string(i)).c_str(),
                         display_color, ImGuiColorEditFlags_NoTooltip,
                         ImVec2(20, 20));
    }
  }
}

void PaletteEditorWidget::DrawColorEditControls(gfx::SnesColor &color,
                                                int color_index) {
  ImVec4 rgba = color.rgb();

  ImGui::PushID(color_index);

  if (ImGui::ColorEdit4("##color_edit", &rgba.x,
                       ImGuiColorEditFlags_NoAlpha |
                           ImGuiColorEditFlags_DisplayRGB)) {
    color = gfx::SnesColor(rgba);
  }

  ImGui::Text("SNES Color: 0x%04X", color.snes());

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

void PaletteEditorWidget::DrawPaletteAnalysis(
    const gfx::SnesPalette &palette) {
  ImGui::Text("Palette Information:");
  ImGui::Text("Size: %zu colors", palette.size());

  std::map<uint16_t, int> color_frequency;
  for (int i = 0; i < static_cast<int>(palette.size()); i++) {
    color_frequency[palette[i].snes()]++;
  }

  ImGui::Text("Unique Colors: %zu", color_frequency.size());

  if (color_frequency.size() < palette.size()) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Warning: Duplicate colors detected!");
    if (ImGui::TreeNode("Duplicate Colors")) {
      for (const auto &[snes_color, count] : color_frequency) {
        if (count > 1) {
          ImVec4 display_color = gfx::SnesColor(snes_color).rgb();
          ImGui::ColorButton(("##dup" + std::to_string(snes_color)).c_str(),
                             display_color, ImGuiColorEditFlags_NoTooltip,
                             ImVec2(16, 16));
          ImGui::SameLine();
          ImGui::Text("0x%04X appears %d times", snes_color, count);
        }
      }
      ImGui::TreePop();
    }
  }

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

  ImGui::Text("Brightness Distribution:");
  ImGui::ProgressBar(avg_brightness, ImVec2(-1, 0), "Avg");
}

void PaletteEditorWidget::LoadROMPalettes() {
  if (!rom_ || rom_palettes_loaded_) return;

  try {
    const auto &palette_groups = rom_->palette_group();
    rom_palette_groups_.clear();
    palette_group_names_.clear();

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
  } catch (const std::exception &e) {
    LOG_ERROR("Enhanced Palette Editor", "Failed to load ROM palettes");
  }
}

}  // namespace gui
}  // namespace yaze

