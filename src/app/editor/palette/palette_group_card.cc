#include "palette_group_card.h"

#include <chrono>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/layout_helpers.h"
#include "app/gui/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using namespace gui;
using gui::ThemedButton;
using gui::ThemedIconButton;
using gui::PrimaryButton;
using gui::DangerButton;
using gui::SectionHeader;

PaletteGroupCard::PaletteGroupCard(const std::string& group_name,
                                   const std::string& display_name,
                                   Rom* rom)
    : group_name_(group_name),
      display_name_(display_name),
      rom_(rom) {
  // Load original palettes from ROM for reset/comparison
  if (rom_ && rom_->is_loaded()) {
    auto* palette_group = GetPaletteGroup();
    if (palette_group) {
      for (size_t i = 0; i < palette_group->size(); i++) {
        original_palettes_.push_back(palette_group->palette(i));
      }
    }
  }
}

void PaletteGroupCard::Draw() {
  if (!show_ || !rom_ || !rom_->is_loaded()) {
    return;
  }

  // Main card window
  if (ImGui::Begin(display_name_.c_str(), &show_)) {
    DrawToolbar();
    ImGui::Separator();

    // Two-column layout: Grid on left, picker on right
    if (ImGui::BeginTable("##PaletteCardLayout", 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch, 0.6f);
      ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch, 0.4f);

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
  }
  ImGui::End();

  // Batch operations popup
  DrawBatchOperationsPopup();
}

