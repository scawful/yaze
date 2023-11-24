#include <string>

#ifdef _WIN32
// Include Windows-specific headers
#include <shobjidl.h>
#include <windows.h>

std::string ShowOpenFileDialog() {
  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  IFileDialog *pfd = NULL;
  HRESULT hr =
      CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileDialog,
                       reinterpret_cast<void **>(&pfd));

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
        psiResult->Release();
        CoTaskMemFree(pszFilePath);
      }
    }
    pfd->Release();
  }

  CoUninitialize();
  return L"";  // Return an empty string if no file was selected or an error
}

#elif defined(__APPLE__)

#include <string>

class FileDialogWrapper {
 public:
  static std::string ShowOpenFileDialog();
};


#elif defined(__linux__)
#include <gtk/gtk.h>

std::string ShowOpenFileDialog() {
  // Linux-specific file dialog implementation using GTK
  // ...
  return "file_path_linux";
}

#else
#error "Unsupported platform."
#endif