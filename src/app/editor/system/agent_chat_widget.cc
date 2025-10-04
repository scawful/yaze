#include "app/editor/system/agent_chat_widget.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/core/platform/file_dialog.h"
#include "app/editor/system/agent_chat_history_codec.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace {

using yaze::cli::agent::ChatMessage;

const ImVec4 kUserColor = ImVec4(0.88f, 0.76f, 0.36f, 1.0f);
const ImVec4 kAgentColor = ImVec4(0.56f, 0.82f, 0.62f, 1.0f);
const ImVec4 kJsonTextColor = ImVec4(0.78f, 0.83f, 0.90f, 1.0f);
const ImVec4 kProposalPanelColor = ImVec4(0.20f, 0.35f, 0.20f, 0.35f);

std::filesystem::path ExpandUserPath(std::string path) {
  if (!path.empty() && path.front() == '~') {
    const char* home = nullptr;
#ifdef _WIN32
    home = std::getenv("USERPROFILE");
#else
    home = std::getenv("HOME");
#endif
    if (home != nullptr) {
      path.replace(0, 1, home);
    }
  }
  return std::filesystem::path(path);
}

std::filesystem::path ResolveHistoryPath(const std::string& session_id = "") {
  std::filesystem::path base = ExpandUserPath(yaze::core::GetConfigDirectory());
  if (base.empty()) {
    base = ExpandUserPath(".yaze");
  }
  auto directory = base / "agent";
  
  // If in a collaborative session, use shared history
  if (!session_id.empty()) {
    directory = directory / "sessions";
    return directory / (session_id + "_history.json");
  }
  
  return directory / "chat_history.json";
}

void RenderTable(const ChatMessage::TableData& table_data) {
  const int column_count = static_cast<int>(table_data.headers.size());
  if (column_count <= 0) {
    ImGui::TextDisabled("(empty)");
    return;
  }

  if (ImGui::BeginTable("structured_table", column_count,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    for (const auto& header : table_data.headers) {
      ImGui::TableSetupColumn(header.c_str());
    }
    ImGui::TableHeadersRow();

    for (const auto& row : table_data.rows) {
      ImGui::TableNextRow();
      for (int col = 0; col < column_count; ++col) {
        ImGui::TableSetColumnIndex(col);
        if (col < static_cast<int>(row.size())) {
          ImGui::TextWrapped("%s", row[col].c_str());
        } else {
          ImGui::TextUnformatted("-");
        }
      }
    }
    ImGui::EndTable();
  }
}

}  // namespace

