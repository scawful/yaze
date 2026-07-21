#include "palette_manager.h"

#include <algorithm>
#include <chrono>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace gfx {

PaletteManager::SessionState* PaletteManager::CurrentState() {
  if (!game_data_) {
    return &unbound_state_;
  }
  return &session_states_.try_emplace(game_data_).first->second;
}

const PaletteManager::SessionState* PaletteManager::CurrentState() const {
  if (!game_data_) {
    return &unbound_state_;
  }
  auto it = session_states_.find(game_data_);
  return it == session_states_.end() ? &unbound_state_ : &it->second;
}

const PaletteManager::SessionState* PaletteManager::FindState(
    const zelda3::GameData* game_data) const {
  if (!game_data) {
    return nullptr;
  }
  auto it = session_states_.find(game_data);
  return it == session_states_.end() ? nullptr : &it->second;
}

bool PaletteManager::IsInitialized() const {
  if (!game_data_) {
    return rom_ != nullptr;
  }
  const auto* state = FindState(game_data_);
  return state != nullptr && state->initialized && rom_ == game_data_->rom();
}

void PaletteManager::ActivateSession(zelda3::GameData* game_data) {
  game_data_ = game_data;
  rom_ = game_data ? game_data->rom() : nullptr;
  if (game_data) {
    session_states_.try_emplace(game_data);
  }
}

void PaletteManager::Initialize(zelda3::GameData* game_data) {
  if (!game_data) {
    return;
  }

  ActivateSession(game_data);
  auto* state = CurrentState();
  if (state->initialized) {
    return;
  }

  *state = SessionState{};

  // Load original palette snapshots for all groups
  auto* palette_groups = &game_data_->palette_groups;

  // Snapshot all palette groups
  const char* group_names[] = {"ow_main",      "ow_aux",         "ow_animated",
                               "hud",          "global_sprites", "armors",
                               "swords",       "shields",        "sprites_aux1",
                               "sprites_aux2", "sprites_aux3",   "dungeon_main",
                               "grass",        "3d_object",      "ow_mini_map"};

  for (const auto& group_name : group_names) {
    try {
      auto* group = palette_groups->get_group(group_name);
      if (group) {
        std::vector<SnesPalette> originals;
        for (size_t i = 0; i < group->size(); i++) {
          originals.push_back(group->palette(i));
        }
        state->original_palettes[group_name] = originals;
      }
    } catch (const std::exception& e) {
      // Group doesn't exist, skip
      continue;
    }
  }

  state->initialized = true;
}

bool PaletteManager::IsManaging(const zelda3::GameData* game_data) const {
  return game_data != nullptr && game_data_ == game_data && IsInitialized() &&
         rom_ == game_data->rom();
}

void PaletteManager::ReleaseSession(const zelda3::GameData* game_data) {
  if (!game_data) {
    return;
  }
  if (game_data_ == game_data) {
    game_data_ = nullptr;
    rom_ = nullptr;
  }
  session_states_.erase(game_data);
}

void PaletteManager::Initialize(Rom* rom) {
  // Legacy initialization - not supported in new architecture
  // Keep ROM pointer for backwards compatibility but log warning
  if (!rom) {
    return;
  }
  game_data_ = nullptr;
  rom_ = rom;
  unbound_state_ = SessionState{};
}

void PaletteManager::ResetForTesting() {
  game_data_ = nullptr;
  rom_ = nullptr;
  session_states_.clear();
  unbound_state_ = SessionState{};
  change_listeners_.clear();
  next_callback_id_ = 1;
}

// ========== Color Operations ==========

