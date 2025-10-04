#include "app/editor/system/agent_chat_widget.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/core/platform/file_dialog.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "nlohmann/json.hpp"

namespace {

using yaze::cli::agent::ChatMessage;

const ImVec4 kUserColor = ImVec4(0.88f, 0.76f, 0.36f, 1.0f);
const ImVec4 kAgentColor = ImVec4(0.56f, 0.82f, 0.62f, 1.0f);
const ImVec4 kJsonTextColor = ImVec4(0.78f, 0.83f, 0.90f, 1.0f);
const ImVec4 kProposalPanelColor = ImVec4(0.20f, 0.35f, 0.20f, 0.35f);

std::filesystem::path ExpandUserPath(std::string path) {
  if (!path.empty() && path.front() == '~') {
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
      path.replace(0, 1, home);
    }
  }
  return std::filesystem::path(path);
}

std::filesystem::path ResolveHistoryPath() {
  std::filesystem::path base = ExpandUserPath(yaze::core::GetConfigDirectory());
  if (base.empty()) {
    base = ExpandUserPath(".yaze");
  }
  auto directory = base / "agent";
  return directory / "chat_history.json";
}

absl::Time ParseTimestamp(const nlohmann::json& value) {
  if (!value.is_string()) {
    return absl::Now();
  }
  absl::Time parsed;
  if (absl::ParseTime(absl::RFC3339_full, value.get<std::string>(),
                      absl::UTCTimeZone(), &parsed)) {
    return parsed;
  }
  return absl::Now();
}

nlohmann::json SerializeTableData(const ChatMessage::TableData& table) {
  nlohmann::json json;
  json["headers"] = table.headers;
  json["rows"] = table.rows;
  return json;
}

std::optional<ChatMessage::TableData> ParseTableData(const nlohmann::json& json) {
  if (!json.is_object()) {
    return std::nullopt;
  }

  ChatMessage::TableData table;
  if (json.contains("headers") && json["headers"].is_array()) {
    for (const auto& header : json["headers"]) {
      if (header.is_string()) {
        table.headers.push_back(header.get<std::string>());
      }
    }
  }

  if (json.contains("rows") && json["rows"].is_array()) {
    for (const auto& row : json["rows"]) {
      if (!row.is_array()) {
        continue;
      }
      std::vector<std::string> row_values;
      for (const auto& value : row) {
        if (value.is_string()) {
          row_values.push_back(value.get<std::string>());
        } else {
          row_values.push_back(value.dump());
        }
      }
      table.rows.push_back(std::move(row_values));
    }
  }

  if (table.headers.empty() && table.rows.empty()) {
    return std::nullopt;
  }

  return table;
}

nlohmann::json SerializeProposal(const ChatMessage::ProposalSummary& proposal) {
  nlohmann::json json;
  json["id"] = proposal.id;
  json["change_count"] = proposal.change_count;
  json["executed_commands"] = proposal.executed_commands;
  json["sandbox_rom_path"] = proposal.sandbox_rom_path.string();
  json["proposal_json_path"] = proposal.proposal_json_path.string();
  return json;
}

