#include "rom_file_manager.h"

#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

RomFileManager::RomFileManager(ToastManager* toast_manager)
    : toast_manager_(toast_manager) {
}

absl::Status RomFileManager::LoadRom(const std::string& filename) {
  if (filename.empty()) {
    // TODO: Show file dialog
    return absl::InvalidArgumentError("No filename provided");
  }
  
  return LoadRomFromFile(filename);
}

absl::Status RomFileManager::LoadRomFromFile(const std::string& filename) {
  if (!IsValidRomFile(filename)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid ROM file: %s", filename));
  }
  
  // Create new ROM instance
  Rom new_rom;
  auto status = new_rom.LoadFromFile(filename);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to load ROM: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }
  
  // Set as current ROM
  current_rom_ = &new_rom;
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("ROM loaded: %s", new_rom.title()),
        ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status RomFileManager::SaveRom() {
  if (!IsRomLoaded()) {
    return absl::FailedPreconditionError("No ROM loaded to save");
  }
  
  Rom::SaveSettings settings;
  settings.backup = true;
  settings.save_new = false;
  settings.z3_save = true;
  
  auto status = current_rom_->SaveToFile(settings);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to save ROM: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }
  
  if (toast_manager_) {
    toast_manager_->Show("ROM saved successfully", ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status RomFileManager::SaveRomAs(const std::string& filename) {
  if (!IsRomLoaded()) {
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
  
  auto status = current_rom_->SaveToFile(settings);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to save ROM as: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("ROM saved as: %s", filename),
        ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status RomFileManager::OpenRomOrProject(const std::string& filename) {
  if (filename.empty()) {
    return absl::InvalidArgumentError("No filename provided");
  }
  
  // Check if it's a project file or ROM file
  std::string extension = std::filesystem::path(filename).extension().string();
  
  if (extension == ".yaze" || extension == ".json") {
    // TODO: Handle project files
    return absl::UnimplementedError("Project file loading not yet implemented");
  } else {
    // Assume it's a ROM file
    return LoadRom(filename);
  }
}

absl::Status RomFileManager::LoadAssets() {
  if (!IsRomLoaded()) {
    return absl::FailedPreconditionError("No ROM loaded to load assets from");
  }
  
  // TODO: Implement asset loading logic
  // This would typically load graphics, music, etc. from the ROM
  
  if (toast_manager_) {
    toast_manager_->Show("Assets loaded", ToastType::kInfo);
  }
  
  return absl::OkStatus();
}

absl::Status RomFileManager::SetCurrentRom(Rom* rom) {
  if (!rom) {
    return absl::InvalidArgumentError("ROM pointer cannot be null");
  }
  
  current_rom_ = rom;
  return absl::OkStatus();
}

std::string RomFileManager::GetRomFilename() const {
  if (!IsRomLoaded()) {
    return "";
  }
  return current_rom_->filename();
}

std::string RomFileManager::GetRomTitle() const {
  if (!IsRomLoaded()) {
    return "";
  }
  return current_rom_->title();
}

absl::Status RomFileManager::ValidateRom() {
  return ValidateRom(current_rom_);
}

absl::Status RomFileManager::ValidateRom(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("No valid ROM to validate");
  }
  
  // TODO: Implement ROM validation logic
  // This would check ROM integrity, checksums, expected data structures, etc.
  
  // Basic validation: check if ROM size is reasonable
  if (rom->size() < 512 * 1024 || rom->size() > 8 * 1024 * 1024) {
    return absl::InvalidArgumentError("ROM size is outside expected range");
  }
  
  // Check if ROM title is readable
  if (rom->title().empty()) {
    return absl::InvalidArgumentError("ROM title is empty or invalid");
  }
  
  if (toast_manager_) {
    toast_manager_->Show("ROM validation passed", ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status RomFileManager::CreateBackup() {
  if (!IsRomLoaded()) {
    return absl::FailedPreconditionError("No ROM loaded to backup");
  }
  
  std::string backup_filename = GenerateBackupFilename(GetRomFilename());
  
  Rom::SaveSettings settings;
  settings.backup = true;
  settings.filename = backup_filename;
  settings.z3_save = true;
  
  auto status = current_rom_->SaveToFile(settings);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to create backup: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Backup created: %s", backup_filename),
        ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

std::string RomFileManager::GenerateBackupFilename(const std::string& original_filename) const {
  std::filesystem::path path(original_filename);
  std::string stem = path.stem().string();
  std::string extension = path.extension().string();
  
  // Add timestamp to make it unique
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
  
  // Check file size (SNES ROMs are typically 2MB, 3MB, 4MB, 6MB, etc.)
  auto file_size = std::filesystem::file_size(filename);
  if (file_size < 1024 * 1024 || file_size > 8 * 1024 * 1024) {
    return false;
  }
  
  // TODO: Add more ROM validation (header checks, etc.)
  
  return true;
}

}  // namespace editor
}  // namespace yaze