namespace yaze {
namespace editor {

AgentChatWidget::AgentChatWidget() {
  title_ = "Agent Chat";
  memset(input_buffer_, 0, sizeof(input_buffer_));
  history_path_ = ResolveHistoryPath();
  history_supported_ = AgentChatHistoryCodec::Available();
}

void AgentChatWidget::SetRomContext(Rom* rom) {
  agent_service_.SetRomContext(rom);
}

void AgentChatWidget::SetToastManager(ToastManager* toast_manager) {
  toast_manager_ = toast_manager;
}

void AgentChatWidget::SetProposalDrawer(ProposalDrawer* drawer) {
  proposal_drawer_ = drawer;
  if (proposal_drawer_ && !pending_focus_proposal_id_.empty()) {
    proposal_drawer_->FocusProposal(pending_focus_proposal_id_);
    pending_focus_proposal_id_.clear();
  }
}

void AgentChatWidget::EnsureHistoryLoaded() {
  if (history_loaded_) {
    return;
  }
  history_loaded_ = true;

  std::error_code ec;
  auto directory = history_path_.parent_path();
  if (!directory.empty()) {
    std::filesystem::create_directories(directory, ec);
    if (ec) {
      if (toast_manager_) {
        toast_manager_->Show(
            "Unable to prepare chat history directory",
            ToastType::kError, 5.0f);
      }
      return;
    }
  }
  if (!history_supported_) {
    if (!history_warning_displayed_ && toast_manager_) {
      toast_manager_->Show(
          "Chat history requires gRPC/JSON support and is disabled",
          ToastType::kWarning, 5.0f);
      history_warning_displayed_ = true;
    }
    return;
  }

  absl::StatusOr<AgentChatHistoryCodec::Snapshot> result =
      AgentChatHistoryCodec::Load(history_path_);
  if (!result.ok()) {
    if (result.status().code() == absl::StatusCode::kUnimplemented) {
      history_supported_ = false;
      if (!history_warning_displayed_ && toast_manager_) {
        toast_manager_->Show(
            "Chat history requires gRPC/JSON support and is disabled",
            ToastType::kWarning, 5.0f);
        history_warning_displayed_ = true;
      }
      return;
    }

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to load chat history: %s",
                          result.status().ToString()),
          ToastType::kError, 6.0f);
    }
    return;
  }

  AgentChatHistoryCodec::Snapshot snapshot = std::move(result.value());

  if (!snapshot.history.empty()) {
    agent_service_.ReplaceHistory(std::move(snapshot.history));
    last_history_size_ = agent_service_.GetHistory().size();
    last_proposal_count_ = CountKnownProposals();
    history_dirty_ = false;
    last_persist_time_ = absl::Now();
    if (toast_manager_) {
      toast_manager_->Show("Restored chat history",
                           ToastType::kInfo, 3.5f);
    }
  }

  collaboration_state_.active = snapshot.collaboration.active;
  collaboration_state_.session_id = snapshot.collaboration.session_id;
  collaboration_state_.session_name = snapshot.collaboration.session_name;
  collaboration_state_.participants = snapshot.collaboration.participants;
  collaboration_state_.last_synced = snapshot.collaboration.last_synced;
  if (collaboration_state_.session_name.empty() &&
      !collaboration_state_.session_id.empty()) {
    collaboration_state_.session_name = collaboration_state_.session_id;
  }

  multimodal_state_.last_capture_path =
      snapshot.multimodal.last_capture_path;
  multimodal_state_.status_message = snapshot.multimodal.status_message;
  multimodal_state_.last_updated = snapshot.multimodal.last_updated;
}

void AgentChatWidget::PersistHistory() {
  if (!history_loaded_ || !history_dirty_) {
    return;
  }

  if (!history_supported_) {
    history_dirty_ = false;
    if (!history_warning_displayed_ && toast_manager_) {
      toast_manager_->Show(
          "Chat history requires gRPC/JSON support and is disabled",
          ToastType::kWarning, 5.0f);
      history_warning_displayed_ = true;
    }
    return;
  }

  AgentChatHistoryCodec::Snapshot snapshot;
  snapshot.history = agent_service_.GetHistory();
  snapshot.collaboration.active = collaboration_state_.active;
  snapshot.collaboration.session_id = collaboration_state_.session_id;
  snapshot.collaboration.session_name = collaboration_state_.session_name;
  snapshot.collaboration.participants = collaboration_state_.participants;
  snapshot.collaboration.last_synced = collaboration_state_.last_synced;
  snapshot.multimodal.last_capture_path =
      multimodal_state_.last_capture_path;
  snapshot.multimodal.status_message = multimodal_state_.status_message;
  snapshot.multimodal.last_updated = multimodal_state_.last_updated;

  absl::Status status = AgentChatHistoryCodec::Save(history_path_, snapshot);
  if (!status.ok()) {
    if (status.code() == absl::StatusCode::kUnimplemented) {
      history_supported_ = false;
      if (!history_warning_displayed_ && toast_manager_) {
        toast_manager_->Show(
            "Chat history requires gRPC/JSON support and is disabled",
            ToastType::kWarning, 5.0f);
        history_warning_displayed_ = true;
      }
      history_dirty_ = false;
      return;
    }

    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to persist chat history: %s",
                          status.ToString()),
          ToastType::kError, 6.0f);
    }
    return;
  }

  history_dirty_ = false;
  last_persist_time_ = absl::Now();
}