std::optional<ChatMessage::ProposalSummary> ParseProposal(
    const nlohmann::json& json) {
  if (!json.is_object()) {
    return std::nullopt;
  }

  ChatMessage::ProposalSummary summary;
  summary.id = json.value("id", "");
  summary.change_count = json.value("change_count", 0);
  summary.executed_commands = json.value("executed_commands", 0);
  if (json.contains("sandbox_rom_path") && json["sandbox_rom_path"].is_string()) {
    summary.sandbox_rom_path = json["sandbox_rom_path"].get<std::string>();
  }
  if (json.contains("proposal_json_path") && json["proposal_json_path"].is_string()) {
    summary.proposal_json_path = json["proposal_json_path"].get<std::string>();
  }
  if (summary.id.empty()) {
    return std::nullopt;
  }
  return summary;
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

  std::ifstream file(history_path_);
  if (!file.good()) {
    return;
  }

  try {
    nlohmann::json json;
    file >> json;
    if (!json.contains("messages") || !json["messages"].is_array()) {
      return;
    }

    std::vector<ChatMessage> history;
    for (const auto& item : json["messages"]) {
      if (!item.is_object()) {
        continue;
      }

      ChatMessage message;
      std::string sender = item.value("sender", "agent");
      message.sender =
          sender == "user" ? ChatMessage::Sender::kUser
                             : ChatMessage::Sender::kAgent;
      message.message = item.value("message", "");
      message.timestamp = ParseTimestamp(item["timestamp"]);

      if (item.contains("json_pretty") && item["json_pretty"].is_string()) {
        message.json_pretty = item["json_pretty"].get<std::string>();
      }

      if (item.contains("table_data")) {
        message.table_data = ParseTableData(item["table_data"]);
      }

      if (item.contains("metrics") && item["metrics"].is_object()) {
        ChatMessage::SessionMetrics metrics;
        const auto& metrics_json = item["metrics"];
        metrics.turn_index = metrics_json.value("turn_index", 0);
        metrics.total_user_messages =
            metrics_json.value("total_user_messages", 0);
        metrics.total_agent_messages =
            metrics_json.value("total_agent_messages", 0);
        metrics.total_tool_calls =
            metrics_json.value("total_tool_calls", 0);
        metrics.total_commands = metrics_json.value("total_commands", 0);
        metrics.total_proposals = metrics_json.value("total_proposals", 0);
        metrics.total_elapsed_seconds =
            metrics_json.value("total_elapsed_seconds", 0.0);
        metrics.average_latency_seconds =
            metrics_json.value("average_latency_seconds", 0.0);
        message.metrics = metrics;
      }

      if (item.contains("proposal")) {
        message.proposal = ParseProposal(item["proposal"]);
      }

      history.push_back(std::move(message));
    }

    if (!history.empty()) {
      agent_service_.ReplaceHistory(std::move(history));
      last_history_size_ = agent_service_.GetHistory().size();
      last_proposal_count_ = CountKnownProposals();
      history_dirty_ = false;
      last_persist_time_ = absl::Now();
      if (toast_manager_) {
        toast_manager_->Show("Restored chat history",
                             ToastType::kInfo, 3.5f);
      }
    }

    if (json.contains("collaboration") && json["collaboration"].is_object()) {
      const auto& collab_json = json["collaboration"];
      collaboration_state_.active = collab_json.value("active", false);
      collaboration_state_.session_id = collab_json.value("session_id", "");
      collaboration_state_.participants.clear();
      if (collab_json.contains("participants") &&
          collab_json["participants"].is_array()) {
        for (const auto& participant : collab_json["participants"]) {
          if (participant.is_string()) {
            collaboration_state_.participants.push_back(
                participant.get<std::string>());
          }
        }
      }
      if (collab_json.contains("last_synced")) {
        collaboration_state_.last_synced =
            ParseTimestamp(collab_json["last_synced"]);
      }
    }

    if (json.contains("multimodal") && json["multimodal"].is_object()) {
      const auto& multimodal_json = json["multimodal"];
      if (multimodal_json.contains("last_capture_path") &&
          multimodal_json["last_capture_path"].is_string()) {
        std::string path = multimodal_json["last_capture_path"].get<std::string>();
        if (!path.empty()) {
          multimodal_state_.last_capture_path = std::filesystem::path(path);
        }
      }
      multimodal_state_.status_message =
          multimodal_json.value("status_message", "");
      if (multimodal_json.contains("last_updated")) {
        multimodal_state_.last_updated =
            ParseTimestamp(multimodal_json["last_updated"]);
      }
    }
  } catch (const std::exception& e) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to load chat history: %s", e.what()),
          ToastType::kError, 6.0f);
    }
  }
}

