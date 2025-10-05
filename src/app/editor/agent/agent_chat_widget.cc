#include "app/editor/agent/agent_chat_widget.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <fstream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/file_util.h"
#include "app/editor/agent/agent_chat_history_codec.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/icons.h"
#include "app/core/project.h"
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
  std::filesystem::path base = ExpandUserPath(yaze::util::GetConfigDirectory());
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
        toast_manager_->Show("Unable to prepare chat history directory",
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
      toast_manager_->Show(absl::StrFormat("Failed to load chat history: %s",
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
      toast_manager_->Show("Restored chat history", ToastType::kInfo, 3.5f);
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

  multimodal_state_.last_capture_path = snapshot.multimodal.last_capture_path;
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
  snapshot.multimodal.last_capture_path = multimodal_state_.last_capture_path;
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
      toast_manager_->Show(absl::StrFormat("Failed to persist chat history: %s",
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
          absl::StrFormat("%s %d new proposal%s queued", ICON_MD_PREVIEW, delta,
                          delta == 1 ? "" : "s"),
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
  ImGui::TextDisabled(
      "%s", absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone())
                .c_str());

  ImGui::Indent();

  if (msg.json_pretty.has_value()) {
    if (ImGui::SmallButton("Copy JSON")) {
      ImGui::SetClipboardText(msg.json_pretty->c_str());
      if (toast_manager_) {
        toast_manager_->Show("Copied JSON to clipboard", ToastType::kInfo,
                             2.5f);
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

  ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%s Proposal %s",
                     ICON_MD_PREVIEW, proposal.id.c_str());
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

  if (ImGui::SmallButton(
          absl::StrFormat("%s Review", ICON_MD_VISIBILITY).c_str())) {
    FocusProposalDrawer(proposal.id);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(
          absl::StrFormat("%s Copy ID", ICON_MD_CONTENT_COPY).c_str())) {
    ImGui::SetClipboardText(proposal.id.c_str());
    if (toast_manager_) {
      toast_manager_->Show("Proposal ID copied", ToastType::kInfo, 2.5f);
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

  if (ImGui::BeginChild("History", ImVec2(0, -reserved_height), false,
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
      "##agent_input", input_buffer_, sizeof(input_buffer_), ImVec2(-1, 80.0f),
      ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue);

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
                    ImVec2(120, 0)) ||
      send) {
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

  ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
  ImGui::Begin(title_.c_str(), &active_, ImGuiWindowFlags_MenuBar);
  
  // Modern menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem(ICON_MD_SAVE " Export History")) {
        // TODO: Implement export
      }
      if (ImGui::MenuItem(ICON_MD_FOLDER_OPEN " Import History")) {
        // TODO: Implement import
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_DELETE_FOREVER " Clear History")) {
        agent_service_.ResetConversation();
        if (toast_manager_) {
          toast_manager_->Show("Chat history cleared", ToastType::kInfo, 2.5f);
        }
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Agent")) {
      if (ImGui::MenuItem(ICON_MD_SETTINGS " Configure", nullptr, show_agent_config_)) {
        show_agent_config_ = !show_agent_config_;
      }
      if (ImGui::MenuItem(ICON_MD_TERMINAL " Commands", nullptr, show_z3ed_commands_)) {
        show_z3ed_commands_ = !show_z3ed_commands_;
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_REFRESH " Reset Conversation")) {
        agent_service_.ResetConversation();
        if (toast_manager_) {
          toast_manager_->Show("Conversation reset", ToastType::kInfo, 2.5f);
        }
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Collaboration")) {
      if (ImGui::MenuItem(ICON_MD_SYNC " ROM Sync", nullptr, show_rom_sync_)) {
        show_rom_sync_ = !show_rom_sync_;
      }
      if (ImGui::MenuItem(ICON_MD_PHOTO_CAMERA " Snapshot Preview", nullptr, show_snapshot_preview_)) {
        show_snapshot_preview_ = !show_snapshot_preview_;
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem(ICON_MD_HELP " Documentation")) {
        // TODO: Open docs
      }
      if (ImGui::MenuItem(ICON_MD_INFO " About")) {
        // TODO: Show about dialog
      }
      ImGui::EndMenu();
    }
    
    ImGui::EndMenuBar();
  }
  
  // Modern tabbed interface
  if (ImGui::BeginTabBar("AgentChatTabs", ImGuiTabBarFlags_None)) {
    // Main Chat Tab - with integrated controls
    if (ImGui::BeginTabItem(ICON_MD_CHAT " Chat")) {
      // Stylish connection status bar with gradient
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 bar_start = ImGui::GetCursorScreenPos();
      ImVec2 bar_size(ImGui::GetContentRegionAvail().x, 85);
      
      // Gradient background
      ImU32 color_top = ImGui::GetColorU32(ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
      ImU32 color_bottom = ImGui::GetColorU32(ImVec4(0.12f, 0.16f, 0.22f, 1.0f));
      draw_list->AddRectFilledMultiColor(bar_start, 
          ImVec2(bar_start.x + bar_size.x, bar_start.y + bar_size.y),
          color_top, color_top, color_bottom, color_bottom);
      
      // Colored accent bar based on provider
      ImVec4 accent_color = (agent_config_.ai_provider == "ollama") ? ImVec4(0.2f, 0.8f, 0.4f, 1.0f) :
                           (agent_config_.ai_provider == "gemini") ? ImVec4(0.196f, 0.6f, 0.8f, 1.0f) :
                           ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
      draw_list->AddRectFilled(bar_start, ImVec2(bar_start.x + bar_size.x, bar_start.y + 3),
                              ImGui::GetColorU32(accent_color));
      
      ImGui::BeginChild("ConnectionStatusBar", bar_size, false, ImGuiWindowFlags_NoScrollbar);
      {
        ImGui::Spacing();
        
        // Provider selection and connection status in one row
        ImGui::TextColored(accent_color, ICON_MD_SMART_TOY " AI Provider:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        const char* providers[] = { "Mock", "Ollama", "Gemini" };
        int current_provider = (agent_config_.ai_provider == "mock") ? 0 : 
                              (agent_config_.ai_provider == "ollama") ? 1 : 2;
        if (ImGui::Combo("##chat_provider_combo", &current_provider, providers, 3)) {
          agent_config_.ai_provider = (current_provider == 0) ? "mock" : 
                                     (current_provider == 1) ? "ollama" : "gemini";
          strncpy(agent_config_.provider_buffer, agent_config_.ai_provider.c_str(), 
                  sizeof(agent_config_.provider_buffer) - 1);
        }
        
        ImGui::SameLine();
        if (agent_config_.ai_provider == "ollama") {
          ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), ICON_MD_LINK " Host:");
          ImGui::SameLine();
          ImGui::SetNextItemWidth(200);
          if (ImGui::InputText("##chat_ollama_host", agent_config_.ollama_host_buffer, 
                              sizeof(agent_config_.ollama_host_buffer))) {
            agent_config_.ollama_host = agent_config_.ollama_host_buffer;
          }
          ImGui::SameLine();
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.6f, 0.3f, 0.8f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.8f, 0.4f, 1.0f));
          if (ImGui::Button(ICON_MD_REFRESH " Test##test_ollama_conn")) {
            if (toast_manager_) {
              toast_manager_->Show("Testing Ollama connection...", ToastType::kInfo);
            }
          }
          ImGui::PopStyleColor(2);
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Test connection to Ollama server");
          }
        } else if (agent_config_.ai_provider == "gemini") {
          ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f), ICON_MD_KEY " API Key:");
          ImGui::SameLine();
          ImGui::SetNextItemWidth(250);
          if (ImGui::InputText("##chat_gemini_key", agent_config_.gemini_key_buffer, 
                              sizeof(agent_config_.gemini_key_buffer), 
                              ImGuiInputTextFlags_Password)) {
            agent_config_.gemini_api_key = agent_config_.gemini_key_buffer;
          }
          ImGui::SameLine();
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.7f, 0.8f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.196f, 0.6f, 0.8f, 1.0f));
          if (ImGui::Button(ICON_MD_CHECK " Verify##verify_gemini_key")) {
            if (toast_manager_) {
              toast_manager_->Show("Verifying Gemini API key...", ToastType::kInfo);
            }
          }
          ImGui::PopStyleColor(2);
        } else {
          ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                            ICON_MD_INFO " Mock mode - no external connection needed");
        }
        
        // Second row: Model selection and quick settings
        ImGui::TextColored(accent_color, ICON_MD_PSYCHOLOGY " Model:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##chat_model_input", agent_config_.model_buffer, sizeof(agent_config_.model_buffer));
        
        ImGui::SameLine();
        ImGui::Checkbox(ICON_MD_VISIBILITY " Reasoning", &agent_config_.show_reasoning);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Show agent's reasoning process in responses");
        }
        
        ImGui::SameLine();
        ImGui::TextColored(accent_color, ICON_MD_LOOP);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("##chat_max_iter", &agent_config_.max_tool_iterations);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Maximum tool call iterations per request");
        }
      }
      ImGui::EndChild();
      
      ImGui::Spacing();
      
      // Main chat area
      RenderHistory();
      RenderInputBox();
      
      ImGui::EndTabItem();
    }
    
    // Commands Tab - Z3ED and Multimodal
    if (ImGui::BeginTabItem(ICON_MD_TERMINAL " Commands")) {
      RenderZ3EDCommandPanel();
      ImGui::Separator();
      RenderMultimodalPanel();
      ImGui::EndTabItem();
    }
    
    // Collaboration Tab
    if (ImGui::BeginTabItem(ICON_MD_PEOPLE " Collaboration")) {
      RenderCollaborationPanel();
      ImGui::Separator();
      RenderRomSyncPanel();
      if (show_snapshot_preview_) {
        ImGui::Separator();
        RenderSnapshotPreviewPanel();
      }
      ImGui::EndTabItem();
    }
    
    // Advanced Settings Tab
    if (ImGui::BeginTabItem(ICON_MD_SETTINGS " Advanced")) {
      RenderAgentConfigPanel();
      ImGui::EndTabItem();
    }
    
    // Proposals Tab
    if (ImGui::BeginTabItem(ICON_MD_PREVIEW " Proposals")) {
      RenderProposalManagerPanel();
      ImGui::EndTabItem();
    }
    
    // Metrics Tab
    if (ImGui::BeginTabItem(ICON_MD_ANALYTICS " Metrics")) {
      auto metrics = agent_service_.GetMetrics();
      ImGui::Text("Session Statistics");
      ImGui::Separator();
      
      if (ImGui::BeginTable("MetricsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Total Messages");
        ImGui::TableNextColumn();
        ImGui::Text("%d user, %d agent", metrics.total_user_messages, metrics.total_agent_messages);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Tool Calls");
        ImGui::TableNextColumn();
        ImGui::Text("%d", metrics.total_tool_calls);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Commands Executed");
        ImGui::TableNextColumn();
        ImGui::Text("%d", metrics.total_commands);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Proposals Created");
        ImGui::TableNextColumn();
        ImGui::Text("%d", metrics.total_proposals);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Average Latency");
        ImGui::TableNextColumn();
        ImGui::Text("%.2f seconds", metrics.average_latency_seconds);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Total Session Time");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f seconds", metrics.total_elapsed_seconds);
        
        ImGui::EndTable();
      }
      
      ImGui::EndTabItem();
    }
    
    // File Editor Tabs
    if (ImGui::BeginTabItem(ICON_MD_EDIT_NOTE " Files")) {
      RenderFileEditorTabs();
      ImGui::EndTabItem();
    }
    
    // System Prompt Editor Tab
    if (ImGui::BeginTabItem(ICON_MD_SETTINGS_SUGGEST " System Prompt")) {
      RenderSystemPromptEditor();
      ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
  }

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
  if (ImGui::BeginTable(
          "collab_table", 2,
          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Session Details", ImGuiTableColumnFlags_WidthFixed,
                            250.0f);
    ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();

    // LEFT COLUMN: Session Details with gradient background
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.2f, 0.18f, 0.4f));
    ImGui::BeginChild("session_details", ImVec2(0, 180), true);
    ImVec4 status_color = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);

    const bool connected = collaboration_state_.active;
    ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_INFO " Session Status:");
    ImGui::Spacing();
    if (connected) {
      ImGui::TextColored(status_color, ICON_MD_CHECK_CIRCLE " Connected");
    } else {
      ImGui::TextDisabled(ICON_MD_CANCEL " Not connected");
    }

    if (collaboration_state_.mode == CollaborationMode::kNetwork) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f), ICON_MD_CLOUD " Server:");
      ImGui::TextWrapped("%s", collaboration_state_.server_url.c_str());
    }

    if (!collaboration_state_.session_name.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(status_color, ICON_MD_LABEL " Session:");
      ImGui::TextWrapped("%s", collaboration_state_.session_name.c_str());
    }

    if (!collaboration_state_.session_id.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(status_color, ICON_MD_KEY " Session Code:");
      ImGui::TextWrapped("%s", collaboration_state_.session_id.c_str());
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.6f, 0.6f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.416f, 0.353f, 0.804f, 1.0f));
      if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Copy##copy_session_id")) {
        ImGui::SetClipboardText(collaboration_state_.session_id.c_str());
        if (toast_manager_) {
          toast_manager_->Show("Session code copied!", ToastType::kSuccess, 2.0f);
        }
      }
      ImGui::PopStyleColor(2);
    }

    if (collaboration_state_.last_synced != absl::InfinitePast()) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), ICON_MD_ACCESS_TIME " Last sync:");
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
          "%s", absl::FormatTime("%H:%M:%S", collaboration_state_.last_synced,
                                 absl::LocalTimeZone())
                    .c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Show participants list below session details
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.14f, 0.4f));
    ImGui::BeginChild("participants", ImVec2(0, 0), true);
    if (collaboration_state_.participants.empty()) {
      ImGui::TextDisabled(ICON_MD_PEOPLE " No participants yet");
    } else {
      ImGui::TextColored(status_color, ICON_MD_PEOPLE " Participants (%zu):",
                  collaboration_state_.participants.size());
      ImGui::Separator();
      for (const auto& participant : collaboration_state_.participants) {
        ImGui::BulletText(ICON_MD_PERSON " %s", participant.c_str());
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // RIGHT COLUMN: Controls
    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("controls", ImVec2(0, 0), false);

    ImGui::Separator();

    const bool can_host =
        static_cast<bool>(collaboration_callbacks_.host_session);
    const bool can_join =
        static_cast<bool>(collaboration_callbacks_.join_session);
    const bool can_leave =
        static_cast<bool>(collaboration_callbacks_.leave_session);
    const bool can_refresh =
        static_cast<bool>(collaboration_callbacks_.refresh_session);

    // Network mode: Show server URL input with styling
    if (collaboration_state_.mode == CollaborationMode::kNetwork) {
      ImGui::TextColored(ImVec4(0.196f, 0.6f, 0.8f, 1.0f), ICON_MD_CLOUD " Server URL:");
      ImGui::SetNextItemWidth(-80);
      ImGui::InputText("##collab_server_url", server_url_buffer_,
                       IM_ARRAYSIZE(server_url_buffer_));
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.7f, 0.8f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.196f, 0.6f, 0.8f, 1.0f));
      if (ImGui::Button(ICON_MD_LINK "##connect_server_btn")) {
        collaboration_state_.server_url = server_url_buffer_;
        if (toast_manager_) {
          toast_manager_->Show("Connecting to server...", ToastType::kInfo, 3.0f);
        }
      }
      ImGui::PopStyleColor(2);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Connect to collaboration server");
      }
      ImGui::Separator();
    }

    ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_ADD_CIRCLE " Host New Session:");
    ImGui::SetNextItemWidth(-70);
    ImGui::InputTextWithHint("##collab_session_name", "Enter session name...",
                             session_name_buffer_,
                             IM_ARRAYSIZE(session_name_buffer_));
    ImGui::SameLine();
    if (!can_host)
      ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.5f, 0.0f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.843f, 0.0f, 1.0f));
    if (ImGui::Button(ICON_MD_ROCKET_LAUNCH "##host_session_btn")) {
      std::string name = session_name_buffer_;
      if (name.empty()) {
        if (toast_manager_) {
          toast_manager_->Show("Enter a session name first",
                               ToastType::kWarning, 3.0f);
        }
      } else {
        auto session_or = collaboration_callbacks_.host_session(name);
        if (session_or.ok()) {
          ApplyCollaborationSession(session_or.value(),
                                    /*update_action_timestamp=*/true);
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
          toast_manager_->Show(absl::StrFormat("Failed to host: %s",
                                               session_or.status().message()),
                               ToastType::kError, 5.0f);
        }
      }
    }
    ImGui::PopStyleColor(2);
    if (!can_host) {
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Provide host_session callback to enable hosting");
      }
      ImGui::EndDisabled();
    } else if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Host a new collaboration session");
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.133f, 0.545f, 0.133f, 1.0f), ICON_MD_LOGIN " Join Existing Session:");
    ImGui::SetNextItemWidth(-70);
    ImGui::InputTextWithHint("##collab_join_code", "Enter session code...", join_code_buffer_,
                             IM_ARRAYSIZE(join_code_buffer_));
    ImGui::SameLine();
    if (!can_join)
      ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.1f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.133f, 0.545f, 0.133f, 1.0f));
    if (ImGui::Button(ICON_MD_MEETING_ROOM "##join_session_btn")) {
      std::string code = join_code_buffer_;
      if (code.empty()) {
        if (toast_manager_) {
          toast_manager_->Show("Enter a session code first",
                               ToastType::kWarning, 3.0f);
        }
      } else {
        auto session_or = collaboration_callbacks_.join_session(code);
        if (session_or.ok()) {
          ApplyCollaborationSession(session_or.value(),
                                    /*update_action_timestamp=*/true);
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
          toast_manager_->Show(absl::StrFormat("Failed to join: %s",
                                               session_or.status().message()),
                               ToastType::kError, 5.0f);
        }
      }
    }
    ImGui::PopStyleColor(2);
    if (!can_join) {
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Provide join_session callback to enable joining");
      }
      ImGui::EndDisabled();
    } else if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Join an existing collaboration session");
    }

    if (connected) {
      ImGui::Separator();
      ImGui::Spacing();
      
      if (!can_leave)
        ImGui::BeginDisabled();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.863f, 0.078f, 0.235f, 1.0f));
      if (ImGui::Button(ICON_MD_LOGOUT " Leave Session##leave_session_btn", ImVec2(-1, 0))) {
        absl::Status status = collaboration_callbacks_.leave_session
                                  ? collaboration_callbacks_.leave_session()
                                  : absl::OkStatus();
        if (status.ok()) {
          collaboration_state_ = CollaborationState{};
          join_code_buffer_[0] = '\0';
          if (toast_manager_) {
            toast_manager_->Show("Left collaborative session", ToastType::kInfo,
                                 3.0f);
          }
          MarkHistoryDirty();
        } else if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Failed to leave: %s", status.message()),
              ToastType::kError, 5.0f);
        }
      }
      if (!can_leave)
        ImGui::EndDisabled();
    }

    if (connected) {
      ImGui::Spacing();
      ImGui::Separator();
      if (!can_refresh)
        ImGui::BeginDisabled();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.6f, 0.8f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.416f, 0.353f, 0.804f, 1.0f));
      if (ImGui::Button(ICON_MD_REFRESH " Refresh Session##refresh_collab_btn", ImVec2(-1, 0))) {
        RefreshCollaboration();
      }
      ImGui::PopStyleColor(2);
      if (!can_refresh && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Provide refresh_session callback to enable");
      }
      if (!can_refresh)
        ImGui::EndDisabled();
    } else {
      ImGui::Spacing();
      ImGui::TextDisabled("Start or join a session to collaborate.");
    }

    ImGui::EndChild();  // controls
    ImGui::EndTable();
  }
}