int AgentChatWidget::CountKnownProposals() const {
  int total = 0;
  const auto& history = agent_service_.GetHistory();
  for (const auto& message : history) {
    if (message.metrics.has_value()) {
      total = std::max(total, message.metrics->total_proposals);
    } else if (message.proposal.has_value()) {
      ++total;
    }
  }
  return total;
}

void AgentChatWidget::FocusProposalDrawer(const std::string& proposal_id) {
  if (proposal_id.empty()) {
    return;
  }
  if (proposal_drawer_) {
    proposal_drawer_->FocusProposal(proposal_id);
  }
  pending_focus_proposal_id_ = proposal_id;
}

void AgentChatWidget::NotifyProposalCreated(const ChatMessage& msg,
                                            int new_total_proposals) {
  int delta = std::max(1, new_total_proposals - last_proposal_count_);
  if (toast_manager_) {
    if (msg.proposal.has_value()) {
      const auto& proposal = *msg.proposal;
      toast_manager_->Show(
          absl::StrFormat("%s Proposal %s ready (%d change%s)", ICON_MD_PREVIEW,
                           proposal.id, proposal.change_count,
                           proposal.change_count == 1 ? "" : "s"),
          ToastType::kSuccess, 5.5f);
    } else {
      toast_manager_->Show(
          absl::StrFormat("%s %d new proposal%s queued",
                           ICON_MD_PREVIEW, delta, delta == 1 ? "" : "s"),
          ToastType::kSuccess, 4.5f);
    }
  }

  if (msg.proposal.has_value()) {
    FocusProposalDrawer(msg.proposal->id);
  }
}

void AgentChatWidget::HandleAgentResponse(
    const absl::StatusOr<ChatMessage>& response) {
  if (!response.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Agent error: %s", response.status().message()),
          ToastType::kError, 5.0f);
    }
    return;
  }

  const ChatMessage& message = response.value();
  int total = CountKnownProposals();
  if (message.metrics.has_value()) {
    total = std::max(total, message.metrics->total_proposals);
  }

  if (total > last_proposal_count_) {
    NotifyProposalCreated(message, total);
  }
  last_proposal_count_ = std::max(last_proposal_count_, total);
}

