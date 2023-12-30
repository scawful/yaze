#ifndef YAZE_APP_CORE_STYLE_H
#define YAZE_APP_CORE_STYLE_H

#include <imgui/imgui.h>

#include "absl/strings/string_view.h"

namespace yaze {
namespace app {
namespace gui {

void DrawDisplaySettings(ImGuiStyle* ref = nullptr);

void TextWithSeparators(const absl::string_view& text);

void ColorsYaze();

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif