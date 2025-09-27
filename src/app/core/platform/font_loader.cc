#include "app/core/platform/font_loader.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/file_dialog.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "util/macro.h"

namespace yaze {
namespace core {

static const char* KARLA_REGULAR = "Karla-Regular.ttf";
static const char* ROBOTO_MEDIUM = "Roboto-Medium.ttf";
static const char* COUSINE_REGULAR = "Cousine-Regular.ttf";
static const char* DROID_SANS = "DroidSans.ttf";
static const char* NOTO_SANS_JP = "NotoSansJP.ttf";
static const char* IBM_PLEX_JP = "IBMPlexSansJP-Bold.ttf";

static const float FONT_SIZE_DEFAULT = 16.0f;
static const float FONT_SIZE_DROID_SANS = 18.0f;
static const float ICON_FONT_SIZE = 18.0f;

namespace {

std::string SetFontPath(const std::string& font_path) {
#ifdef __APPLE__
#if TARGET_OS_IOS == 1
  const std::string kBundlePath = GetBundleResourcePath();
  return kBundlePath + font_path;
#else
  return absl::StrCat(GetBundleResourcePath(), "Contents/Resources/font/",
                      font_path);
#endif
#else
  return absl::StrCat("assets/font/", font_path);
#endif
}

absl::Status LoadFont(const FontConfig& font_config) {
  ImGuiIO& io = ImGui::GetIO();
  std::string actual_font_path = SetFontPath(font_config.font_path);
  // Check if the file exists with std library first, since ImGui IO will assert
  // if the file does not exist
  if (!std::filesystem::exists(actual_font_path)) {
    return absl::InternalError(
        absl::StrFormat("Font file %s does not exist", actual_font_path));
  }

  if (!io.Fonts->AddFontFromFileTTF(actual_font_path.data(),
                                    font_config.font_size)) {
    return absl::InternalError(
        absl::StrFormat("Failed to load font from %s", actual_font_path));
  }
  return absl::OkStatus();
}

absl::Status AddIconFont(const FontConfig& config) {
  static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;
  std::string icon_font_path = SetFontPath(FONT_ICON_FILE_NAME_MD);
  ImGuiIO& io = ImGui::GetIO();
  if (!io.Fonts->AddFontFromFileTTF(icon_font_path.c_str(), ICON_FONT_SIZE,
                                    &icons_config, icons_ranges)) {
    return absl::InternalError("Failed to add icon fonts");
  }
  return absl::OkStatus();
}

absl::Status AddJapaneseFont(const FontConfig& config) {
  ImFontConfig japanese_font_config;
  japanese_font_config.MergeMode = true;
  japanese_font_config.GlyphOffset.y = 5.0f;
  japanese_font_config.GlyphMinAdvanceX = 13.0f;
  japanese_font_config.PixelSnapH = true;
  std::string japanese_font_path = SetFontPath(NOTO_SANS_JP);
  ImGuiIO& io = ImGui::GetIO();
  if (!io.Fonts->AddFontFromFileTTF(japanese_font_path.data(), ICON_FONT_SIZE,
                                    &japanese_font_config,
                                    io.Fonts->GetGlyphRangesJapanese())) {
    return absl::InternalError("Failed to add Japanese fonts");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status LoadPackageFonts() {
  if (font_registry.fonts.empty()) {
    // Initialize the font names and sizes
    font_registry.fonts = {
        {KARLA_REGULAR, FONT_SIZE_DEFAULT},
        {ROBOTO_MEDIUM, FONT_SIZE_DEFAULT},
        {COUSINE_REGULAR, FONT_SIZE_DEFAULT},
        {IBM_PLEX_JP, FONT_SIZE_DEFAULT},
        {DROID_SANS, FONT_SIZE_DROID_SANS},
    };
  }

  // Load fonts with associated icon and Japanese merges
  for (const auto& font_config : font_registry.fonts) {
    RETURN_IF_ERROR(LoadFont(font_config));
    RETURN_IF_ERROR(AddIconFont(font_config));
    RETURN_IF_ERROR(AddJapaneseFont(font_config));
  }
  return absl::OkStatus();
}

absl::Status ReloadPackageFont(const FontConfig& config) {
  ImGuiIO& io = ImGui::GetIO();
  std::string actual_font_path = SetFontPath(config.font_path);
  if (!io.Fonts->AddFontFromFileTTF(actual_font_path.data(),
                                    config.font_size)) {
    return absl::InternalError(
        absl::StrFormat("Failed to load font from %s", actual_font_path));
  }
  RETURN_IF_ERROR(AddIconFont(config));
  RETURN_IF_ERROR(AddJapaneseFont(config));
  return absl::OkStatus();
}

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
          std::string fontPath(reinterpret_cast<char*>(valueData),
                               valueDataSize);

          fontPaths.push_back(fontPath);
        }
      }
    }

    delete[] valueName;
    delete[] valueData;

    RegCloseKey(hKey);
  }

  ImGuiIO& io = ImGui::GetIO();

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

  for (auto& fontPath : fontPaths) {
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
}

#endif

}  // namespace core
}  // namespace yaze
