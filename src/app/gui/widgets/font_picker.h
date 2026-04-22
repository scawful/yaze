#ifndef YAZE_APP_GUI_WIDGETS_FONT_PICKER_H_
#define YAZE_APP_GUI_WIDGETS_FONT_PICKER_H_

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Dropdown listing all fonts registered with ImGui::GetIO().Fonts. The
// preview row renders each font's name in its own face. Returns true on
// the frame the user selects a different font and mutates *index.
//
// Persistence, active-font application, and undo are intentionally the
// caller's responsibility:
//
//   int idx = prefs.font_family_index;
//   if (gui::FontPicker("Font", &idx)) {
//     prefs.font_family_index = idx;
//     gui::SetActiveFontIndex(idx);
//     settings_dirty_ = true;
//   }
//
// `label` is shown left of the combo per ImGui convention. Pass the empty
// string or "##foo" to suppress the label.
bool FontPicker(const char* label, int* index);

namespace font_picker_internal {

// Returns a human-readable name for the font at the given index.
// Uses ImFont::GetDebugName() when non-empty, otherwise "Font #N".
const char* FontNameAt(int index);

// Returns the count of registered fonts (ImGui::GetIO().Fonts->Fonts.Size),
// or 0 when the ImGui context/font atlas isn't set up yet.
int RegisteredFontCount();

}  // namespace font_picker_internal

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_FONT_PICKER_H_
