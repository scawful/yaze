#ifndef YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_

#include <string>

#include "absl/status/status.h"
#include "app/rom.h"

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

  // Utility helpers
  bool IsRomLoaded(Rom* rom) const;
  std::string GetRomFilename(Rom* rom) const;

 private:
  ToastManager* toast_manager_ = nullptr;

  absl::Status LoadRomFromFile(Rom* rom, const std::string& filename);
  std::string GenerateBackupFilename(
      const std::string& original_filename) const;
  bool IsValidRomFile(const std::string& filename) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_
