#include "file_dialog.h"

#ifdef _WIN32
// Include Windows-specific headers
#include <shobjidl.h>
#include <windows.h>
#else  // Linux and MacOS
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <fstream>
#include <sstream>
#include <cstring>

#include "app/core/features.h"

namespace yaze {
namespace core {

std::string GetFileExtension(const std::string &filename) {
  size_t dot = filename.find_last_of('.');
  if (dot == std::string::npos) {
    return "";
  }
  return filename.substr(dot + 1);
}

std::string GetFileName(const std::string &filename) {
  size_t slash = filename.find_last_of('/');
  if (slash == std::string::npos) {
    return filename;
  }
  return filename.substr(slash + 1);
}

std::string LoadFile(const std::string &filename) {
  std::string contents;
  std::ifstream file(filename);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    file.close();
  } else {
    // Throw an exception
    throw std::runtime_error("Could not open file: " + filename);
  }
  return contents;
}

std::string LoadConfigFile(const std::string &filename) {
  std::string contents;
  std::string filepath = GetConfigDirectory() + "/" + filename;
  std::ifstream file(filepath);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    file.close();
  }
  return contents;
}

void SaveFile(const std::string &filename, const std::string &contents) {
  std::string filepath = GetConfigDirectory() + "/" + filename;
  std::ofstream file(filepath);
  if (file.is_open()) {
    file << contents;
    file.close();
  }
}

std::string GetResourcePath(const std::string &resource_path) {
#ifdef __APPLE__
#if TARGET_OS_IOS == 1
  const std::string kBundlePath = GetBundleResourcePath();
  return kBundlePath + resource_path;
#else
  return GetBundleResourcePath() + "Contents/Resources/" + resource_path;
#endif
#else
  return resource_path;  // On Linux/Windows, resources are relative to executable
#endif
}

std::string GetConfigDirectory() {
  std::string config_directory = ".yaze";
  Platform platform;
#if defined(__APPLE__) && defined(__MACH__)
#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
  platform = Platform::kiOS;
#elif TARGET_OS_MAC == 1
  platform = Platform::kMacOS;
#else
  platform = Platform::kMacOS; // Default for macOS
#endif
#elif defined(_WIN32)
  platform = Platform::kWindows;
#elif defined(__linux__)
  platform = Platform::kLinux;
#else
  platform = Platform::kUnknown;
#endif
  switch (platform) {
    case Platform::kWindows:
      config_directory = "~/AppData/Roaming/yaze";
      break;
    case Platform::kMacOS:
    case Platform::kLinux:
      config_directory = "~/.config/yaze";
      break;
    default:
      break;
  }
  return config_directory;
}

#ifdef _WIN32

#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
#include <nfd.h>
#endif

std::string FileDialogWrapper::ShowOpenFileDialog() {
  // Use global feature flag to choose implementation
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFileDialogNFD();
  } else {
    return ShowOpenFileDialogBespoke();
  }
}

std::string FileDialogWrapper::ShowOpenFileDialogNFD() {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdu8filteritem_t filters[1] = {{"ROM Files", "sfc,smc"}};
  nfdopendialogu8args_t args = {0};
  args.filterList = filters;
  args.filterCount = 1;
  nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return file_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not available - fallback to bespoke
  return ShowOpenFileDialogBespoke();
#endif
}

std::string FileDialogWrapper::ShowOpenFileDialogBespoke() {
  // For CI/CD, just return a placeholder path
  return ""; // Placeholder for bespoke implementation
}

std::string FileDialogWrapper::ShowSaveFileDialog(const std::string& default_name, 
                                                  const std::string& default_extension) {
  // Use global feature flag to choose implementation
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowSaveFileDialogNFD(default_name, default_extension);
  } else {
    return ShowSaveFileDialogBespoke(default_name, default_extension);
  }
}

std::string FileDialogWrapper::ShowSaveFileDialogNFD(const std::string& default_name, 
                                                     const std::string& default_extension) {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  
  nfdsavedialogu8args_t args = {0};
  if (!default_extension.empty()) {
    // Create filter for the save dialog
    static nfdu8filteritem_t filters[3] = {
      {"Theme Files", "theme"},
      {"YAZE Project Files", "yaze"},
      {"ROM Files", "sfc,smc"}
    };
    
    if (default_extension == "theme") {
      args.filterList = &filters[0];
      args.filterCount = 1;
    } else if (default_extension == "yaze") {
      args.filterList = &filters[1]; 
      args.filterCount = 1;
    } else if (default_extension == "sfc" || default_extension == "smc") {
      args.filterList = &filters[2];
      args.filterCount = 1;
    }
  }
  
  if (!default_name.empty()) {
    args.defaultName = default_name.c_str();
  }
  
  nfdresult_t result = NFD_SaveDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return file_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not available - fallback to bespoke
  return ShowSaveFileDialogBespoke(default_name, default_extension);
#endif
}

std::string FileDialogWrapper::ShowSaveFileDialogBespoke(const std::string& default_name, 
                                                        const std::string& default_extension) {
  // For CI/CD, just return a placeholder path
  if (!default_name.empty() && !default_extension.empty()) {
    return default_name + "." + default_extension;
  }
  return ""; // Placeholder for bespoke implementation
}

std::string FileDialogWrapper::ShowOpenFolderDialog() {
  // Use global feature flag to choose implementation
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFolderDialogNFD();
  } else {
    return ShowOpenFolderDialogBespoke();
  }
}

