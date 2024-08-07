#include "app/core/platform/font_loader.h"

#include <string>
#include <vector>

#include "imgui/imgui.h"

#ifdef _WIN32
#include <Windows.h>

int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme,
                               DWORD FontType, LPARAM lParam) {
  // Step 3: Load the font into ImGui
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF(lpelfe->lfFaceName, 16.0f);

  return 1;
}

void LoadSystemFonts() {
  HKEY hKey;
  std::vector<std::string> fontPaths;

  // Open the registry key where fonts are listed
  if (RegOpenKeyEx(
          HKEY_LOCAL_MACHINE,
          TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"), 0,
          KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD valueCount;
    DWORD maxValueNameSize;
    DWORD maxValueDataSize;

    // Query the number of entries and the maximum size of the names and values
    RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount,
                    &maxValueNameSize, &maxValueDataSize, NULL, NULL);

    char* valueName = new char[maxValueNameSize + 1];  // +1 for null terminator
    BYTE* valueData = new BYTE[maxValueDataSize + 1];  // +1 for null terminator

    // Enumerate all font entries
    for (DWORD i = 0; i < valueCount; i++) {
      DWORD valueNameSize = maxValueNameSize + 1;  // +1 for null terminator
      DWORD valueDataSize = maxValueDataSize + 1;  // +1 for null terminator
      DWORD valueType;

      // Clear buffers
      memset(valueName, 0, valueNameSize);
      memset(valueData, 0, valueDataSize);

      // Get the font name and file path
      if (RegEnumValue(hKey, i, valueName, &valueNameSize, NULL, &valueType,
                       valueData, &valueDataSize) == ERROR_SUCCESS) {
        if (valueType == REG_SZ) {
          // Add the font file path to the vector
          std::string fontPath((char*)valueData);
          fontPaths.push_back(fontPath);
        }
      }
    }

    delete[] valueName;
    delete[] valueData;

    RegCloseKey(hKey);
  }

  ImGuiIO& io = ImGui::GetIO();

  for (const auto& fontPath : fontPaths) {
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
  }
}

#elif defined(__linux__)

void LoadSystemFonts() {
  // Load Linux System Fonts into ImGui
  // ...
}

#endif