void AgentChatWidget::RenderMessage(const ChatMessage& msg, int index) {
  ImGui::PushID(index);

  const bool from_user = (msg.sender == ChatMessage::Sender::kUser);
  const ImVec4 header_color = from_user ? kUserColor : kAgentColor;
  const char* header_label = from_user ? "You" : "Agent";

  ImGui::TextColored(header_color, "%s", header_label);

  ImGui::SameLine();
  ImGui::TextDisabled("%s",
                      absl::FormatTime("%H:%M:%S", msg.timestamp,
                                       absl::LocalTimeZone()).c_str());

  ImGui::Indent();

  if (msg.json_pretty.has_value()) {
    if (ImGui::SmallButton("Copy JSON")) {
      ImGui::SetClipboardText(msg.json_pretty->c_str());
      if (toast_manager_) {
        toast_manager_->Show("Copied JSON to clipboard",
                             ToastType::kInfo, 2.5f);
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Structured response");
  }

  if (msg.table_data.has_value()) {
    RenderTable(*msg.table_data);
  } else if (msg.json_pretty.has_value()) {
    ImGui::PushStyleColor(ImGuiCol_Text, kJsonTextColor);
    ImGui::TextUnformatted(msg.json_pretty->c_str());
    ImGui::PopStyleColor();
  } else {
    ImGui::TextWrapped("%s", msg.message.c_str());
  }

  if (msg.proposal.has_value()) {
    RenderProposalQuickActions(msg, index);
  }

  ImGui::Unindent();
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::PopID();
}

void AgentChatWidget::RenderProposalQuickActions(const ChatMessage& msg,
                                                 int index) {
  if (!msg.proposal.has_value()) {
    return;
  }

  const auto& proposal = *msg.proposal;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kProposalPanelColor);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
  ImGui::BeginChild(absl::StrFormat("proposal_panel_%d", index).c_str(),
                    ImVec2(0, ImGui::GetFrameHeight() * 3.2f), true,
                    ImGuiWindowFlags_None);

  ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f),
                     "%s Proposal %s", ICON_MD_PREVIEW, proposal.id.c_str());
  ImGui::Text("Changes: %d", proposal.change_count);
  ImGui::Text("Commands: %d", proposal.executed_commands);

  if (!proposal.sandbox_rom_path.empty()) {
    ImGui::TextDisabled("Sandbox: %s",
                        proposal.sandbox_rom_path.string().c_str());
  }
  if (!proposal.proposal_json_path.empty()) {
    ImGui::TextDisabled("Manifest: %s",
                        proposal.proposal_json_path.string().c_str());
  }

  if (ImGui::SmallButton(absl::StrFormat("%s Review", ICON_MD_VISIBILITY).c_str())) {
    FocusProposalDrawer(proposal.id);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(absl::StrFormat("%s Copy ID", ICON_MD_CONTENT_COPY).c_str())) {
    ImGui::SetClipboardText(proposal.id.c_str());
    if (toast_manager_) {
      toast_manager_->Show("Proposal ID copied",
                           ToastType::kInfo, 2.5f);
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

void AgentChatWidget::RenderHistory() {
  const auto& history = agent_service_.GetHistory();
  float reserved_height = ImGui::GetFrameHeightWithSpacing() * 4.0f;
  reserved_height += 220.0f;

  if (ImGui::BeginChild("History",
                        ImVec2(0, -reserved_height),
                        false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_HorizontalScrollbar)) {
    if (history.empty()) {
      ImGui::TextDisabled("No messages yet. Start the conversation below.");
    } else {
      for (size_t index = 0; index < history.size(); ++index) {
        RenderMessage(history[index], static_cast<int>(index));
      }
    }

    if (history.size() > last_history_size_) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  ImGui::EndChild();
  last_history_size_ = history.size();
}

void AgentChatWidget::RenderInputBox() {
  ImGui::Separator();
  ImGui::Text("Message:");

  bool submitted = ImGui::InputTextMultiline(
      "##agent_input", input_buffer_, sizeof(input_buffer_),
      ImVec2(-1, 80.0f),
      ImGuiInputTextFlags_AllowTabInput |
          ImGuiInputTextFlags_EnterReturnsTrue);

  bool send = submitted;
  if (submitted && ImGui::GetIO().KeyShift) {
    size_t len = std::strlen(input_buffer_);
    if (len + 1 < sizeof(input_buffer_)) {
      input_buffer_[len] = '\n';
      input_buffer_[len + 1] = '\0';
    }
    ImGui::SetKeyboardFocusHere(-1);
    send = false;
  }

  ImGui::Spacing();
  if (ImGui::Button(absl::StrFormat("%s Send", ICON_MD_SEND).c_str(),
                    ImVec2(120, 0)) || send) {
    if (std::strlen(input_buffer_) > 0) {
      history_dirty_ = true;
      EnsureHistoryLoaded();
      auto response = agent_service_.SendMessage(input_buffer_);
      memset(input_buffer_, 0, sizeof(input_buffer_));
      HandleAgentResponse(response);
      PersistHistory();
      ImGui::SetKeyboardFocusHere(-1);
    }
  }

  ImGui::SameLine();
  ImGui::TextDisabled("Enter to send • Shift+Enter for newline");
}

void AgentChatWidget::Draw() {
  if (!active_) {
    return;
  }

  EnsureHistoryLoaded();
  
  // Poll for new messages in collaborative sessions
  PollSharedHistory();

  ImGui::Begin(title_.c_str(), &active_);
  RenderHistory();
  RenderCollaborationPanel();
  RenderMultimodalPanel();
  RenderInputBox();
  ImGui::End();
}

void AgentChatWidget::RenderCollaborationPanel() {
  if (!ImGui::CollapsingHeader("Collaborative Session",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  // Mode selector
  ImGui::Text("Mode:");
  ImGui::SameLine();
  int mode = static_cast<int>(collaboration_state_.mode);
  if (ImGui::RadioButton("Local", &mode, 0)) {
    collaboration_state_.mode = CollaborationMode::kLocal;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Network", &mode, 1)) {
    collaboration_state_.mode = CollaborationMode::kNetwork;
  }

  ImGui::Separator();

  // Table layout: Left side = Session Details, Right side = Controls
  if (ImGui::BeginTable("collab_table", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Session Details", ImGuiTableColumnFlags_WidthFixed, 250.0f);
    ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);
    
    ImGui::TableNextRow();
    
    // LEFT COLUMN: Session Details
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::BeginChild("session_details", ImVec2(0, 180), true);
    
    const bool connected = collaboration_state_.active;
    ImGui::TextColored(connected ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                      "%s %s", connected ? "●" : "○",
                      connected ? "Connected" : "Not connected");
    
    ImGui::Separator();
    
    if (collaboration_state_.mode == CollaborationMode::kNetwork) {
      ImGui::Text("Server:");
      ImGui::TextWrapped("%s", collaboration_state_.server_url.c_str());
      ImGui::Spacing();
    }
    
    if (!collaboration_state_.session_name.empty()) {
      ImGui::Text("Session:");
      ImGui::TextWrapped("%s", collaboration_state_.session_name.c_str());
      ImGui::Spacing();
    }
    
    if (!collaboration_state_.session_id.empty()) {
      ImGui::Text("Code:");
      ImGui::TextWrapped("%s", collaboration_state_.session_id.c_str());
      if (ImGui::SmallButton("Copy")) {
        ImGui::SetClipboardText(collaboration_state_.session_id.c_str());
        if (toast_manager_) {
          toast_manager_->Show("Code copied", ToastType::kInfo, 2.0f);
        }
      }
      ImGui::Spacing();
    }
    
    if (collaboration_state_.last_synced != absl::InfinitePast()) {
      ImGui::TextDisabled("Last sync:");
      ImGui::TextDisabled("%s",
          absl::FormatTime("%H:%M:%S", collaboration_state_.last_synced,
                          absl::LocalTimeZone()).c_str());
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    // Show participants list below session details
    ImGui::BeginChild("participants", ImVec2(0, 0), true);
    if (collaboration_state_.participants.empty()) {
      ImGui::TextDisabled("No participants");
    } else {
      ImGui::Text("Participants (%zu):", collaboration_state_.participants.size());
      ImGui::Separator();
      for (const auto& participant : collaboration_state_.participants) {
        ImGui::BulletText("%s", participant.c_str());
      }
    }
    ImGui::EndChild();
    
    // RIGHT COLUMN: Controls
    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("controls", ImVec2(0, 0), false);

  ImGui::Separator();

  const bool can_host = static_cast<bool>(collaboration_callbacks_.host_session);
  const bool can_join = static_cast<bool>(collaboration_callbacks_.join_session);
  const bool can_leave = static_cast<bool>(collaboration_callbacks_.leave_session);
  const bool can_refresh = static_cast<bool>(collaboration_callbacks_.refresh_session);

  // Network mode: Show server URL input
  if (collaboration_state_.mode == CollaborationMode::kNetwork) {
    ImGui::Text("Server URL:");
    ImGui::InputText("##server_url", server_url_buffer_,
                     IM_ARRAYSIZE(server_url_buffer_));
    if (ImGui::Button("Connect to Server")) {
      collaboration_state_.server_url = server_url_buffer_;
      // TODO: Trigger network coordinator connection
      if (toast_manager_) {
        toast_manager_->Show("Network mode: connecting...",
                            ToastType::kInfo, 3.0f);
      }
    }
    ImGui::Separator();
  }

  ImGui::Text("Host New Session:");
  ImGui::InputTextWithHint("##session_name", "Session name",
                           session_name_buffer_,
                           IM_ARRAYSIZE(session_name_buffer_));
  ImGui::SameLine();
  if (!can_host) ImGui::BeginDisabled();
  if (ImGui::Button("Host")) {
    std::string name = session_name_buffer_;
    if (name.empty()) {
      if (toast_manager_) {
        toast_manager_->Show("Enter a session name first",
                             ToastType::kWarning, 3.0f);
      }
    } else {
      auto session_or = collaboration_callbacks_.host_session(name);
      if (session_or.ok()) {
        ApplyCollaborationSession(session_or.value(), /*update_action_timestamp=*/true);
        std::snprintf(join_code_buffer_, sizeof(join_code_buffer_), "%s",
                      collaboration_state_.session_id.c_str());
        session_name_buffer_[0] = '\0';
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Hosting session %s",
                              collaboration_state_.session_id.c_str()),
              ToastType::kSuccess, 3.5f);
        }
        MarkHistoryDirty();
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Failed to host: %s",
                            session_or.status().message()),
            ToastType::kError, 5.0f);
      }
    }
  }
  if (!can_host) {
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Provide host_session callback to enable hosting");
    }
    ImGui::EndDisabled();
  }

  ImGui::Spacing();
  ImGui::Text("Join Existing Session:");
  ImGui::InputTextWithHint("##join_code", "Session code",
                           join_code_buffer_,
                           IM_ARRAYSIZE(join_code_buffer_));
  ImGui::SameLine();
  if (!can_join) ImGui::BeginDisabled();
  if (ImGui::Button("Join")) {
    std::string code = join_code_buffer_;
    if (code.empty()) {
      if (toast_manager_) {
        toast_manager_->Show("Enter a session code first",
                             ToastType::kWarning, 3.0f);
      }
    } else {
      auto session_or = collaboration_callbacks_.join_session(code);
      if (session_or.ok()) {
        ApplyCollaborationSession(session_or.value(), /*update_action_timestamp=*/true);
        std::snprintf(join_code_buffer_, sizeof(join_code_buffer_), "%s",
                      collaboration_state_.session_id.c_str());
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Joined session %s",
                              collaboration_state_.session_id.c_str()),
              ToastType::kSuccess, 3.5f);
        }
        MarkHistoryDirty();
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Failed to join: %s",
                            session_or.status().message()),
            ToastType::kError, 5.0f);
      }
    }
  }
  if (!can_join) {
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Provide join_session callback to enable joining");
    }
    ImGui::EndDisabled();
  }

  if (connected) {
    if (!can_leave) ImGui::BeginDisabled();
    if (ImGui::Button("Leave Session")) {
      absl::Status status = collaboration_callbacks_.leave_session
                                ? collaboration_callbacks_.leave_session()
                                : absl::OkStatus();
      if (status.ok()) {
        collaboration_state_ = CollaborationState{};
        join_code_buffer_[0] = '\0';
        if (toast_manager_) {
          toast_manager_->Show("Left collaborative session",
                               ToastType::kInfo, 3.0f);
        }
        MarkHistoryDirty();
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Failed to leave: %s", status.message()),
            ToastType::kError, 5.0f);
      }
    }
    if (!can_leave) ImGui::EndDisabled();
  }

  if (connected) {
    ImGui::Spacing();
    ImGui::Separator();
    if (!can_refresh) ImGui::BeginDisabled();
    if (ImGui::Button("Refresh Session")) {
      RefreshCollaboration();
    }
    if (!can_refresh && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Provide refresh_session callback to enable");
    }
    if (!can_refresh) ImGui::EndDisabled();
  } else {
    ImGui::Spacing();
    ImGui::TextDisabled("Start or join a session to collaborate.");
  }
  
  ImGui::EndChild();  // controls
  ImGui::EndTable();
  }
}

