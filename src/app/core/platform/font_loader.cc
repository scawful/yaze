#include "app/core/platform/font_loader.h"

#include <string>
#include <unordered_set>
#include <vector>
#include <filesystem>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/file_path.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace core {

absl::Status LoadPackageFonts() {
  ImGuiIO &io = ImGui::GetIO();

  static const char *KARLA_REGULAR = "Karla-Regular.ttf";
  static const char *ROBOTO_MEDIUM = "Roboto-Medium.ttf";
  static const char *COUSINE_REGULAR = "Cousine-Regular.ttf";
  static const char *DROID_SANS = "DroidSans.ttf";
  static const char *NOTO_SANS_JP = "NotoSansJP.ttf";
  static const char *IBM_PLEX_JP = "IBMPlexSansJP-Bold.ttf";
  static const float FONT_SIZE_DEFAULT = 16.0f;
  static const float FONT_SIZE_DROID_SANS = 18.0f;
  static const float ICON_FONT_SIZE = 18.0f;

  // Icon configuration
  static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;

  // Japanese font configuration
  ImFontConfig japanese_font_config;
  japanese_font_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;

  // List of fonts to be loaded
  std::vector<const char *> font_paths = {
      KARLA_REGULAR, ROBOTO_MEDIUM, COUSINE_REGULAR, IBM_PLEX_JP, DROID_SANS};

  // Load fonts with associated icon and Japanese merges
  for (const auto &font_path : font_paths) {
    float font_size =
        (font_path == DROID_SANS) ? FONT_SIZE_DROID_SANS : FONT_SIZE_DEFAULT;

    std::string actual_font_path;
#ifdef __APPLE__
#if TARGET_OS_IOS == 1
    const std::string kBundlePath = GetBundleResourcePath();
    actual_font_path = kBundlePath + font_path;
#else
    actual_font_path = absl::StrCat(GetBundleResourcePath(),
                                    "Contents/Resources/font/", font_path);
#endif
#else
    actual_font_path = std::filesystem::absolute(font_path).string();
#endif

    if (!io.Fonts->AddFontFromFileTTF(actual_font_path.data(), font_size)) {
      return absl::InternalError(
          absl::StrFormat("Failed to load font from %s", actual_font_path));
    }

    // Merge icon set
    std::string actual_icon_font_path = "";
    const char *icon_font_path = FONT_ICON_FILE_NAME_MD;
#if defined(__APPLE__) && defined(__MACH__)
#if TARGET_OS_IOS == 1
    const std::string kIconBundlePath = GetBundleResourcePath();
    actual_icon_font_path = kIconBundlePath + "MaterialIcons-Regular.ttf";
#else
    actual_icon_font_path =
        absl::StrCat(GetBundleResourcePath(),
                     "Contents/Resources/font/MaterialIcons-Regular.ttf");
#endif
#else
    actual_icon_font_path = std::filesystem::absolute(icon_font_path).string();
#endif
    io.Fonts->AddFontFromFileTTF(actual_icon_font_path.data(), ICON_FONT_SIZE,
                                 &icons_config, icons_ranges);

    // Merge Japanese font
    std::string actual_japanese_font_path = "";
    const char *japanese_font_path = NOTO_SANS_JP;
#if defined(__APPLE__) && defined(__MACH__)
#if TARGET_OS_IOS == 1
    const std::string kJapaneseBundlePath = GetBundleResourcePath();
    actual_japanese_font_path = kJapaneseBundlePath + japanese_font_path;
#else
    actual_japanese_font_path =
        absl::StrCat(GetBundleResourcePath(), "Contents/Resources/font/",
                     japanese_font_path);
#endif
#else
    actual_japanese_font_path =
        std::filesystem::absolute(japanese_font_path).string();
#endif
    io.Fonts->AddFontFromFileTTF(actual_japanese_font_path.data(), 18.0f,
                                 &japanese_font_config,
                                 io.Fonts->GetGlyphRangesJapanese());
  }
  return absl::OkStatus();
}

#ifdef _WIN32
#include <Windows.h>

int CALLBACK EnumFontFamExProc(const LOGFONT *lpelfe, const TEXTMETRIC *lpntme,
                               DWORD FontType, LPARAM lParam) {
  // Step 3: Load the font into ImGui
  ImGuiIO &io = ImGui::GetIO();
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

    char *valueName = new char[maxValueNameSize + 1];  // +1 for null terminator
    BYTE *valueData = new BYTE[maxValueDataSize + 1];  // +1 for null terminator

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
          std::string fontPath(reinterpret_cast<char *>(valueData),
                               valueDataSize);

          fontPaths.push_back(fontPath);
        }
      }
    }

    delete[] valueName;
    delete[] valueData;

    RegCloseKey(hKey);
  }

  ImGuiIO &io = ImGui::GetIO();

  // List of common font face names
  static const std::unordered_set<std::string> commonFontFaceNames = {
      "arial",
      "times",
      "cour",
      "verdana",
      "tahoma",
      "comic",
      "Impact",
      "ariblk",
      "Trebuchet MS",
      "Georgia",
      "Palatino Linotype",
      "Lucida Sans Unicode",
      "Tahoma",
      "Lucida Console"};

  for (auto &fontPath : fontPaths) {
    // Check if the font path has a "C:\" prefix
    if (fontPath.substr(0, 2) != "C:") {
      // Add "C:\Windows\Fonts\" prefix to the font path
      fontPath = absl::StrFormat("C:\\Windows\\Fonts\\%s", fontPath.c_str());
    }

    // Check if the font file has a .ttf or .TTF extension
    std::string extension = fontPath.substr(fontPath.find_last_of(".") + 1);
    if (extension == "ttf" || extension == "TTF") {
      // Get the font face name from the font path
      std::string fontFaceName =
          fontPath.substr(fontPath.find_last_of("\\/") + 1);
      fontFaceName = fontFaceName.substr(0, fontFaceName.find_last_of("."));

      // Check if the font face name is in the common font face names list
      if (commonFontFaceNames.find(fontFaceName) != commonFontFaceNames.end()) {
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);

        // Merge icon set
        // Icon configuration
        static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
        ImFontConfig icons_config;
        static const float ICON_FONT_SIZE = 18.0f;
        icons_config.MergeMode = true;
        icons_config.GlyphOffset.y = 5.0f;
        icons_config.GlyphMinAdvanceX = 13.0f;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MD, ICON_FONT_SIZE,
                                     &icons_config, icons_ranges);
      }
    }
  }
}

#elif defined(__linux__)

void LoadSystemFonts() {
  // Load Linux System Fonts into ImGui
  // ...
}

#endif

}  // namespace core
}  // namespace app
}  // namespace yaze