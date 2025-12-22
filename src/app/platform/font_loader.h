#ifndef YAZE_APP_PLATFORM_FONTLOADER_H
#define YAZE_APP_PLATFORM_FONTLOADER_H

#include <vector>

#include "absl/status/status.h"
#include "imgui/imgui.h"

namespace yaze {

struct FontConfig {
  const char* font_path;
  float font_size;
  ImFontConfig im_font_config;
  ImFontConfig jp_conf_config;
};

struct FontState {
  std::vector<FontConfig> fonts;
};

static FontState font_registry;

absl::Status LoadPackageFonts();

absl::Status ReloadPackageFont(const FontConfig& config);

absl::Status LoadFontFromMemory(const std::string& name,
                                const std::string& data, float size_pixels);

void LoadSystemFonts();

}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_FONTLOADER_H
