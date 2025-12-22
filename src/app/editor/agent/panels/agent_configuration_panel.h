#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_CONFIGURATION_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_CONFIGURATION_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/agent/agent_state.h"

namespace yaze {
namespace editor {

class ToastManager;

class AgentConfigurationPanel {
 public:
  struct Callbacks {
    std::function<void(const AgentConfigState&)> update_config;
    std::function<void(bool force)> refresh_models;
    std::function<void(const ModelPreset&)> apply_preset;
    std::function<void()> apply_tool_preferences;
  };

  AgentConfigurationPanel() = default;

  void Draw(AgentUIContext* context, const Callbacks& callbacks, ToastManager* toast_manager);

 private:
  void RenderModelConfigControls(AgentUIContext* context, const Callbacks& callbacks, ToastManager* toast_manager);
  void RenderModelDeck(AgentUIContext* context, const Callbacks& callbacks, ToastManager* toast_manager);
  void RenderParameterControls(AgentConfigState& config);
  void RenderToolingControls(AgentConfigState& config, const Callbacks& callbacks);
  void RenderChainModeControls(AgentConfigState& config);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_CONFIGURATION_PANEL_H_
