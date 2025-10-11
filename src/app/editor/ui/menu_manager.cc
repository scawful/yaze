#include "app/editor/ui/menu_manager.h"

#include "app/editor/editor_manager.h"
#include "app/gui/icons.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace editor {

MenuManager::MenuManager(EditorManager* editor_manager)
    : editor_manager_(editor_manager) {}

void MenuManager::BuildAndDraw() {
  if (!editor_manager_) {
    return;
  }

  if (ImGui::BeginMenuBar()) {
    editor_manager_->BuildModernMenu();
    editor_manager_->DrawMenuBarExtras();
    ImGui::EndMenuBar();
  }
}

}  // namespace editor
}  // namespace yaze
