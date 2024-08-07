#ifndef YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
#define YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H

#include <string>

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
};

#elif defined(__APPLE__) || defined(__linux__)

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

#else
#error "Unsupported platform."
#endif

#endif  // YAZE_APP_CORE_PLATFORM_FILE_DIALOG_H
