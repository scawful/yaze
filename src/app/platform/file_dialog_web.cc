#include "util/file_util.h"

#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace yaze {
namespace util {

// Web implementation of FileDialogWrapper
// For now, these are stubs. In Milestone 3, we will connect these to HTML/JS.

std::string FileDialogWrapper::ShowOpenFileDialog() {
  // TODO(web): Implement file picker via JS
  return "";
}

std::string FileDialogWrapper::ShowOpenFolderDialog() {
  // Folder picking not supported on web in the same way
  return "";
}

std::string FileDialogWrapper::ShowSaveFileDialog(
    const std::string& default_name, const std::string& default_extension) {
  // TODO(web): Implement download trigger via JS
  return "";
}

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string& folder_path) {
  // Emscripten's VFS might support this if mounted
  return {};
}

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(
    const std::string& folder_path) {
  return {};
}

}  // namespace util
}  // namespace yaze

