#include "rom_file_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"
#include "util/file_util.h"

namespace yaze::editor {

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

  Rom::SaveSettings settings;
  settings.backup = true;
  settings.save_new = false;
  settings.z3_save = true;

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

  Rom::SaveSettings settings;
  settings.backup = false;
  settings.save_new = true;
  settings.filename = filename;
  settings.z3_save = true;

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
  settings.backup = true;
  settings.filename = backup_filename;
  settings.z3_save = true;

  auto status = rom->SaveToFile(settings);
  if (!status.ok() && toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to create backup: %s", status.message()),
        ToastType::kError);
  } else if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Backup created: %s", backup_filename),
                         ToastType::kSuccess);
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

  return absl::StrFormat("%s_backup_%ld%s", stem, time_t, extension);
}

bool RomFileManager::IsValidRomFile(const std::string& filename) const {
  if (filename.empty()) {
    return false;
  }

  if (!std::filesystem::exists(filename)) {
    return false;
  }

  auto file_size = std::filesystem::file_size(filename);
  if (file_size < 1024 * 1024 || file_size > 8 * 1024 * 1024) {
    return false;
  }

  return true;
}

}  // namespace yaze::editor
