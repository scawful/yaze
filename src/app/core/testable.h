#ifndef YAZE_APP_CORE_TESTABLE_H
#define YAZE_APP_CORE_TESTABLE_H

#include <imgui_test_engine/imgui_te_context.h>

namespace yaze {
namespace app {
namespace core {
class GuiTestable {
 public:
  virtual void RegisterTests(ImGuiTestEngine* e) = 0;

  ImGuiTestEngine* test_engine;
};
}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_TESTABLE_H