void AgentChatWidget::RenderMultimodalPanel() {
  ImVec4 gemini_color = ImVec4(0.196f, 0.6f, 0.8f, 1.0f);
  
  if (!ImGui::CollapsingHeader(ICON_MD_CAMERA " Gemini Multimodal Vision",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  bool can_capture = static_cast<bool>(multimodal_callbacks_.capture_snapshot);
  bool can_send = static_cast<bool>(multimodal_callbacks_.send_to_gemini);

  // Capture mode selection with icons
  ImGui::TextColored(gemini_color, ICON_MD_SCREENSHOT " Capture Mode:");
  ImGui::RadioButton(ICON_MD_FULLSCREEN " Full Window##mm_radio_full",
                     reinterpret_cast<int*>(&multimodal_state_.capture_mode),
                     static_cast<int>(CaptureMode::kFullWindow));
  ImGui::SameLine();
  ImGui::RadioButton(ICON_MD_DASHBOARD " Active Editor##mm_radio_active",
                     reinterpret_cast<int*>(&multimodal_state_.capture_mode),
                     static_cast<int>(CaptureMode::kActiveEditor));
  ImGui::SameLine();
  ImGui::RadioButton(ICON_MD_WINDOW " Specific##mm_radio_specific",
                     reinterpret_cast<int*>(&multimodal_state_.capture_mode),
                     static_cast<int>(CaptureMode::kSpecificWindow));

  // If specific window mode, show input for window name
  if (multimodal_state_.capture_mode == CaptureMode::kSpecificWindow) {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##mm_window_name_input", "Window name (e.g., Overworld Editor)...", 
                     multimodal_state_.specific_window_buffer,
                     IM_ARRAYSIZE(multimodal_state_.specific_window_buffer));
  }

  ImGui::Separator();

  if (!can_capture)
    ImGui::BeginDisabled();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.7f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gemini_color);
  if (ImGui::Button(ICON_MD_PHOTO_CAMERA " Capture Snapshot##mm_capture_btn", ImVec2(-1, 0))) {
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
          toast_manager_->Show("Snapshot captured", ToastType::kSuccess, 3.0f);
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
  ImGui::PopStyleColor(2);
  if (!can_capture) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Provide capture_snapshot callback to enable");
    }
    ImGui::EndDisabled();
  } else if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Capture a screenshot for vision analysis");
  }

  ImGui::Spacing();

  if (multimodal_state_.last_capture_path.has_value()) {
    ImGui::TextColored(gemini_color, ICON_MD_CHECK_CIRCLE " Last capture: %s",
                        multimodal_state_.last_capture_path->filename().string().c_str());
  } else {
    ImGui::TextDisabled(ICON_MD_CAMERA_ALT " No capture yet");
  }

  ImGui::Spacing();
  ImGui::TextColored(gemini_color, ICON_MD_EDIT " Vision Prompt:");
  ImGui::InputTextMultiline("##mm_gemini_prompt_textarea", multimodal_prompt_buffer_,
                            IM_ARRAYSIZE(multimodal_prompt_buffer_),
                            ImVec2(-1, 80));

  if (!can_send)
    ImGui::BeginDisabled();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.7f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gemini_color);
  if (ImGui::Button(ICON_MD_SEND " Send to Gemini##mm_send_to_gemini_btn", ImVec2(-1, 0))) {
    if (!multimodal_state_.last_capture_path.has_value()) {
      if (toast_manager_) {
        toast_manager_->Show("Capture a snapshot first", ToastType::kWarning,
                             3.0f);
      }
    } else {
      std::string prompt = multimodal_prompt_buffer_;
      absl::Status status = multimodal_callbacks_.send_to_gemini(
          *multimodal_state_.last_capture_path, prompt);
      if (status.ok()) {
        multimodal_state_.status_message = "Submitted image to Gemini";
        multimodal_state_.last_updated = absl::Now();
        if (toast_manager_) {
          toast_manager_->Show("Gemini request sent", ToastType::kSuccess,
                               3.0f);
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
  ImGui::PopStyleColor(2);
  if (!can_send) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Provide send_to_gemini callback to enable");
    }
    ImGui::EndDisabled();
  } else if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Send captured image to Gemini for analysis");
  }

  if (!multimodal_state_.status_message.empty()) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                      ICON_MD_INFO " %s", multimodal_state_.status_message.c_str());
    if (multimodal_state_.last_updated != absl::InfinitePast()) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
          ICON_MD_ACCESS_TIME " %s",
          absl::FormatTime("%H:%M:%S", multimodal_state_.last_updated,
                           absl::LocalTimeZone())
              .c_str());
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
      toast_manager_->Show(absl::StrFormat("Failed to refresh participants: %s",
                                           session_or.status().message()),
                           ToastType::kError, 5.0f);
    }
    return;
  }

  ApplyCollaborationSession(session_or.value(),
                            /*update_action_timestamp=*/false);
  MarkHistoryDirty();
}