void AgentChatWidget::PersistHistory() {
  if (!history_loaded_ || !history_dirty_) {
    return;
  }

  const auto& history = agent_service_.GetHistory();

  nlohmann::json json;
  json["version"] = 2;
  json["messages"] = nlohmann::json::array();

  for (const auto& message : history) {
    nlohmann::json entry;
    entry["sender"] =
        message.sender == ChatMessage::Sender::kUser ? "user" : "agent";
    entry["message"] = message.message;
    entry["timestamp"] = absl::FormatTime(absl::RFC3339_full,
                                           message.timestamp,
                                           absl::UTCTimeZone());

    if (message.json_pretty.has_value()) {
      entry["json_pretty"] = *message.json_pretty;
    }
    if (message.table_data.has_value()) {
      entry["table_data"] = SerializeTableData(*message.table_data);
    }
    if (message.metrics.has_value()) {
      const auto& metrics = *message.metrics;
      nlohmann::json metrics_json;
      metrics_json["turn_index"] = metrics.turn_index;
      metrics_json["total_user_messages"] = metrics.total_user_messages;
      metrics_json["total_agent_messages"] = metrics.total_agent_messages;
      metrics_json["total_tool_calls"] = metrics.total_tool_calls;
      metrics_json["total_commands"] = metrics.total_commands;
      metrics_json["total_proposals"] = metrics.total_proposals;
      metrics_json["total_elapsed_seconds"] = metrics.total_elapsed_seconds;
      metrics_json["average_latency_seconds"] =
          metrics.average_latency_seconds;
      entry["metrics"] = metrics_json;
    }
    if (message.proposal.has_value()) {
      entry["proposal"] = SerializeProposal(*message.proposal);
    }

    json["messages"].push_back(std::move(entry));
  }

  nlohmann::json collab_json;
  collab_json["active"] = collaboration_state_.active;
  collab_json["session_id"] = collaboration_state_.session_id;
  collab_json["participants"] = collaboration_state_.participants;
  if (collaboration_state_.last_synced != absl::InfinitePast()) {
    collab_json["last_synced"] = absl::FormatTime(
        absl::RFC3339_full, collaboration_state_.last_synced,
        absl::UTCTimeZone());
  }
  json["collaboration"] = std::move(collab_json);

  nlohmann::json multimodal_json;
  if (multimodal_state_.last_capture_path.has_value()) {
    multimodal_json["last_capture_path"] =
        multimodal_state_.last_capture_path->string();
  } else {
    multimodal_json["last_capture_path"] = "";
  }
  multimodal_json["status_message"] = multimodal_state_.status_message;
  if (multimodal_state_.last_updated != absl::InfinitePast()) {
    multimodal_json["last_updated"] = absl::FormatTime(
        absl::RFC3339_full, multimodal_state_.last_updated,
        absl::UTCTimeZone());
  }
  json["multimodal"] = std::move(multimodal_json);

  std::error_code ec;
  auto directory = history_path_.parent_path();
  if (!directory.empty()) {
    std::filesystem::create_directories(directory, ec);
    if (ec) {
      if (toast_manager_) {
        toast_manager_->Show(
            "Unable to create chat history directory",
            ToastType::kError, 5.0f);
      }
      return;
    }
  }

  std::ofstream file(history_path_);
  if (!file.is_open()) {
    if (toast_manager_) {
      toast_manager_->Show("Cannot write chat history",
                           ToastType::kError, 5.0f);
    }
    return;
  }

  file << json.dump(2);
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

  ImGui::Begin(title_.c_str(), &active_);
  RenderHistory();
  RenderCollaborationPanel();
  RenderMultimodalPanel();
  RenderInputBox();
  ImGui::End();
}

