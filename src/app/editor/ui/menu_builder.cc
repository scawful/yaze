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
  bool is_submenu = false;
  for (auto it = current_menu_->items.rbegin(); 
       it != current_menu_->items.rend(); ++it) {
    if (it->type == MenuItem::Type::kSubMenuBegin) {
      is_submenu = true;
      break;
    } else if (it->type == MenuItem::Type::kSubMenuEnd) {
      break;
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
    std::string menu_label = menu.icon.empty() 
        ? menu.label 
        : absl::StrCat(menu.icon, " ", menu.label);
    
    if (ImGui::BeginMenu(menu_label.c_str())) {
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
      ImGui::Separator();
      break;
      
    case MenuItem::Type::kDisabled: {
      std::string label = item.icon.empty()
          ? item.label
          : absl::StrCat(item.icon, " ", item.label);
      ImGui::BeginDisabled();
      ImGui::MenuItem(label.c_str(), nullptr, false, false);
      ImGui::EndDisabled();
      break;
    }
      
    case MenuItem::Type::kSubMenuBegin: {
      std::string label = item.icon.empty()
          ? item.label
          : absl::StrCat(item.icon, " ", item.label);
      
      bool enabled = !item.enabled || item.enabled();
      if (enabled && ImGui::BeginMenu(label.c_str())) {
        // Submenu items will be drawn in subsequent calls
      } else if (!enabled) {
        ImGui::BeginDisabled();
        ImGui::MenuItem(label.c_str(), nullptr, false, false);
        ImGui::EndDisabled();
      }
      break;
    }
      
    case MenuItem::Type::kSubMenuEnd:
      ImGui::EndMenu();
      break;
      
    case MenuItem::Type::kItem: {
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
