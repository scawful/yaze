#include "rom_file_manager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "absl/strings/str_format.h"
#include "app/editor/ui/toast_manager.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/log.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

namespace {

std::time_t ToTimeT(const std::filesystem::file_time_type& ftime) {
  using namespace std::chrono;
  auto sctp = time_point_cast<system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() +
      system_clock::now());
  return system_clock::to_time_t(sctp);
}

std::string DayKey(std::time_t timestamp) {
  std::tm local_tm{};
#ifdef _WIN32
  localtime_s(&local_tm, &timestamp);
#else
  localtime_r(&timestamp, &local_tm);
#endif
  char buffer[16];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local_tm);
  return std::string(buffer);
}

}  // namespace

RomFileManager::RomFileManager(ToastManager* toast_manager)
    : toast_manager_(toast_manager) {}

absl::Status RomFileManager::LoadRom(Rom* rom, const std::string& filename) {
  if (!rom) {
    return absl::InvalidArgumentError("ROM pointer cannot be null");
  }
  if (filename.empty()) {
    return absl::InvalidArgumentError("No filename provided");
  }
  return LoadRomFromFile(rom, filename);
}

absl::Status RomFileManager::SaveRom(Rom* rom) {
  if (!IsRomLoaded(rom)) {
    return absl::FailedPreconditionError("No ROM loaded to save");
  }

  if (backup_before_save_) {
    auto backup_status = CreateBackup(rom);
    if (!backup_status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Backup failed: %s", backup_status.message()),
            ToastType::kError);
      }
      return backup_status;
    }
  }

  Rom::SaveSettings settings;
  settings.backup = false;
  settings.save_new = false;

  auto status = rom->SaveToFile(settings);
  if (!status.ok() && toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to save ROM: %s", status.message()),
        ToastType::kError);
  } else if (toast_manager_) {
    toast_manager_->Show("ROM saved successfully", ToastType::kSuccess);
  }
  return status;
}

absl::Status RomFileManager::SaveRomAs(Rom* rom, const std::string& filename) {
  if (!IsRomLoaded(rom)) {
    return absl::FailedPreconditionError("No ROM loaded to save");
  }
  if (filename.empty()) {
    return absl::InvalidArgumentError("No filename provided for save as");
  }

  if (backup_before_save_) {
    auto backup_status = CreateBackup(rom);
    if (!backup_status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Backup failed: %s", backup_status.message()),
            ToastType::kError);
      }
      return backup_status;
    }
  }

  Rom::SaveSettings settings;
  settings.backup = false;
  settings.save_new = true;
  settings.filename = filename;
  // settings.z3_save = true; // Deprecated

  auto status = rom->SaveToFile(settings);
  if (!status.ok() && toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to save ROM as: %s", status.message()),
        ToastType::kError);
  } else if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("ROM saved as: %s", filename),
                         ToastType::kSuccess);
  }
  return status;
}

absl::Status RomFileManager::OpenRomOrProject(Rom* rom,
                                              const std::string& filename) {
  if (!rom) {
    return absl::InvalidArgumentError("ROM pointer cannot be null");
  }
  if (filename.empty()) {
    return absl::InvalidArgumentError("No filename provided");
  }

  std::string extension = std::filesystem::path(filename).extension().string();

  if (extension == ".yaze" || extension == ".json") {
    return absl::UnimplementedError("Project file loading not yet implemented");
  }

  return LoadRom(rom, filename);
}

absl::Status RomFileManager::CreateBackup(Rom* rom) {
  if (!IsRomLoaded(rom)) {
    return absl::FailedPreconditionError("No ROM loaded to backup");
  }

  std::string backup_filename = GenerateBackupFilename(rom->filename());

  Rom::SaveSettings settings;
  settings.backup = false;
  settings.filename = backup_filename;
  // settings.z3_save = true; // Deprecated

  auto status = rom->SaveToFile(settings);
  if (!status.ok() && toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to create backup: %s", status.message()),
        ToastType::kError);
  } else if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Backup created: %s", backup_filename),
                         ToastType::kSuccess);
  }
  if (status.ok()) {
    auto prune_status = PruneBackups(rom->filename());
    if (!prune_status.ok()) {
      LOG_WARN("RomFileManager", "Backup prune failed: %s",
               prune_status.message().data());
    }
  }
  return status;
}

absl::Status RomFileManager::ValidateRom(Rom* rom) {
  if (!IsRomLoaded(rom)) {
    return absl::FailedPreconditionError("No valid ROM to validate");
  }

  if (rom->size() < 512 * 1024 || rom->size() > 8 * 1024 * 1024) {
    return absl::InvalidArgumentError("ROM size is outside expected range");
  }
  if (rom->title().empty()) {
    return absl::InvalidArgumentError("ROM title is empty or invalid");
  }

  if (toast_manager_) {
    toast_manager_->Show("ROM validation passed", ToastType::kSuccess);
  }
  return absl::OkStatus();
}

bool RomFileManager::IsRomLoaded(Rom* rom) const {
  return rom && rom->is_loaded();
}

std::string RomFileManager::GetRomFilename(Rom* rom) const {
  if (!IsRomLoaded(rom)) {
    return "";
  }
  return rom->filename();
}

