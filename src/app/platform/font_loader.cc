#include "app/platform/font_loader.h"

#include <filesystem>
#include <string>
#include <vector>
#include <cstring>


#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/file_util.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "util/macro.h"

namespace yaze {

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
  const std::string kBundlePath = util::GetBundleResourcePath();
  return kBundlePath + font_path;
#else
  return absl::StrCat(util::GetBundleResourcePath(), "Contents/Resources/font/",
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

#ifdef __linux__
void LoadSystemFonts() {
  // Load Linux System Fonts into ImGui
  // System font loading is now handled by NFD (Native File Dialog)
  // This function is kept for compatibility but does nothing
}
#endif

}  // namespace yaze
