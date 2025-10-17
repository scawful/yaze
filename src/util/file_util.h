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

/**
 * @brief Gets the file extension from a filename.
 * 
 * Uses std::filesystem for cross-platform consistency.
 *
 * @param filename The name of the file.
 * @return The file extension, or an empty string if none is found.
 */
std::string GetFileExtension(const std::string &filename);

/**
 * @brief Gets the filename from a full path.
 * 
 * Uses std::filesystem for cross-platform consistency.
 *
 * @param filename The full path to the file.
 * @return The filename, including the extension.
 */
std::string GetFileName(const std::string &filename);
std::string GetResourcePath(const std::string &resource_path);
void SaveFile(const std::string &filename, const std::string &data);

  /**
   * @brief Loads the entire contents of a file into a string.
   *
   * Throws a std::runtime_error if the file cannot be opened.
   *
   * @param filename The full, absolute path to the file.
   * @return The contents of the file as a string.
   */
  std::string LoadFile(const std::string &filename);

  /**
   * @brief Loads a file from the user's config directory.
   *
   * @param filename The name of the file inside the config directory.
   * @return The contents of the file, or an empty string if not found.
   */
  std::string LoadFileFromConfigDir(const std::string &filename);

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_FILE_UTIL_H_
