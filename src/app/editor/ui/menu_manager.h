#ifndef YAZE_APP_EDITOR_UI_MENU_MANAGER_H_
#define YAZE_APP_EDITOR_UI_MENU_MANAGER_H_

#include "app/editor/ui/menu_builder.h"

namespace yaze {
namespace editor {

class EditorManager;

class MenuManager {
 public:
  explicit MenuManager(EditorManager* editor_manager);

  void BuildAndDraw();

 private:
  EditorManager* editor_manager_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_MENU_MANAGER_H_