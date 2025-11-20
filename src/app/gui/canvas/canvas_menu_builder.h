#ifndef YAZE_APP_GUI_CANVAS_CANVAS_MENU_BUILDER_H
#define YAZE_APP_GUI_CANVAS_CANVAS_MENU_BUILDER_H

#include <functional>
#include <string>
#include <vector>

#include "app/gui/canvas/canvas_menu.h"

namespace yaze {
namespace gui {

/**
 * @brief Builder pattern for constructing canvas menus fluently
 * 
 * Phase 4: Simplifies menu construction with chainable methods.
 * 
 * Example usage:
 * @code
 * CanvasMenuBuilder builder;
 * builder
 *   .AddItem("Cut", ICON_MD_CONTENT_CUT, []() { DoCut(); })
 *   .AddItem("Copy", ICON_MD_CONTENT_COPY, []() { DoCopy(); })
 *   .AddSeparator()
 *   .AddPopupItem("Properties", ICON_MD_SETTINGS, "props_popup", 
 *                 []() { RenderPropertiesPopup(); })
 *   .Build();
 * @endcode
 */
class CanvasMenuBuilder {
 public:
  CanvasMenuBuilder() = default;

  /**
   * @brief Add a simple menu item
   * @param label Menu item label
   * @param callback Action to perform when selected
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddItem(const std::string& label,
                             std::function<void()> callback);

  /**
   * @brief Add a menu item with icon
   * @param label Menu item label
   * @param icon Material Design icon or Unicode glyph
   * @param callback Action to perform when selected
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddItem(const std::string& label, const std::string& icon,
                             std::function<void()> callback);

  /**
   * @brief Add a menu item with icon and shortcut hint
   * @param label Menu item label
   * @param icon Material Design icon or Unicode glyph
   * @param shortcut Keyboard shortcut hint (e.g., "Ctrl+S")
   * @param callback Action to perform when selected
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddItem(const std::string& label, const std::string& icon,
                             const std::string& shortcut,
                             std::function<void()> callback);

  /**
   * @brief Add a menu item that opens a persistent popup
   * @param label Menu item label
   * @param popup_id Unique popup identifier
   * @param render_callback Callback to render popup content
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddPopupItem(const std::string& label,
                                  const std::string& popup_id,
                                  std::function<void()> render_callback);

  /**
   * @brief Add a menu item with icon that opens a persistent popup
   * @param label Menu item label
   * @param icon Material Design icon or Unicode glyph
   * @param popup_id Unique popup identifier
   * @param render_callback Callback to render popup content
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddPopupItem(const std::string& label,
                                  const std::string& icon,
                                  const std::string& popup_id,
                                  std::function<void()> render_callback);

  /**
   * @brief Add a conditional menu item (enabled only when condition is true)
   * @param label Menu item label
   * @param callback Action to perform when selected
   * @param condition Function that returns true when item should be enabled
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddConditionalItem(const std::string& label,
                                        std::function<void()> callback,
                                        std::function<bool()> condition);

  /**
   * @brief Add a submenu with nested items
   * @param label Submenu label
   * @param subitems Nested menu items
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddSubmenu(const std::string& label,
                                const std::vector<CanvasMenuItem>& subitems);

  /**
   * @brief Add a separator to visually group items
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& AddSeparator();

  /**
   * @brief Start a new section with optional title
   * @param title Section title (empty for no title)
   * @param priority Section priority (controls rendering order)
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& BeginSection(
      const std::string& title = "",
      MenuSectionPriority priority = MenuSectionPriority::kEditorSpecific);

  /**
   * @brief End the current section
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& EndSection();

  /**
   * @brief Build the final menu definition
   * @return Complete menu definition ready for rendering
   */
  CanvasMenuDefinition Build();

  /**
   * @brief Reset the builder to start building a new menu
   * @return Reference to this builder for chaining
   */
  CanvasMenuBuilder& Reset();

 private:
  CanvasMenuDefinition menu_;
  CanvasMenuSection* current_section_ = nullptr;
  std::vector<CanvasMenuItem> pending_items_;

  void FlushPendingItems();
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_MENU_BUILDER_H
