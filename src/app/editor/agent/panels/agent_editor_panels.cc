#include "app/editor/agent/panels/agent_editor_panels.h"

#include "app/editor/agent/agent_chat_widget.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void AgentChatPanel::Draw(bool* p_open) {
  if (chat_widget_) {
    chat_widget_->set_active(true);
    chat_widget_->Draw();
  } else {
    ImGui::TextDisabled("Chat widget not available");
  }
}

}  // namespace editor
}  // namespace yaze
