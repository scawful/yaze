#ifndef YAZE_APP_GUI_CANVAS_ITEM_CONTEXT_MENU_H
#define YAZE_APP_GUI_CANVAS_ITEM_CONTEXT_MENU_H

#include <functional>
#include <string>
#include <vector>

#include "app/gui/canvas/canvas_menu.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Right-click menu for the last submitted ImGui item.
 *
 * Wraps BeginPopupContextItem / RenderMenuItem / EndPopup so list rows, tabs,
 * chips and swatches share the canvas menu renderer (icons, shortcut hints,
 * destructive color, two-step confirmation). `build` runs only while the
 * popup is open, so per-row menus cost nothing when closed.
 *
 * @param id Popup string ID, or nullptr to bind to the last item's ID.
 * @return true when the popup was open this frame.
 */
bool ItemContextMenu(const char* id,
                     const std::function<std::vector<MenuItemSpec>()>& build,
                     ImGuiPopupFlags flags = ImGuiPopupFlags_None);

// Same, for menus that need section titles.
bool ItemContextMenu(const char* id,
                     const std::function<CanvasMenuDefinition()>& build,
                     ImGuiPopupFlags flags = ImGuiPopupFlags_None);

// Render a flat list of items (no popup handling). For use inside an existing
// BeginPopup / BeginPopupContext* block.
void RenderMenuItems(const std::vector<MenuItemSpec>& items);

// "Copy <thing>" item that places `text` on the clipboard.
MenuItemSpec CopyToClipboardItem(const std::string& label,
                                 const std::string& text);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_ITEM_CONTEXT_MENU_H
