#ifndef YAZE_APP_EDITOR_POPUP_MANAGER_H
#define YAZE_APP_EDITOR_POPUP_MANAGER_H

namespace yaze {
namespace editor {

// ImGui popup manager.
class PopupManager {
 public:
  PopupManager();
  ~PopupManager();

  void Show(const char* name);
};

} // namespace editor
} // namespace yaze

#endif // YAZE_APP_EDITOR_POPUP_MANAGER_H
