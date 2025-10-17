#include "canvas_popup.h"

#include <algorithm>

namespace yaze {
namespace gui {

void PopupRegistry::Open(const std::string& popup_id,
                        std::function<void()> render_callback) {
  // Check if popup already exists
  auto it = FindPopup(popup_id);
  
  if (it != popups_.end()) {
    // Update existing popup
    it->is_open = true;
    it->render_callback = std::move(render_callback);
    ImGui::OpenPopup(popup_id.c_str());
    return;
  }
  
  // Add new popup
  PopupState new_popup;
  new_popup.popup_id = popup_id;
  new_popup.is_open = true;
  new_popup.render_callback = std::move(render_callback);
  new_popup.persist = true;
  
  popups_.push_back(new_popup);
  
  // Open the popup in ImGui
  ImGui::OpenPopup(popup_id.c_str());
}

void PopupRegistry::Close(const std::string& popup_id) {
  auto it = FindPopup(popup_id);
  
  if (it != popups_.end()) {
    it->is_open = false;
    
    // Close in ImGui if it's the current popup
    // Note: ImGui::CloseCurrentPopup() only works if this is the active popup
    // In practice, the popup will be removed on next RenderAll() call
    if (ImGui::IsPopupOpen(popup_id.c_str())) {
      ImGui::CloseCurrentPopup();
    }
  }
}

bool PopupRegistry::IsOpen(const std::string& popup_id) const {
  auto it = FindPopup(popup_id);
  return it != popups_.end() && it->is_open;
}

void PopupRegistry::RenderAll() {
  // Render all active popups
  auto it = popups_.begin();
  while (it != popups_.end()) {
    if (it->is_open && it->render_callback) {
      // Call the render callback which should handle BeginPopup/EndPopup
      it->render_callback();
      
      // Check if popup was closed by user (clicking outside, pressing Escape, etc.)
      if (!ImGui::IsPopupOpen(it->popup_id.c_str())) {
        it->is_open = false;
      }
    }
    
    // Remove closed popups from the registry
    if (!it->is_open) {
      it = popups_.erase(it);
    } else {
      ++it;
    }
  }
}

size_t PopupRegistry::GetActiveCount() const {
  return std::count_if(popups_.begin(), popups_.end(),
                      [](const PopupState& popup) { return popup.is_open; });
}

void PopupRegistry::Clear() {
  // Close all popups
  for (auto& popup : popups_) {
    if (popup.is_open && ImGui::IsPopupOpen(popup.popup_id.c_str())) {
      ImGui::CloseCurrentPopup();
    }
    popup.is_open = false;
  }
  
  // Clear the registry
  popups_.clear();
}

std::vector<PopupState>::iterator PopupRegistry::FindPopup(
    const std::string& popup_id) {
  return std::find_if(popups_.begin(), popups_.end(),
                     [&popup_id](const PopupState& popup) {
                       return popup.popup_id == popup_id;
                     });
}

std::vector<PopupState>::const_iterator PopupRegistry::FindPopup(
    const std::string& popup_id) const {
  return std::find_if(popups_.begin(), popups_.end(),
                     [&popup_id](const PopupState& popup) {
                       return popup.popup_id == popup_id;
                     });
}

}  // namespace gui
}  // namespace yaze

