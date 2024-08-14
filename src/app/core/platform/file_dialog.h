#ifndef YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
#define YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H

#include <string>
#include <vector>

namespace yaze {
namespace app {
namespace core {

#ifdef _WIN32
// Include Windows-specific headers
#include <shobjidl.h>
#include <windows.h>

class FileDialogWrapper {
 public:
  static std::string ShowOpenFileDialog() {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    IFileDialog *pfd = NULL;
    HRESULT hr =
        CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                         IID_IFileDialog, reinterpret_cast<void **>(&pfd));
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

  static std::string ShowOpenFolderDialog() {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    IFileDialog *pfd = NULL;
    HRESULT hr =
        CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                         IID_IFileDialog, reinterpret_cast<void **>(&pfd));
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

  static std::vector<std::string> GetSubdirectoriesInFolder(
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

  static std::vector<std::string> GetFilesInFolder(
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
};

#else

#include <string>
#include <vector>

class FileDialogWrapper {
 public:
  static std::string ShowOpenFileDialog();
  static std::string ShowOpenFolderDialog();
  static std::vector<std::string> GetSubdirectoriesInFolder(
      const std::string& folder_path);
  static std::vector<std::string> GetFilesInFolder(
      const std::string& folder_path);
};

#endif

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
