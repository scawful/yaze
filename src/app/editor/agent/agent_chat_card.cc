#include "app/editor/agent/agent_chat_card.h"

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace editor {

AgentChatCard::AgentChatCard(const std::string& agent_id,
                             AgentSessionManager* session_manager)
    : agent_id_(agent_id), session_manager_(session_manager) {
  // Configure chat view for full (non-compact) mode
  chat_view_.SetCompactMode(false);

  // Get the session and set up the chat view's context
  if (AgentSession* session = session_manager_->GetSession(agent_id_)) {
    chat_view_.SetContext(&session->context);
    chat_view_.SetChatCallbacks(session->chat_callbacks);
    chat_view_.SetProposalCallbacks(session->proposal_callbacks);
  }

  LOG_INFO("AgentChatCard", "Created card for agent: %s", agent_id_.c_str());
}

void AgentChatCard::SetToastManager(ToastManager* toast_manager) {
  toast_manager_ = toast_manager;
  chat_view_.SetToastManager(toast_manager);
}

void AgentChatCard::SetAgentService(
    cli::agent::ConversationalAgentService* service) {
  chat_view_.SetAgentService(service);
}

std::string AgentChatCard::GetWindowTitle() const {
  if (const AgentSession* session = session_manager_->GetSession(agent_id_)) {
    return absl::StrFormat("%s %s###AgentPanel_%s", ICON_MD_SMART_TOY,
                           session->display_name, agent_id_);
  }
  return absl::StrFormat("%s Agent###AgentPanel_%s", ICON_MD_SMART_TOY,
                         agent_id_);
}

void AgentChatCard::Draw(bool* p_open) {
  if (!p_open || !*p_open) {
    return;
  }

  // Verify session still exists
  AgentSession* session = session_manager_->GetSession(agent_id_);
  if (!session) {
    LOG_WARN("AgentChatCard", "Session no longer exists: %s", agent_id_.c_str());
    *p_open = false;
    return;
  }

  // Ensure chat view has current context (in case session was modified)
  chat_view_.SetContext(&session->context);

  // Window flags - dockable, with menu bar for controls
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;

  // Set initial size on first frame
  if (first_frame_) {
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
    first_frame_ = false;
  }

  // Apply agent theme colors
  const auto& theme = AgentUI::GetTheme();
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.panel_bg_color);
  ImGui::PushStyleColor(ImGuiCol_TitleBg, theme.panel_bg_darker);
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, theme.panel_bg_color);

  std::string window_title = GetWindowTitle();
  if (ImGui::Begin(window_title.c_str(), p_open, flags)) {
    // Header with session info
    ImGui::PushStyleColor(ImGuiCol_Text, theme.agent_message_color);
    ImGui::Text("%s", session->display_name.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("| %s", session->context.agent_config().ai_model.empty()
                                    ? "No model selected"
                                    : session->context.agent_config().ai_model.c_str());

    ImGui::Separator();

    // Calculate available height for chat view
    float available_height = ImGui::GetContentRegionAvail().y;

    // Draw the full chat view
    chat_view_.Draw(available_height);
  }
  ImGui::End();

  ImGui::PopStyleColor(3);
}

}  // namespace editor
}  // namespace yaze
