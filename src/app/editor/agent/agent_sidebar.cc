#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/agent/agent_sidebar.h"

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

AgentSidebar::AgentSidebar() {
  // Set compact mode for sidebar usage
  chat_view_.SetCompactMode(true);
  proposals_panel_.SetCompactMode(true);
}

void AgentSidebar::Initialize(ToastManager* toast_manager, Rom* rom) {
  toast_manager_ = toast_manager;
  rom_ = rom;

  chat_view_.SetToastManager(toast_manager);
  proposals_panel_.SetToastManager(toast_manager);
  proposals_panel_.SetRom(rom);
}

void AgentSidebar::SetSessionManager(AgentSessionManager* session_manager) {
  session_manager_ = session_manager;
  UpdateChatViewContext();
}

void AgentSidebar::SetContext(AgentUIContext* context) {
  context_ = context;
  chat_view_.SetContext(context);
  proposals_panel_.SetContext(context);
}

void AgentSidebar::UpdateChatViewContext() {
  // If we have a session manager, use the active session's context
  if (session_manager_) {
    if (AgentSession* session = session_manager_->GetActiveSession()) {
      chat_view_.SetContext(&session->context);
      proposals_panel_.SetContext(&session->context);
      return;
    }
  }
  // Fallback to directly set context
  if (context_) {
    chat_view_.SetContext(context_);
    proposals_panel_.SetContext(context_);
  }
}

void AgentSidebar::SetAgentService(
    cli::agent::ConversationalAgentService* service) {
  agent_service_ = service;
  chat_view_.SetAgentService(service);
}

void AgentSidebar::SetChatCallbacks(const ChatCallbacks& callbacks) {
  chat_view_.SetChatCallbacks(callbacks);
}

void AgentSidebar::SetProposalCallbacks(const ProposalCallbacks& callbacks) {
  chat_view_.SetProposalCallbacks(callbacks);
  proposals_panel_.SetProposalCallbacks(callbacks);
}

void AgentSidebar::SetCollaborationCallbacks(
    const CollaborationCallbacks& callbacks) {
  collaboration_callbacks_ = callbacks;
}

void AgentSidebar::Draw() {
  const auto& theme = AgentUI::GetTheme();
  float available_height = ImGui::GetContentRegionAvail().y;

  // Agent tabs (if multi-agent mode)
  if (session_manager_ && session_manager_->GetSessionCount() > 0) {
    DrawAgentTabs();
  }

  // Header with model selector and status
  DrawHeader();

  ImGui::Separator();

  // Ensure chat view has correct context after potential tab switch
  UpdateChatViewContext();

  // Calculate section heights
  float header_height = ImGui::GetCursorPosY();
  float proposals_section_height =
      proposals_expanded_ ? proposals_height_ + 30.0f : 30.0f;
  float chat_height = available_height - header_height - proposals_section_height - 16.0f;

  // Chat section (primary, gets remaining space)
  DrawChatSection();

  // Proposals section (collapsible)
  DrawProposalsSection();
}

void AgentSidebar::DrawAgentTabs() {
  const auto& theme = AgentUI::GetTheme();

  if (!session_manager_) {
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_Tab, theme.panel_bg_darker);
  ImGui::PushStyleColor(ImGuiCol_TabHovered, theme.panel_bg_color);
  ImGui::PushStyleColor(ImGuiCol_TabActive, theme.agent_message_color);
  ImGui::PushStyleColor(ImGuiCol_TabUnfocused, theme.panel_bg_darker);
  ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, theme.panel_bg_color);

  if (ImGui::BeginTabBar("AgentTabs", ImGuiTabBarFlags_AutoSelectNewTabs |
                                          ImGuiTabBarFlags_Reorderable |
                                          ImGuiTabBarFlags_FittingPolicyScroll)) {
    for (auto& session : session_manager_->GetAllSessions()) {
      ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;

      // Show close button if more than one session
      bool closable = session_manager_->GetSessionCount() > 1;

      bool open = true;
      if (ImGui::BeginTabItem(session.display_name.c_str(),
                              closable ? &open : nullptr, flags)) {
        // Tab was selected - activate this session
        if (!session.is_active) {
          session_manager_->SetActiveSession(session.agent_id);
        }
        ImGui::EndTabItem();
      }

      // Handle tab close
      if (closable && !open) {
        session_manager_->CloseSession(session.agent_id);
        break;  // Iterator invalidated
      }
    }

    // "+" button to create new session
    if (ImGui::TabItemButton(ICON_MD_ADD, ImGuiTabItemFlags_Trailing |
                                              ImGuiTabItemFlags_NoTooltip)) {
      session_manager_->CreateSession();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("New Agent Session");
    }

    ImGui::EndTabBar();
  }

  ImGui::PopStyleColor(5);
}

void AgentSidebar::DrawHeader() {
  const auto& theme = AgentUI::GetTheme();

  // Model selector and status in a compact row
  ImGui::BeginGroup();

  // Model selector button
  DrawModelSelector();

  ImGui::SameLine();

  // Collaboration status
  DrawCollaborationStatus();

  ImGui::SameLine();

  // Pop-out button (to open full card in docking space)
  if (session_manager_) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.panel_bg_color);
    if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW)) {
      if (AgentSession* session = session_manager_->GetActiveSession()) {
        if (pop_out_callback_) {
          pop_out_callback_(session->agent_id);
        }
      }
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Pop out to dockable window");
    }
  }

  ImGui::EndGroup();
}

