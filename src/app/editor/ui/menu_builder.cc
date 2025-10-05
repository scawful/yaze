#include "app/editor/ui/menu_builder.h"

#include "absl/strings/str_cat.h"

namespace yaze {
namespace editor {

MenuBuilder& MenuBuilder::BeginMenu(const char* label, const char* icon) {
  Menu menu;
  menu.label = label;
  if (icon) {
    menu.icon = icon;
  }
  menus_.push_back(menu);
  current_menu_ = &menus_.back();
  return *this;
}

MenuBuilder& MenuBuilder::BeginSubMenu(const char* label, const char* icon,
                                       EnabledCheck enabled) {
  if (!current_menu_) return *this;
  
  MenuItem item;
  item.type = MenuItem::Type::kSubMenuBegin;
  item.label = label;
  if (icon) {
    item.icon = icon;
  }
  item.enabled = enabled;
  current_menu_->items.push_back(item);
  return *this;
}

MenuBuilder& MenuBuilder::EndMenu() {
  if (!current_menu_) return *this;
  
  // Check if we're ending a submenu or top-level menu
  // We need to track nesting depth to handle nested submenus correctly
  bool is_submenu = false;
  int depth = 0;
  
  for (auto it = current_menu_->items.rbegin(); 
       it != current_menu_->items.rend(); ++it) {
    if (it->type == MenuItem::Type::kSubMenuEnd) {
      depth++;  // Found an end, so we need to skip its matching begin
    } else if (it->type == MenuItem::Type::kSubMenuBegin) {
      if (depth == 0) {
        // Found an unmatched begin - this is what we're closing
        is_submenu = true;
        break;
      }
      depth--;  // This begin matches a previous end
    }
  }
  
  if (is_submenu) {
    MenuItem item;
    item.type = MenuItem::Type::kSubMenuEnd;
    current_menu_->items.push_back(item);
  } else {
    current_menu_ = nullptr;
  }
  
  return *this;
}

MenuBuilder& MenuBuilder::Item(const char* label, const char* icon,
                               Callback callback, const char* shortcut,
                               EnabledCheck enabled, EnabledCheck checked) {
  if (!current_menu_) return *this;
  
  MenuItem item;
  item.type = MenuItem::Type::kItem;
  item.label = label;
  if (icon) {
    item.icon = icon;
  }
  if (shortcut) {
    item.shortcut = shortcut;
  }
  item.callback = callback;
  item.enabled = enabled;
  item.checked = checked;
  current_menu_->items.push_back(item);
  return *this;
}

MenuBuilder& MenuBuilder::Item(const char* label, Callback callback,
                               const char* shortcut, EnabledCheck enabled) {
  return Item(label, nullptr, callback, shortcut, enabled, nullptr);
}

MenuBuilder& MenuBuilder::Separator() {
  if (!current_menu_) return *this;
  
  MenuItem item;
  item.type = MenuItem::Type::kSeparator;
  current_menu_->items.push_back(item);
  return *this;
}

MenuBuilder& MenuBuilder::DisabledItem(const char* label, const char* icon) {
  if (!current_menu_) return *this;
  
  MenuItem item;
  item.type = MenuItem::Type::kDisabled;
  item.label = label;
  if (icon) {
    item.icon = icon;
  }
  current_menu_->items.push_back(item);
  return *this;
}

void MenuBuilder::Draw() {
  for (const auto& menu : menus_) {
    // Don't add icons to top-level menus as they get cut off
    std::string menu_label = menu.label;
    
    if (ImGui::BeginMenu(menu_label.c_str())) {
      submenu_stack_.clear();  // Reset submenu stack for each top-level menu
      skip_depth_ = 0;  // Reset skip depth
      for (const auto& item : menu.items) {
        DrawMenuItem(item);
      }
      ImGui::EndMenu();
    }
  }
}

void MenuBuilder::DrawMenuItem(const MenuItem& item) {
  switch (item.type) {
    case MenuItem::Type::kSeparator:
      if (skip_depth_ == 0) {
        ImGui::Separator();
      }
      break;
      
    case MenuItem::Type::kDisabled: {
      if (skip_depth_ == 0) {
        std::string label = item.icon.empty()
            ? item.label
            : absl::StrCat(item.icon, " ", item.label);
        ImGui::BeginDisabled();
        ImGui::MenuItem(label.c_str(), nullptr, false, false);
        ImGui::EndDisabled();
      }
      break;
    }
      
    case MenuItem::Type::kSubMenuBegin: {
      // If we're already skipping, increment skip depth and continue
      if (skip_depth_ > 0) {
        skip_depth_++;
        submenu_stack_.push_back(false);
        break;
      }
      
      std::string label = item.icon.empty()
          ? item.label
          : absl::StrCat(item.icon, " ", item.label);
      
      bool enabled = !item.enabled || item.enabled();
      bool opened = false;
      
      if (!enabled) {
        // Disabled submenu - show as disabled item but don't open
        ImGui::BeginDisabled();
        ImGui::MenuItem(label.c_str(), nullptr, false, false);
        ImGui::EndDisabled();
        submenu_stack_.push_back(false);
        skip_depth_++;  // Skip contents of disabled submenu
      } else {
        // BeginMenu returns true if submenu is currently open/visible
        opened = ImGui::BeginMenu(label.c_str());
        submenu_stack_.push_back(opened);
        if (!opened) {
          skip_depth_++;  // Skip contents of closed submenu
        }
      }
      break;
    }
      
    case MenuItem::Type::kSubMenuEnd:
      // Decrement skip depth if we were skipping
      if (skip_depth_ > 0) {
        skip_depth_--;
      }
      
      // Pop the stack and call EndMenu only if submenu was opened
      if (!submenu_stack_.empty()) {
        bool was_opened = submenu_stack_.back();
        submenu_stack_.pop_back();
        if (was_opened && skip_depth_ == 0) {
          ImGui::EndMenu();
        }
      }
      break;
      
    case MenuItem::Type::kItem: {
      if (skip_depth_ > 0) {
        break;  // Skip items in closed submenus
      }
      
      std::string label = item.icon.empty()
          ? item.label
          : absl::StrCat(item.icon, " ", item.label);
      
      bool enabled = !item.enabled || item.enabled();
      bool checked = item.checked && item.checked();
      
      const char* shortcut_str = item.shortcut.empty() 
          ? nullptr 
          : item.shortcut.c_str();
      
      if (ImGui::MenuItem(label.c_str(), shortcut_str, checked, enabled)) {
        if (item.callback) {
          item.callback();
        }
      }
      break;
    }
  }
}

void MenuBuilder::Clear() {
  menus_.clear();
  current_menu_ = nullptr;
}

}  // namespace editor
}  // namespace yaze
