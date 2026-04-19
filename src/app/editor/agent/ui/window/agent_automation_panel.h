#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_AUTOMATION_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_AUTOMATION_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/agent/agent_state.h"

namespace yaze {
namespace editor {

class AgentAutomationPanel {
 public:
  AgentAutomationPanel() = default;

  void Draw(AgentUIContext* context, const AutomationCallbacks& callbacks);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_AUTOMATION_PANEL_H_