void PaletteGroupCard::DrawToolbar() {
  bool has_changes = HasUnsavedChanges();

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

  // Modified indicator badge
  if (has_changes) {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                      "%s %zu modified",
                      ICON_MD_EDIT,
                      modified_palettes_.size());
  }

  ImGui::SameLine();
  ImGui::Dummy(ImVec2(20, 0));  // Spacer
  ImGui::SameLine();

  // Undo/Redo
  ImGui::BeginDisabled(!CanUndo());
  if (ThemedIconButton(ICON_MD_UNDO, "Undo")) {
    Undo();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(!CanRedo());
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

void PaletteGroupCard::DrawPaletteSelector() {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group) return;

  int num_palettes = palette_group->size();

  ImGui::Text("Palette:");
  ImGui::SameLine();

  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  if (ImGui::BeginCombo("##PaletteSelect",
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

void PaletteGroupCard::DrawColorPicker() {
  if (selected_color_ < 0) return;

  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette) return;

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
  if (ImGui::ColorButton("##original", orig_col,
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

void PaletteGroupCard::DrawColorInfo() {
  if (selected_color_ < 0) return;

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
      ImGui::SetClipboardText(absl::StrFormat("$%04X", editing_color_.snes()).c_str());
    }
  }

  // Hex value
  if (show_hex_format_) {
    ImGui::Text("Hex: #%02X%02X%02X", r, g, b);
    if (ImGui::IsItemClicked()) {
      ImGui::SetClipboardText(absl::StrFormat("#%02X%02X%02X", r, g, b).c_str());
    }
  }

  ImGui::TextDisabled("Click any value to copy");
}

void PaletteGroupCard::DrawMetadataInfo() {
  const auto& metadata = GetMetadata();
  if (selected_palette_ >= metadata.palettes.size()) return;

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

  // ROM Address
  ImGui::Text("ROM Address: $%06X", pal_meta.rom_address);
  if (ImGui::IsItemClicked()) {
    ImGui::SetClipboardText(absl::StrFormat("$%06X", pal_meta.rom_address).c_str());
  }

  // VRAM Address (if applicable)
  if (pal_meta.vram_address > 0) {
    ImGui::Text("VRAM Address: $%04X", pal_meta.vram_address);
    if (ImGui::IsItemClicked()) {
      ImGui::SetClipboardText(absl::StrFormat("$%04X", pal_meta.vram_address).c_str());
    }
  }

  // Usage notes
  if (!pal_meta.usage_notes.empty()) {
    ImGui::Separator();
    ImGui::TextDisabled("Usage Notes:");
    ImGui::TextWrapped("%s", pal_meta.usage_notes.c_str());
  }
}

void PaletteGroupCard::DrawBatchOperationsPopup() {
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

void PaletteGroupCard::SetColor(int palette_index, int color_index,
                                 const gfx::SnesColor& new_color) {
  auto* palette = GetMutablePalette(palette_index);
  if (!palette) return;

  auto original_color = (*palette)[color_index];

  // Update in-memory palette
  (*palette)[color_index] = new_color;

  // Track modification
  MarkModified(palette_index, color_index);

  // Record for undo
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();

  undo_stack_.push_back(
      {palette_index, color_index, original_color, new_color, static_cast<uint64_t>(timestamp)});

  // Limit undo history
  if (undo_stack_.size() > kMaxUndoHistory) {
    undo_stack_.erase(undo_stack_.begin());
  }

  redo_stack_.clear();

  // Auto-save if enabled
  if (auto_save_enabled_) {
    WriteColorToRom(palette_index, color_index, new_color);
  }
}

absl::Status PaletteGroupCard::SaveToRom() {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group) {
    return absl::NotFoundError("Palette group not found");
  }

  // Save each modified palette
  for (int palette_idx : modified_palettes_) {
    auto* palette = palette_group->mutable_palette(palette_idx);

    // Write each modified color in this palette
    for (int color_idx : modified_colors_[palette_idx]) {
      RETURN_IF_ERROR(WriteColorToRom(palette_idx, color_idx, (*palette)[color_idx]));
    }
  }

  // Clear modified flags after successful save
  modified_palettes_.clear();
  modified_colors_.clear();

  // Update original palettes
  original_palettes_.clear();
  for (size_t i = 0; i < palette_group->size(); i++) {
    original_palettes_.push_back(palette_group->palette(i));
  }

  rom_->set_dirty(true);
  return absl::OkStatus();
}

void PaletteGroupCard::DiscardChanges() {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group) return;

  // Restore all palettes from original
  for (int palette_idx : modified_palettes_) {
    if (palette_idx < original_palettes_.size()) {
      *palette_group->mutable_palette(palette_idx) = original_palettes_[palette_idx];
    }
  }

  // Clear modified tracking
  modified_palettes_.clear();
  modified_colors_.clear();

  // Clear undo/redo
  ClearHistory();

  // Reset selection
  selected_color_ = -1;
}

void PaletteGroupCard::ResetPalette(int palette_index) {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group || palette_index >= original_palettes_.size()) {
    return;
  }

  // Restore from original
  *palette_group->mutable_palette(palette_index) = original_palettes_[palette_index];

  // Clear modified flags for this palette
  ClearModified(palette_index);
}

void PaletteGroupCard::ResetColor(int palette_index, int color_index) {
  auto original = GetOriginalColor(palette_index, color_index);
  SetColor(palette_index, color_index, original);

  // Remove from modified tracking
  if (modified_colors_.contains(palette_index)) {
    modified_colors_[palette_index].erase(color_index);
    if (modified_colors_[palette_index].empty()) {
      modified_palettes_.erase(palette_index);
    }
  }
}

// ========== History Management ==========

void PaletteGroupCard::Undo() {
  if (!CanUndo()) return;

  auto change = undo_stack_.back();
  undo_stack_.pop_back();

  // Restore original color
  auto* palette = GetMutablePalette(change.palette_index);
  if (palette) {
    (*palette)[change.color_index] = change.original_color;
  }

  // Update ROM if auto-save enabled
  if (auto_save_enabled_) {
    WriteColorToRom(change.palette_index, change.color_index, change.original_color);
  }

  // Move to redo stack
  redo_stack_.push_back(change);
}

void PaletteGroupCard::Redo() {
  if (!CanRedo()) return;

  auto change = redo_stack_.back();
  redo_stack_.pop_back();

  // Reapply new color
  auto* palette = GetMutablePalette(change.palette_index);
  if (palette) {
    (*palette)[change.color_index] = change.new_color;
  }

  // Update ROM if auto-save enabled
  if (auto_save_enabled_) {
    WriteColorToRom(change.palette_index, change.color_index, change.new_color);
  }

  // Move back to undo stack
  undo_stack_.push_back(change);
}

void PaletteGroupCard::ClearHistory() {
  undo_stack_.clear();
  redo_stack_.clear();
}

// ========== State Queries ==========

bool PaletteGroupCard::IsPaletteModified(int palette_index) const {
  return modified_palettes_.contains(palette_index);
}

bool PaletteGroupCard::IsColorModified(int palette_index, int color_index) const {
  auto it = modified_colors_.find(palette_index);
  if (it == modified_colors_.end()) {
    return false;
  }
  return it->second.contains(color_index);
}

// ========== Helper Methods ==========

gfx::SnesPalette* PaletteGroupCard::GetMutablePalette(int index) {
  auto* palette_group = GetPaletteGroup();
  if (!palette_group || index < 0 || index >= palette_group->size()) {
    return nullptr;
  }
  return palette_group->mutable_palette(index);
}

gfx::SnesColor PaletteGroupCard::GetOriginalColor(int palette_index,
                                                   int color_index) const {
  if (palette_index >= original_palettes_.size()) {
    return gfx::SnesColor();
  }
  return original_palettes_[palette_index][color_index];
}

absl::Status PaletteGroupCard::WriteColorToRom(int palette_index, int color_index,
                                                const gfx::SnesColor& color) {
  uint32_t address = gfx::GetPaletteAddress(group_name_, palette_index, color_index);
  return rom_->WriteColor(address, color);
}

void PaletteGroupCard::MarkModified(int palette_index, int color_index) {
  modified_palettes_.insert(palette_index);
  modified_colors_[palette_index].insert(color_index);
}

void PaletteGroupCard::ClearModified(int palette_index) {
  modified_palettes_.erase(palette_index);
  modified_colors_.erase(palette_index);
}

// ========== Export/Import ==========

std::string PaletteGroupCard::ExportToJson() const {
  // TODO: Implement JSON export
  return "{}";
}

absl::Status PaletteGroupCard::ImportFromJson(const std::string& json) {
  // TODO: Implement JSON import
  return absl::UnimplementedError("Import from JSON not yet implemented");
}

std::string PaletteGroupCard::ExportToClipboard() const {
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

absl::Status PaletteGroupCard::ImportFromClipboard() {
  // TODO: Implement clipboard import
  return absl::UnimplementedError("Import from clipboard not yet implemented");
}

// ============================================================================
// Concrete Palette Card Implementations
// ============================================================================

// ========== Overworld Main Palette Card ==========

const PaletteGroupMetadata OverworldMainPaletteCard::metadata_ =
    OverworldMainPaletteCard::InitializeMetadata();

OverworldMainPaletteCard::OverworldMainPaletteCard(Rom* rom)
    : PaletteGroupCard("ow_main", "Overworld Main Palettes", rom) {}

PaletteGroupMetadata OverworldMainPaletteCard::InitializeMetadata() {
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

gfx::PaletteGroup* OverworldMainPaletteCard::GetPaletteGroup() {
  return rom_->mutable_palette_group()->get_group("ow_main");
}

const gfx::PaletteGroup* OverworldMainPaletteCard::GetPaletteGroup() const {
  // Note: rom_->palette_group() returns by value, so we need to use the mutable version
  return const_cast<Rom*>(rom_)->mutable_palette_group()->get_group("ow_main");
}

void OverworldMainPaletteCard::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette) return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
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

// ========== Overworld Animated Palette Card ==========

const PaletteGroupMetadata OverworldAnimatedPaletteCard::metadata_ =
    OverworldAnimatedPaletteCard::InitializeMetadata();

OverworldAnimatedPaletteCard::OverworldAnimatedPaletteCard(Rom* rom)
    : PaletteGroupCard("ow_animated", "Overworld Animated Palettes", rom) {}

PaletteGroupMetadata OverworldAnimatedPaletteCard::InitializeMetadata() {
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
    pal.description = absl::StrFormat("%s animated palette cycle", anim_names[i]);
    pal.rom_address = 0xDE86C + (i * 16);
    pal.vram_address = 0;
    pal.usage_notes = "These palettes cycle through multiple frames for animation";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* OverworldAnimatedPaletteCard::GetPaletteGroup() {
  return rom_->mutable_palette_group()->get_group("ow_animated");
}

const gfx::PaletteGroup* OverworldAnimatedPaletteCard::GetPaletteGroup() const {
  return const_cast<Rom*>(rom_)->mutable_palette_group()->get_group("ow_animated");
}

void OverworldAnimatedPaletteCard::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette) return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
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

// ========== Dungeon Main Palette Card ==========

const PaletteGroupMetadata DungeonMainPaletteCard::metadata_ =
    DungeonMainPaletteCard::InitializeMetadata();

DungeonMainPaletteCard::DungeonMainPaletteCard(Rom* rom)
    : PaletteGroupCard("dungeon_main", "Dungeon Main Palettes", rom) {}

PaletteGroupMetadata DungeonMainPaletteCard::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "dungeon_main";
  metadata.display_name = "Dungeon Main Palettes";
  metadata.colors_per_palette = 16;
  metadata.colors_per_row = 16;

  // Dungeon palettes (0-19)
  const char* dungeon_names[] = {
      "Sewers", "Hyrule Castle", "Eastern Palace", "Desert Palace",
      "Agahnim's Tower", "Swamp Palace", "Palace of Darkness", "Misery Mire",
      "Skull Woods", "Ice Palace", "Tower of Hera", "Thieves' Town",
      "Turtle Rock", "Ganon's Tower", "Generic 1", "Generic 2",
      "Generic 3", "Generic 4", "Generic 5", "Generic 6"
  };

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

gfx::PaletteGroup* DungeonMainPaletteCard::GetPaletteGroup() {
  return rom_->mutable_palette_group()->get_group("dungeon_main");
}

const gfx::PaletteGroup* DungeonMainPaletteCard::GetPaletteGroup() const {
  return const_cast<Rom*>(rom_)->mutable_palette_group()->get_group("dungeon_main");
}

void DungeonMainPaletteCard::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette) return;

  const float button_size = 28.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
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

// ========== Sprite Palette Card ==========

const PaletteGroupMetadata SpritePaletteCard::metadata_ =
    SpritePaletteCard::InitializeMetadata();

SpritePaletteCard::SpritePaletteCard(Rom* rom)
    : PaletteGroupCard("sprites", "Sprite Palettes", rom) {}

PaletteGroupMetadata SpritePaletteCard::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "sprites";
  metadata.display_name = "Sprite Palettes";
  metadata.colors_per_palette = 8;
  metadata.colors_per_row = 8;

  // Global sprite palettes (0-3)
  const char* sprite_names[] = {
      "Green Mail Sprite", "Blue Mail Sprite", "Red Mail Sprite", "Gold Armor"
  };

  for (int i = 0; i < 4; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = sprite_names[i];
    pal.description = "Global sprite palette";
    pal.rom_address = 0xDD218 + (i * 16);
    pal.vram_address = 0x8D00 + (i * 16);  // VRAM sprite palette area
    pal.usage_notes = "Used by sprites throughout the game";
    metadata.palettes.push_back(pal);
  }

  // Auxiliary sprite palettes (4-5)
  for (int i = 4; i < 6; i++) {
    PaletteMetadata pal;
    pal.palette_id = i;
    pal.name = absl::StrFormat("Auxiliary %d", i - 4);
    pal.description = "Auxiliary sprite palette";
    pal.rom_address = 0xDD218 + (i * 16);
    pal.vram_address = 0x8D00 + (i * 16);
    pal.usage_notes = "Used by specific sprites";
    metadata.palettes.push_back(pal);
  }

  return metadata;
}

