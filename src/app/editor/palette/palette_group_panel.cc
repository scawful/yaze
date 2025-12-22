#include "palette_group_panel.h"

#include <chrono>

#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using namespace yaze::gui;
using gui::DangerButton;
using gui::PrimaryButton;
using gui::SectionHeader;
using gui::ThemedButton;
using gui::ThemedIconButton;

PaletteGroupPanel::PaletteGroupPanel(const std::string& group_name,
                                   const std::string& display_name, Rom* rom,
                                   zelda3::GameData* game_data)
    : group_name_(group_name), display_name_(display_name), rom_(rom),
      game_data_(game_data) {
  // Note: We can't call GetPaletteGroup() here because it's a pure virtual
  // function and the derived class isn't fully constructed yet. Original
  // palettes will be loaded on first Draw() call instead.
}

void PaletteGroupPanel::Draw(bool* p_open) {
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }

  // PaletteManager handles initialization of original palettes
  // No need for local snapshot management anymore

  // Main card window
  // Note: Window management is handled by PanelManager/EditorPanel
  
  DrawToolbar();
  ImGui::Separator();

  // Two-column layout: Grid on left, picker on right
  if (ImGui::BeginTable(
          "##PalettePanelLayout", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch,
                            0.4f);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    // Left: Palette selector + grid
    DrawPaletteSelector();
    ImGui::Separator();
    DrawPaletteGrid();

    ImGui::TableNextColumn();

    // Right: Color picker + info
    if (selected_color_ >= 0) {
      DrawColorPicker();
      ImGui::Separator();
      DrawColorInfo();
      ImGui::Separator();
      DrawMetadataInfo();
    } else {
      ImGui::TextDisabled("Select a color to edit");
      ImGui::Separator();
      DrawMetadataInfo();
    }

    // Custom panels from derived classes
    DrawCustomPanels();

    ImGui::EndTable();
  }

  // Batch operations popup
  DrawBatchOperationsPopup();
}

