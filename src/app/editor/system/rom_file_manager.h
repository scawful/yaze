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
  absl::Status LoadRom(const std::string& filename = "");
  absl::Status SaveRom();
  absl::Status SaveRomAs(const std::string& filename);
  absl::Status OpenRomOrProject(const std::string& filename);
  
  // Asset operations
  absl::Status LoadAssets();
  
  // ROM state management
  absl::Status SetCurrentRom(Rom* rom);
  Rom* GetCurrentRom() const { return current_rom_; }
  
  // ROM information
  bool IsRomLoaded() const { return current_rom_ && current_rom_->is_loaded(); }
  std::string GetRomFilename() const;
  std::string GetRomTitle() const;
  
  // Validation and backup
  absl::Status ValidateRom();
  absl::Status CreateBackup();

 private:
  Rom* current_rom_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  
  // Helper methods
  absl::Status LoadRomFromFile(const std::string& filename);
  std::string GenerateBackupFilename(const std::string& original_filename) const;
  bool IsValidRomFile(const std::string& filename) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_ROM_FILE_MANAGER_H_
