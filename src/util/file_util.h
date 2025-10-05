#ifndef YAZE_UTIL_FILE_UTIL_H_
#define YAZE_UTIL_FILE_UTIL_H_

#include <string>
#include <vector>

namespace yaze {
namespace util {

class FileDialogWrapper {
 public:
  /**
   * @brief ShowOpenFileDialog opens a file dialog and returns the selected
   * filepath. Uses global feature flag to choose implementation.
   */
  static std::string ShowOpenFileDialog();

  /**
   * @brief ShowOpenFolderDialog opens a file dialog and returns the selected
   * folder path. Uses global feature flag to choose implementation.
   */
  static std::string ShowOpenFolderDialog();

  /**
   * @brief ShowSaveFileDialog opens a save file dialog and returns the selected
   * filepath. Uses global feature flag to choose implementation.
   */
  static std::string ShowSaveFileDialog(const std::string& default_name = "", 
                                       const std::string& default_extension = "");
  
  // Specific implementations for testing
  static std::string ShowOpenFileDialogNFD();
  static std::string ShowOpenFileDialogBespoke();
  static std::string ShowSaveFileDialogNFD(const std::string& default_name = "", 
                                          const std::string& default_extension = "");
  static std::string ShowSaveFileDialogBespoke(const std::string& default_name = "", 
                                              const std::string& default_extension = "");
  static std::string ShowOpenFolderDialogNFD();
  static std::string ShowOpenFolderDialogBespoke();
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
std::string GetConfigDirectory();
bool EnsureConfigDirectoryExists();
std::string ExpandHomePath(const std::string& path);
std::string GetResourcePath(const std::string &resource_path);
void SaveFile(const std::string &filename, const std::string &data);

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_FILE_UTIL_H_