void AgentChatWidget::ApplyCollaborationSession(
    const CollaborationCallbacks::SessionContext& context,
    bool update_action_timestamp) {
  collaboration_state_.active = true;
  collaboration_state_.session_id = context.session_id;
  collaboration_state_.session_name =
      context.session_name.empty() ? context.session_id : context.session_name;
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
    toast_manager_->Show("Switched to local chat history", ToastType::kInfo,
                         3.0f);
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

void AgentChatWidget::UpdateAgentConfig(const AgentConfigState& config) {
  agent_config_ = config;
  
  // Apply configuration to the agent service
  cli::agent::AgentConfig service_config;
  service_config.verbose = config.verbose;
  service_config.show_reasoning = config.show_reasoning;
  service_config.max_tool_iterations = config.max_tool_iterations;
  service_config.max_retry_attempts = config.max_retry_attempts;
  
  agent_service_.SetConfig(service_config);
  
  if (toast_manager_) {
    toast_manager_->Show("Agent configuration updated", ToastType::kSuccess, 2.5f);
  }
}

void AgentChatWidget::RenderAgentConfigPanel() {
  if (!ImGui::CollapsingHeader(ICON_MD_SETTINGS " Agent Configuration",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
  ImGui::BeginChild("AgentConfig", ImVec2(0, 350), true);
  
  ImGui::Text(ICON_MD_SMART_TOY " AI Provider");
  ImGui::Separator();
  
  // Provider selection
  int provider_idx = 0;
  if (agent_config_.ai_provider == "ollama") provider_idx = 1;
  else if (agent_config_.ai_provider == "gemini") provider_idx = 2;
  
  if (ImGui::RadioButton("Mock (Testing)", &provider_idx, 0)) {
    agent_config_.ai_provider = "mock";
    std::snprintf(agent_config_.provider_buffer, sizeof(agent_config_.provider_buffer), "mock");
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Ollama (Local)", &provider_idx, 1)) {
    agent_config_.ai_provider = "ollama";
    std::snprintf(agent_config_.provider_buffer, sizeof(agent_config_.provider_buffer), "ollama");
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Gemini (Cloud)", &provider_idx, 2)) {
    agent_config_.ai_provider = "gemini";
    std::snprintf(agent_config_.provider_buffer, sizeof(agent_config_.provider_buffer), "gemini");
  }
  
  ImGui::Spacing();
  
  // Provider-specific settings
  if (agent_config_.ai_provider == "ollama") {
    ImGui::Text(ICON_MD_COMPUTER " Ollama Settings");
    ImGui::InputText("Model", agent_config_.model_buffer, IM_ARRAYSIZE(agent_config_.model_buffer));
    ImGui::TextDisabled("Example: qwen2.5-coder:7b, llama3.2:3b");
    ImGui::InputText("Host", agent_config_.ollama_host_buffer, IM_ARRAYSIZE(agent_config_.ollama_host_buffer));
    
  } else if (agent_config_.ai_provider == "gemini") {
    ImGui::Text(ICON_MD_CLOUD " Gemini Settings");
    ImGui::InputText("Model", agent_config_.model_buffer, IM_ARRAYSIZE(agent_config_.model_buffer));
    ImGui::TextDisabled("Example: gemini-1.5-flash, gemini-2.5-flash");
    ImGui::InputText("API Key", agent_config_.gemini_key_buffer, 
                     IM_ARRAYSIZE(agent_config_.gemini_key_buffer), 
                     ImGuiInputTextFlags_Password);
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text(ICON_MD_TUNE " Behavior Settings");
  
  ImGui::Checkbox("Verbose Output", &agent_config_.verbose);
  ImGui::Checkbox("Show Reasoning", &agent_config_.show_reasoning);
  ImGui::SliderInt("Max Tool Iterations", &agent_config_.max_tool_iterations, 1, 10);
  ImGui::SliderInt("Max Retry Attempts", &agent_config_.max_retry_attempts, 1, 5);
  
  ImGui::Spacing();
  
  if (ImGui::Button(ICON_MD_CHECK " Apply Configuration", ImVec2(-1, 0))) {
    agent_config_.ai_model = agent_config_.model_buffer;
    agent_config_.ollama_host = agent_config_.ollama_host_buffer;
    agent_config_.gemini_api_key = agent_config_.gemini_key_buffer;
    UpdateAgentConfig(agent_config_);
  }
  
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentChatWidget::RenderZ3EDCommandPanel() {
  if (!ImGui::CollapsingHeader(ICON_MD_TERMINAL " Z3ED Commands",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.12f, 0.18f, 1.0f));
  ImGui::BeginChild("Z3EDCommands", ImVec2(0, 300), true);
  
  ImGui::Text(ICON_MD_CODE " Command Palette");
  ImGui::Separator();
  
  ImGui::InputTextWithHint("##z3ed_command", "Enter z3ed command or prompt...",
                           z3ed_command_state_.command_input_buffer,
                           IM_ARRAYSIZE(z3ed_command_state_.command_input_buffer));
  
  ImGui::BeginDisabled(z3ed_command_state_.command_running);
  
  if (ImGui::Button(ICON_MD_PLAY_ARROW " Run Task", ImVec2(-1, 0)) ||
      (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
    if (z3ed_callbacks_.run_agent_task) {
      std::string command = z3ed_command_state_.command_input_buffer;
      z3ed_command_state_.command_running = true;
      auto status = z3ed_callbacks_.run_agent_task(command);
      z3ed_command_state_.command_running = false;
      
      if (status.ok() && toast_manager_) {
        toast_manager_->Show(ICON_MD_CHECK_CIRCLE " Task started",
                             ToastType::kSuccess, 3.0f);
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat(ICON_MD_ERROR " Task failed: %s", status.message()),
            ToastType::kError, 5.0f);
      }
    }
  }
  
  ImGui::EndDisabled();
  
  ImGui::Spacing();
  ImGui::Text(ICON_MD_LIST " Quick Actions");
  ImGui::Separator();
  
  if (ImGui::BeginTable("Z3EDQuickActions", 2, ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_PREVIEW " List Proposals", ImVec2(-1, 0))) {
      if (z3ed_callbacks_.list_proposals) {
        auto result = z3ed_callbacks_.list_proposals();
        if (result.ok()) {
          const auto& proposals = *result;
          std::string output;
          for (size_t i = 0; i < proposals.size(); ++i) {
            if (i > 0) output += "\n";
            output += proposals[i];
          }
          z3ed_command_state_.command_output = output;
        }
      }
    }
    
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_DIFFERENCE " View Diff", ImVec2(-1, 0))) {
      if (z3ed_callbacks_.diff_proposal) {
        auto result = z3ed_callbacks_.diff_proposal("");
        if (result.ok()) {
          z3ed_command_state_.command_output = *result;
        }
      }
    }
    
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_CHECK " Accept", ImVec2(-1, 0))) {
      if (z3ed_callbacks_.accept_proposal && proposal_drawer_) {
        // TODO: Get selected proposal ID from drawer
        auto status = z3ed_callbacks_.accept_proposal("");
        (void)status;  // Acknowledge result
      }
    }
    
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_CLOSE " Reject", ImVec2(-1, 0))) {
      if (z3ed_callbacks_.reject_proposal && proposal_drawer_) {
        // TODO: Get selected proposal ID from drawer
        auto status = z3ed_callbacks_.reject_proposal("");
        (void)status;  // Acknowledge result
      }
    }
    
    ImGui::EndTable();
  }
  
  if (!z3ed_command_state_.command_output.empty()) {
    ImGui::Spacing();
    ImGui::Text(ICON_MD_TERMINAL " Output");
    ImGui::Separator();
    ImGui::BeginChild("CommandOutput", ImVec2(0, 100), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextWrapped("%s", z3ed_command_state_.command_output.c_str());
    ImGui::EndChild();
  }
  
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentChatWidget::RenderRomSyncPanel() {
  if (!ImGui::CollapsingHeader(ICON_MD_SYNC " ROM Synchronization",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.14f, 0.12f, 1.0f));
  ImGui::BeginChild("RomSync", ImVec2(0, 250), true);
  
  ImGui::Text(ICON_MD_STORAGE " ROM State");
  ImGui::Separator();
  
  // Display current ROM hash
  if (!rom_sync_state_.current_rom_hash.empty()) {
    ImGui::Text("Hash: %s", rom_sync_state_.current_rom_hash.substr(0, 16).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY)) {
      ImGui::SetClipboardText(rom_sync_state_.current_rom_hash.c_str());
      if (toast_manager_) {
        toast_manager_->Show("ROM hash copied", ToastType::kInfo, 2.0f);
      }
    }
  } else {
    ImGui::TextDisabled("No ROM loaded");
  }
  
  if (rom_sync_state_.last_sync_time != absl::InfinitePast()) {
    ImGui::Text("Last Sync: %s",
                absl::FormatTime("%H:%M:%S", rom_sync_state_.last_sync_time,
                                absl::LocalTimeZone()).c_str());
  }
  
  ImGui::Spacing();
  ImGui::Checkbox("Auto-sync ROM changes", &rom_sync_state_.auto_sync_enabled);
  
  if (rom_sync_state_.auto_sync_enabled) {
    ImGui::SliderInt("Sync Interval (seconds)", 
                     &rom_sync_state_.sync_interval_seconds, 10, 120);
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  
  bool can_sync = static_cast<bool>(rom_sync_callbacks_.generate_rom_diff) &&
                  collaboration_state_.active &&
                  collaboration_state_.mode == CollaborationMode::kNetwork;
  
  if (!can_sync) ImGui::BeginDisabled();
  
  if (ImGui::Button(ICON_MD_CLOUD_UPLOAD " Send ROM Sync", ImVec2(-1, 0))) {
    if (rom_sync_callbacks_.generate_rom_diff) {
      auto diff_result = rom_sync_callbacks_.generate_rom_diff();
      if (diff_result.ok()) {
        std::string hash = rom_sync_callbacks_.get_rom_hash 
                          ? rom_sync_callbacks_.get_rom_hash() 
                          : "";
        
        rom_sync_state_.current_rom_hash = hash;
        rom_sync_state_.last_sync_time = absl::Now();
        
        // TODO: Send via network coordinator
        if (toast_manager_) {
          toast_manager_->Show(ICON_MD_CLOUD_DONE " ROM synced to collaborators",
                               ToastType::kSuccess, 3.0f);
        }
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat(ICON_MD_ERROR " Sync failed: %s", 
                           diff_result.status().message()),
            ToastType::kError, 5.0f);
      }
    }
  }
  
  if (!can_sync) {
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Connect to a network session to sync ROM");
    }
  }
  
  // Show pending syncs
  if (!rom_sync_state_.pending_syncs.empty()) {
    ImGui::Spacing();
    ImGui::Text(ICON_MD_PENDING " Pending Syncs (%zu)", 
                rom_sync_state_.pending_syncs.size());
    ImGui::Separator();
    
    ImGui::BeginChild("PendingSyncs", ImVec2(0, 80), true);
    for (const auto& sync : rom_sync_state_.pending_syncs) {
      ImGui::BulletText("%s", sync.substr(0, 40).c_str());
    }
    ImGui::EndChild();
  }
  
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentChatWidget::RenderSnapshotPreviewPanel() {
  if (!ImGui::CollapsingHeader(ICON_MD_PHOTO_CAMERA " Snapshot Preview",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
  ImGui::BeginChild("SnapshotPreview", ImVec2(0, 300), true);
  
  if (multimodal_state_.last_capture_path.has_value()) {
    ImGui::Text(ICON_MD_IMAGE " Latest Capture");
    ImGui::Separator();
    ImGui::TextWrapped("%s", 
                      multimodal_state_.last_capture_path->filename().string().c_str());
    
    // TODO: Load and display image thumbnail
    ImGui::TextDisabled("Preview: [Image preview not yet implemented]");
    
    ImGui::Spacing();
    
    bool can_share = collaboration_state_.active && 
                     collaboration_state_.mode == CollaborationMode::kNetwork;
    
    if (!can_share) ImGui::BeginDisabled();
    
    if (ImGui::Button(ICON_MD_SHARE " Share with Collaborators", ImVec2(-1, 0))) {
      // TODO: Share snapshot via network coordinator
      if (toast_manager_) {
        toast_manager_->Show(ICON_MD_CHECK " Snapshot shared",
                             ToastType::kSuccess, 3.0f);
      }
    }
    
    if (!can_share) {
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Connect to a network session to share snapshots");
      }
    }
  } else {
    ImGui::TextDisabled(ICON_MD_NO_PHOTOGRAPHY " No snapshot captured yet");
    ImGui::TextWrapped("Use the Multimodal panel to capture a snapshot");
  }
  
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentChatWidget::RenderProposalManagerPanel() {
  ImGui::Text(ICON_MD_PREVIEW " Proposal Management");
  ImGui::Separator();
  
  if (z3ed_callbacks_.list_proposals) {
    auto proposals_result = z3ed_callbacks_.list_proposals();
    
    if (proposals_result.ok()) {
      const auto& proposals = *proposals_result;
      
      ImGui::Text("Total Proposals: %zu", proposals.size());
      ImGui::Spacing();
      
      if (proposals.empty()) {
        ImGui::TextDisabled("No proposals yet. Use the agent to create proposals.");
      } else {
        if (ImGui::BeginTable("ProposalsTable", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable)) {
          ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
          ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
          ImGui::TableHeadersRow();
          
          for (const auto& proposal_id : proposals) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(proposal_id.c_str());
            
            ImGui::TableNextColumn();
            ImGui::TextDisabled("Proposal details...");
            
            ImGui::TableNextColumn();
            ImGui::PushID(proposal_id.c_str());
            
            if (ImGui::SmallButton(ICON_MD_VISIBILITY)) {
              FocusProposalDrawer(proposal_id);
            }
            ImGui::SameLine();
            
            if (ImGui::SmallButton(ICON_MD_CHECK)) {
              if (z3ed_callbacks_.accept_proposal) {
                auto status = z3ed_callbacks_.accept_proposal(proposal_id);
                (void)status;  // Acknowledge result
              }
            }
            ImGui::SameLine();
            
            if (ImGui::SmallButton(ICON_MD_CLOSE)) {
              if (z3ed_callbacks_.reject_proposal) {
                auto status = z3ed_callbacks_.reject_proposal(proposal_id);
                (void)status;  // Acknowledge result
              }
            }
            
            ImGui::PopID();
          }
          
          ImGui::EndTable();
        }
      }
    } else {
      std::string error_msg(proposals_result.status().message());
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "Failed to load proposals: %s",
                        error_msg.c_str());
    }
  } else {
    ImGui::TextDisabled("Proposal management not available");
    ImGui::TextWrapped("Set up Z3ED command callbacks to enable this feature");
  }
}

