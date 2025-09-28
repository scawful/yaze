#include "app/core/platform/font_loader.h"

#include <filesystem>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>
#


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

static const float FONT_SIZE_DEFAULT = 16.0F;
static const float FONT_SIZE_DROID_SANS = 18.0F;
static const float ICON_FONT_SIZE = 18.0F;

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
  ImGuiIO& imgui_io = ImGui::GetIO();
  std::string actual_font_path = SetFontPath(font_config.font_path);
  // Check if the file exists with std library first, since ImGui IO will assert
  // if the file does not exist
  if (!std::filesystem::exists(actual_font_path)) {
    return absl::InternalError(
        absl::StrFormat("Font file %s does not exist", actual_font_path));
  }

  if (!imgui_io.Fonts->AddFontFromFileTTF(actual_font_path.data(),
                                    font_config.font_size)) {
    return absl::InternalError(
        absl::StrFormat("Failed to load font from %s", actual_font_path));
  }
  return absl::OkStatus();
}

absl::Status AddIconFont(const FontConfig& /*config*/) {
  static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ImFontConfig icons_config{};
  icons_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0F;
  icons_config.GlyphMinAdvanceX = 13.0F;
  icons_config.PixelSnapH = true;
  std::string icon_font_path = SetFontPath(FONT_ICON_FILE_NAME_MD);
  ImGuiIO& imgui_io = ImGui::GetIO();
  if (!imgui_io.Fonts->AddFontFromFileTTF(icon_font_path.c_str(), ICON_FONT_SIZE,
                                    &icons_config, icons_ranges)) {
    return absl::InternalError("Failed to add icon fonts");
  }
  return absl::OkStatus();
}