std::string FileDialogWrapper::ShowOpenFolderDialogNFD() {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdresult_t result = NFD_PickFolderU8(&out_path, NULL);
  if (result == NFD_OKAY) {
    std::string folder_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return folder_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not available - fallback to bespoke
  return ShowOpenFolderDialogBespoke();
#endif
}

std::string FileDialogWrapper::ShowOpenFolderDialogBespoke() {
  // For CI/CD, just return a placeholder path
  return ""; // Placeholder for bespoke implementation
}

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string &folder_path) {
  std::vector<std::string> subdirectories;
  WIN32_FIND_DATA findFileData;
  HANDLE hFind = FindFirstFile((folder_path + "\\*").c_str(), &findFileData);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (strcmp(findFileData.cFileName, ".") != 0 &&
            strcmp(findFileData.cFileName, "..") != 0) {
          subdirectories.push_back(findFileData.cFileName);
        }
      }
    } while (FindNextFile(hFind, &findFileData) != 0);
    FindClose(hFind);
  }
  return subdirectories;
}

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(
    const std::string &folder_path) {
  std::vector<std::string> files;
  WIN32_FIND_DATA findFileData;
  HANDLE hFind = FindFirstFile((folder_path + "\\*").c_str(), &findFileData);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        files.push_back(findFileData.cFileName);
      }
    } while (FindNextFile(hFind, &findFileData) != 0);
    FindClose(hFind);
  }
  return files;
}

#elif defined(__linux__)

#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
#include <nfd.h>
#endif

std::string FileDialogWrapper::ShowOpenFileDialog() {
  // Use global feature flag to choose implementation
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFileDialogNFD();
  } else {
    return ShowOpenFileDialogBespoke();
  }
}

std::string FileDialogWrapper::ShowOpenFileDialogNFD() {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdu8filteritem_t filters[1] = {{"Rom File", "sfc,smc"}};
  nfdopendialogu8args_t args = {0};
  args.filterList = filters;
  args.filterCount = 1;
  nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path_linux(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return file_path_linux;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "Error: NFD_OpenDialog";
#else
  // NFD not available - fallback to bespoke
  return ShowOpenFileDialogBespoke();
#endif
}

std::string FileDialogWrapper::ShowOpenFileDialogBespoke() {
  // Implement bespoke file dialog or return placeholder
  // This would contain the custom Linux implementation
  return ""; // Placeholder for bespoke implementation
}

std::string FileDialogWrapper::ShowSaveFileDialogNFD(const std::string& default_name, 
                                                     const std::string& default_extension) {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  
  nfdsavedialogu8args_t args = {0};
  if (!default_extension.empty()) {
    // Create filter for the save dialog
    static nfdu8filteritem_t filters[3] = {
      {"Theme File", "theme"},
      {"Project File", "yaze"},
      {"ROM File", "sfc,smc"}
    };
    
    if (default_extension == "theme") {
      args.filterList = &filters[0];
      args.filterCount = 1;
    } else if (default_extension == "yaze") {
      args.filterList = &filters[1]; 
      args.filterCount = 1;
    } else if (default_extension == "sfc" || default_extension == "smc") {
      args.filterList = &filters[2];
      args.filterCount = 1;
    }
  }
  
  if (!default_name.empty()) {
    args.defaultName = default_name.c_str();
  }
  
  nfdresult_t result = NFD_SaveDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return file_path;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "";
#else
  // NFD not available - fallback to bespoke
  return ShowSaveFileDialogBespoke(default_name, default_extension);
#endif
}

std::string FileDialogWrapper::ShowSaveFileDialogBespoke(const std::string& default_name, 
                                                        const std::string& default_extension) {
  // Basic Linux implementation using system command
  // For CI/CD, just return a placeholder path
  if (!default_name.empty() && !default_extension.empty()) {
    return default_name + "." + default_extension;
  }
  return ""; // For now return empty - full implementation can be added later
}

std::string FileDialogWrapper::ShowSaveFileDialog(const std::string& default_name, 
                                                 const std::string& default_extension) {
  // Use global feature flag to choose implementation
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowSaveFileDialogNFD(default_name, default_extension);
  } else {
    return ShowSaveFileDialogBespoke(default_name, default_extension);
  }
}

std::string FileDialogWrapper::ShowOpenFolderDialog() {
  // Use global feature flag to choose implementation
  if (FeatureFlags::get().kUseNativeFileDialog) {
    return ShowOpenFolderDialogNFD();
  } else {
    return ShowOpenFolderDialogBespoke();
  }
}

std::string FileDialogWrapper::ShowOpenFolderDialogNFD() {
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdresult_t result = NFD_PickFolderU8(&out_path, NULL);
  if (result == NFD_OKAY) {
    std::string folder_path_linux(out_path);
    NFD_FreePath(out_path);
    NFD_Quit();
    return folder_path_linux;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  }
  NFD_Quit();
  return "Error: NFD_PickFolder";
#else
  // NFD not available - fallback to bespoke
  return ShowOpenFolderDialogBespoke();
#endif
}

std::string FileDialogWrapper::ShowOpenFolderDialogBespoke() {
  // Implement bespoke folder dialog or return placeholder
  // This would contain the custom macOS implementation
  return ""; // Placeholder for bespoke implementation
}

std::vector<std::string> FileDialogWrapper::GetSubdirectoriesInFolder(
    const std::string &folder_path) {
  std::vector<std::string> subdirectories;
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(folder_path.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_type == DT_DIR) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
          subdirectories.push_back(ent->d_name);
        }
      }
    }
    closedir(dir);
  }
  return subdirectories;
}

std::vector<std::string> FileDialogWrapper::GetFilesInFolder(
    const std::string &folder_path) {
  std::vector<std::string> files;
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(folder_path.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_type == DT_REG) {
        files.push_back(ent->d_name);
      }
    }
    closedir(dir);
  }
  return files;
}

#endif

}  // namespace core
}  // namespace yaze