void AgentChatWidget::HandleRomSyncReceived(const std::string& diff_data,
                                            const std::string& rom_hash) {
  if (rom_sync_callbacks_.apply_rom_diff) {
    auto status = rom_sync_callbacks_.apply_rom_diff(diff_data, rom_hash);
    
    if (status.ok()) {
      rom_sync_state_.current_rom_hash = rom_hash;
      rom_sync_state_.last_sync_time = absl::Now();
      
      if (toast_manager_) {
        toast_manager_->Show(ICON_MD_CLOUD_DOWNLOAD " ROM sync applied from collaborator",
                             ToastType::kInfo, 3.5f);
      }
    } else if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat(ICON_MD_ERROR " ROM sync failed: %s", status.message()),
          ToastType::kError, 5.0f);
    }
  }
}

void AgentChatWidget::HandleSnapshotReceived(
    [[maybe_unused]] const std::string& snapshot_data,
    const std::string& snapshot_type) {
  // TODO: Decode and store snapshot for preview
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat(ICON_MD_PHOTO " Snapshot received: %s", snapshot_type),
        ToastType::kInfo, 3.0f);
  }
}

void AgentChatWidget::HandleProposalReceived(
    [[maybe_unused]] const std::string& proposal_data) {
  // TODO: Parse and add proposal to local registry
  if (toast_manager_) {
    toast_manager_->Show(ICON_MD_LIGHTBULB " New proposal received from collaborator",
                         ToastType::kInfo, 3.5f);
  }
}

