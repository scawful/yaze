#ifndef YAZE_APP_GUI_WIDGETS_CENTERED_ICON_BUTTON_H_
#define YAZE_APP_GUI_WIDGETS_CENTERED_ICON_BUTTON_H_

namespace yaze::gui {

// Square button for one visible font glyph, optionally followed by an ImGui
// ## ID suffix. Centers the glyph ink independently of its inline-text bearing.
// Retains ordinary button input, navigation, IDs, styling and disabled alpha.
bool CenteredIconButton(const char* label, float button_size);

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_WIDGETS_CENTERED_ICON_BUTTON_H_
