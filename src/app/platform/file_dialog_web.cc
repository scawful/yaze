#include "util/file_util.h"

#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace yaze {
namespace util {

// Web implementation of FileDialogWrapper
// Triggers the existing file input element in the HTML

std::string FileDialogWrapper::ShowOpenFileDialog(
    const FileDialogOptions& options) {
  (void)options;
  return ShowOpenFileDialog();
}

std::string FileDialogWrapper::ShowOpenFileDialog() {
#ifdef __EMSCRIPTEN__
  // Trigger the existing file input element
  // The file input handler in app.js will handle the file selection
  // and call LoadRomFromWeb, which will update the ROM
  EM_ASM({
    var romInput = document.getElementById('rom-input');
    if (romInput) {
      romInput.click();
    }
  });
  
  // Return empty string - the actual loading happens asynchronously
  // via the file input handler which calls LoadRomFromWeb
  return "";
#else
  return "";
#endif
}

void FileDialogWrapper::ShowOpenFileDialogAsync(
    const FileDialogOptions& options,
    std::function<void(const std::string&)> callback) {
  if (!callback) {
    return;
  }
  callback(ShowOpenFileDialog(options));
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