absl::Status AddJapaneseFont(const FontConfig& /*config*/) {
  ImFontConfig japanese_font_config{};
  japanese_font_config.MergeMode = true;
  japanese_font_config.GlyphOffset.y = 5.0F;
  japanese_font_config.GlyphMinAdvanceX = 13.0F;
  japanese_font_config.PixelSnapH = true;
  std::string japanese_font_path = SetFontPath(NOTO_SANS_JP);
  ImGuiIO& imgui_io = ImGui::GetIO();
  if (!imgui_io.Fonts->AddFontFromFileTTF(japanese_font_path.data(), ICON_FONT_SIZE,
                                    &japanese_font_config,
                                    imgui_io.Fonts->GetGlyphRangesJapanese())) {
    return absl::InternalError("Failed to add Japanese fonts");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status LoadPackageFonts() {
  if (font_registry.fonts.empty()) {
    // Initialize the font names and sizes with proper ImFontConfig initialization
    font_registry.fonts = {
        FontConfig{KARLA_REGULAR, FONT_SIZE_DEFAULT, {}, {}},
        FontConfig{ROBOTO_MEDIUM, FONT_SIZE_DEFAULT, {}, {}},
        FontConfig{COUSINE_REGULAR, FONT_SIZE_DEFAULT, {}, {}},
        FontConfig{IBM_PLEX_JP, FONT_SIZE_DEFAULT, {}, {}},
        FontConfig{DROID_SANS, FONT_SIZE_DROID_SANS, {}, {}},
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
  ImGuiIO& imgui_io = ImGui::GetIO();
  std::string actual_font_path = SetFontPath(config.font_path);
  if (!imgui_io.Fonts->AddFontFromFileTTF(actual_font_path.data(),
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
#include <ShlObj.h>
#include <algorithm>
#include <cctype>

namespace {
  // Helper function to convert wide string to UTF-8
  std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return std::string();
    
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size_needed, NULL, NULL);
    return result;
  }

  // Helper function to get Windows fonts directory
  std::string GetWindowsFontsDirectory() {
    wchar_t* fontsPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Fonts, 0, NULL, &fontsPath);
    
    if (SUCCEEDED(hr) && fontsPath) {
      std::string result = WideToUtf8(fontsPath) + "\\";
      CoTaskMemFree(fontsPath);
      return result;
    }
    
    // Fallback to default path
    return "C:\\Windows\\Fonts\\";
  }

  // Helper function to normalize font name (lowercase, remove spaces)
  std::string NormalizeFontName(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
    return result;
  }

  // Check if file exists and is accessible
  bool FontFileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
  }
}

void LoadSystemFonts() {
  ImGuiIO& imgui_io = ImGui::GetIO();
  
  // Get the Windows fonts directory
  std::string fontsDir = GetWindowsFontsDirectory();
  
  // List of essential Windows fonts to load
  static const std::vector<std::string> essentialFonts = {
    "arial.ttf",
    "arialbd.ttf", 
    "times.ttf",
    "timesbd.ttf",
    "cour.ttf",
    "courbd.ttf",
    "verdana.ttf",
    "verdanab.ttf",
    "tahoma.ttf",
    "tahomabd.ttf",
    "segoeui.ttf",
    "segoeuib.ttf"
  };

  // Load essential fonts
  for (const auto& fontName : essentialFonts) {
    std::string fontPath = fontsDir + fontName;
    
    if (FontFileExists(fontPath)) {
      try {
        // Load the font
        ImFont* font = imgui_io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0F);
        if (font) {
          // Merge icon fonts if available
          static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
          ImFontConfig icons_config{};
          icons_config.MergeMode = true;
          icons_config.GlyphOffset.y = 5.0F;
          icons_config.GlyphMinAdvanceX = 13.0F;
          icons_config.PixelSnapH = true;
          
          std::string iconFontPath = SetFontPath(FONT_ICON_FILE_NAME_MD);
          if (FontFileExists(iconFontPath)) {
            imgui_io.Fonts->AddFontFromFileTTF(iconFontPath.c_str(), ICON_FONT_SIZE,
                                         &icons_config, icons_ranges);
          }
        }
      } catch (...) {
        // Silently continue if font loading fails
        continue;
      }
    }
  }

  // Try to load additional fonts from registry (safer approach)
  HKEY hKey = nullptr;
  LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                              "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
                              0, KEY_READ, &hKey);
  
  if (result == ERROR_SUCCESS && hKey) {
    DWORD valueCount = 0;
    DWORD maxValueNameSize = 0;
    DWORD maxValueDataSize = 0;
    
    // Get registry info
    result = RegQueryInfoKeyA(hKey, nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, &valueCount, &maxValueNameSize,
                              &maxValueDataSize, nullptr, nullptr);
    
    if (result == ERROR_SUCCESS && valueCount > 0) {
      // Allocate buffers with proper size limits
      size_t maxNameSize = maxValueNameSize + 1;
      if (maxNameSize > 1024) maxNameSize = 1024;
      size_t maxDataSize = maxValueDataSize + 1;
      if (maxDataSize > 4096) maxDataSize = 4096;

      std::vector<char> valueName(maxNameSize);
      std::vector<BYTE> valueData(maxDataSize);

      // Enumerate font entries (limit to prevent excessive loading)
      DWORD maxFontsToLoad = valueCount;
      if (maxFontsToLoad > 50) maxFontsToLoad = 50;
      
      for (DWORD i = 0; i < maxFontsToLoad; i++) {
        DWORD valueNameSize = static_cast<DWORD>(maxNameSize);
        DWORD valueDataSize = static_cast<DWORD>(maxDataSize);
        DWORD valueType = 0;
        
        result = RegEnumValueA(hKey, i, valueName.data(), &valueNameSize,
                               nullptr, &valueType, valueData.data(), &valueDataSize);
        
        if (result == ERROR_SUCCESS && valueType == REG_SZ && valueDataSize > 0) {
          // Ensure null termination
          valueName[valueNameSize] = '\0';
          valueData[valueDataSize] = '\0';
          
          std::string fontPath(reinterpret_cast<char*>(valueData.data()));
          
          // Normalize the font path
          if (!fontPath.empty()) {
            // If it's a relative path, prepend the fonts directory
            if (fontPath.find(':') == std::string::npos) {
              fontPath = fontsDir + fontPath;
            }
            
            // Check if it's a TTF file and exists
            std::string lowerPath = fontPath;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
            
            if ((lowerPath.find(".ttf") != std::string::npos ||
                 lowerPath.find(".otf") != std::string::npos) &&
                FontFileExists(fontPath)) {
              try {
                imgui_io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0F);
              } catch (...) {
                // Continue if font loading fails
                continue;
              }
            }
          }
        }
      }
    }
    
    RegCloseKey(hKey);
  }
}

#elif defined(__linux__)

void LoadSystemFonts() {
  // Load Linux System Fonts into ImGui
}

#endif

}  // namespace core
}  // namespace yaze
