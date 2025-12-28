// Windows and Linux implementation of FileDialogWrapper using
// nativefiledialog-extended
#include <nfd.h>

#include <filesystem>
#include <string>
#include <vector>

#include "util/file_util.h"

namespace yaze {
namespace util {

std::string FileDialogWrapper::ShowOpenFileDialog(
    const FileDialogOptions& options) {
  nfdchar_t* outPath = nullptr;
  const nfdfilteritem_t* filter_list = nullptr;
  size_t filter_count = 0;
  std::vector<nfdfilteritem_t> filter_items;
  std::vector<std::string> filter_names;
  std::vector<std::string> filter_specs;

  if (!options.filters.empty()) {
    filter_items.reserve(options.filters.size());
    filter_names.reserve(options.filters.size());
    filter_specs.reserve(options.filters.size());

    for (const auto& filter : options.filters) {
      std::string label = filter.label.empty() ? "Files" : filter.label;
      std::string spec = filter.spec.empty() ? "*" : filter.spec;
      filter_names.push_back(label);
      filter_specs.push_back(spec);
      filter_items.push_back(
          {filter_names.back().c_str(), filter_specs.back().c_str()});
    }

    filter_list = filter_items.data();
    filter_count = filter_items.size();
  }

  nfdresult_t result =
      NFD_OpenDialog(&outPath, filter_list, filter_count, nullptr);

  if (result == NFD_OKAY) {
    std::string path(outPath);
    NFD_FreePath(outPath);
    return path;
  }

  return "";
}

std::string FileDialogWrapper::ShowOpenFileDialog() {
  return ShowOpenFileDialog(FileDialogOptions{});
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
  nfdchar_t* outPath = nullptr;
  nfdresult_t result = NFD_PickFolder(&outPath, nullptr);

  if (result == NFD_OKAY) {
    std::string path(outPath);
    NFD_FreePath(outPath);
    return path;
  }

  return "";
}

std::string FileDialogWrapper::ShowSaveFileDialog(
    const std::string& default_name, const std::string& default_extension) {
  nfdchar_t* outPath = nullptr;
  nfdfilteritem_t filterItem[1] = {
      {default_extension.empty() ? "All Files" : default_extension.c_str(),
       default_extension.empty() ? "*" : default_extension.c_str()}};

  nfdresult_t result = NFD_SaveDialog(
      &outPath, default_extension.empty() ? nullptr : filterItem,
      default_extension.empty() ? 0 : 1, nullptr, default_name.c_str());

  if (result == NFD_OKAY) {
    std::string path(outPath);
    NFD_FreePath(outPath);
    return path;
  }

  return "";
}

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string& folder_path) {
  std::vector<std::string> subdirs;

  try {
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
      if (entry.is_directory()) {
        subdirs.push_back(entry.path().string());
      }
    }
  } catch (...) {
    // Return empty vector on error
  }

  return subdirs;
}

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(
    const std::string& folder_path) {
  std::vector<std::string> files;

  try {
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
      if (entry.is_regular_file()) {
        files.push_back(entry.path().string());
      }
    }
  } catch (...) {
    // Return empty vector on error
  }

  return files;
}

// Delegate to main implementations
std::string FileDialogWrapper::ShowOpenFileDialogNFD() {
  return ShowOpenFileDialog();
}

std::string FileDialogWrapper::ShowOpenFileDialogBespoke() {
  return ShowOpenFileDialog();
}

std::string FileDialogWrapper::ShowSaveFileDialogNFD(
    const std::string& default_name, const std::string& default_extension) {
  return ShowSaveFileDialog(default_name, default_extension);
}

std::string FileDialogWrapper::ShowSaveFileDialogBespoke(
    const std::string& default_name, const std::string& default_extension) {
  return ShowSaveFileDialog(default_name, default_extension);
}

std::string FileDialogWrapper::ShowOpenFolderDialogNFD() {
  return ShowOpenFolderDialog();
}

std::string FileDialogWrapper::ShowOpenFolderDialogBespoke() {
  return ShowOpenFolderDialog();
}

}  // namespace util
}  // namespace yaze
