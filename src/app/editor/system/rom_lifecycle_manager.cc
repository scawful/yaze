#include "app/editor/system/rom_lifecycle_manager.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"

namespace yaze::editor {

namespace {

std::string NormalizeHash(std::string value) {
  absl::AsciiStrToLower(&value);
  if (absl::StartsWith(value, "0x")) {
    value = value.substr(2);
  }
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char c) { return std::isspace(c); }),
              value.end());
  return value;
}

}  // namespace

RomLifecycleManager::RomLifecycleManager(Dependencies deps)
    : rom_file_manager_(deps.rom_file_manager),
      session_coordinator_(deps.session_coordinator),
      toast_manager_(deps.toast_manager),
      popup_manager_(deps.popup_manager),
      project_(deps.project) {}

void RomLifecycleManager::Initialize(Dependencies deps) {
  rom_file_manager_ = deps.rom_file_manager;
  session_coordinator_ = deps.session_coordinator;
  toast_manager_ = deps.toast_manager;
  popup_manager_ = deps.popup_manager;
  project_ = deps.project;
}

// =============================================================================
// ROM Hash & Write Policy
// =============================================================================

void RomLifecycleManager::UpdateCurrentRomHash(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    current_rom_hash_.clear();
    return;
  }
  current_rom_hash_ = util::ComputeRomHash(rom->data(), rom->size());
}

bool RomLifecycleManager::IsRomHashMismatch() const {
  if (!project_ || !project_->project_opened()) {
    return false;
  }
  const auto expected = NormalizeHash(project_->rom_metadata.expected_hash);
  const auto actual = NormalizeHash(current_rom_hash_);
  if (expected.empty() || actual.empty()) {
    return false;
  }
  return expected != actual;
}

absl::Status RomLifecycleManager::CheckRomWritePolicy(Rom* /*rom*/) {
  if (!project_ || !project_->project_opened()) {
    return absl::OkStatus();
  }

  const auto policy = project_->rom_metadata.write_policy;
  const auto expected = NormalizeHash(project_->rom_metadata.expected_hash);
  const auto actual = NormalizeHash(current_rom_hash_);

  if (expected.empty() || actual.empty() || expected == actual) {
    return absl::OkStatus();
  }

  if (policy == project::RomWritePolicy::kAllow) {
    return absl::OkStatus();
  }

  if (policy == project::RomWritePolicy::kBlock) {
    if (toast_manager_) {
      toast_manager_->Show(
          "ROM write blocked: project expects a different ROM hash",
          ToastType::kError);
    }
    return absl::PermissionDeniedError(
        "ROM write blocked by project write policy");
  }

  if (!bypass_rom_write_confirm_once_) {
    pending_rom_write_confirm_ = true;
    if (popup_manager_) {
      popup_manager_->Show(PopupID::kRomWriteConfirm);
    }
    if (toast_manager_) {
      toast_manager_->Show("ROM hash mismatch: confirmation required to save",
                           ToastType::kWarning);
    }
    return absl::CancelledError("ROM write confirmation required");
  }

  return absl::OkStatus();
}

absl::Status RomLifecycleManager::CheckOracleRomSafetyPreSave(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  if (!project_ || !project_->project_opened() ||
      !project_->hack_manifest.loaded() ||
      !project_->hack_manifest.HasProjectRegistry()) {
    return absl::OkStatus();
  }

  zelda3::OracleRomSafetyPreflightOptions options;
  options.require_water_fill_reserved_region = true;
  options.require_custom_collision_write_support = false;
  options.validate_water_fill_table = true;
  options.validate_custom_collision_maps = true;
  options.max_collision_errors = 6;

  const auto preflight = zelda3::RunOracleRomSafetyPreflight(rom, options);
  if (preflight.ok()) {
    return absl::OkStatus();
  }

  const auto& first = preflight.errors.front();
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Oracle ROM safety [%s]: %s",
                                         first.code, first.message.c_str()),
                         ToastType::kError);
  }
  return preflight.ToStatus();
}

// =============================================================================
// ROM Write Confirmation State Machine
// =============================================================================

void RomLifecycleManager::ConfirmRomWrite() {
  bypass_rom_write_confirm_once_ = true;
  pending_rom_write_confirm_ = false;
}

void RomLifecycleManager::CancelRomWriteConfirm() {
  bypass_rom_write_confirm_once_ = false;
  pending_rom_write_confirm_ = false;
}

// =============================================================================
// Pot Item Save Confirmation
// =============================================================================

void RomLifecycleManager::SetPotItemConfirmPending(int unloaded_rooms,
                                                   int total_rooms) {
  pending_pot_item_save_confirm_ = true;
  pending_pot_item_unloaded_rooms_ = unloaded_rooms;
  pending_pot_item_total_rooms_ = total_rooms;
}

RomLifecycleManager::PotItemSaveDecision
RomLifecycleManager::ResolvePotItemSaveConfirmation(
    PotItemSaveDecision decision) {
  pending_pot_item_save_confirm_ = false;
  pending_pot_item_unloaded_rooms_ = 0;
  pending_pot_item_total_rooms_ = 0;

  if (decision == PotItemSaveDecision::kCancel) {
    return decision;
  }

  if (decision == PotItemSaveDecision::kSaveWithoutPotItems) {
    suppress_pot_item_save_once_ = true;
  }
  bypass_pot_item_confirm_once_ = true;
  return decision;
}

// =============================================================================
// Backup Management
// =============================================================================

std::vector<RomFileManager::BackupEntry> RomLifecycleManager::GetRomBackups(
    Rom* rom) const {
  if (!rom || !rom_file_manager_) {
    return {};
  }
  return rom_file_manager_->ListBackups(rom->filename());
}

absl::Status RomLifecycleManager::PruneRomBackups(Rom* rom) {
  if (!rom) {
    return absl::FailedPreconditionError("No ROM loaded");
  }
  if (!rom_file_manager_) {
    return absl::FailedPreconditionError("No ROM file manager");
  }
  return rom_file_manager_->PruneBackups(rom->filename());
}

void RomLifecycleManager::ApplyDefaultBackupPolicy(
    bool enabled, const std::string& folder, int retention_count,
    bool keep_daily, int keep_daily_days) {
  if (!rom_file_manager_) return;
  rom_file_manager_->SetBackupBeforeSave(enabled);
  rom_file_manager_->SetBackupFolder(folder);
  rom_file_manager_->SetBackupRetentionCount(retention_count);
  rom_file_manager_->SetBackupKeepDaily(keep_daily);
  rom_file_manager_->SetBackupKeepDailyDays(keep_daily_days);
}

}  // namespace yaze::editor