void AgentChatWidget::RenderCollaborationPanel() {
  if (!ImGui::CollapsingHeader("Collaborative Session (Preview)",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  const bool connected = collaboration_state_.active;
  ImGui::Text("Status: %s", connected ? "Connected" : "Not connected");
  if (!collaboration_state_.session_id.empty()) {
    ImGui::Text("Session ID: %s", collaboration_state_.session_id.c_str());
  }
  if (collaboration_state_.last_synced != absl::InfinitePast()) {
    ImGui::TextDisabled(
        "Last sync: %s",
        absl::FormatTime("%H:%M:%S", collaboration_state_.last_synced,
                         absl::LocalTimeZone()).c_str());
  }

  ImGui::Separator();

  bool can_host = static_cast<bool>(collaboration_callbacks_.host_session);
  bool can_join = static_cast<bool>(collaboration_callbacks_.join_session);
  bool can_leave = static_cast<bool>(collaboration_callbacks_.leave_session);

  ImGui::InputTextWithHint("##session_name", "Session name",
                           session_name_buffer_,
                           IM_ARRAYSIZE(session_name_buffer_));
  ImGui::SameLine();
  if (!can_host) ImGui::BeginDisabled();
  if (ImGui::Button("Host Session")) {
    std::string name = session_name_buffer_;
    if (name.empty()) {
      if (toast_manager_) {
        toast_manager_->Show("Enter a session name first",
                             ToastType::kWarning, 3.0f);
      }
    } else {
      absl::Status status =
          collaboration_callbacks_.host_session(name);
      if (status.ok()) {
        collaboration_state_.active = true;
        collaboration_state_.session_id = name;
        collaboration_state_.participants.clear();
        collaboration_state_.last_synced = absl::Now();
        last_collaboration_action_ = absl::Now();
        RefreshParticipants();
        if (toast_manager_) {
          toast_manager_->Show("Hosting collaborative session",
                               ToastType::kSuccess, 3.5f);
        }
        MarkHistoryDirty();
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Failed to host: %s", status.message()),
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

  ImGui::InputTextWithHint("##join_code", "Session code",
                           join_code_buffer_,
                           IM_ARRAYSIZE(join_code_buffer_));
  ImGui::SameLine();
  if (!can_join) ImGui::BeginDisabled();
  if (ImGui::Button("Join Session")) {
    std::string code = join_code_buffer_;
    if (code.empty()) {
      if (toast_manager_) {
        toast_manager_->Show("Enter a session code first",
                             ToastType::kWarning, 3.0f);
      }
    } else {
      absl::Status status =
          collaboration_callbacks_.join_session(code);
      if (status.ok()) {
        collaboration_state_.active = true;
        collaboration_state_.session_id = code;
        collaboration_state_.last_synced = absl::Now();
        last_collaboration_action_ = absl::Now();
        RefreshParticipants();
        if (toast_manager_) {
          toast_manager_->Show(
              absl::StrFormat("Joined session %s", code.c_str()),
              ToastType::kSuccess, 3.5f);
        }
        MarkHistoryDirty();
      } else if (toast_manager_) {
        toast_manager_->Show(
            absl::StrFormat("Failed to join: %s", status.message()),
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
    ImGui::Separator();
    if (ImGui::Button("Refresh Participants")) {
      RefreshParticipants();
    }
    if (collaboration_state_.participants.empty()) {
      ImGui::TextDisabled("Awaiting participant list...");
    } else {
      ImGui::Text("Participants (%zu):",
                  collaboration_state_.participants.size());
      for (const auto& participant : collaboration_state_.participants) {
        ImGui::BulletText("%s", participant.c_str());
      }
    }
  } else {
    ImGui::TextDisabled("Start or join a session to collaborate in real time.");
  }
}

void AgentChatWidget::RenderMultimodalPanel() {
  if (!ImGui::CollapsingHeader("Gemini Multimodal (Preview)",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  bool can_capture = static_cast<bool>(multimodal_callbacks_.capture_snapshot);
  bool can_send = static_cast<bool>(multimodal_callbacks_.send_to_gemini);

  if (!can_capture) ImGui::BeginDisabled();
  if (ImGui::Button("Capture Map Snapshot")) {
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

void AgentChatWidget::RefreshParticipants() {
  if (!collaboration_callbacks_.refresh_participants) {
    return;
  }
  auto participants_or = collaboration_callbacks_.refresh_participants();
  if (!participants_or.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to refresh participants: %s",
                           participants_or.status().message()),
          ToastType::kError, 5.0f);
    }
    return;
  }

  collaboration_state_.participants = participants_or.value();
  collaboration_state_.last_synced = absl::Now();
  MarkHistoryDirty();
}

void AgentChatWidget::MarkHistoryDirty() {
  history_dirty_ = true;
  const absl::Time now = absl::Now();
  if (last_persist_time_ == absl::InfinitePast() ||
      now - last_persist_time_ > absl::Seconds(2)) {
    PersistHistory();
  }
}

}  // namespace editor
}  // namespace yaze
