#include "file_dialog.h"

#ifdef _WIN32
// Include Windows-specific headers
#include <shobjidl.h>
#include <windows.h>
#else // Linux and MacOS
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <fstream>
#include <sstream>

namespace yaze {
namespace core {

std::string GetFileExtension(const std::string &filename) {
  size_t dot = filename.find_last_of(".");
  if (dot == std::string::npos) {
    return "";
  }
  return filename.substr(dot + 1);
}

std::string GetFileName(const std::string &filename) {
  size_t slash = filename.find_last_of("/");
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
  Platform platform;
#if defined(_WIN32)
  platform = Platform::kWindows;
#elif defined(__APPLE__)
  platform = Platform::kMacOS;
#else
  platform = Platform::kLinux;
#endif
  std::string filepath = GetConfigDirectory(platform) + "/" + filename;
  std::ifstream file(filepath);
  if (file.is_open()) {
    std::stringstream buffer;
    buffer << file.rdbuf();
    contents = buffer.str();
    file.close();
  }
  return contents;
}

void SaveFile(const std::string &filename, const std::string &contents,
              Platform platform) {
  std::string filepath = GetConfigDirectory(platform) + "/" + filename;
  std::ofstream file(filepath);
  if (file.is_open()) {
    file << contents;
    file.close();
  }
}

std::string GetConfigDirectory(Platform platform) {
  std::string config_directory = ".yaze";
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

std::string FileDialogWrapper::ShowOpenFileDialog() {
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  IFileDialog *pfd = NULL;
  HRESULT hr =
      CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog,
                       reinterpret_cast<void **>(&pfd));
  std::string file_path_windows;
  if (SUCCEEDED(hr)) {
    // Show the dialog
    hr = pfd->Show(NULL);
    if (SUCCEEDED(hr)) {
      IShellItem *psiResult;
      hr = pfd->GetResult(&psiResult);
      if (SUCCEEDED(hr)) {
        // Get the file path
        PWSTR pszFilePath;
        psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        char str[128];
        wcstombs(str, pszFilePath, 128);
        file_path_windows = str;
        psiResult->Release();
        CoTaskMemFree(pszFilePath);
      }
    }
    pfd->Release();
  }

  CoUninitialize();
  return file_path_windows;
}

std::string FileDialogWrapper::ShowOpenFolderDialog() {
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  IFileDialog *pfd = NULL;
  HRESULT hr =
      CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog,
                       reinterpret_cast<void **>(&pfd));
  std::string folder_path_windows;
  if (SUCCEEDED(hr)) {
    // Show the dialog
    DWORD dwOptions;
    hr = pfd->GetOptions(&dwOptions);
    if (SUCCEEDED(hr)) {
      hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
      if (SUCCEEDED(hr)) {
        hr = pfd->Show(NULL);
        if (SUCCEEDED(hr)) {
          IShellItem *psiResult;
          hr = pfd->GetResult(&psiResult);
          if (SUCCEEDED(hr)) {
            // Get the folder path
            PWSTR pszFolderPath;
            psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFolderPath);
            char str[128];
            wcstombs(str, pszFolderPath, 128);
            folder_path_windows = str;
            psiResult->Release();
            CoTaskMemFree(pszFolderPath);
          }
        }
      }
    }
    pfd->Release();
  }

  CoUninitialize();
  return folder_path_windows;
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

#include <nfd.h>

std::string FileDialogWrapper::ShowOpenFileDialog() {
  NFD_Init();
  nfdu8char_t *out_path = NULL;
  nfdu8filter_item_t filters[1] = {{ "Rom File", "sfc,smc" } };
  nfdopendialogu8args_t args = { 0 };
  args.filterList = filters;
  args.filterCount = 1;
  nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);
  if (result == NFD_OKAY) {
    std::string file_path_linux(out_path);
    NFD_Free(out_path);
    NFD_Quit();
    return file_path_linux;
  } else if (result == NFD_CANCEL) {
    NFD_Quit();
    return "";
  } 
  NFD_Quit();
  return "Error: NFD_OpenDialog";
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

}  // namespace core
}  // namespace yaze