void PaletteGroupPanel::DrawToolbar() {
  // Query PaletteManager for group-specific modification status
  bool has_changes = gfx::PaletteManager::Get().IsGroupModified(group_name_);

  // Save button (primary action)
  ImGui::BeginDisabled(!has_changes);
  if (PrimaryButton(absl::StrFormat("%s Save to ROM", ICON_MD_SAVE).c_str())) {
    auto status = SaveToRom();
    if (!status.ok()) {
      // TODO: Show error toast
    }
  }
  ImGui::EndDisabled();

  ImGui::SameLine();

  // Discard button (danger action)
  ImGui::BeginDisabled(!has_changes);
  if (DangerButton(absl::StrFormat("%s Discard", ICON_MD_UNDO).c_str())) {
    DiscardChanges();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();

  // Modified indicator badge (show modified color count for this group)
  if (has_changes) {
    size_t modified_count = 0;
    auto* group = GetPaletteGroup();
    if (group) {
      for (int p = 0; p < group->size(); p++) {
        if (gfx::PaletteManager::Get().IsPaletteModified(group_name_, p)) {
          modified_count++;
        }
      }
    }
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%s %zu modified",
                       ICON_MD_EDIT, modified_count);
  }

  ImGui::SameLine();
  ImGui::Dummy(ImVec2(20, 0));  // Spacer
  ImGui::SameLine();

  // Undo/Redo (global operations via PaletteManager)
  ImGui::BeginDisabled(!gfx::PaletteManager::Get().CanUndo());
  if (ThemedIconButton(ICON_MD_UNDO, "Undo")) {
    Undo();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(!gfx::PaletteManager::Get().CanRedo());
  if (ThemedIconButton(ICON_MD_REDO, "Redo")) {
    Redo();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::Dummy(ImVec2(20, 0));  // Spacer
  ImGui::SameLine();

  // Export/Import
  if (ThemedIconButton(ICON_MD_FILE_DOWNLOAD, "Export to clipboard")) {
    ExportToClipboard();
  }

  ImGui::SameLine();
  if (ThemedIconButton(ICON_MD_FILE_UPLOAD, "Import from clipboard")) {
    ImportFromClipboard();
  }

  ImGui::SameLine();
  if (ThemedIconButton(ICON_MD_MORE_VERT, "Batch operations")) {
    ImGui::OpenPopup("BatchOperations");
  }

  // Custom toolbar buttons from derived classes
  DrawCustomToolbarButtons();
}

void PaletteGroupPanel::DrawPaletteSelector() {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group)
    return;

  int num_palettes = palette_group->size();

  ImGui::Text("Palette:");
  ImGui::SameLine();

  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  if (ImGui::BeginCombo(
          "##PaletteSelect",
          absl::StrFormat("Palette %d", selected_palette_).c_str())) {
    for (int i = 0; i < num_palettes; i++) {
      bool is_selected = (selected_palette_ == i);
      bool is_modified = IsPaletteModified(i);

      std::string label = absl::StrFormat("Palette %d", i);
      if (is_modified) {
        label += " *";
      }

      if (ImGui::Selectable(label.c_str(), is_selected)) {
        selected_palette_ = i;
        selected_color_ = -1;  // Reset color selection
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Show reset button for current palette
  ImGui::SameLine();
  ImGui::BeginDisabled(!IsPaletteModified(selected_palette_));
  if (ThemedIconButton(ICON_MD_RESTORE, "Reset palette to original")) {
    ResetPalette(selected_palette_);
  }
  ImGui::EndDisabled();
}

void PaletteGroupPanel::DrawColorPicker() {
  if (selected_color_ < 0)
    return;

  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  SectionHeader("Color Editor");

  auto& color = (*palette)[selected_color_];
  auto original = GetOriginalColor(selected_palette_, selected_color_);

  // Color picker with hue wheel
  ImVec4 col = ConvertSnesColorToImVec4(editing_color_);
  if (ImGui::ColorPicker4("##picker", &col.x,
                          ImGuiColorEditFlags_NoAlpha |
                              ImGuiColorEditFlags_PickerHueWheel |
                              ImGuiColorEditFlags_DisplayRGB |
                              ImGuiColorEditFlags_DisplayHSV)) {
    editing_color_ = ConvertImVec4ToSnesColor(col);
    SetColor(selected_palette_, selected_color_, editing_color_);
  }

  // Current vs Original comparison
  ImGui::Separator();
  ImGui::Text("Current vs Original");

  ImGui::ColorButton("##current", col,
                     ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker,
                     ImVec2(60, 40));

  LayoutHelpers::HelpMarker("Current color being edited");

  ImGui::SameLine();

  ImVec4 orig_col = ConvertSnesColorToImVec4(original);
  if (ImGui::ColorButton(
          "##original", orig_col,
          ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker,
          ImVec2(60, 40))) {
    // Click to restore original
    editing_color_ = original;
    SetColor(selected_palette_, selected_color_, original);
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Click to restore original color");
  }

  // Reset button
  ImGui::BeginDisabled(!IsColorModified(selected_palette_, selected_color_));
  if (ThemedButton(absl::StrFormat("%s Reset", ICON_MD_RESTORE).c_str(),
                   ImVec2(-1, 0))) {
    ResetColor(selected_palette_, selected_color_);
  }
  ImGui::EndDisabled();
}

void PaletteGroupPanel::DrawColorInfo() {
  if (selected_color_ < 0)
    return;

  SectionHeader("Color Information");

  auto col = editing_color_.rgb();
  int r = static_cast<int>(col.x);
  int g = static_cast<int>(col.y);
  int b = static_cast<int>(col.z);

  // RGB values
  ImGui::Text("RGB (0-255): (%d, %d, %d)", r, g, b);
  if (ImGui::IsItemClicked()) {
    ImGui::SetClipboardText(absl::StrFormat("(%d, %d, %d)", r, g, b).c_str());
  }

  // SNES BGR555 value
  if (show_snes_format_) {
    ImGui::Text("SNES BGR555: $%04X", editing_color_.snes());
    if (ImGui::IsItemClicked()) {
      ImGui::SetClipboardText(
          absl::StrFormat("$%04X", editing_color_.snes()).c_str());
    }
  }

  // Hex value
  if (show_hex_format_) {
    ImGui::Text("Hex: #%02X%02X%02X", r, g, b);
    if (ImGui::IsItemClicked()) {
      ImGui::SetClipboardText(
          absl::StrFormat("#%02X%02X%02X", r, g, b).c_str());
    }
  }

  ImGui::TextDisabled("Click any value to copy");
}

void PaletteGroupPanel::DrawMetadataInfo() {
  const auto& metadata = GetMetadata();
  if (selected_palette_ >= metadata.palettes.size())
    return;

  const auto& pal_meta = metadata.palettes[selected_palette_];

  SectionHeader("Palette Metadata");

  // Palette ID
  ImGui::Text("Palette ID: %d", pal_meta.palette_id);

  // Name
  if (!pal_meta.name.empty()) {
    ImGui::Text("Name: %s", pal_meta.name.c_str());
  }

  // Description
  if (!pal_meta.description.empty()) {
    ImGui::TextWrapped("%s", pal_meta.description.c_str());
  }

  ImGui::Separator();

  // Palette dimensions and color depth
  ImGui::Text("Dimensions: %d colors (%dx%d)", metadata.colors_per_palette,
              metadata.colors_per_row,
              (metadata.colors_per_palette + metadata.colors_per_row - 1) /
                  metadata.colors_per_row);

  ImGui::Text("Color Depth: %d BPP (4-bit SNES)", 4);
  ImGui::TextDisabled("(16 colors per palette possible)");

  ImGui::Separator();

  // ROM Address
  ImGui::Text("ROM Address: $%06X", pal_meta.rom_address);
  if (ImGui::IsItemClicked()) {
    ImGui::SetClipboardText(
        absl::StrFormat("$%06X", pal_meta.rom_address).c_str());
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Click to copy address");
  }

  // VRAM Address (if applicable)
  if (pal_meta.vram_address > 0) {
    ImGui::Text("VRAM Address: $%04X", pal_meta.vram_address);
    if (ImGui::IsItemClicked()) {
      ImGui::SetClipboardText(
          absl::StrFormat("$%04X", pal_meta.vram_address).c_str());
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Click to copy VRAM address");
    }
  }

  // Usage notes
  if (!pal_meta.usage_notes.empty()) {
    ImGui::Separator();
    ImGui::TextDisabled("Usage Notes:");
    ImGui::TextWrapped("%s", pal_meta.usage_notes.c_str());
  }
}

void PaletteGroupPanel::DrawBatchOperationsPopup() {
  if (ImGui::BeginPopup("BatchOperations")) {
    SectionHeader("Batch Operations");

    if (ThemedButton("Copy Current Palette", ImVec2(-1, 0))) {
      ExportToClipboard();
      ImGui::CloseCurrentPopup();
    }

    if (ThemedButton("Paste to Current Palette", ImVec2(-1, 0))) {
      ImportFromClipboard();
      ImGui::CloseCurrentPopup();
    }

    ImGui::Separator();

    if (ThemedButton("Reset All Palettes", ImVec2(-1, 0))) {
      DiscardChanges();
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

// ========== Palette Operations ==========

void PaletteGroupPanel::SetColor(int palette_index, int color_index,
                                const gfx::SnesColor& new_color) {
  // Delegate to PaletteManager for centralized tracking and undo/redo
  auto status = gfx::PaletteManager::Get().SetColor(group_name_, palette_index,
                                                    color_index, new_color);
  if (!status.ok()) {
    // TODO: Show error notification
    return;
  }

  // Auto-save if enabled (PaletteManager doesn't handle this)
  if (auto_save_enabled_) {
    WriteColorToRom(palette_index, color_index, new_color);
  }
}

absl::Status PaletteGroupPanel::SaveToRom() {
  // Delegate to PaletteManager for centralized save operation
  return gfx::PaletteManager::Get().SaveGroup(group_name_);
}

void PaletteGroupPanel::DiscardChanges() {
  // Delegate to PaletteManager for centralized discard operation
  gfx::PaletteManager::Get().DiscardGroup(group_name_);

  // Reset selection
  selected_color_ = -1;
}

void PaletteGroupPanel::ResetPalette(int palette_index) {
  // Delegate to PaletteManager for centralized reset operation
  gfx::PaletteManager::Get().ResetPalette(group_name_, palette_index);
}

void PaletteGroupPanel::ResetColor(int palette_index, int color_index) {
  // Delegate to PaletteManager for centralized reset operation
  gfx::PaletteManager::Get().ResetColor(group_name_, palette_index,
                                        color_index);
}

// ========== History Management ==========

void PaletteGroupPanel::Undo() {
  // Delegate to PaletteManager's global undo system
  gfx::PaletteManager::Get().Undo();
}

void PaletteGroupPanel::Redo() {
  // Delegate to PaletteManager's global redo system
  gfx::PaletteManager::Get().Redo();
}

void PaletteGroupPanel::ClearHistory() {
  // Delegate to PaletteManager's global history
  gfx::PaletteManager::Get().ClearHistory();
}

// ========== State Queries ==========

bool PaletteGroupPanel::IsPaletteModified(int palette_index) const {
  // Query PaletteManager for modification status
  return gfx::PaletteManager::Get().IsPaletteModified(group_name_,
                                                      palette_index);
}

bool PaletteGroupPanel::IsColorModified(int palette_index,
                                       int color_index) const {
  // Query PaletteManager for modification status
  return gfx::PaletteManager::Get().IsColorModified(group_name_, palette_index,
                                                    color_index);
}

bool PaletteGroupPanel::HasUnsavedChanges() const {
  // Query PaletteManager for group-specific modification status
  return gfx::PaletteManager::Get().IsGroupModified(group_name_);
}

bool PaletteGroupPanel::CanUndo() const {
  // Query PaletteManager for global undo availability
  return gfx::PaletteManager::Get().CanUndo();
}

bool PaletteGroupPanel::CanRedo() const {
  // Query PaletteManager for global redo availability
  return gfx::PaletteManager::Get().CanRedo();
}

// ========== Helper Methods ==========

gfx::SnesPalette* PaletteGroupPanel::GetMutablePalette(int index) {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group || index < 0 || index >= palette_group->size()) {
    return nullptr;
  }
  return palette_group->mutable_palette(index);
}

gfx::SnesColor PaletteGroupPanel::GetOriginalColor(int palette_index,
                                                  int color_index) const {
  // Get original color from PaletteManager's snapshots
  return gfx::PaletteManager::Get().GetColor(group_name_, palette_index,
                                             color_index);
}

absl::Status PaletteGroupPanel::WriteColorToRom(int palette_index,
                                               int color_index,
                                               const gfx::SnesColor& color) {
  uint32_t address =
      gfx::GetPaletteAddress(group_name_, palette_index, color_index);
  return rom_->WriteColor(address, color);
}

// MarkModified and ClearModified removed - PaletteManager handles tracking now

// ========== Export/Import ==========

std::string PaletteGroupPanel::ExportToJson() const {
  // TODO: Implement JSON export
  return "{}";
}

absl::Status PaletteGroupPanel::ImportFromJson(const std::string& /*json*/) {
  // TODO: Implement JSON import
  return absl::UnimplementedError("Import from JSON not yet implemented");
}

std::string PaletteGroupPanel::ExportToClipboard() const {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group || selected_palette_ >= palette_group->size()) {
    return "";
  }

  auto palette = palette_group->palette(selected_palette_);
  std::string result;

  for (size_t i = 0; i < palette.size(); i++) {
    result += absl::StrFormat("$%04X", palette[i].snes());
    if (i < palette.size() - 1) {
      result += ",";
    }
  }

  ImGui::SetClipboardText(result.c_str());
  return result;
}

absl::Status PaletteGroupPanel::ImportFromClipboard() {
  // TODO: Implement clipboard import
  return absl::UnimplementedError("Import from clipboard not yet implemented");
}

// ============================================================================
// Concrete Palette Panel Implementations
// ============================================================================

// ========== Overworld Main Palette Panel ==========

const PaletteGroupMetadata OverworldMainPalettePanel::metadata_ =
    OverworldMainPalettePanel::InitializeMetadata();

OverworldMainPalettePanel::OverworldMainPalettePanel(Rom* rom, zelda3::GameData* game_data)
    : PaletteGroupPanel("ow_main", "Overworld Main Palettes", rom, game_data) {}

PaletteGroupMetadata OverworldMainPalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "ow_main";
  metadata.display_name = "Overworld Main Palettes";
  metadata.colors_per_palette = 8;
  metadata.colors_per_row = 8;

  // Light World palettes (0-19)
  for (int i = 0; i < 20; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Light World %d", i);
    pal.description = "Used for Light World overworld graphics";
    pal.rom_address = 0xDE6C8 + (i * 16);  // Base address + offset
    pal.vram_address = 0;
    pal.usage_notes = "Modifying these colors affects Light World appearance";
    metadata.palettes.push_back(pal);
  }

  // Dark World palettes (20-39)
  for (int i = 20; i < 40; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Dark World %d", i - 20);
    pal.description = "Used for Dark World overworld graphics";
    pal.rom_address = 0xDE6C8 + (i * 16);
    pal.vram_address = 0;
    pal.usage_notes = "Modifying these colors affects Dark World appearance";
    metadata.palettes.push_back(pal);
  }

  // Special World palettes (40-59)
  for (int i = 40; i < 60; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Special %d", i - 40);
    pal.description = "Used for Special World and triforce room";
    pal.rom_address = 0xDE6C8 + (i * 16);
    pal.vram_address = 0;
    pal.usage_notes = "Modifying these colors affects Special World areas";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* OverworldMainPalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("ow_main");
}

const gfx::PaletteGroup* OverworldMainPalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group("ow_main");
}

void OverworldMainPalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                      (*palette)[i], is_selected, is_modified,
                                      ImVec2(button_size, button_size))) {
      selected_color_ = i;
      editing_color_ = (*palette)[i];
    }

    ImGui::PopID();

    // Wrap to next row
    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

// ========== Overworld Animated Palette Panel ==========

const PaletteGroupMetadata OverworldAnimatedPalettePanel::metadata_ =
    OverworldAnimatedPalettePanel::InitializeMetadata();

OverworldAnimatedPalettePanel::OverworldAnimatedPalettePanel(Rom* rom, zelda3::GameData* game_data)
    : PaletteGroupPanel("ow_animated", "Overworld Animated Palettes", rom, game_data) {}

PaletteGroupMetadata OverworldAnimatedPalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "ow_animated";
  metadata.display_name = "Overworld Animated Palettes";
  metadata.colors_per_palette = 8;
  metadata.colors_per_row = 8;

  // Animated palettes
  const char* anim_names[] = {"Water", "Lava", "Poison Water", "Ice"};
  for (int i = 0; i < 4; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = anim_names[i];
    pal.description =
        absl::StrFormat("%s animated palette cycle", anim_names[i]);
    pal.rom_address = 0xDE86C + (i * 16);
    pal.vram_address = 0;
    pal.usage_notes =
        "These palettes cycle through multiple frames for animation";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* OverworldAnimatedPalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("ow_animated");
}

const gfx::PaletteGroup* OverworldAnimatedPalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group(
      "ow_animated");
}

void OverworldAnimatedPalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                      (*palette)[i], is_selected, is_modified,
                                      ImVec2(button_size, button_size))) {
      selected_color_ = i;
      editing_color_ = (*palette)[i];
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

// ========== Dungeon Main Palette Panel ==========

const PaletteGroupMetadata DungeonMainPalettePanel::metadata_ =
    DungeonMainPalettePanel::InitializeMetadata();

DungeonMainPalettePanel::DungeonMainPalettePanel(Rom* rom, zelda3::GameData* game_data)
    : PaletteGroupPanel("dungeon_main", "Dungeon Main Palettes", rom, game_data) {}

PaletteGroupMetadata DungeonMainPalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "dungeon_main";
  metadata.display_name = "Dungeon Main Palettes";
  metadata.colors_per_palette = 16;
  metadata.colors_per_row = 16;

  // Dungeon palettes (0-19)
  const char* dungeon_names[] = {
      "Sewers",          "Hyrule Castle", "Eastern Palace",     "Desert Palace",
      "Agahnim's Tower", "Swamp Palace",  "Palace of Darkness", "Misery Mire",
      "Skull Woods",     "Ice Palace",    "Tower of Hera",      "Thieves' Town",
      "Turtle Rock",     "Ganon's Tower", "Generic 1",          "Generic 2",
      "Generic 3",       "Generic 4",     "Generic 5",          "Generic 6"};

  for (int i = 0; i < 20; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = dungeon_names[i];
    pal.description = absl::StrFormat("Dungeon palette %d", i);
    pal.rom_address = 0xDE604 + (i * 32);
    pal.vram_address = 0;
    pal.usage_notes = "16 colors per dungeon palette";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* DungeonMainPalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("dungeon_main");
}

const gfx::PaletteGroup* DungeonMainPalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group(
      "dungeon_main");
}

void DungeonMainPalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 28.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                      (*palette)[i], is_selected, is_modified,
                                      ImVec2(button_size, button_size))) {
      selected_color_ = i;
      editing_color_ = (*palette)[i];
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

// ========== Sprite Palette Panel ==========

const PaletteGroupMetadata SpritePalettePanel::metadata_ =
    SpritePalettePanel::InitializeMetadata();

SpritePalettePanel::SpritePalettePanel(Rom* rom, zelda3::GameData* game_data)
    : PaletteGroupPanel("global_sprites", "Sprite Palettes", rom, game_data) {}

PaletteGroupMetadata SpritePalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "global_sprites";
  metadata.display_name = "Global Sprite Palettes";
  metadata.colors_per_palette =
      60;  // 60 colors: 4 rows of 16 colors (with transparent at 0, 16, 32, 48)
  metadata.colors_per_row = 16;  // Display in 16-color rows

  // 2 palette sets: Light World and Dark World
  const char* sprite_names[] = {"Global Sprites (Light World)",
                                "Global Sprites (Dark World)"};

  for (int i = 0; i < 2; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = sprite_names[i];
    pal.description =
        "60 colors = 4 sprite sub-palettes (rows) with transparent at 0, 16, "
        "32, 48";
    pal.rom_address = (i == 0) ? 0xDD218 : 0xDD290;  // LW or DW address
    pal.vram_address = 0;                            // Loaded dynamically
    pal.usage_notes =
        "4 sprite sub-palettes of 15 colors + transparent each. "
        "Row 0: colors 0-15, Row 1: 16-31, Row 2: 32-47, Row 3: 48-59";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* SpritePalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("global_sprites");
}

const gfx::PaletteGroup* SpritePalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group(
      "global_sprites");
}

void SpritePalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 28.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    // Draw transparent color indicator at start of each 16-color row (0, 16,
    // 32, 48, ...)
    bool is_transparent_slot = (i % 16 == 0);
    if (is_transparent_slot) {
      ImGui::BeginGroup();
      if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                        (*palette)[i], is_selected, is_modified,
                                        ImVec2(button_size, button_size))) {
        selected_color_ = i;
        editing_color_ = (*palette)[i];
      }
      // Draw "T" for transparent
      ImVec2 pos = ImGui::GetItemRectMin();
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(pos.x + button_size / 2 - 4, pos.y + button_size / 2 - 8),
          IM_COL32(255, 255, 255, 200), "T");
      ImGui::EndGroup();

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Transparent color slot for sprite sub-palette %d",
                          i / 16);
      }
    } else {
      if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                        (*palette)[i], is_selected, is_modified,
                                        ImVec2(button_size, button_size))) {
        selected_color_ = i;
        editing_color_ = (*palette)[i];
      }
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

void SpritePalettePanel::DrawCustomPanels() {
  // Show VRAM info panel
  SectionHeader("VRAM Information");

  const auto& metadata = GetMetadata();
  if (selected_palette_ < metadata.palettes.size()) {
    const auto& pal_meta = metadata.palettes[selected_palette_];

    ImGui::TextWrapped("This sprite palette is loaded to VRAM address $%04X",
                       pal_meta.vram_address);
    ImGui::TextDisabled(
        "VRAM palettes are used by the SNES PPU for sprite rendering");
  }
}

