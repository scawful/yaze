#ifndef YAZE_APP_CORE_PLATFORM_FONTLOADER_H
#define YAZE_APP_CORE_PLATFORM_FONTLOADER_H

#include "absl/status/status.h"

namespace yaze {
namespace core {

struct FontConfig {
	const char* font_path;
	float font_size;
};

absl::Status LoadPackageFonts();

absl::Status ReloadPackageFont(const FontConfig& config);

void LoadSystemFonts();

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FONTLOADER_H
