#include "file_dialog.h"

#if defined(__linux__)

std::string FileDialogWrapper::ShowOpenFileDialog() {
  return "Linux: Open file dialog";
}

std::string FileDialogWrapper::ShowOpenFolderDialog() {
  return "Linux: Open folder dialog";
}

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string& folder_path) {
  return {"Linux: Subdirectories in folder"};
}

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(
    const std::string& folder_path) {
  return {"Linux: Files in folder"};
}

#endif