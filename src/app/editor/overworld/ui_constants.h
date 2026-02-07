#ifndef YAZE_APP_EDITOR_OVERWORLD_UI_CONSTANTS_H
#define YAZE_APP_EDITOR_OVERWORLD_UI_CONSTANTS_H

namespace yaze {
namespace editor {

// Game State Labels
inline constexpr const char* kGameStateNames[] = {"Rain & Rescue Zelda",
                                                  "Pendants", "Crystals"};

// World Labels
inline constexpr const char* kWorldNames[] = {"Light World", "Dark World",
                                              "Special World"};

// Area Size Names
inline constexpr const char* kAreaSizeNames[] = {"Small (1x1)", "Large (2x2)",
                                                 "Wide (2x1)", "Tall (1x2)"};

// UI Styling Constants
inline constexpr float kInputFieldSize = 30.f;
inline constexpr float kHexByteInputWidth = 50.f;
inline constexpr float kHexWordInputWidth = 70.f;
inline constexpr float kCompactButtonWidth = 60.f;
inline constexpr float kIconButtonWidth = 40.f;         // Comfortable touch target
inline constexpr float kPanelToggleButtonWidth = 40.f;  // Panel toggle buttons
inline constexpr float kSmallButtonWidth = 80.f;
inline constexpr float kMediumButtonWidth = 90.f;
inline constexpr float kLargeButtonWidth = 100.f;

// Table Column Width Constants
inline constexpr float kTableColumnWorld = 120.f;
inline constexpr float kTableColumnMap = 80.f;
inline constexpr float kTableColumnAreaSize = 120.f;
inline constexpr float kTableColumnLock = 50.f;
inline constexpr float kTableColumnGraphics = 80.f;
inline constexpr float kTableColumnPalettes = 80.f;
inline constexpr float kTableColumnProperties = 100.f;
inline constexpr float kTableColumnTools = 80.f;
inline constexpr float kTableColumnView = 80.f;
inline constexpr float kTableColumnQuick = 80.f;

// Combo Box Width Constants
inline constexpr float kComboWorldWidth = 115.f;
inline constexpr float kComboAreaSizeWidth = 115.f;
inline constexpr float kComboGameStateWidth = 100.f;

// Button Width Constants for Table
inline constexpr float kTableButtonGraphics = 75.f;
inline constexpr float kTableButtonPalettes = 75.f;
inline constexpr float kTableButtonProperties = 95.f;
inline constexpr float kTableButtonTools = 75.f;
inline constexpr float kTableButtonView = 75.f;
inline constexpr float kTableButtonQuick = 75.f;

// Spacing and Padding
inline constexpr float kCompactItemSpacing = 4.f;
inline constexpr float kCompactFramePadding = 2.f;

// Map Size Constants - using the one from overworld_editor.h

enum class EditingMode { MOUSE = 0, DRAW_TILE = 1, FILL_TILE = 2 };

enum class EntityEditMode {
  NONE = 0,
  ENTRANCES = 1,
  EXITS = 2,
  ITEMS = 3,
  SPRITES = 4,
  TRANSPORTS = 5,
  MUSIC = 6
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_UI_CONSTANTS_H
