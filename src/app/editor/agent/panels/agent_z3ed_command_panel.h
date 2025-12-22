#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_Z3ED_COMMAND_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_Z3ED_COMMAND_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/agent/agent_state.h"

namespace yaze {
namespace editor {

class ToastManager;

class AgentZ3EDCommandPanel {
 public:
  AgentZ3EDCommandPanel() = default;

  void Draw(AgentUIContext* context, const Z3EDCommandCallbacks& callbacks, ToastManager* toast_manager);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_Z3ED_COMMAND_PANEL_H_