// ========== Equipment Palette Panel ==========

const PaletteGroupMetadata EquipmentPalettePanel::metadata_ =
    EquipmentPalettePanel::InitializeMetadata();

EquipmentPalettePanel::EquipmentPalettePanel(Rom* rom, zelda3::GameData* game_data)
    : PaletteGroupPanel("armors", "Equipment Palettes", rom, game_data) {}

PaletteGroupMetadata EquipmentPalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "armors";
  metadata.display_name = "Equipment Palettes";
  metadata.colors_per_palette = 8;
  metadata.colors_per_row = 8;

  const char* armor_names[] = {"Green Mail", "Blue Mail", "Red Mail"};

  for (int i = 0; i < 3; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = armor_names[i];
    pal.description = absl::StrFormat("Link's %s colors", armor_names[i]);
    pal.rom_address = 0xDD308 + (i * 16);
    pal.vram_address = 0;
    pal.usage_notes = "Changes Link's tunic appearance";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* EquipmentPalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("armors");
}

const gfx::PaletteGroup* EquipmentPalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group("armors");
}

void EquipmentPalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                      (*palette)[i], is_selected, is_modified,
                                      ImVec2(button_size, button_size))) {
      selected_color_ = i;
      editing_color_ = (*palette)[i];
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

// ========== Sprites Aux1 Palette Panel ==========

const PaletteGroupMetadata SpritesAux1PalettePanel::metadata_ =
    SpritesAux1PalettePanel::InitializeMetadata();

SpritesAux1PalettePanel::SpritesAux1PalettePanel(Rom* rom,
                                                       zelda3::GameData* game_data)
    : PaletteGroupPanel("sprites_aux1", "Sprites Aux 1", rom, game_data) {}

PaletteGroupMetadata SpritesAux1PalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "sprites_aux1";
  metadata.display_name = "Sprites Aux 1";
  metadata.colors_per_palette = 8;  // 7 colors + transparent
  metadata.colors_per_row = 8;

  for (int i = 0; i < 12; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Sprites Aux1 %02d", i);
    pal.description = "Auxiliary sprite palette (7 colors + transparent)";
    pal.rom_address = 0xDD39E + (i * 14);  // 7 colors * 2 bytes
    pal.vram_address = 0;
    pal.usage_notes = "Used by specific sprites. Color 0 is transparent.";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* SpritesAux1PalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("sprites_aux1");
}

const gfx::PaletteGroup* SpritesAux1PalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group(
      "sprites_aux1");
}

