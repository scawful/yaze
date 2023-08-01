#ifndef YAZE_APP_CORE_STYLE_H
#define YAZE_APP_CORE_STYLE_H

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "absl/strings/string_view.h"

namespace yaze {
namespace gui {

void TextWithSeparators(const absl::string_view& text);

void ColorsYaze();

}  // namespace gui
}  // namespace yaze

#endif