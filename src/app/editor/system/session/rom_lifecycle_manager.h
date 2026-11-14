#ifndef YAZE_APP_EDITOR_SYSTEM_ROM_LIFECYCLE_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_ROM_LIFECYCLE_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/session_types.h"
#include "app/editor/system/session/project_build_workflow.h"
#include "app/editor/system/session/rom_file_manager.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"
#include "util/rom_hash.h"

namespace yaze::editor {

class SessionCoordinator;
class ToastManager;
class PopupManager;
struct RomSession;

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

  /// Select the project policy used by subsequent hash/write/safety checks.
  /// The caller retains ownership and must keep the project alive.
  void SetProjectContext(const project::YazeProject* project) {
    project_ = project;
  }

  // =========================================================================
  // ROM Hash & Write Policy
  // =========================================================================

  /// Restore a session's captured baseline, or clear identity for no/empty ROM.
  /// Never hashes mutable ROM bytes during activation.
  void ActivateRomIdentity(const RomSession* session);

  /// Get the cached ROM hash string.
  const std::string& current_rom_hash() const {
    return current_rom_identity_.crc32;
  }

  /// Check whether the ROM hash mismatches the project's expected hash.
  bool IsRomHashMismatch() const;

  /// Admit a freshly loaded native page Tile16 (revision 5) ROM for a saved
  /// project that opted in. The ROM must be the project's ROM backing file and
  /// match its saved expected hash; otherwise revision 5 keeps refusing.
  /// Call after the load reset and before any layout, GameData or overworld use.
  static bool AdmitProjectPageTile16(const project::YazeProject& project,
                                     Rom* rom);

  /// Validate that the loaded ROM is a safe project target to open/edit.
  absl::Status CheckRomOpenPolicy(Rom* rom);

  /// Validate a project ROM rebind before changing its descriptor or models.
  /// Guarded projects with a fixed manifest development ROM retain that exact
  /// backing file; changing rom_filename cannot change the manifest contract.
  absl::Status CheckRomRebindPolicy(Rom* rom);

  /// Enforce project write policy; may set pending_rom_write_confirm.
  absl::Status CheckRomWritePolicy(
      Rom* rom,
      const std::optional<std::string>& target_filename = std::nullopt);

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
  void CancelPotItemSaveConfirmation() {
    pending_pot_item_save_confirm_ = false;
    pending_pot_item_unloaded_rooms_ = 0;
    pending_pot_item_total_rooms_ = 0;
    ClearPotItemBypass();
  }
  void ClearPotItemBypass() {
    bypass_pot_item_confirm_once_ = false;
    suppress_pot_item_save_once_ = false;
  }

  /// Clear every confirmation and one-shot bypass associated with a logical
  /// save request. This must be called when that request succeeds, fails, is
  /// cancelled, or loses its originating ROM session.
  void ResetPendingSaveState() {
    CancelRomWriteConfirm();
    CancelPotItemSaveConfirmation();
    pending_write_conflicts_.clear();
    bypass_write_conflict_once_ = false;
  }

  // =========================================================================
  // Pending ROM save request (target/session/ROM binding)
  // =========================================================================

  bool HasPendingRomSave() const { return pending_rom_save_.has_value(); }

  bool HasPendingRomSaveConfirmation() const {
    return IsRomWriteConfirmPending() || HasPendingPotItemSaveConfirmation() ||
           !pending_write_conflicts_.empty();
  }

  bool HasBlockingSaveConfirmation() const {
    return IsRomWriteConfirmPending() || HasPendingPotItemSaveConfirmation() ||
           (!pending_write_conflicts_.empty() && !ShouldBypassWriteConflict());
  }

  bool PendingRomSaveMatchesSession(size_t session_index, Rom* rom) const;

  void CancelPendingRomSaveRequest();

  void StartPendingRomSave(const std::optional<std::string>& save_as_filename,
                           size_t session_index, Rom* rom);

  void FinishPendingRomSaveAttempt(const absl::Status& status);

  std::optional<std::string> pending_rom_save_as_filename() const;

  // =========================================================================
  // Project editing work before ROM (session-owned sequencing)
  // =========================================================================

  struct ProjectEditingSaveBinding {
    size_t session_id = SIZE_MAX;
    project::YazeProject project_snapshot;
    Rom* rom_object = nullptr;
    std::optional<RomBaselineIdentity> rom_baseline;
    std::string rom_backing_path;
    bool rom_was_loaded_at_capture = false;
    bool session_has_pending_rom_work = false;
  };

  struct ActiveProjectEditingSaveCallbacks {
    std::function<absl::Status(const ProjectEditingSaveBinding&)>
        capture_editing_state;
    std::function<absl::Status(const ProjectEditingSaveBinding&)>
        save_project_descriptor;
    std::function<absl::Status(const ProjectEditingSaveBinding&)>
        save_raw_project_file;
    std::function<absl::StatusOr<bool>(const ProjectEditingSaveBinding&)>
        raw_draft_targets_current_project;
  };

  bool ShouldDeferProjectNativeCompletion() const {
    return defer_project_native_completion_;
  }

  absl::StatusOr<ProjectEditingSaveBinding> CaptureProjectEditingSaveBinding(
      size_t session_id, bool session_has_pending_rom_work,
      const project::YazeProject& live_project) const;

  absl::Status ValidateLiveProjectIdentity(
      const ProjectEditingSaveBinding& binding,
      const project::YazeProject& live_project) const;

  absl::Status SaveActiveProjectEditingWork(
      const ProjectEditingSaveBinding& binding, bool allow_dialogs,
      const ActiveProjectEditingSaveCallbacks& callbacks);

  absl::Status SaveProjectWorkBeforeRomSave(
      const ProjectEditingSaveBinding& binding, bool allow_dialogs,
      const ActiveProjectEditingSaveCallbacks& callbacks);

  // =========================================================================
  // Saved-input identity for Build / Run freshness joins
  // =========================================================================

  struct ProjectOriginSnapshot {
    size_t session_id = SIZE_MAX;
    std::string project_descriptor_path;
    std::string editing_source_path;
    std::optional<RomBaselineIdentity> rom_baseline;
    bool session_open = false;
  };

  absl::StatusOr<ProjectOriginSnapshot> ResolveProjectOriginSnapshot(
      size_t session_id) const;

  absl::Status ValidateActiveProjectOrigin(size_t expected_session_id) const;

  absl::StatusOr<ProjectSavedInputIdentity> CaptureSavedBuildInputIdentity(
      const project::YazeProject& project,
      const std::string& editing_source_path,
      bool native_save_completion_pending) const;

  absl::Status SavedBuildInputIdentityMatchesLive(
      const ProjectSavedInputIdentity& captured,
      const project::YazeProject& project,
      const std::string& editing_source_path,
      bool native_save_completion_pending) const;

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
  static absl::Status CompareBoundProjectPaths(const std::string& lhs,
                                               const std::string& rhs);

  static absl::Status ValidateProjectEditingSaveBinding(
      const RomSession& session, const ProjectEditingSaveBinding& binding);

  absl::StatusOr<RomSession*> ReacquireActiveProjectEditingSaveSession(
      const ProjectEditingSaveBinding& binding) const;

  absl::Status SaveActiveProjectEditingWorkImpl(
      const ProjectEditingSaveBinding& binding, bool allow_dialogs,
      const ActiveProjectEditingSaveCallbacks& callbacks);

  struct PendingRomSaveRequest {
    std::optional<std::string> save_as_filename;
    size_t session_index = SIZE_MAX;
    Rom* rom = nullptr;
  };

  bool HasEditableSource(const std::string& source_path) const;

  // Dependencies (non-owning pointers)
  RomFileManager* rom_file_manager_ = nullptr;
  SessionCoordinator* session_coordinator_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  PopupManager* popup_manager_ = nullptr;
  const project::YazeProject* project_ = nullptr;

  // Active view of the session-owned baseline; replaced on every activation.
  util::RomIdentityHashes current_rom_identity_;
  std::string current_rom_path_;
  std::string current_rom_editing_source_path_;

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

  std::optional<PendingRomSaveRequest> pending_rom_save_;

  // While project work runs ahead of a ROM save, defer the native receipt until
  // the ROM write publishes one receipt for both.
  bool defer_project_native_completion_ = false;
  bool project_save_sequence_active_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_ROM_LIFECYCLE_MANAGER_H_
