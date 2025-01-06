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
      const std::string &folder_path);
  static std::vector<std::string> GetFilesInFolder(
      const std::string &folder_path);
};

/**
 * @brief GetBundleResourcePath returns the path to the bundle resource
 * directory. Specific to MacOS.
 */
std::string GetBundleResourcePath();

enum class Platform { kUnknown, kMacOS, kiOS, kWindows, kLinux };

std::string GetFileExtension(const std::string &filename);
std::string GetFileName(const std::string &filename);
std::string LoadFile(const std::string &filename);
std::string LoadConfigFile(const std::string &filename);
std::string GetConfigDirectory(Platform platform);

void SaveFile(const std::string &filename, const std::string &data,
              Platform platform);

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
