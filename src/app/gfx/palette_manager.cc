#include "palette_manager.h"

#include <chrono>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_palette.h"
#include "util/macro.h"

namespace yaze {
namespace gfx {

void PaletteManager::Initialize(Rom* rom) {
  if (!rom) {
    return;
  }

  rom_ = rom;

  // Load original palette snapshots for all groups
  auto* palette_groups = rom_->mutable_palette_group();

  // Snapshot all palette groups
  const char* group_names[] = {
      "ow_main", "ow_aux", "ow_animated", "hud", "global_sprites",
      "armors", "swords", "shields", "sprites_aux1", "sprites_aux2",
      "sprites_aux3", "dungeon_main", "grass", "3d_object", "ow_mini_map"
  };

  for (const auto& group_name : group_names) {
    try {
      auto* group = palette_groups->get_group(group_name);
      if (group) {
        std::vector<SnesPalette> originals;
        for (size_t i = 0; i < group->size(); i++) {
          originals.push_back(group->palette(i));
        }
        original_palettes_[group_name] = originals;
      }
    } catch (const std::exception& e) {
      // Group doesn't exist, skip
      continue;
    }
  }

  // Clear any existing state
  modified_palettes_.clear();
  modified_colors_.clear();
  ClearHistory();
}

// ========== Color Operations ==========

SnesColor PaletteManager::GetColor(const std::string& group_name,
                                    int palette_index,
                                    int color_index) const {
  const auto* group = GetGroup(group_name);
  if (!group || palette_index < 0 || palette_index >= group->size()) {
    return SnesColor();
  }

  const auto& palette = group->palette_ref(palette_index);
  if (color_index < 0 || color_index >= palette.size()) {
    return SnesColor();
  }

  return palette[color_index];
}

absl::Status PaletteManager::SetColor(const std::string& group_name,
                                       int palette_index, int color_index,
                                       const SnesColor& new_color) {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  auto* group = GetMutableGroup(group_name);
  if (!group) {
    return absl::NotFoundError(
        absl::StrFormat("Palette group '%s' not found", group_name));
  }

  if (palette_index < 0 || palette_index >= group->size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Palette index %d out of range [0, %d)", palette_index,
                        group->size()));
  }

  auto* palette = group->mutable_palette(palette_index);
  if (color_index < 0 || color_index >= palette->size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Color index %d out of range [0, %d)", color_index,
                        palette->size()));
  }

  // Get original color
  SnesColor original_color = (*palette)[color_index];

  // Update in-memory palette
  (*palette)[color_index] = new_color;

  // Track modification
  MarkModified(group_name, palette_index, color_index);

  // Record for undo (unless in batch mode - batch changes recorded separately)
  if (!InBatch()) {
    auto now = std::chrono::system_clock::now();
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch())
                            .count();

    PaletteColorChange change{group_name,      palette_index, color_index,
                              original_color,  new_color,
                              static_cast<uint64_t>(timestamp_ms)};
    RecordChange(change);

    // Notify listeners
    PaletteChangeEvent event{PaletteChangeEvent::Type::kColorChanged,
                             group_name, palette_index, color_index};
    NotifyListeners(event);
  } else {
    // Store in batch buffer
    auto now = std::chrono::system_clock::now();
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch())
                            .count();
    batch_changes_.push_back(
        {group_name, palette_index, color_index, original_color, new_color,
         static_cast<uint64_t>(timestamp_ms)});
  }

  return absl::OkStatus();
}

absl::Status PaletteManager::ResetColor(const std::string& group_name,
                                         int palette_index, int color_index) {
  SnesColor original = GetOriginalColor(group_name, palette_index, color_index);
  return SetColor(group_name, palette_index, color_index, original);
}

absl::Status PaletteManager::ResetPalette(const std::string& group_name,
                                           int palette_index) {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  // Check if original snapshot exists
  auto it = original_palettes_.find(group_name);
  if (it == original_palettes_.end() ||
      palette_index >= it->second.size()) {
    return absl::NotFoundError("Original palette not found");
  }

  auto* group = GetMutableGroup(group_name);
  if (!group || palette_index >= group->size()) {
    return absl::NotFoundError("Palette group or index not found");
  }

  // Restore from original
  *group->mutable_palette(palette_index) = it->second[palette_index];

  // Clear modified flags for this palette
  modified_palettes_[group_name].erase(palette_index);
  modified_colors_[group_name].erase(palette_index);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kPaletteReset,
                           group_name, palette_index, -1};
  NotifyListeners(event);

  return absl::OkStatus();
}

// ========== Dirty Tracking ==========

