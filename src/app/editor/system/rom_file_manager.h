#ifndef YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

class ToastManager;

/**
 * @class RomFileManager
 * @brief Handles all ROM file I/O operations
 *
 * Extracted from EditorManager to provide focused ROM file management:
 * - ROM loading and saving
 * - Asset loading
 * - ROM validation and backup
 * - File path management
 */
class RomFileManager {
 public:
  explicit RomFileManager(ToastManager* toast_manager);
  ~RomFileManager() = default;

  // ROM file operations
  absl::Status LoadRom(Rom* rom, const std::string& filename);
  absl::Status SaveRom(Rom* rom);
  absl::Status SaveRomAs(Rom* rom, const std::string& filename);
  absl::Status OpenRomOrProject(Rom* rom, const std::string& filename);
  absl::Status CreateBackup(Rom* rom);
  absl::Status ValidateRom(Rom* rom);

  void SetBackupFolder(const std::string& folder) { backup_folder_ = folder; }
  void SetBackupBeforeSave(bool enabled) { backup_before_save_ = enabled; }
  void SetBackupRetentionCount(int count) { backup_retention_count_ = count; }
  void SetBackupKeepDaily(bool enabled) { backup_keep_daily_ = enabled; }
  void SetBackupKeepDailyDays(int days) { backup_keep_daily_days_ = days; }

  struct BackupEntry {
    std::string path;
    std::string filename;
    std::time_t timestamp = 0;
    uintmax_t size_bytes = 0;
  };

  std::vector<BackupEntry> ListBackups(
      const std::string& rom_filename) const;
  absl::Status PruneBackups(const std::string& rom_filename) const;

  // Utility helpers
  bool IsRomLoaded(Rom* rom) const;
  std::string GetRomFilename(Rom* rom) const;

 private:
  ToastManager* toast_manager_ = nullptr;

  absl::Status LoadRomFromFile(Rom* rom, const std::string& filename);
  std::string GenerateBackupFilename(
      const std::string& original_filename) const;
  std::filesystem::path GetBackupDirectory(
      const std::string& original_filename) const;
  bool IsValidRomFile(const std::string& filename) const;

  bool backup_before_save_ = true;
  std::string backup_folder_;
  int backup_retention_count_ = 20;
  bool backup_keep_daily_ = true;
  int backup_keep_daily_days_ = 14;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_
