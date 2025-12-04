#ifndef YAZE_APP_GUI_CORE_POPUP_ID_H
#define YAZE_APP_GUI_CORE_POPUP_ID_H

#include <string>

#include "absl/strings/str_format.h"

namespace yaze {
namespace gui {

/**
 * @brief Generate session-aware popup IDs to prevent conflicts in multi-editor
 * layouts.
 *
 * ImGui popup IDs must be unique across the entire application. When multiple
 * editors are docked together, they may have popups with the same name (e.g.,
 * "Entrance Editor" in both OverworldEditor and entity.cc). This utility
 * generates unique IDs using the pattern: "s{session_id}.{editor}::{popup}"
 *
 * Example: "s0.Overworld::Entrance Editor"
 */

inline std::string MakePopupId(size_t session_id, const std::string& editor_name,
                               const std::string& popup_name) {
  return absl::StrFormat("s%zu.%s::%s", session_id, editor_name, popup_name);
}

/**
 * @brief Shorthand for editors without explicit session tracking.
 *
 * Uses session ID 0 by default. Prefer the session-aware overload when
 * the editor has access to its session context.
 */
inline std::string MakePopupId(const std::string& editor_name,
                               const std::string& popup_name) {
  return absl::StrFormat("s0.%s::%s", editor_name, popup_name);
}

/**
 * @brief Generate popup ID with instance pointer for guaranteed uniqueness.
 *
 * When you need absolute uniqueness even within the same editor type,
 * append the instance pointer. This is useful for reusable components.
 */
inline std::string MakePopupIdWithInstance(const std::string& editor_name,
                                           const std::string& popup_name,
                                           const void* instance) {
  return absl::StrFormat("s0.%s::%s##%p", editor_name, popup_name, instance);
}

// Common editor names for consistency
namespace EditorNames {
constexpr const char* kOverworld = "Overworld";
constexpr const char* kDungeon = "Dungeon";
constexpr const char* kGraphics = "Graphics";
constexpr const char* kPalette = "Palette";
constexpr const char* kSprite = "Sprite";
constexpr const char* kScreen = "Screen";
constexpr const char* kMusic = "Music";
constexpr const char* kMessage = "Message";
constexpr const char* kAssembly = "Assembly";
constexpr const char* kEntity = "Entity";  // For entity.cc shared popups
}  // namespace EditorNames

// Common popup names for consistency
namespace PopupNames {
// Entity editor popups
constexpr const char* kEntranceEditor = "Entrance Editor";
constexpr const char* kExitEditor = "Exit Editor";
constexpr const char* kItemEditor = "Item Editor";
constexpr const char* kSpriteEditor = "Sprite Editor";

// Map properties popups
constexpr const char* kGraphicsPopup = "GraphicsPopup";
constexpr const char* kPalettesPopup = "PalettesPopup";
constexpr const char* kConfigPopup = "ConfigPopup";
constexpr const char* kViewPopup = "ViewPopup";
constexpr const char* kQuickPopup = "QuickPopup";
constexpr const char* kOverlayTypesHelp = "OverlayTypesHelp";
constexpr const char* kInteractiveOverlayHelp = "InteractiveOverlayHelp";

// Palette editor popups
constexpr const char* kColorPicker = "ColorPicker";
constexpr const char* kCopyPopup = "CopyPopup";
constexpr const char* kSaveError = "SaveError";
constexpr const char* kConfirmDiscardAll = "ConfirmDiscardAll";
constexpr const char* kPalettePanelManager = "PalettePanelManager";

// General popups
constexpr const char* kColorEdit = "Color Edit";
constexpr const char* kConfirmDelete = "Confirm Delete";
constexpr const char* kConfirmDiscard = "Confirm Discard";
}  // namespace PopupNames

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_POPUP_ID_H