bool PaletteManager::HasUnsavedChanges() const {
  return !modified_palettes_.empty();
}

std::vector<std::string> PaletteManager::GetModifiedGroups() const {
  std::vector<std::string> groups;
  for (const auto& [group_name, _] : modified_palettes_) {
    groups.push_back(group_name);
  }
  return groups;
}

bool PaletteManager::IsGroupModified(const std::string& group_name) const {
  auto it = modified_palettes_.find(group_name);
  return it != modified_palettes_.end() && !it->second.empty();
}

bool PaletteManager::IsPaletteModified(const std::string& group_name,
                                        int palette_index) const {
  auto it = modified_palettes_.find(group_name);
  if (it == modified_palettes_.end()) {
    return false;
  }
  return it->second.contains(palette_index);
}

bool PaletteManager::IsColorModified(const std::string& group_name,
                                      int palette_index,
                                      int color_index) const {
  auto group_it = modified_colors_.find(group_name);
  if (group_it == modified_colors_.end()) {
    return false;
  }

  auto pal_it = group_it->second.find(palette_index);
  if (pal_it == group_it->second.end()) {
    return false;
  }

  return pal_it->second.contains(color_index);
}

size_t PaletteManager::GetModifiedColorCount() const {
  size_t count = 0;
  for (const auto& [_, palette_map] : modified_colors_) {
    for (const auto& [__, color_set] : palette_map) {
      count += color_set.size();
    }
  }
  return count;
}

// ========== Persistence ==========

absl::Status PaletteManager::SaveGroup(const std::string& group_name) {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  auto* group = GetMutableGroup(group_name);
  if (!group) {
    return absl::NotFoundError(
        absl::StrFormat("Palette group '%s' not found", group_name));
  }

  // Get modified palettes for this group
  auto pal_it = modified_palettes_.find(group_name);
  if (pal_it == modified_palettes_.end() || pal_it->second.empty()) {
    // No changes to save
    return absl::OkStatus();
  }

  // Write each modified palette
  for (int palette_idx : pal_it->second) {
    auto* palette = group->mutable_palette(palette_idx);

    // Get modified colors for this palette
    auto color_it = modified_colors_[group_name].find(palette_idx);
    if (color_it != modified_colors_[group_name].end()) {
      for (int color_idx : color_it->second) {
        // Calculate ROM address using the helper function
        uint32_t address = GetPaletteAddress(group_name, palette_idx, color_idx);

        // Write color to ROM - write the 16-bit SNES color value
        rom_->WriteShort(address, (*palette)[color_idx].snes());
      }
    }
  }

  // Update original snapshots
  auto& originals = original_palettes_[group_name];
  for (size_t i = 0; i < group->size() && i < originals.size(); i++) {
    originals[i] = group->palette(i);
  }

  // Clear modified flags for this group
  ClearModifiedFlags(group_name);

  // Mark ROM as dirty
  rom_->set_dirty(true);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kGroupSaved, group_name,
                           -1, -1};
  NotifyListeners(event);

  return absl::OkStatus();
}

absl::Status PaletteManager::SaveAllToRom() {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  // Save all modified groups
  for (const auto& group_name : GetModifiedGroups()) {
    RETURN_IF_ERROR(SaveGroup(group_name));
  }

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kAllSaved, "", -1, -1};
  NotifyListeners(event);

  return absl::OkStatus();
}

void PaletteManager::DiscardGroup(const std::string& group_name) {
  if (!IsInitialized()) {
    return;
  }

  auto* group = GetMutableGroup(group_name);
  if (!group) {
    return;
  }

  // Get modified palettes
  auto pal_it = modified_palettes_.find(group_name);
  if (pal_it == modified_palettes_.end()) {
    return;
  }

  // Restore from original snapshots
  auto orig_it = original_palettes_.find(group_name);
  if (orig_it != original_palettes_.end()) {
    for (int palette_idx : pal_it->second) {
      if (palette_idx < orig_it->second.size()) {
        *group->mutable_palette(palette_idx) = orig_it->second[palette_idx];
      }
    }
  }

  // Clear modified flags
  ClearModifiedFlags(group_name);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kGroupDiscarded,
                           group_name, -1, -1};
  NotifyListeners(event);
}

void PaletteManager::DiscardAllChanges() {
  if (!IsInitialized()) {
    return;
  }

  // Discard all modified groups
  for (const auto& group_name : GetModifiedGroups()) {
    DiscardGroup(group_name);
  }

  // Clear undo/redo
  ClearHistory();

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kAllDiscarded, "", -1,
                           -1};
  NotifyListeners(event);
}