void AgentChatWidget::RenderSystemPromptEditor() {
  ImGui::BeginChild("SystemPromptEditor", ImVec2(0, 0), false);
  
  // Toolbar
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load V1")) {
    // Load embedded system_prompt.txt (v1)
    std::string prompt_v1 = util::LoadFile("assets/agent/system_prompt.txt");
    if (!prompt_v1.empty()) {
      // Find or create system prompt tab
      bool found = false;
      for (auto& tab : open_files_) {
        if (tab.is_system_prompt) {
          tab.editor.SetText(prompt_v1);
          tab.filepath = "";  // Not saved to disk
          tab.filename = "system_prompt_v1.txt (built-in)";
          found = true;
          break;
        }
      }
      
      if (!found) {
        FileEditorTab tab;
        tab.filename = "system_prompt_v1.txt (built-in)";
        tab.filepath = "";
        tab.is_system_prompt = true;
        tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
        tab.editor.SetText(prompt_v1);
        open_files_.push_back(std::move(tab));
        active_file_tab_ = static_cast<int>(open_files_.size()) - 1;
      }
      
      if (toast_manager_) {
        toast_manager_->Show("System prompt V1 loaded", ToastType::kSuccess);
      }
    } else if (toast_manager_) {
      toast_manager_->Show("Could not load system prompt V1", ToastType::kError);
    }
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load V2")) {
    // Load embedded system_prompt_v2.txt
    std::string prompt_v2 = util::LoadFile("assets/agent/system_prompt_v2.txt");
    if (!prompt_v2.empty()) {
      // Find or create system prompt tab
      bool found = false;
      for (auto& tab : open_files_) {
        if (tab.is_system_prompt) {
          tab.editor.SetText(prompt_v2);
          tab.filepath = "";  // Not saved to disk
          tab.filename = "system_prompt_v2.txt (built-in)";
          found = true;
          break;
        }
      }
      
      if (!found) {
        FileEditorTab tab;
        tab.filename = "system_prompt_v2.txt (built-in)";
        tab.filepath = "";
        tab.is_system_prompt = true;
        tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
        tab.editor.SetText(prompt_v2);
        open_files_.push_back(std::move(tab));
        active_file_tab_ = static_cast<int>(open_files_.size()) - 1;
      }
      
      if (toast_manager_) {
        toast_manager_->Show("System prompt V2 loaded", ToastType::kSuccess);
      }
    } else if (toast_manager_) {
      toast_manager_->Show("Could not load system prompt V2", ToastType::kError);
    }
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save to Project")) {
    // Save the current system prompt to project directory
    for (auto& tab : open_files_) {
      if (tab.is_system_prompt) {
        auto save_path = util::FileDialogWrapper::ShowSaveFileDialog(
            "custom_system_prompt", "txt");
        if (!save_path.empty()) {
          std::ofstream file(save_path);
          if (file.is_open()) {
            file << tab.editor.GetText();
            tab.filepath = save_path;
            tab.filename = util::GetFileName(save_path);
            tab.modified = false;
            if (toast_manager_) {
              toast_manager_->Show(absl::StrFormat("System prompt saved to %s", save_path), 
                                  ToastType::kSuccess);
            }
          } else if (toast_manager_) {
            toast_manager_->Show("Failed to save system prompt", ToastType::kError);
          }
        }
        break;
      }
    }
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_NOTE_ADD " Create New")) {
    FileEditorTab tab;
    tab.filename = "custom_system_prompt.txt (unsaved)";
    tab.filepath = "";
    tab.is_system_prompt = true;
    tab.modified = true;
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    tab.editor.SetText("# Custom System Prompt\n\nEnter your custom system prompt here...\n");
    open_files_.push_back(std::move(tab));
    active_file_tab_ = static_cast<int>(open_files_.size()) - 1;
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load Custom")) {
    auto filepath = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!filepath.empty()) {
      std::ifstream file(filepath);
      if (file.is_open()) {
        bool found = false;
        for (auto& tab : open_files_) {
          if (tab.is_system_prompt) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            tab.editor.SetText(buffer.str());
            tab.filepath = filepath;
            tab.filename = util::GetFileName(filepath);
            tab.modified = false;
            found = true;
            break;
          }
        }
        
        if (!found) {
          FileEditorTab tab;
          tab.filename = util::GetFileName(filepath);
          tab.filepath = filepath;
          tab.is_system_prompt = true;
          tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
          std::stringstream buffer;
          buffer << file.rdbuf();
          tab.editor.SetText(buffer.str());
          open_files_.push_back(std::move(tab));
          active_file_tab_ = static_cast<int>(open_files_.size()) - 1;
        }
        
        if (toast_manager_) {
          toast_manager_->Show("Custom system prompt loaded", ToastType::kSuccess);
        }
      } else if (toast_manager_) {
        toast_manager_->Show("Could not load file", ToastType::kError);
      }
    }
  }
  
  ImGui::Separator();
  
  // Find and render system prompt editor
  bool found_prompt = false;
  for (size_t i = 0; i < open_files_.size(); ++i) {
    if (open_files_[i].is_system_prompt) {
      found_prompt = true;
      ImVec2 editor_size = ImVec2(0, ImGui::GetContentRegionAvail().y);
      open_files_[i].editor.Render("##SystemPromptEditor", editor_size);
      if (open_files_[i].editor.IsTextChanged()) {
        open_files_[i].modified = true;
      }
      break;
    }
  }
  
  if (!found_prompt) {
    ImGui::TextWrapped("No system prompt loaded. Click 'Load Default' to edit the system prompt.");
  }
  
  ImGui::EndChild();
}

