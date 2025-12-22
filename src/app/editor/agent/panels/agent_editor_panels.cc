#include "app/editor/agent/panels/agent_editor_panels.h"

#include "app/editor/agent/agent_chat.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void AgentChatPanel::Draw(bool* p_open) {
  if (chat_) {
    chat_->set_active(true);
    chat_->Draw();
  } else {
    ImGui::TextDisabled("Chat not available");
  }
}

}  // namespace editor
}  // namespace yaze