void AgentChatWidget::RenderMultimodalPanel() {
  if (!ImGui::CollapsingHeader("Gemini Multimodal (Preview)",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  bool can_capture = static_cast<bool>(multimodal_callbacks_.capture_snapshot);
  bool can_send = static_cast<bool>(multimodal_callbacks_.send_to_gemini);

  // Capture mode selection
  ImGui::Text("Capture Mode:");
  ImGui::RadioButton("Full Window", 
      reinterpret_cast<int*>(&multimodal_state_.capture_mode), 
      static_cast<int>(CaptureMode::kFullWindow));
  ImGui::SameLine();
  ImGui::RadioButton("Active Editor", 
      reinterpret_cast<int*>(&multimodal_state_.capture_mode), 
      static_cast<int>(CaptureMode::kActiveEditor));
  ImGui::SameLine();
  ImGui::RadioButton("Specific Window", 
      reinterpret_cast<int*>(&multimodal_state_.capture_mode), 
      static_cast<int>(CaptureMode::kSpecificWindow));

  // If specific window mode, show input for window name
  if (multimodal_state_.capture_mode == CaptureMode::kSpecificWindow) {
    ImGui::InputText("Window Name", multimodal_state_.specific_window_buffer,
                     IM_ARRAYSIZE(multimodal_state_.specific_window_buffer));
    ImGui::TextDisabled("Examples: Overworld Editor, Dungeon Editor, Sprite Editor");
  }

  ImGui::Separator();

  if (!can_capture) ImGui::BeginDisabled();
  if (ImGui::Button("Capture Snapshot")) {
    if (multimodal_callbacks_.capture_snapshot) {
      std::filesystem::path captured_path;
      absl::Status status =
          multimodal_callbacks_.capture_snapshot(&captured_path);
      if (status.ok()) {
        multimodal_state_.last_capture_path = captured_path;
        multimodal_state_.status_message =
            absl::StrFormat("Captured %s", captured_path.string());
        multimodal_state_.last_updated = absl::Now();
        if (toast_manager_) {
          toast_manager_->Show("Snapshot captured",
                               ToastType::kSuccess, 3.0f);
        }
        MarkHistoryDirty();
      } else {
        multimodal_state_.status_message = status.message();
        multimodal_state_.last_updated = absl::Now();
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Snapshot failed: %s", status.message()),
              ToastType::kError, 5.0f);
        }
      }
    }
  }
  if (!can_capture) {
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Provide capture_snapshot callback to enable");
    }
    ImGui::EndDisabled();
  }

  if (multimodal_state_.last_capture_path.has_value()) {
    ImGui::TextDisabled("Last capture: %s",
                        multimodal_state_.last_capture_path->string().c_str());
  } else {
    ImGui::TextDisabled("No capture yet");
  }

  ImGui::InputTextMultiline("##gemini_prompt", multimodal_prompt_buffer_,
                            IM_ARRAYSIZE(multimodal_prompt_buffer_),
                            ImVec2(-1, 60.0f));
  if (!can_send) ImGui::BeginDisabled();
  if (ImGui::Button("Send to Gemini")) {
    if (!multimodal_state_.last_capture_path.has_value()) {
      if (toast_manager_) {
        toast_manager_->Show("Capture a snapshot first",
                             ToastType::kWarning, 3.0f);
      }
    } else {
      std::string prompt = multimodal_prompt_buffer_;
      absl::Status status = multimodal_callbacks_.send_to_gemini(
          *multimodal_state_.last_capture_path, prompt);
      if (status.ok()) {
        multimodal_state_.status_message =
            "Submitted image to Gemini";
        multimodal_state_.last_updated = absl::Now();
        if (toast_manager_) {
          toast_manager_->Show("Gemini request sent",
                               ToastType::kSuccess, 3.0f);
        }
        MarkHistoryDirty();
      } else {
        multimodal_state_.status_message = status.message();
        multimodal_state_.last_updated = absl::Now();
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Gemini request failed: %s", status.message()),
              ToastType::kError, 5.0f);
        }
      }
    }
  }
  if (!can_send) {
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Provide send_to_gemini callback to enable");
    }
    ImGui::EndDisabled();
  }

  if (!multimodal_state_.status_message.empty()) {
    ImGui::TextDisabled("Status: %s", multimodal_state_.status_message.c_str());
    if (multimodal_state_.last_updated != absl::InfinitePast()) {
      ImGui::TextDisabled(
          "Updated: %s",
          absl::FormatTime("%H:%M:%S", multimodal_state_.last_updated,
                           absl::LocalTimeZone()).c_str());
    }
  }
}

