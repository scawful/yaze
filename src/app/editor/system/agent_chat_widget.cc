#include "app/editor/system/agent_chat_widget.h"
#include "imgui.h"

namespace yaze {
namespace editor {

AgentChatWidget::AgentChatWidget() {
  title_ = "Agent Chat";
  memset(input_buffer_, 0, sizeof(input_buffer_));
}

void AgentChatWidget::Draw() {
  if (!active_) {
    return;
  }

  ImGui::Begin(title_.c_str(), &active_);

  // Display message history
  ImGui::BeginChild("History", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
  for (const auto& msg : agent_service_.GetHistory()) {
    std::string prefix =
        msg.sender == cli::agent::ChatMessage::Sender::kUser ? "You: " : "Agent: ";
    ImGui::TextWrapped((prefix + msg.message).c_str());
  }
  ImGui::EndChild();

  // Display input text box
  if (ImGui::InputText("Input", input_buffer_, sizeof(input_buffer_),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    if (strlen(input_buffer_) > 0) {
      (void)agent_service_.SendMessage(input_buffer_);
      memset(input_buffer_, 0, sizeof(input_buffer_));
    }
    ImGui::SetKeyboardFocusHere(-1); // Refocus input
  }

  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
