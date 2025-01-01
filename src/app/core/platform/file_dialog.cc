#include "file_dialog.h"

#ifdef _WIN32
// Include Windows-specific headers
#include <shobjidl.h>
#include <windows.h>
#endif  // _WIN32

namespace yaze {
namespace core {

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

}  // namespace core
}  // namespace yaze