// ========== Undo/Redo ==========

void PaletteManager::Undo() {
  if (!CanUndo()) {
    return;
  }

  auto change = undo_stack_.back();
  undo_stack_.pop_back();

  // Restore original color
  auto* group = GetMutableGroup(change.group_name);
  if (group && change.palette_index < group->size()) {
    auto* palette = group->mutable_palette(change.palette_index);
    if (change.color_index < palette->size()) {
      (*palette)[change.color_index] = change.original_color;
    }
  }

  // Move to redo stack
  redo_stack_.push_back(change);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kColorChanged,
                           change.group_name, change.palette_index,
                           change.color_index};
  NotifyListeners(event);
}

void PaletteManager::Redo() {
  if (!CanRedo()) {
    return;
  }

  auto change = redo_stack_.back();
  redo_stack_.pop_back();

  // Reapply new color
  auto* group = GetMutableGroup(change.group_name);
  if (group && change.palette_index < group->size()) {
    auto* palette = group->mutable_palette(change.palette_index);
    if (change.color_index < palette->size()) {
      (*palette)[change.color_index] = change.new_color;
    }
  }

  // Move back to undo stack
  undo_stack_.push_back(change);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kColorChanged,
                           change.group_name, change.palette_index,
                           change.color_index};
  NotifyListeners(event);
}

void PaletteManager::ClearHistory() {
  undo_stack_.clear();
  redo_stack_.clear();
}

// ========== Change Notifications ==========

int PaletteManager::RegisterChangeListener(ChangeCallback callback) {
  int id = next_callback_id_++;
  change_listeners_[id] = callback;
  return id;
}

void PaletteManager::UnregisterChangeListener(int callback_id) {
  change_listeners_.erase(callback_id);
}

// ========== Batch Operations ==========

void PaletteManager::BeginBatch() {
  batch_depth_++;
  if (batch_depth_ == 1) {
    batch_changes_.clear();
  }
}

void PaletteManager::EndBatch() {
  if (batch_depth_ == 0) {
    return;
  }

  batch_depth_--;

  if (batch_depth_ == 0 && !batch_changes_.empty()) {
    // Commit all batch changes as a single undo step
    for (const auto& change : batch_changes_) {
      RecordChange(change);

      // Notify listeners for each change
      PaletteChangeEvent event{PaletteChangeEvent::Type::kColorChanged,
                               change.group_name, change.palette_index,
                               change.color_index};
      NotifyListeners(event);
    }

    batch_changes_.clear();
  }
}

// ========== Private Helpers ==========

PaletteGroup* PaletteManager::GetMutableGroup(const std::string& group_name) {
  if (!IsInitialized()) {
    return nullptr;
  }
  try {
    return rom_->mutable_palette_group()->get_group(group_name);
  } catch (const std::exception&) {
    return nullptr;
  }
}

const PaletteGroup* PaletteManager::GetGroup(
    const std::string& group_name) const {
  if (!IsInitialized()) {
    return nullptr;
  }
  try {
    // Need to const_cast because get_group() is not const
    return const_cast<Rom*>(rom_)->mutable_palette_group()->get_group(
        group_name);
  } catch (const std::exception&) {
    return nullptr;
  }
}

SnesColor PaletteManager::GetOriginalColor(const std::string& group_name,
                                            int palette_index,
                                            int color_index) const {
  auto it = original_palettes_.find(group_name);
  if (it == original_palettes_.end() || palette_index >= it->second.size()) {
    return SnesColor();
  }

  const auto& palette = it->second[palette_index];
  if (color_index >= palette.size()) {
    return SnesColor();
  }

  return palette[color_index];
}

void PaletteManager::RecordChange(const PaletteColorChange& change) {
  undo_stack_.push_back(change);

  // Limit history size
  if (undo_stack_.size() > kMaxUndoHistory) {
    undo_stack_.pop_front();
  }

  // Clear redo stack (can't redo after a new change)
  redo_stack_.clear();
}

void PaletteManager::NotifyListeners(const PaletteChangeEvent& event) {
  for (const auto& [_, callback] : change_listeners_) {
    callback(event);
  }
}

void PaletteManager::MarkModified(const std::string& group_name,
                                   int palette_index, int color_index) {
  modified_palettes_[group_name].insert(palette_index);
  modified_colors_[group_name][palette_index].insert(color_index);
}

void PaletteManager::ClearModifiedFlags(const std::string& group_name) {
  modified_palettes_.erase(group_name);
  modified_colors_.erase(group_name);
}

}  // namespace gfx
}  // namespace yaze
