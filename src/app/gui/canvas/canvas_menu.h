#ifndef YAZE_APP_GUI_CANVAS_CANVAS_MENU_H
#define YAZE_APP_GUI_CANVAS_CANVAS_MENU_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Declarative popup definition for menu items
 * 
 * Links a menu item to a persistent popup that should open when the menu
 * item is selected. This separates popup definition from popup rendering.
 */
struct CanvasPopupDefinition {
  // Unique popup identifier for ImGui
  std::string popup_id;
  
  // Callback that renders the popup content (should call ImGui::BeginPopup/EndPopup)
  std::function<void()> render_callback;
  
  // Whether to automatically open the popup when menu item is selected
  bool auto_open_on_select = true;
  
  // Whether the popup should persist across frames until explicitly closed
  bool persist_across_frames = true;
  
  // Default constructor
  CanvasPopupDefinition() = default;
  
  // Constructor for simple popups
  CanvasPopupDefinition(const std::string& id, std::function<void()> callback)
      : popup_id(id), render_callback(std::move(callback)) {}
};

/**
 * @brief Declarative menu item definition
 * 
 * Pure data structure representing a menu item with optional popup linkage.
 * Can be composed into hierarchical menus via subitems.
 */
struct CanvasMenuItem {
  // Display label for the menu item
  std::string label;
  
  // Optional icon (Material Design icon name or Unicode glyph)
  std::string icon;
  
  // Optional keyboard shortcut display (e.g., "Ctrl+S")
  std::string shortcut;
  
  // Callback invoked when menu item is selected
  std::function<void()> callback;
  
  // Optional popup definition - if present, popup will be managed automatically
  std::optional<CanvasPopupDefinition> popup;
  
  // Condition to determine if menu item is enabled
  std::function<bool()> enabled_condition = []() { return true; };
  
  // Condition to determine if menu item is visible
  std::function<bool()> visible_condition = []() { return true; };
  
  // Nested submenu items
  std::vector<CanvasMenuItem> subitems;
  
  // Color for the menu item label
  ImVec4 color = ImVec4(1, 1, 1, 1);
  
  // Whether to show a separator after this item
  bool separator_after = false;
  
  // Default constructor
  CanvasMenuItem() = default;
  
  // Simple menu item constructor
  CanvasMenuItem(const std::string& lbl, std::function<void()> cb)
      : label(lbl), callback(std::move(cb)) {}
  
  // Menu item with icon
  CanvasMenuItem(const std::string& lbl, const std::string& ico,
                 std::function<void()> cb)
      : label(lbl), icon(ico), callback(std::move(cb)) {}
  
  // Menu item with icon and shortcut
  CanvasMenuItem(const std::string& lbl, const std::string& ico,
                 std::function<void()> cb, const std::string& sc)
      : label(lbl), icon(ico), callback(std::move(cb)), shortcut(sc) {}
  
  // Helper to create a disabled menu item
  static CanvasMenuItem Disabled(const std::string& lbl) {
    CanvasMenuItem item;
    item.label = lbl;
    item.enabled_condition = []() { return false; };
    return item;
  }
  
  // Helper to create a conditional menu item
  static CanvasMenuItem Conditional(const std::string& lbl,
                                   std::function<void()> cb,
                                   std::function<bool()> condition) {
    CanvasMenuItem item;
    item.label = lbl;
    item.callback = std::move(cb);
    item.enabled_condition = std::move(condition);
    return item;
  }
  
  // Helper to create a menu item with popup
  static CanvasMenuItem WithPopup(const std::string& lbl,
                                  const std::string& popup_id,
                                  std::function<void()> render_callback) {
    CanvasMenuItem item;
    item.label = lbl;
    item.popup = CanvasPopupDefinition(popup_id, std::move(render_callback));
    return item;
  }
};

/**
 * @brief Menu section grouping related menu items
 * 
 * Provides visual organization of menu items with optional section titles.
 */
struct CanvasMenuSection {
  // Optional section title (rendered as colored text)
  std::string title;
  
  // Color for section title
  ImVec4 title_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
  
  // Menu items in this section
  std::vector<CanvasMenuItem> items;
  
  // Whether to show a separator after this section
  bool separator_after = true;
  
  // Default constructor
  CanvasMenuSection() = default;
  
  // Constructor with title
  explicit CanvasMenuSection(const std::string& t) : title(t) {}
  
  // Constructor with title and items
  CanvasMenuSection(const std::string& t, const std::vector<CanvasMenuItem>& its)
      : title(t), items(its) {}
};

/**
 * @brief Complete menu definition
 * 
 * Aggregates menu sections for a complete context menu or popup menu.
 */
struct CanvasMenuDefinition {
  // Menu sections (rendered in order)
  std::vector<CanvasMenuSection> sections;
  
  // Whether the menu is enabled
  bool enabled = true;
  
  // Default constructor
  CanvasMenuDefinition() = default;
  
  // Constructor with sections
  explicit CanvasMenuDefinition(const std::vector<CanvasMenuSection>& secs)
      : sections(secs) {}
  
  // Add a section
  void AddSection(const CanvasMenuSection& section) {
    sections.push_back(section);
  }
  
  // Add items without a section title
  void AddItems(const std::vector<CanvasMenuItem>& items) {
    CanvasMenuSection section;
    section.items = items;
    section.separator_after = false;
    sections.push_back(section);
  }
};

// ==================== Free Function API ====================

/**
 * @brief Render a single menu item
 * 
 * Handles visibility, enabled state, subitems, and popup linkage.
 * 
 * @param item Menu item to render
 * @param popup_opened_callback Optional callback invoked when popup is opened
 */
void RenderMenuItem(const CanvasMenuItem& item,
                   std::function<void(const std::string&, std::function<void()>)> 
                       popup_opened_callback = nullptr);

/**
 * @brief Render a menu section
 * 
 * Renders section title (if present), all items, and separator.
 * 
 * @param section Menu section to render
 * @param popup_opened_callback Optional callback invoked when popup is opened
 */
void RenderMenuSection(const CanvasMenuSection& section,
                      std::function<void(const std::string&, std::function<void()>)>
                          popup_opened_callback = nullptr);

/**
 * @brief Render a complete menu definition
 * 
 * Renders all sections in order. Does not handle ImGui::BeginPopup/EndPopup -
 * caller is responsible for popup context.
 * 
 * @param menu Menu definition to render
 * @param popup_opened_callback Optional callback invoked when popup is opened
 */
void RenderCanvasMenu(const CanvasMenuDefinition& menu,
                     std::function<void(const std::string&, std::function<void()>)>
                         popup_opened_callback = nullptr);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_MENU_H

