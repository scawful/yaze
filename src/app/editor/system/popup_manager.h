#ifndef YAZE_APP_EDITOR_POPUP_MANAGER_H
#define YAZE_APP_EDITOR_POPUP_MANAGER_H

#include <string>

namespace yaze {
namespace editor {

struct PopupParams {
  std::string name;
};

// ImGui popup manager.
class PopupManager {
 public:

  void Show(const char* name);

 private:
  std::vector<PopupParams> popups_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_POPUP_MANAGER_H