void AgentChatWidget::RefreshCollaboration() {
  if (!collaboration_callbacks_.refresh_session) {
    return;
  }
  auto session_or = collaboration_callbacks_.refresh_session();
  if (!session_or.ok()) {
    if (session_or.status().code() == absl::StatusCode::kNotFound) {
      collaboration_state_ = CollaborationState{};
      join_code_buffer_[0] = '\0';
      MarkHistoryDirty();
    }
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to refresh participants: %s",
                           session_or.status().message()),
          ToastType::kError, 5.0f);
    }
    return;
  }

  ApplyCollaborationSession(session_or.value(), /*update_action_timestamp=*/false);
  MarkHistoryDirty();
}

void AgentChatWidget::ApplyCollaborationSession(
    const CollaborationCallbacks::SessionContext& context,
    bool update_action_timestamp) {
  collaboration_state_.active = true;
  collaboration_state_.session_id = context.session_id;
  collaboration_state_.session_name = context.session_name.empty()
                                         ? context.session_id
                                         : context.session_name;
  collaboration_state_.participants = context.participants;
  collaboration_state_.last_synced = absl::Now();
  if (update_action_timestamp) {
    last_collaboration_action_ = absl::Now();
  }
}

void AgentChatWidget::MarkHistoryDirty() {
  history_dirty_ = true;
  const absl::Time now = absl::Now();
  if (last_persist_time_ == absl::InfinitePast() ||
      now - last_persist_time_ > absl::Seconds(2)) {
    PersistHistory();
  }
}

