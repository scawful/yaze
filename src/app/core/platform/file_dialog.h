#ifndef YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
#define YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H

#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace core {

class FileDialogWrapper {
 public:
  static std::string ShowOpenFileDialog();
  static std::string ShowOpenFolderDialog();
  static std::vector<std::string> GetSubdirectoriesInFolder(
      const std::string& folder_path);
  static std::vector<std::string> GetFilesInFolder(
      const std::string& folder_path);
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