void SpritesAux1PalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                      (*palette)[i], is_selected, is_modified,
                                      ImVec2(button_size, button_size))) {
      selected_color_ = i;
      editing_color_ = (*palette)[i];
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

// ========== Sprites Aux2 Palette Panel ==========

const PaletteGroupMetadata SpritesAux2PalettePanel::metadata_ =
    SpritesAux2PalettePanel::InitializeMetadata();

SpritesAux2PalettePanel::SpritesAux2PalettePanel(Rom* rom,
                                                       zelda3::GameData* game_data)
    : PaletteGroupPanel("sprites_aux2", "Sprites Aux 2", rom, game_data) {}

PaletteGroupMetadata SpritesAux2PalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "sprites_aux2";
  metadata.display_name = "Sprites Aux 2";
  metadata.colors_per_palette = 8;  // 7 colors + transparent
  metadata.colors_per_row = 8;

  for (int i = 0; i < 11; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Sprites Aux2 %02d", i);
    pal.description = "Auxiliary sprite palette (7 colors + transparent)";
    pal.rom_address = 0xDD446 + (i * 14);  // 7 colors * 2 bytes
    pal.vram_address = 0;
    pal.usage_notes = "Used by specific sprites. Color 0 is transparent.";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* SpritesAux2PalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("sprites_aux2");
}

const gfx::PaletteGroup* SpritesAux2PalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group(
      "sprites_aux2");
}

