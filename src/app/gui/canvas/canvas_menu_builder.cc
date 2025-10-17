#include "canvas_menu_builder.h"

namespace yaze {
namespace gui {

CanvasMenuBuilder& CanvasMenuBuilder::AddItem(const std::string& label,
                                               std::function<void()> callback) {
  CanvasMenuItem item;
  item.label = label;
  item.callback = std::move(callback);
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddItem(const std::string& label,
                                               const std::string& icon,
                                               std::function<void()> callback) {
  CanvasMenuItem item;
  item.label = label;
  item.icon = icon;
  item.callback = std::move(callback);
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddItem(const std::string& label,
                                               const std::string& icon,
                                               const std::string& shortcut,
                                               std::function<void()> callback) {
  CanvasMenuItem item;
  item.label = label;
  item.icon = icon;
  item.shortcut = shortcut;
  item.callback = std::move(callback);
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddPopupItem(
    const std::string& label, const std::string& popup_id,
    std::function<void()> render_callback) {
  CanvasMenuItem item = CanvasMenuItem::WithPopup(label, popup_id, render_callback);
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddPopupItem(
    const std::string& label, const std::string& icon, 
    const std::string& popup_id, std::function<void()> render_callback) {
  CanvasMenuItem item = CanvasMenuItem::WithPopup(label, popup_id, render_callback);
  item.icon = icon;
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddConditionalItem(
    const std::string& label, std::function<void()> callback,
    std::function<bool()> condition) {
  CanvasMenuItem item = CanvasMenuItem::Conditional(label, callback, condition);
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddSubmenu(
    const std::string& label, const std::vector<CanvasMenuItem>& subitems) {
  CanvasMenuItem item;
  item.label = label;
  item.subitems = subitems;
  pending_items_.push_back(item);
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::AddSeparator() {
  if (!pending_items_.empty()) {
    pending_items_.back().separator_after = true;
  }
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::BeginSection(
    const std::string& title, MenuSectionPriority priority) {
  // Flush any pending items to previous section
  FlushPendingItems();
  
  // Create new section
  CanvasMenuSection section;
  section.title = title;
  section.priority = priority;
  section.separator_after = true;
  menu_.sections.push_back(section);
  
  // Point current_section_ to the newly added section
  current_section_ = &menu_.sections.back();
  
  return *this;
}

CanvasMenuBuilder& CanvasMenuBuilder::EndSection() {
  FlushPendingItems();
  current_section_ = nullptr;
  return *this;
}

CanvasMenuDefinition CanvasMenuBuilder::Build() {
  FlushPendingItems();
  return menu_;
}

CanvasMenuBuilder& CanvasMenuBuilder::Reset() {
  menu_.sections.clear();
  pending_items_.clear();
  current_section_ = nullptr;
  return *this;
}

void CanvasMenuBuilder::FlushPendingItems() {
  if (pending_items_.empty()) {
    return;
  }
  
  // If no section exists yet, create a default one
  if (menu_.sections.empty()) {
    CanvasMenuSection section;
    section.priority = MenuSectionPriority::kEditorSpecific;
    section.separator_after = true;
    menu_.sections.push_back(section);
    current_section_ = &menu_.sections.back();
  }
  
  // Add pending items to current section
  if (current_section_) {
    current_section_->items.insert(current_section_->items.end(),
                                  pending_items_.begin(), pending_items_.end());
  } else {
    // Add to last section if current_section_ is null
    menu_.sections.back().items.insert(menu_.sections.back().items.end(),
                                       pending_items_.begin(), pending_items_.end());
  }
  
  pending_items_.clear();
}

}  // namespace gui
}  // namespace yaze