void AgentSidebar::DrawModelSelector() {
  const auto& theme = AgentUI::GetTheme();

  std::string current_model = "mock";
  if (context_) {
    const auto& config = context_->agent_config();
    current_model = config.ai_model.empty() ? config.ai_provider : config.ai_model;
  }

  // Truncate if too long
  if (current_model.length() > 12) {
    current_model = current_model.substr(0, 10) + "...";
  }

  ImGui::PushStyleColor(ImGuiCol_Button, theme.panel_bg_color);
  if (ImGui::Button(
          absl::StrFormat("%s %s " ICON_MD_ARROW_DROP_DOWN, ICON_MD_SMART_TOY,
                          current_model)
              .c_str())) {
    show_model_popup_ = true;
    ImGui::OpenPopup("ModelSelector");
  }
  ImGui::PopStyleColor();

  // Model selector popup
  if (ImGui::BeginPopup("ModelSelector")) {
    ImGui::TextDisabled("Select Provider");
    ImGui::Separator();

    const char* providers[] = {"mock", "ollama", "gemini"};
    for (const char* provider : providers) {
      bool selected = context_ && context_->agent_config().ai_provider == provider;
      if (ImGui::Selectable(provider, selected)) {
        if (context_) {
          context_->agent_config().ai_provider = provider;
          // Set default model for provider
          if (std::string(provider) == "ollama") {
            context_->agent_config().ai_model = "qwen2.5-coder:7b";
          } else if (std::string(provider) == "gemini") {
            context_->agent_config().ai_model = "gemini-2.5-flash";
          } else {
            context_->agent_config().ai_model = "";
          }
          context_->NotifyChanged();
        }
      }
    }

    // Model input for ollama/gemini
    if (context_) {
      const auto& config = context_->agent_config();
      if (config.ai_provider == "ollama" || config.ai_provider == "gemini") {
        ImGui::Separator();
        ImGui::TextDisabled("Model Name");
        static char model_buf[128];
        strncpy(model_buf, config.ai_model.c_str(), sizeof(model_buf) - 1);
        model_buf[sizeof(model_buf) - 1] = '\0';
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##model_input", model_buf, sizeof(model_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
          context_->agent_config().ai_model = model_buf;
          context_->NotifyChanged();
          ImGui::CloseCurrentPopup();
        }
      }
    }

    ImGui::Separator();
    if (ImGui::Selectable(ICON_MD_SETTINGS " Full Config...")) {
      // Signal to open the AI Configuration card
      if (context_) {
        // This would require a callback or flag to open the config card
      }
    }

    ImGui::EndPopup();
  }
}

void AgentSidebar::DrawCollaborationStatus() {
  const auto& theme = AgentUI::GetTheme();

  bool is_collaborating = false;
  if (context_) {
    is_collaborating = context_->collaboration_state().active;
  }

  ImVec4 status_color =
      is_collaborating ? theme.collaboration_active : theme.collaboration_inactive;

  ImGui::TextColored(status_color, "%s",
                     is_collaborating ? ICON_MD_PEOPLE : ICON_MD_PERSON);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(is_collaborating ? "Collaborating" : "Solo mode");
  }
}

void AgentSidebar::DrawChatSection() {
  float proposals_section_height =
      proposals_expanded_ ? proposals_height_ + 30.0f : 30.0f;
  float available_height = ImGui::GetContentRegionAvail().y - proposals_section_height - 8.0f;

  chat_view_.Draw(available_height);
}

void AgentSidebar::DrawProposalsSection() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::Separator();

  // Collapsible header with badge
  int pending_count = proposals_panel_.GetPendingCount();
  std::string header_label =
      absl::StrFormat("%s Proposals", proposals_expanded_ ? ICON_MD_EXPAND_MORE
                                                          : ICON_MD_CHEVRON_RIGHT);

  // Make header clickable
  ImGui::PushStyleColor(ImGuiCol_Header, theme.panel_bg_color);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme.panel_bg_darker);

  if (ImGui::Selectable(header_label.c_str(), false,
                        ImGuiSelectableFlags_None, ImVec2(0, 24.0f))) {
    proposals_expanded_ = !proposals_expanded_;
  }

  ImGui::PopStyleColor(2);

  // Badge for pending count
  if (pending_count > 0) {
    ImGui::SameLine();
    AgentUI::StatusBadge(absl::StrFormat("%d", pending_count).c_str(),
                         AgentUI::ButtonColor::Warning);
  }

  // Proposals content (if expanded)
  if (proposals_expanded_) {
    // Resizable area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_darker);
    ImGui::BeginChild("ProposalsContent", ImVec2(0, proposals_height_), true);
    proposals_panel_.DrawProposalList();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Resize handle
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
    ImGui::Button("##resize", ImVec2(-1, 4));
    ImGui::PopStyleColor();

    if (ImGui::IsItemActive()) {
      proposals_height_ += ImGui::GetIO().MouseDelta.y;
      proposals_height_ = std::max(80.0f, std::min(proposals_height_, 400.0f));
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
  }
}

int AgentSidebar::GetPendingProposalCount() const {
  return proposals_panel_.GetPendingCount();
}

void AgentSidebar::FocusProposal(const std::string& proposal_id) {
  proposals_expanded_ = true;
  proposals_panel_.FocusProposal(proposal_id);
}

}  // namespace editor
}  // namespace yaze
