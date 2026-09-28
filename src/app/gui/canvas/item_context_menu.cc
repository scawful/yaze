#include "app/gui/canvas/item_context_menu.h"

#include "app/gui/core/icons.h"

namespace yaze {
namespace gui {

bool ItemContextMenu(const char* id,
                     const std::function<std::vector<MenuItemSpec>()>& build,
                     ImGuiPopupFlags flags) {
  // Pushed before Begin: a row inside a zero-padding region (a canvas) would
  // otherwise hand that padding to the popup window.
  PopupStyleScope popup_style;
  if (!ImGui::BeginPopupContextItem(id, flags)) {
    return false;
  }
  if (build) {
    RenderMenuItems(build());
  }
  ImGui::EndPopup();
  return true;
}

bool ItemContextMenu(const char* id,
                     const std::function<CanvasMenuDefinition()>& build,
                     ImGuiPopupFlags flags) {
  // Pushed before Begin: a row inside a zero-padding region (a canvas) would
  // otherwise hand that padding to the popup window.
  PopupStyleScope popup_style;
  if (!ImGui::BeginPopupContextItem(id, flags)) {
    return false;
  }
  if (build) {
    RenderCanvasMenu(build());
  }
  ImGui::EndPopup();
  return true;
}

void RenderMenuItems(const std::vector<MenuItemSpec>& items) {
  for (const auto& item : items) {
    RenderMenuItem(item);
  }
}

MenuItemSpec CopyToClipboardItem(const std::string& label,
                                 const std::string& text) {
  return MenuItemSpec(label, ICON_MD_CONTENT_COPY,
                      [text]() { ImGui::SetClipboardText(text.c_str()); });
}

}  // namespace gui
}  // namespace yaze