void SpritesAux2PalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    // Draw transparent color indicator for index 0
    if (i == 0) {
      ImGui::BeginGroup();
      if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                        (*palette)[i], is_selected, is_modified,
                                        ImVec2(button_size, button_size))) {
        selected_color_ = i;
        editing_color_ = (*palette)[i];
      }
      // Draw "T" for transparent
      ImVec2 pos = ImGui::GetItemRectMin();
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(pos.x + button_size / 2 - 4, pos.y + button_size / 2 - 8),
          IM_COL32(255, 255, 255, 200), "T");
      ImGui::EndGroup();
    } else {
      if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                        (*palette)[i], is_selected, is_modified,
                                        ImVec2(button_size, button_size))) {
        selected_color_ = i;
        editing_color_ = (*palette)[i];
      }
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

// ========== Sprites Aux3 Palette Panel ==========

const PaletteGroupMetadata SpritesAux3PalettePanel::metadata_ =
    SpritesAux3PalettePanel::InitializeMetadata();

SpritesAux3PalettePanel::SpritesAux3PalettePanel(Rom* rom,
                                                       zelda3::GameData* game_data)
    : PaletteGroupPanel("sprites_aux3", "Sprites Aux 3", rom, game_data) {}

PaletteGroupMetadata SpritesAux3PalettePanel::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "sprites_aux3";
  metadata.display_name = "Sprites Aux 3";
  metadata.colors_per_palette = 8;  // 7 colors + transparent
  metadata.colors_per_row = 8;

  for (int i = 0; i < 24; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Sprites Aux3 %02d", i);
    pal.description = "Auxiliary sprite palette (7 colors + transparent)";
    pal.rom_address = 0xDD4E0 + (i * 14);  // 7 colors * 2 bytes
    pal.vram_address = 0;
    pal.usage_notes = "Used by specific sprites. Color 0 is transparent.";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* SpritesAux3PalettePanel::GetPaletteGroup() {
  if (!game_data_) return nullptr;
  return game_data_->palette_groups.get_group("sprites_aux3");
}

const gfx::PaletteGroup* SpritesAux3PalettePanel::GetPaletteGroup() const {
  if (!game_data_) return nullptr;
  return const_cast<zelda3::GameData*>(game_data_)->palette_groups.get_group(
      "sprites_aux3");
}

void SpritesAux3PalettePanel::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette)
    return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    // Draw transparent color indicator for index 0
    if (i == 0) {
      ImGui::BeginGroup();
      if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                        (*palette)[i], is_selected, is_modified,
                                        ImVec2(button_size, button_size))) {
        selected_color_ = i;
        editing_color_ = (*palette)[i];
      }
      // Draw "T" for transparent
      ImVec2 pos = ImGui::GetItemRectMin();
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(pos.x + button_size / 2 - 4, pos.y + button_size / 2 - 8),
          IM_COL32(255, 255, 255, 200), "T");
      ImGui::EndGroup();
    } else {
      if (yaze::gui::PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
                                        (*palette)[i], is_selected, is_modified,
                                        ImVec2(button_size, button_size))) {
        selected_color_ = i;
        editing_color_ = (*palette)[i];
      }
    }

    ImGui::PopID();

    if ((i + 1) % colors_per_row != 0 && i + 1 < palette->size()) {
      ImGui::SameLine();
    }
  }
}

}  // namespace editor
}  // namespace yaze
