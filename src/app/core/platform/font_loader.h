#ifndef YAZE_APP_CORE_PLATFORM_FONTLOADER_H
#define YAZE_APP_CORE_PLATFORM_FONTLOADER_H

#include "absl/status/status.h"

namespace yaze {
namespace core {

void LoadSystemFonts();
absl::Status LoadPackageFonts();

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FONTLOADER_H