SnesColor PaletteManager::GetColor(const std::string& group_name,
                                   int palette_index, int color_index) const {
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
    return absl::InvalidArgumentError(absl::StrFormat(
        "Palette index %d out of range [0, %d)", palette_index, group->size()));
  }

  auto* palette = group->mutable_palette(palette_index);
  if (color_index < 0 || color_index >= palette->size()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Color index %d out of range [0, %d)", color_index, palette->size()));
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

    PaletteColorChange change{group_name,  palette_index,
                              color_index, original_color,
                              new_color,   static_cast<uint64_t>(timestamp_ms)};
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
    CurrentState()->batch_changes.push_back(
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
  auto* state = CurrentState();
  auto it = state->original_palettes.find(group_name);
  if (it == state->original_palettes.end() || palette_index < 0 ||
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
  if (auto modified_it = state->modified_palettes.find(group_name);
      modified_it != state->modified_palettes.end()) {
    modified_it->second.erase(palette_index);
    if (modified_it->second.empty()) {
      state->modified_palettes.erase(modified_it);
    }
  }
  if (auto colors_it = state->modified_colors.find(group_name);
      colors_it != state->modified_colors.end()) {
    colors_it->second.erase(palette_index);
    if (colors_it->second.empty()) {
      state->modified_colors.erase(colors_it);
    }
  }

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kPaletteReset, group_name,
                           palette_index, -1};
  NotifyListeners(event);

  return absl::OkStatus();
}

// ========== Dirty Tracking ==========

bool PaletteManager::HasUnsavedChanges() const {
  return !CurrentState()->modified_palettes.empty();
}

bool PaletteManager::HasUnsavedChanges(
    const zelda3::GameData* game_data) const {
  const auto* state = FindState(game_data);
  return state != nullptr && !state->modified_palettes.empty();
}

std::vector<std::string> PaletteManager::GetModifiedGroups() const {
  std::vector<std::string> groups;
  for (const auto& [group_name, _] : CurrentState()->modified_palettes) {
    groups.push_back(group_name);
  }
  return groups;
}

bool PaletteManager::IsGroupModified(const std::string& group_name) const {
  auto it = CurrentState()->modified_palettes.find(group_name);
  return it != CurrentState()->modified_palettes.end() && !it->second.empty();
}

bool PaletteManager::IsPaletteModified(const std::string& group_name,
                                       int palette_index) const {
  auto it = CurrentState()->modified_palettes.find(group_name);
  if (it == CurrentState()->modified_palettes.end()) {
    return false;
  }
  return it->second.contains(palette_index);
}

bool PaletteManager::IsColorModified(const std::string& group_name,
                                     int palette_index, int color_index) const {
  auto group_it = CurrentState()->modified_colors.find(group_name);
  if (group_it == CurrentState()->modified_colors.end()) {
    return false;
  }

  auto pal_it = group_it->second.find(palette_index);
  if (pal_it == group_it->second.end()) {
    return false;
  }

  return pal_it->second.contains(color_index);
}

size_t PaletteManager::GetModifiedColorCount() const {
  return GetModifiedColorCount(game_data_);
}

size_t PaletteManager::GetModifiedColorCount(
    const zelda3::GameData* game_data) const {
  const auto* state = FindState(game_data);
  if (state == nullptr) {
    return 0;
  }

  size_t count = 0;
  for (const auto& [_, palette_map] : state->modified_colors) {
    for (const auto& [__, color_set] : palette_map) {
      count += color_set.size();
    }
  }
  return count;
}

std::vector<std::pair<uint32_t, uint32_t>>
PaletteManager::GetModifiedColorWriteRanges() const {
  return GetModifiedColorWriteRanges(game_data_);
}

std::vector<std::pair<uint32_t, uint32_t>>
PaletteManager::GetModifiedColorWriteRanges(
    const zelda3::GameData* game_data) const {
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  const auto* state = FindState(game_data);
  if (state == nullptr) {
    return ranges;
  }
  ranges.reserve(GetModifiedColorCount(game_data));

  for (const auto& [group_name, palette_map] : state->modified_colors) {
    for (const auto& [palette_index, color_indices] : palette_map) {
      for (int color_index : color_indices) {
        const uint32_t begin =
            GetPaletteAddress(group_name, palette_index, color_index);
        ranges.emplace_back(begin, begin + 2u);
      }
    }
  }

  std::sort(ranges.begin(), ranges.end());
  std::vector<std::pair<uint32_t, uint32_t>> coalesced;
  coalesced.reserve(ranges.size());
  for (const auto& range : ranges) {
    if (coalesced.empty() || coalesced.back().second < range.first) {
      coalesced.push_back(range);
      continue;
    }
    coalesced.back().second = std::max(coalesced.back().second, range.second);
  }
  return coalesced;
}

// ========== Persistence ==========

absl::Status PaletteManager::SaveGroup(const std::string& group_name) {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  Rom* rom = rom_;
  if (!rom && game_data_) {
    rom = game_data_->rom();
  }
  if (!rom) {
    return absl::FailedPreconditionError("No ROM available for palette save");
  }

  auto* group = GetMutableGroup(group_name);
  if (!group) {
    return absl::NotFoundError(
        absl::StrFormat("Palette group '%s' not found", group_name));
  }

  // Get modified palettes for this group
  auto pal_it = CurrentState()->modified_palettes.find(group_name);
  if (pal_it == CurrentState()->modified_palettes.end() ||
      pal_it->second.empty()) {
    // No changes to save
    return absl::OkStatus();
  }

  // Write each modified palette
  for (int palette_idx : pal_it->second) {
    auto* palette = group->mutable_palette(palette_idx);

    // Get modified colors for this palette
    auto color_it =
        CurrentState()->modified_colors[group_name].find(palette_idx);
    if (color_it != CurrentState()->modified_colors[group_name].end()) {
      for (int color_idx : color_it->second) {
        // Calculate ROM address using the helper function
        uint32_t address =
            GetPaletteAddress(group_name, palette_idx, color_idx);

        // Write color to ROM - write the 16-bit SNES color value
        RETURN_IF_ERROR(rom->WriteShort(address, (*palette)[color_idx].snes()));
      }
    }
  }

  // Update original snapshots
  auto& originals = CurrentState()->original_palettes[group_name];
  for (size_t i = 0; i < group->size() && i < originals.size(); i++) {
    originals[i] = group->palette(i);
  }

  // Clear modified flags for this group
  ClearModifiedFlags(group_name);

  // Mark ROM as dirty
  rom->set_dirty(true);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kGroupSaved, group_name,
                           -1, -1};
  NotifyListeners(event);

  // Notify Arena for bitmap propagation to other editors
  Arena::Get().NotifyPaletteModified(group_name, -1);

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

absl::Status PaletteManager::BeginSaveTransaction() {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  auto* state = CurrentState();
  if (state->save_transaction_snapshot.has_value()) {
    return absl::FailedPreconditionError(
        "Palette save transaction is already active");
  }
  state->save_transaction_snapshot =
      SaveTransactionSnapshot{state->original_palettes,
                              state->modified_palettes, state->modified_colors};
  return absl::OkStatus();
}

void PaletteManager::RollbackSaveTransaction() {
  auto* state = CurrentState();
  if (!state->save_transaction_snapshot.has_value()) {
    return;
  }
  state->original_palettes =
      std::move(state->save_transaction_snapshot->original_palettes);
  state->modified_palettes =
      std::move(state->save_transaction_snapshot->modified_palettes);
  state->modified_colors =
      std::move(state->save_transaction_snapshot->modified_colors);
  state->save_transaction_snapshot.reset();
}

void PaletteManager::CommitSaveTransaction() {
  CurrentState()->save_transaction_snapshot.reset();
}

absl::Status PaletteManager::ApplyPreviewChanges() {
  if (!IsInitialized()) {
    return absl::FailedPreconditionError("PaletteManager not initialized");
  }

  // Get all modified groups and notify Arena for each
  // This triggers bitmap refresh in other editors WITHOUT saving to ROM
  auto modified_groups = GetModifiedGroups();

  if (modified_groups.empty()) {
    return absl::OkStatus();  // Nothing to preview
  }

  for (const auto& group_name : modified_groups) {
    Arena::Get().NotifyPaletteModified(group_name, -1);
  }

  // Notify listeners that preview was applied
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
  auto pal_it = CurrentState()->modified_palettes.find(group_name);
  if (pal_it == CurrentState()->modified_palettes.end()) {
    return;
  }

  // Restore from original snapshots
  auto orig_it = CurrentState()->original_palettes.find(group_name);
  if (orig_it != CurrentState()->original_palettes.end()) {
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
  PaletteChangeEvent event{PaletteChangeEvent::Type::kAllDiscarded, "", -1, -1};
  NotifyListeners(event);
}

// ========== Undo/Redo ==========

void PaletteManager::Undo() {
  if (!CanUndo()) {
    return;
  }

  auto change = CurrentState()->undo_stack.back();
  CurrentState()->undo_stack.pop_back();

  // Restore original color
  auto* group = GetMutableGroup(change.group_name);
  if (group && change.palette_index < group->size()) {
    auto* palette = group->mutable_palette(change.palette_index);
    if (change.color_index < palette->size()) {
      (*palette)[change.color_index] = change.original_color;
      MarkModified(change.group_name, change.palette_index, change.color_index);
    }
  }

  // Move to redo stack
  CurrentState()->redo_stack.push_back(change);

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

  auto change = CurrentState()->redo_stack.back();
  CurrentState()->redo_stack.pop_back();

  // Reapply new color
  auto* group = GetMutableGroup(change.group_name);
  if (group && change.palette_index < group->size()) {
    auto* palette = group->mutable_palette(change.palette_index);
    if (change.color_index < palette->size()) {
      (*palette)[change.color_index] = change.new_color;
      MarkModified(change.group_name, change.palette_index, change.color_index);
    }
  }

  // Move back to undo stack
  CurrentState()->undo_stack.push_back(change);

  // Notify listeners
  PaletteChangeEvent event{PaletteChangeEvent::Type::kColorChanged,
                           change.group_name, change.palette_index,
                           change.color_index};
  NotifyListeners(event);
}

void PaletteManager::ClearHistory() {
  CurrentState()->undo_stack.clear();
  CurrentState()->redo_stack.clear();
}

bool PaletteManager::CanUndo() const {
  return !CurrentState()->undo_stack.empty();
}

bool PaletteManager::CanRedo() const {
  return !CurrentState()->redo_stack.empty();
}

size_t PaletteManager::GetUndoStackSize() const {
  return CurrentState()->undo_stack.size();
}

size_t PaletteManager::GetRedoStackSize() const {
  return CurrentState()->redo_stack.size();
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
  CurrentState()->batch_depth++;
  if (CurrentState()->batch_depth == 1) {
    CurrentState()->batch_changes.clear();
  }
}

void PaletteManager::EndBatch() {
  if (CurrentState()->batch_depth == 0) {
    return;
  }

  CurrentState()->batch_depth--;

  if (CurrentState()->batch_depth == 0 &&
      !CurrentState()->batch_changes.empty()) {
    // Commit all batch changes as a single undo step
    for (const auto& change : CurrentState()->batch_changes) {
      RecordChange(change);

      // Notify listeners for each change
      PaletteChangeEvent event{PaletteChangeEvent::Type::kColorChanged,
                               change.group_name, change.palette_index,
                               change.color_index};
      NotifyListeners(event);
    }

    CurrentState()->batch_changes.clear();
  }
}

bool PaletteManager::InBatch() const {
  return CurrentState()->batch_depth > 0;
}

// ========== Private Helpers ==========

PaletteGroup* PaletteManager::GetMutableGroup(const std::string& group_name) {
  if (!IsInitialized()) {
    return nullptr;
  }
  try {
    if (game_data_) {
      return game_data_->palette_groups.get_group(group_name);
    }
    return nullptr;  // Legacy ROM-only mode not supported
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
    if (game_data_) {
      return const_cast<PaletteGroupMap*>(&game_data_->palette_groups)
          ->get_group(group_name);
    }
    return nullptr;  // Legacy ROM-only mode not supported
  } catch (const std::exception&) {
    return nullptr;
  }
}

SnesColor PaletteManager::GetOriginalColor(const std::string& group_name,
                                           int palette_index,
                                           int color_index) const {
  const auto* state = CurrentState();
  auto it = state->original_palettes.find(group_name);
  if (it == state->original_palettes.end() || palette_index < 0 ||
      palette_index >= it->second.size()) {
    return SnesColor();
  }

  const auto& palette = it->second[palette_index];
  if (color_index < 0 || color_index >= palette.size()) {
    return SnesColor();
  }

  return palette[color_index];
}

void PaletteManager::RecordChange(const PaletteColorChange& change) {
  CurrentState()->undo_stack.push_back(change);

  // Limit history size
  if (CurrentState()->undo_stack.size() > kMaxUndoHistory) {
    CurrentState()->undo_stack.pop_front();
  }

  // Clear redo stack (can't redo after a new change)
  CurrentState()->redo_stack.clear();
}

void PaletteManager::NotifyListeners(const PaletteChangeEvent& event) {
  for (const auto& [_, callback] : change_listeners_) {
    callback(event);
  }
}

void PaletteManager::MarkModified(const std::string& group_name,
                                  int palette_index, int color_index) {
  auto* state = CurrentState();
  const auto* group = GetGroup(group_name);
  const auto original_it = state->original_palettes.find(group_name);
  const bool matches_original =
      group != nullptr && palette_index >= 0 && palette_index < group->size() &&
      color_index >= 0 &&
      color_index < group->palette_ref(palette_index).size() &&
      original_it != state->original_palettes.end() &&
      palette_index < original_it->second.size() &&
      color_index < original_it->second[palette_index].size() &&
      group->palette_ref(palette_index)[color_index].snes() ==
          original_it->second[palette_index][color_index].snes();

  if (!matches_original) {
    state->modified_palettes[group_name].insert(palette_index);
    state->modified_colors[group_name][palette_index].insert(color_index);
    return;
  }

  if (auto group_it = state->modified_colors.find(group_name);
      group_it != state->modified_colors.end()) {
    if (auto palette_it = group_it->second.find(palette_index);
        palette_it != group_it->second.end()) {
      palette_it->second.erase(color_index);
      if (palette_it->second.empty()) {
        group_it->second.erase(palette_it);
      }
    }
    if (group_it->second.empty()) {
      state->modified_colors.erase(group_it);
    }
  }

  const auto colors_group_it = state->modified_colors.find(group_name);
  const bool palette_still_modified =
      colors_group_it != state->modified_colors.end() &&
      colors_group_it->second.contains(palette_index);
  if (!palette_still_modified) {
    if (auto group_it = state->modified_palettes.find(group_name);
        group_it != state->modified_palettes.end()) {
      group_it->second.erase(palette_index);
      if (group_it->second.empty()) {
        state->modified_palettes.erase(group_it);
      }
    }
  }
}

void PaletteManager::ClearModifiedFlags(const std::string& group_name) {
  CurrentState()->modified_palettes.erase(group_name);
  CurrentState()->modified_colors.erase(group_name);
}

}  // namespace gfx
}  // namespace yaze
