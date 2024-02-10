#ifndef YAZE_APP_CORE_STYLE_H
#define YAZE_APP_CORE_STYLE_H

#include <imgui/imgui.h>

#include "absl/strings/string_view.h"

namespace yaze {
namespace app {
namespace gui {

void BeginWindowWithDisplaySettings(const char* id, bool* active,
                                    const ImVec2& size = ImVec2(0, 0),
                                    ImGuiWindowFlags flags = 0);

void EndWindowWithDisplaySettings();

void BeginPadding(int i);
void EndPadding();

void BeginNoPadding();
void EndNoPadding();

void BeginChildWithScrollbar(const char *str_id);

void BeginChildBothScrollbars(int id);

void DrawDisplaySettings(ImGuiStyle* ref = nullptr);

void TextWithSeparators(const absl::string_view& text);

void ColorsYaze();

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif