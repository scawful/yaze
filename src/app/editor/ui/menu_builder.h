#ifndef YAZE_APP_EDITOR_UI_MENU_BUILDER_H_
#define YAZE_APP_EDITOR_UI_MENU_BUILDER_H_

#include <functional>
#include <string>
#include <vector>

// Must define before including imgui.h
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @class MenuBuilder
 * @brief Fluent interface for building ImGui menus with icons
 * 
 * Provides a cleaner, more maintainable way to construct menus:
 * 
 * MenuBuilder menu;
 * menu.BeginMenu("File", ICON_MD_FOLDER)
 *     .Item("Open", ICON_MD_FILE_OPEN, []() { OpenFile(); })
 *     .Separator()
 *     .Item("Quit", ICON_MD_EXIT_TO_APP, []() { Quit(); })
 *     .EndMenu();
 */
class MenuBuilder {
 public:
  using Callback = std::function<void()>;
  using EnabledCheck = std::function<bool()>;
  
  MenuBuilder() = default;
  
  /**
   * @brief Begin a top-level menu
   */
  MenuBuilder& BeginMenu(const char* label, const char* icon = nullptr);
  
  /**
   * @brief Begin a submenu
   */
  MenuBuilder& BeginSubMenu(const char* label, const char* icon = nullptr,
                            EnabledCheck enabled = nullptr);
  
  /**
   * @brief End the current menu/submenu
   */
  MenuBuilder& EndMenu();
  
  /**
   * @brief Add a menu item
   */
  MenuBuilder& Item(const char* label, const char* icon, Callback callback,
                    const char* shortcut = nullptr,
                    EnabledCheck enabled = nullptr,
                    EnabledCheck checked = nullptr);
  
  /**
   * @brief Add a menu item without icon (convenience)
   */
  MenuBuilder& Item(const char* label, Callback callback,
                    const char* shortcut = nullptr,
                    EnabledCheck enabled = nullptr);
  
  /**
   * @brief Add a separator
   */
  MenuBuilder& Separator();
  
  /**
   * @brief Add a disabled item (grayed out)
   */
  MenuBuilder& DisabledItem(const char* label, const char* icon = nullptr);
  
  /**
   * @brief Draw the menu bar (call in main menu bar)
   */
  void Draw();
  
  /**
   * @brief Clear all menus
   */
  void Clear();
  
 private:
  struct MenuItem {
    enum class Type {
      kItem,
      kSubMenuBegin,
      kSubMenuEnd,
      kSeparator,
      kDisabled
    };
    
    Type type;
    std::string label;
    std::string icon;
    std::string shortcut;
    Callback callback;
    EnabledCheck enabled;
    EnabledCheck checked;
  };
  
  struct Menu {
    std::string label;
    std::string icon;
    std::vector<MenuItem> items;
  };
  
  std::vector<Menu> menus_;
  Menu* current_menu_ = nullptr;
  
  // Track which submenus are actually open during drawing
  mutable std::vector<bool> submenu_stack_;
  mutable int skip_depth_ = 0;  // Track nesting depth when skipping closed submenus
  
  void DrawMenuItem(const MenuItem& item);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_MENU_BUILDER_H_