gfx::PaletteGroup* SpritePaletteCard::GetPaletteGroup() {
  return rom_->mutable_palette_group()->get_group("sprites");
}

const gfx::PaletteGroup* SpritePaletteCard::GetPaletteGroup() const {
  return const_cast<Rom*>(rom_)->mutable_palette_group()->get_group("sprites");
}

void SpritePaletteCard::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette) return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
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

void SpritePaletteCard::DrawCustomPanels() {
  // Show VRAM info panel
  SectionHeader("VRAM Information");

  const auto& metadata = GetMetadata();
  if (selected_palette_ < metadata.palettes.size()) {
    const auto& pal_meta = metadata.palettes[selected_palette_];

    ImGui::TextWrapped("This sprite palette is loaded to VRAM address $%04X",
                      pal_meta.vram_address);
    ImGui::TextDisabled("VRAM palettes are used by the SNES PPU for sprite rendering");
  }
}

// ========== Equipment Palette Card ==========

const PaletteGroupMetadata EquipmentPaletteCard::metadata_ =
    EquipmentPaletteCard::InitializeMetadata();

EquipmentPaletteCard::EquipmentPaletteCard(Rom* rom)
    : PaletteGroupCard("armor", "Equipment Palettes", rom) {}

PaletteGroupMetadata EquipmentPaletteCard::InitializeMetadata() {
  PaletteGroupMetadata metadata;
  metadata.group_name = "armor";
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

gfx::PaletteGroup* EquipmentPaletteCard::GetPaletteGroup() {
  return rom_->mutable_palette_group()->get_group("armor");
}

const gfx::PaletteGroup* EquipmentPaletteCard::GetPaletteGroup() const {
  return const_cast<Rom*>(rom_)->mutable_palette_group()->get_group("armor");
}

void EquipmentPaletteCard::DrawPaletteGrid() {
  auto* palette = GetMutablePalette(selected_palette_);
  if (!palette) return;

  const float button_size = 32.0f;
  const int colors_per_row = GetColorsPerRow();

  for (int i = 0; i < palette->size(); i++) {
    bool is_selected = (i == selected_color_);
    bool is_modified = IsColorModified(selected_palette_, i);

    ImGui::PushID(i);

    if (PaletteColorButton(absl::StrFormat("##color%d", i).c_str(),
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

}  // namespace editor
}  // namespace yaze