void AgentChatWidget::SwitchToSharedHistory(const std::string& session_id) {
  // Save current local history before switching
  if (history_loaded_ && history_dirty_) {
    PersistHistory();
  }

  // Switch to shared history path
  history_path_ = ResolveHistoryPath(session_id);
  history_loaded_ = false;
  
  // Load shared history
  EnsureHistoryLoaded();
  
  // Initialize polling state
  last_known_history_size_ = agent_service_.GetHistory().size();
  last_shared_history_poll_ = absl::Now();
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Switched to shared chat history for session %s",
                        session_id),
        ToastType::kInfo, 3.0f);
  }
}

void AgentChatWidget::SwitchToLocalHistory() {
  // Save shared history before switching
  if (history_loaded_ && history_dirty_) {
    PersistHistory();
  }

  // Switch back to local history
  history_path_ = ResolveHistoryPath("");
  history_loaded_ = false;
  
  // Load local history
  EnsureHistoryLoaded();
  
  if (toast_manager_) {
    toast_manager_->Show("Switched to local chat history",
                         ToastType::kInfo, 3.0f);
  }
}

void AgentChatWidget::PollSharedHistory() {
  if (!collaboration_state_.active) {
    return;  // Not in a collaborative session
  }

  const absl::Time now = absl::Now();
  
  // Poll every 2 seconds
  if (now - last_shared_history_poll_ < absl::Seconds(2)) {
    return;
  }
  
  last_shared_history_poll_ = now;

  // Check if the shared history file has been updated
  auto result = AgentChatHistoryCodec::Load(history_path_);
  if (!result.ok()) {
    return;  // File might not exist yet or be temporarily locked
  }

  const size_t new_size = result->history.size();
  
  // If history has grown, reload it
  if (new_size > last_known_history_size_) {
    const size_t new_messages = new_size - last_known_history_size_;
    
    agent_service_.ReplaceHistory(std::move(result->history));
    last_history_size_ = new_size;
    last_known_history_size_ = new_size;
    
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("📬 %zu new message%s from collaborators",
                          new_messages, new_messages == 1 ? "" : "s"),
          ToastType::kInfo, 3.0f);
    }
  }
}

}  // namespace editor
}  // namespace yaze