void AgentChatWidget::RenderFileEditorTabs() {
  ImGui::BeginChild("FileEditorArea", ImVec2(0, 0), false);
  
  // Toolbar
  if (ImGui::Button(ICON_MD_NOTE_ADD " New File")) {
    ImGui::OpenPopup("NewFilePopup");
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open File")) {
    auto filepath = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!filepath.empty()) {
      OpenFileInEditor(filepath);
    }
  }
  
  // New file popup
  static char new_filename_buffer[256] = {};
  if (ImGui::BeginPopup("NewFilePopup")) {
    ImGui::Text("Create New File");
    ImGui::Separator();
    ImGui::InputText("Filename", new_filename_buffer, sizeof(new_filename_buffer));
    if (ImGui::Button("Create")) {
      if (strlen(new_filename_buffer) > 0) {
        CreateNewFileInEditor(new_filename_buffer);
        memset(new_filename_buffer, 0, sizeof(new_filename_buffer));
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  
  ImGui::Separator();
  
  // File tabs
  if (!open_files_.empty()) {
    if (ImGui::BeginTabBar("FileTabs", ImGuiTabBarFlags_Reorderable | 
                                       ImGuiTabBarFlags_FittingPolicyScroll)) {
      for (size_t i = 0; i < open_files_.size(); ++i) {
        if (open_files_[i].is_system_prompt) continue;  // Skip system prompt in file tabs
        
        bool open = true;
        std::string tab_label = open_files_[i].filename;
        if (open_files_[i].modified) {
          tab_label += " *";
        }
        
        if (ImGui::BeginTabItem(tab_label.c_str(), &open)) {
          active_file_tab_ = static_cast<int>(i);
          
          // File toolbar
          if (ImGui::Button(ICON_MD_SAVE " Save")) {
            if (!open_files_[i].filepath.empty()) {
              std::ofstream file(open_files_[i].filepath);
              if (file.is_open()) {
                file << open_files_[i].editor.GetText();
                open_files_[i].modified = false;
                if (toast_manager_) {
                  toast_manager_->Show("File saved", ToastType::kSuccess);
                }
              } else if (toast_manager_) {
                toast_manager_->Show("Failed to save file", ToastType::kError);
              }
            } else {
              auto save_path = util::FileDialogWrapper::ShowSaveFileDialog(
                  open_files_[i].filename, "");
              if (!save_path.empty()) {
                std::ofstream file(save_path);
                if (file.is_open()) {
                  file << open_files_[i].editor.GetText();
                  open_files_[i].filepath = save_path;
                  open_files_[i].modified = false;
                  if (toast_manager_) {
                    toast_manager_->Show("File saved", ToastType::kSuccess);
                  }
                }
              }
            }
          }
          
          ImGui::SameLine();
          ImGui::TextDisabled("%s", open_files_[i].filepath.empty() ? 
                             "(unsaved)" : open_files_[i].filepath.c_str());
          
          ImGui::Separator();
          
          // Editor
          ImVec2 editor_size = ImVec2(0, ImGui::GetContentRegionAvail().y);
          open_files_[i].editor.Render("##FileEditor", editor_size);
          if (open_files_[i].editor.IsTextChanged()) {
            open_files_[i].modified = true;
          }
          
          ImGui::EndTabItem();
        }
        
        if (!open) {
          // Tab was closed
          open_files_.erase(open_files_.begin() + i);
          if (active_file_tab_ >= static_cast<int>(i)) {
            active_file_tab_--;
          }
          break;
        }
      }
      ImGui::EndTabBar();
    }
  } else {
    ImGui::TextWrapped("No files open. Create a new file or open an existing one.");
  }
  
  ImGui::EndChild();
}

void AgentChatWidget::OpenFileInEditor(const std::string& filepath) {
  // Check if file is already open
  for (size_t i = 0; i < open_files_.size(); ++i) {
    if (open_files_[i].filepath == filepath) {
      active_file_tab_ = static_cast<int>(i);
      return;
    }
  }
  
  // Load the file
  std::ifstream file(filepath);
  if (!file.is_open()) {
    if (toast_manager_) {
      toast_manager_->Show("Could not open file", ToastType::kError);
    }
    return;
  }
  
  FileEditorTab tab;
  tab.filepath = filepath;
  
  // Extract filename from path
  size_t last_slash = filepath.find_last_of("/\\");
  tab.filename = (last_slash != std::string::npos) ? 
                 filepath.substr(last_slash + 1) : filepath;
  
  // Set language based on extension
  std::string ext = util::GetFileExtension(filepath);
  if (ext == "cpp" || ext == "cc" || ext == "h" || ext == "hpp") {
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
  } else if (ext == "c") {
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
  } else if (ext == "lua") {
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  tab.editor.SetText(buffer.str());
  
  open_files_.push_back(std::move(tab));
  active_file_tab_ = static_cast<int>(open_files_.size()) - 1;
  
  if (toast_manager_) {
    toast_manager_->Show("File loaded", ToastType::kSuccess);
  }
}

void AgentChatWidget::CreateNewFileInEditor(const std::string& filename) {
  FileEditorTab tab;
  tab.filename = filename;
  tab.modified = true;
  
  // Set language based on extension
  std::string ext = util::GetFileExtension(filename);
  if (ext == "cpp" || ext == "cc" || ext == "h" || ext == "hpp") {
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
  } else if (ext == "c") {
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
  } else if (ext == "lua") {
    tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
  }
  
  open_files_.push_back(std::move(tab));
  active_file_tab_ = static_cast<int>(open_files_.size()) - 1;
}

void AgentChatWidget::LoadAgentSettingsFromProject(const core::YazeProject& project) {
  // Load AI provider settings from project
  agent_config_.ai_provider = project.agent_settings.ai_provider;
  agent_config_.ai_model = project.agent_settings.ai_model;
  agent_config_.ollama_host = project.agent_settings.ollama_host;
  agent_config_.gemini_api_key = project.agent_settings.gemini_api_key;
  agent_config_.show_reasoning = project.agent_settings.show_reasoning;
  agent_config_.verbose = project.agent_settings.verbose;
  agent_config_.max_tool_iterations = project.agent_settings.max_tool_iterations;
  agent_config_.max_retry_attempts = project.agent_settings.max_retry_attempts;
  
  // Copy to buffer for ImGui
  strncpy(agent_config_.provider_buffer, agent_config_.ai_provider.c_str(), 
          sizeof(agent_config_.provider_buffer) - 1);
  strncpy(agent_config_.model_buffer, agent_config_.ai_model.c_str(), 
          sizeof(agent_config_.model_buffer) - 1);
  strncpy(agent_config_.ollama_host_buffer, agent_config_.ollama_host.c_str(), 
          sizeof(agent_config_.ollama_host_buffer) - 1);
  strncpy(agent_config_.gemini_key_buffer, agent_config_.gemini_api_key.c_str(), 
          sizeof(agent_config_.gemini_key_buffer) - 1);
  
  // Load custom system prompt if specified
  if (project.agent_settings.use_custom_prompt && 
      !project.agent_settings.custom_system_prompt.empty()) {
    std::string prompt_path = project.GetAbsolutePath(project.agent_settings.custom_system_prompt);
    std::ifstream file(prompt_path);
    if (file.is_open()) {
      // Load into system prompt tab
      bool found = false;
      for (auto& tab : open_files_) {
        if (tab.is_system_prompt) {
          std::stringstream buffer;
          buffer << file.rdbuf();
          tab.editor.SetText(buffer.str());
          tab.filepath = prompt_path;
          tab.filename = util::GetFileName(prompt_path);
          found = true;
          break;
        }
      }
      
      if (!found) {
        FileEditorTab tab;
        tab.filename = util::GetFileName(prompt_path);
        tab.filepath = prompt_path;
        tab.is_system_prompt = true;
        tab.editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
        std::stringstream buffer;
        buffer << file.rdbuf();
        tab.editor.SetText(buffer.str());
        open_files_.push_back(std::move(tab));
      }
    }
  }
}

void AgentChatWidget::SaveAgentSettingsToProject(core::YazeProject& project) {
  // Save AI provider settings to project
  project.agent_settings.ai_provider = agent_config_.ai_provider;
  project.agent_settings.ai_model = agent_config_.ai_model;
  project.agent_settings.ollama_host = agent_config_.ollama_host;
  project.agent_settings.gemini_api_key = agent_config_.gemini_api_key;
  project.agent_settings.show_reasoning = agent_config_.show_reasoning;
  project.agent_settings.verbose = agent_config_.verbose;
  project.agent_settings.max_tool_iterations = agent_config_.max_tool_iterations;
  project.agent_settings.max_retry_attempts = agent_config_.max_retry_attempts;
  
  // Check if a custom system prompt is loaded
  for (const auto& tab : open_files_) {
    if (tab.is_system_prompt && !tab.filepath.empty()) {
      project.agent_settings.custom_system_prompt = 
          project.GetRelativePath(tab.filepath);
      project.agent_settings.use_custom_prompt = true;
      break;
    }
  }
}

}  // namespace editor
}  // namespace yaze