absl::Status RomFileManager::LoadRomFromFile(Rom* rom,
                                             const std::string& filename) {
  if (!rom) {
    return absl::InvalidArgumentError("ROM pointer cannot be null");
  }
  if (!IsValidRomFile(filename)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid ROM file: %s", filename));
  }

  auto status = rom->LoadFromFile(filename);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to load ROM: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }

  // IMPORTANT: Game data loading is now decoupled and should be handled
  // by the caller (EditorManager) or a higher-level orchestration layer
  // that manages both the generic Rom and the Zelda3::GameData.
  // This class is strictly for generic ROM file operations.

  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("ROM loaded: %s", rom->title()),
                         ToastType::kSuccess);
  }
  return absl::OkStatus();
}

std::string RomFileManager::GenerateBackupFilename(
    const std::string& original_filename) const {
  std::filesystem::path path(original_filename);
  std::string stem = path.stem().string();
  std::string extension = path.extension().string();

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::filesystem::path backup_dir = GetBackupDirectory(original_filename);

  std::string filename =
      absl::StrFormat("%s_backup_%ld%s", stem, time_t, extension);
  return (backup_dir / filename).string();
}

std::filesystem::path RomFileManager::GetBackupDirectory(
    const std::string& original_filename) const {
  std::filesystem::path path(original_filename);
  std::filesystem::path backup_dir = path.parent_path();
  if (!backup_folder_.empty()) {
    backup_dir = std::filesystem::path(backup_folder_);
  }
  std::error_code ec;
  std::filesystem::create_directories(backup_dir, ec);
  return backup_dir;
}

std::vector<RomFileManager::BackupEntry> RomFileManager::ListBackups(
    const std::string& rom_filename) const {
  std::vector<BackupEntry> backups;
  if (rom_filename.empty()) {
    return backups;
  }

  std::filesystem::path rom_path(rom_filename);
  const std::string stem = rom_path.stem().string();
  const std::string extension = rom_path.extension().string();
  const std::string prefix = stem + "_backup_";
  std::filesystem::path backup_dir = GetBackupDirectory(rom_filename);

  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(backup_dir, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const auto path = entry.path();
    if (path.extension() != extension) {
      continue;
    }
    const std::string filename = path.filename().string();
    if (filename.rfind(prefix, 0) != 0) {
      continue;
    }

    BackupEntry backup;
    backup.path = path.string();
    backup.filename = filename;
    backup.size_bytes = entry.file_size(ec);
    auto ftime = entry.last_write_time(ec);
    if (!ec) {
      backup.timestamp = ToTimeT(ftime);
    }
    backups.push_back(std::move(backup));
  }

  std::sort(backups.begin(), backups.end(),
            [](const BackupEntry& a, const BackupEntry& b) {
              return a.timestamp > b.timestamp;
            });
  return backups;
}

absl::Status RomFileManager::PruneBackups(
    const std::string& rom_filename) const {
  if (backup_retention_count_ <= 0) {
    return absl::OkStatus();
  }

  auto backups = ListBackups(rom_filename);
  if (backups.size() <= static_cast<size_t>(backup_retention_count_)) {
    return absl::OkStatus();
  }

  std::unordered_set<std::string> keep_paths;
  const auto now = std::chrono::system_clock::now();
  const auto keep_daily_days =
      std::chrono::hours(24 * std::max(1, backup_keep_daily_days_));

  if (backup_keep_daily_) {
    std::unordered_set<std::string> seen_days;
    for (const auto& backup : backups) {
      if (backup.timestamp == 0) {
        continue;
      }
      const auto backup_time =
          std::chrono::system_clock::from_time_t(backup.timestamp);
      if (now - backup_time > keep_daily_days) {
        continue;
      }
      std::string day = DayKey(backup.timestamp);
      if (seen_days.insert(day).second) {
        keep_paths.insert(backup.path);
      }
    }
  }

  for (size_t i = 0; i < backups.size() &&
                     keep_paths.size() < static_cast<size_t>(backup_retention_count_);
       ++i) {
    keep_paths.insert(backups[i].path);
  }

  for (const auto& backup : backups) {
    if (keep_paths.count(backup.path) > 0) {
      continue;
    }
    std::error_code remove_ec;
    std::filesystem::remove(backup.path, remove_ec);
    if (remove_ec) {
      LOG_WARN("RomFileManager", "Failed to delete backup: %s",
               backup.path.c_str());
    }
  }

  return absl::OkStatus();
}

bool RomFileManager::IsValidRomFile(const std::string& filename) const {
  if (filename.empty()) {
    return false;
  }

  std::error_code ec;
  if (!std::filesystem::exists(filename, ec) || ec) {
    return false;
  }

  auto file_size = std::filesystem::file_size(filename, ec);
  if (ec) {
    return false;
  }
  // Zelda 3 ROMs are 1MB (0x100000 = 1,048,576 bytes), possibly with 512-byte
  // SMC header. Allow ROMs from 512KB to 8MB to be safe.
  if (file_size < 512 * 1024 || file_size > 8 * 1024 * 1024) {
    return false;
  }

  return true;
}

}  // namespace yaze::editor
