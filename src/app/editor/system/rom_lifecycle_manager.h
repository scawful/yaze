#ifndef YAZE_APP_EDITOR_SYSTEM_ROM_LIFECYCLE_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_ROM_LIFECYCLE_MANAGER_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/system/rom_file_manager.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"

namespace yaze::editor {

class SessionCoordinator;
class ToastManager;
class PopupManager;

/**
 * @class RomLifecycleManager
 * @brief Manages ROM and project persistence state
 *
 * Extracted from EditorManager to consolidate all ROM/project
 * save/load state management, write-policy enforcement, backup
 * management, and confirmation dialogs into a single component.
 *
 * EditorManager delegates ROM operations to this class and receives
 * results/errors back. The manager does NOT own the ROM or project
 * objects - those live in the session hierarchy.
 */
class RomLifecycleManager {
 public:
  enum class PotItemSaveDecision {
    kSaveWithPotItems,
    kSaveWithoutPotItems,
    kCancel
  };

  struct Dependencies {
    RomFileManager* rom_file_manager = nullptr;
    SessionCoordinator* session_coordinator = nullptr;
    ToastManager* toast_manager = nullptr;
    PopupManager* popup_manager = nullptr;
    project::YazeProject* project = nullptr;
  };

  RomLifecycleManager() = default;
  explicit RomLifecycleManager(Dependencies deps);
  ~RomLifecycleManager() = default;

  /// Late-initialize dependencies (for when deps aren't available at
  /// construction time, e.g. EditorManager's member initialization).
  void Initialize(Dependencies deps);

  // =========================================================================
  // ROM Hash & Write Policy
  // =========================================================================

  /// Recompute the hash of the current ROM.
  void UpdateCurrentRomHash(Rom* rom);

  /// Get the cached ROM hash string.
  const std::string& current_rom_hash() const { return current_rom_hash_; }

  /// Check whether the ROM hash mismatches the project's expected hash.
  bool IsRomHashMismatch() const;

  /// Validate that the loaded ROM is a safe project target to open/edit.
  absl::Status CheckRomOpenPolicy(Rom* rom);

  /// Enforce project write policy; may set pending_rom_write_confirm.
  absl::Status CheckRomWritePolicy(Rom* rom);

  /// Run Oracle-specific ROM safety preflight before saving.
  absl::Status CheckOracleRomSafetyPreSave(Rom* rom);

  // =========================================================================
  // ROM Write Confirmation State Machine
  // =========================================================================

  bool IsRomWriteConfirmPending() const { return pending_rom_write_confirm_; }
  void ConfirmRomWrite();
  void CancelRomWriteConfirm();
  void AllowBuildOutputOpenOnce() { bypass_build_output_open_once_ = true; }

  // =========================================================================
  // Write Conflict Management (ASM-owned address protection)
  // =========================================================================

  const std::vector<core::WriteConflict>& pending_write_conflicts() const {
    return pending_write_conflicts_;
  }
  bool ShouldBypassWriteConflict() const { return bypass_write_conflict_once_; }
  void BypassWriteConflictOnce() { bypass_write_conflict_once_ = true; }
  void ClearPendingWriteConflicts() { pending_write_conflicts_.clear(); }
  void SetPendingWriteConflicts(std::vector<core::WriteConflict> conflicts) {
    pending_write_conflicts_ = std::move(conflicts);
  }
  void ConsumeWriteConflictBypass() {
    bypass_write_conflict_once_ = false;
    pending_write_conflicts_.clear();
  }

  // =========================================================================
  // Pot Item Save Confirmation
  // =========================================================================

  bool HasPendingPotItemSaveConfirmation() const {
    return pending_pot_item_save_confirm_;
  }
  int pending_pot_item_unloaded_rooms() const {
    return pending_pot_item_unloaded_rooms_;
  }
  int pending_pot_item_total_rooms() const {
    return pending_pot_item_total_rooms_;
  }

  /// Set pot-item confirmation pending (called by SaveRom when needed).
  void SetPotItemConfirmPending(int unloaded_rooms, int total_rooms);

  /// Resolve the pot item confirmation dialog.
  /// If decision is kSaveWithPotItems or kSaveWithoutPotItems, the
  /// caller should invoke SaveRom() again (with the appropriate bypass).
  PotItemSaveDecision ResolvePotItemSaveConfirmation(
      PotItemSaveDecision decision);

  bool ShouldBypassPotItemConfirm() const {
    return bypass_pot_item_confirm_once_;
  }
  bool ShouldSuppressPotItemSave() const {
    return suppress_pot_item_save_once_;
  }
  void ClearPotItemBypass() {
    bypass_pot_item_confirm_once_ = false;
    suppress_pot_item_save_once_ = false;
  }

  // =========================================================================
  // Backup Management (delegates to RomFileManager)
  // =========================================================================

  std::vector<RomFileManager::BackupEntry> GetRomBackups(Rom* rom) const;
  absl::Status PruneRomBackups(Rom* rom);

  /// Apply default backup policy from user settings.
  void ApplyDefaultBackupPolicy(bool enabled, const std::string& folder,
                                int retention_count, bool keep_daily,
                                int keep_daily_days);

 private:
  // Dependencies (non-owning pointers)
  RomFileManager* rom_file_manager_ = nullptr;
  SessionCoordinator* session_coordinator_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  PopupManager* popup_manager_ = nullptr;
  project::YazeProject* project_ = nullptr;

  // ROM hash state
  std::string current_rom_hash_;
  std::string current_rom_path_;

  // Write confirmation state machine
  bool pending_rom_write_confirm_ = false;
  bool bypass_rom_write_confirm_once_ = false;
  bool bypass_build_output_open_once_ = false;

  // Pot item save confirmation state
  bool pending_pot_item_save_confirm_ = false;
  int pending_pot_item_unloaded_rooms_ = 0;
  int pending_pot_item_total_rooms_ = 0;
  bool bypass_pot_item_confirm_once_ = false;
  bool suppress_pot_item_save_once_ = false;

  // Write conflict state (ASM-owned address protection)
  std::vector<core::WriteConflict> pending_write_conflicts_;
  bool bypass_write_conflict_once_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_ROM_LIFECYCLE_MANAGER_H_
