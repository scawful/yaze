#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_ROM_SYNC_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_ROM_SYNC_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/agent/agent_state.h"

namespace yaze {
namespace editor {

class ToastManager;

class AgentRomSyncPanel {
 public:
  AgentRomSyncPanel() = default;

  void Draw(AgentUIContext* context, const RomSyncCallbacks& callbacks, ToastManager* toast_manager);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_ROM_SYNC_PANEL_H_
