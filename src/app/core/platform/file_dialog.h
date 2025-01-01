#ifndef YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
#define YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H

#include <string>
#include <vector>

namespace yaze {
namespace core {

class FileDialogWrapper {
 public:
  /**
   * @brief ShowOpenFileDialog opens a file dialog and returns the selected
   * filepath.
   */
  static std::string ShowOpenFileDialog();

  /**
   * @brief ShowOpenFolderDialog opens a file dialog and returns the selected
   * folder path.
   */
  static std::string ShowOpenFolderDialog();
  static std::vector<std::string> GetSubdirectoriesInFolder(
      const std::string& folder_path);
  static std::vector<std::string> GetFilesInFolder(
      const std::string& folder_path);
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
