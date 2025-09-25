#ifndef YAZE_APP_EDITOR_OVERWORLD_UI_CONSTANTS_H
#define YAZE_APP_EDITOR_OVERWORLD_UI_CONSTANTS_H

namespace yaze {
namespace editor {

// Game State Labels
inline constexpr const char* kGameStateNames[] = {
    "Rain & Rescue Zelda", 
    "Pendants", 
    "Crystals"
};

// World Labels
inline constexpr const char* kWorldNames[] = {
    "Light World", 
    "Dark World", 
    "Special World"
};

// Area Size Names
inline constexpr const char* kAreaSizeNames[] = {
    "Small (1x1)", 
    "Large (2x2)", 
    "Wide (2x1)", 
    "Tall (1x2)"
};

// UI Styling Constants
inline constexpr float kInputFieldSize = 30.f;
inline constexpr float kCompactButtonWidth = 60.f;
inline constexpr float kIconButtonWidth = 30.f;
inline constexpr float kSmallButtonWidth = 80.f;
inline constexpr float kMediumButtonWidth = 90.f;
inline constexpr float kLargeButtonWidth = 100.f;

// Spacing and Padding
inline constexpr float kCompactItemSpacing = 4.f;
inline constexpr float kCompactFramePadding = 2.f;

// Map Size Constants - using the one from overworld_editor.h

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_UI_CONSTANTS_H
