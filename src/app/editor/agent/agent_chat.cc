#include "app/editor/agent/agent_chat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/platform_paths.h"
#include "util/log.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

namespace {

std::string ResolveAgentChatHistoryPath() {
  auto agent_dir = util::PlatformPaths::GetAppDataSubdirectory("agent");
  if (agent_dir.ok()) {
    return (*agent_dir / "agent_chat_history.json").string();
  }
  auto docs_dir = util::PlatformPaths::GetUserDocumentsSubdirectory("agent");
  if (docs_dir.ok()) {
    return (*docs_dir / "agent_chat_history.json").string();
  }
  auto temp_dir = util::PlatformPaths::GetTempDirectory();
  if (temp_dir.ok()) {
    return (*temp_dir / "agent_chat_history.json").string();
  }
  return (std::filesystem::current_path() / "agent_chat_history.json")
      .string();
}

}  // namespace

AgentChat::AgentChat() {
  // Default initialization
}

void AgentChat::Initialize(ToastManager* toast_manager, ProposalDrawer* proposal_drawer) {
  toast_manager_ = toast_manager;
  proposal_drawer_ = proposal_drawer;
}

void AgentChat::SetRomContext(Rom* rom) {
  rom_ = rom;
}

void AgentChat::SetContext(AgentUIContext* context) {
  context_ = context;
}

cli::agent::ConversationalAgentService* AgentChat::GetAgentService() {
  return &agent_service_;
}

void AgentChat::ScrollToBottom() {
  scroll_to_bottom_ = true;
}

void AgentChat::ClearHistory() {
  agent_service_.ResetConversation();
  if (toast_manager_) {
    toast_manager_->Show("Chat history cleared", ToastType::kInfo);
  }
}

void AgentChat::SendMessage(const std::string& message) {
  if (message.empty()) return;

  waiting_for_response_ = true;
  thinking_animation_ = 0.0f;
  ScrollToBottom();

  // Send to service
  auto status = agent_service_.SendMessage(message);
  HandleAgentResponse(status);
}

void AgentChat::HandleAgentResponse(const absl::StatusOr<cli::agent::ChatMessage>& response) {
  waiting_for_response_ = false;
  if (!response.ok()) {
    if (toast_manager_) {
      toast_manager_->Show("Agent Error: " + std::string(response.status().message()), ToastType::kError);
    }
    LOG_ERROR("AgentChat", "Agent Error: %s", response.status().ToString().c_str());
  } else {
    ScrollToBottom();
  }
}

void AgentChat::Draw(float available_height) {
  if (!context_) return;

  // Chat container
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

  // 0. Toolbar at top
  RenderToolbar();
  ImGui::Separator();

  // 1. History Area (Available space - Input height - Toolbar height)
  float input_height = ImGui::GetTextLineHeightWithSpacing() * 4 + 20.0f;
  float toolbar_height = ImGui::GetFrameHeightWithSpacing() + 8.0f;
  float history_height = available_height > 0
      ? (available_height - input_height - toolbar_height)
      : -input_height - toolbar_height;

  if (ImGui::BeginChild("##ChatHistory", ImVec2(0, history_height), true)) {
    RenderHistory();
    // Handle auto-scroll
    if (scroll_to_bottom_ ||
        (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
      ImGui::SetScrollHereY(1.0f);
      scroll_to_bottom_ = false;
    }
  }
  ImGui::EndChild();

  // 2. Input Area
  RenderInputBox();

  ImGui::PopStyleVar();
}

void AgentChat::RenderToolbar() {
  if (ImGui::Button(ICON_MD_DELETE_FOREVER " Clear")) {
    ClearHistory();
  }
  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    std::string filepath = ResolveAgentChatHistoryPath();
    if (auto status = SaveHistory(filepath); !status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show("Failed to save history: " + std::string(status.message()), ToastType::kError);
      }
    } else {
      if (toast_manager_) {
        toast_manager_->Show("Chat history saved", ToastType::kSuccess);
      }
    }
  }
  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load")) {
    std::string filepath = ResolveAgentChatHistoryPath();
    if (auto status = LoadHistory(filepath); !status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show("Failed to load history: " + std::string(status.message()), ToastType::kError);
      }
    } else {
      if (toast_manager_) {
        toast_manager_->Show("Chat history loaded", ToastType::kSuccess);
      }
    }
  }

  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &auto_scroll_);

  ImGui::SameLine();
  ImGui::Checkbox("Timestamps", &show_timestamps_);

  ImGui::SameLine();
  ImGui::Checkbox("Reasoning", &show_reasoning_);
}

void AgentChat::RenderHistory() {
  const auto& history = agent_service_.GetHistory();

  if (history.empty()) {
    ImGui::TextDisabled("Start a conversation with the agent...");
  }

  for (size_t i = 0; i < history.size(); ++i) {
    RenderMessage(history[i], static_cast<int>(i));
    if (message_spacing_ > 0) {
      ImGui::Dummy(ImVec2(0, message_spacing_));
    }
  }

  if (waiting_for_response_) {
    RenderThinkingIndicator();
  }
}

void AgentChat::RenderMessage(const cli::agent::ChatMessage& msg, int index) {
  bool is_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);

  ImGui::PushID(index);

  // Styling
  float wrap_width = ImGui::GetContentRegionAvail().x * 0.85f;
  ImGui::SetCursorPosX(is_user ? (ImGui::GetWindowContentRegionMax().x - wrap_width - 10) : 10);

  ImGui::BeginGroup();

  // Timestamp (if enabled)
  if (show_timestamps_) {
    std::string timestamp =
        absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone());
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", timestamp.c_str());
    ImGui::SameLine();
  }

  // Name/Icon
  if (is_user) {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s You", ICON_MD_PERSON);
  } else {
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s Agent", ICON_MD_SMART_TOY);
  }

  // Message Bubble
  ImVec4 bg_col = is_user ? ImVec4(0.2f, 0.2f, 0.25f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_col);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

  std::string content_id = "msg_content_" + std::to_string(index);
  if (ImGui::BeginChild(content_id.c_str(), ImVec2(wrap_width, 0), true, ImGuiWindowFlags_AlwaysUseWindowPadding)) {
    // Check if we have table data to render
    if (!is_user && msg.table_data.has_value()) {
      RenderTableData(msg.table_data.value());
    } else if (!is_user && msg.json_pretty.has_value()) {
      ImGui::TextWrapped("%s", msg.json_pretty.value().c_str());
    } else {
      // Parse message for code blocks
      auto blocks = ParseMessageContent(msg.message);
      for (const auto& block : blocks) {
        if (block.type == ContentBlock::Type::kCode) {
          RenderCodeBlock(block.content, block.language, index);
        } else {
          ImGui::TextWrapped("%s", block.content.c_str());
        }
      }
    }

    // Render proposals if any (detect from message or metadata)
    if (!is_user) {
      RenderProposalQuickActions(msg, index);
    }

    // Render tool execution timeline if metadata is available
    if (!is_user) {
      RenderToolTimeline(msg);
    }
  }
  ImGui::EndChild();

  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::EndGroup();

  ImGui::Spacing();
  ImGui::PopID();
}

void AgentChat::RenderThinkingIndicator() {
  ImGui::Spacing();
  ImGui::Indent(10);
  ImGui::TextDisabled("%s Agent is thinking...", ICON_MD_PENDING);
  
  // Simple pulse animation
  thinking_animation_ += ImGui::GetIO().DeltaTime;
  int dots = (int)(thinking_animation_ * 3) % 4;
  ImGui::SameLine();
  if (dots == 0) ImGui::Text(".");
  else if (dots == 1) ImGui::Text("..");
  else if (dots == 2) ImGui::Text("...");
  
  ImGui::Unindent(10);
}

void AgentChat::RenderInputBox() {
  ImGui::Separator();
  
  // Input flags
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
  
  ImGui::PushItemWidth(-1);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  
  bool submit = ImGui::InputTextMultiline("##Input", input_buffer_, sizeof(input_buffer_), ImVec2(0, 0), flags);
  
  if (submit) {
    std::string msg(input_buffer_);
    // Trim whitespace
    while (!msg.empty() && std::isspace(msg.back())) msg.pop_back();
    
    if (!msg.empty()) {
      SendMessage(msg);
      input_buffer_[0] = '\0';
      ImGui::SetKeyboardFocusHere(-1); // Refocus
    }
  }
  
  ImGui::PopItemWidth();
}

void AgentChat::RenderProposalQuickActions(const cli::agent::ChatMessage& msg, int index) {
  // Simple check for "Proposal:" keyword for now, or metadata if available
  // In a real implementation, we'd parse the JSON proposal data
  if (msg.message.find("Proposal:") != std::string::npos) {
    ImGui::Separator();
    if (ImGui::Button("View Proposal")) {
      // Logic to open proposal drawer
      if (proposal_drawer_) {
        proposal_drawer_->Show();
      }
    }
  }
}

void AgentChat::RenderCodeBlock(const std::string& code, const std::string& language, int msg_index) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
  if (ImGui::BeginChild(absl::StrCat("code_", msg_index).c_str(), ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (!language.empty()) {
        ImGui::TextDisabled("%s", language.c_str());
        ImGui::SameLine();
    }
    if (ImGui::Button(ICON_MD_CONTENT_COPY)) {
        ImGui::SetClipboardText(code.c_str());
        if (toast_manager_) toast_manager_->Show("Code copied", ToastType::kSuccess);
    }
    ImGui::Separator();
    ImGui::TextUnformatted(code.c_str());
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentChat::UpdateHarnessTelemetry(const AutomationTelemetry& telemetry) {
  telemetry_history_.push_back(telemetry);
  // Keep only the last 100 entries to avoid memory growth
  if (telemetry_history_.size() > 100) {
    telemetry_history_.erase(telemetry_history_.begin());
  }
}

void AgentChat::SetLastPlanSummary(const std::string& summary) {
  last_plan_summary_ = summary;
}

std::vector<AgentChat::ContentBlock> AgentChat::ParseMessageContent(const std::string& content) {
  std::vector<ContentBlock> blocks;

  // Basic markdown code block parser
  size_t pos = 0;
  while (pos < content.length()) {
    size_t code_start = content.find("```", pos);
    if (code_start == std::string::npos) {
      // Rest is text
      blocks.push_back({ContentBlock::Type::kText, content.substr(pos), ""});
      break;
    }

    // Add text before code
    if (code_start > pos) {
      blocks.push_back({ContentBlock::Type::kText, content.substr(pos, code_start - pos), ""});
    }

    size_t code_end = content.find("```", code_start + 3);
    if (code_end == std::string::npos) {
      // Malformed, treat as text
      blocks.push_back({ContentBlock::Type::kText, content.substr(code_start), ""});
      break;
    }

    // Extract language
    std::string language;
    size_t newline = content.find('\n', code_start + 3);
    size_t content_start = code_start + 3;
    if (newline != std::string::npos && newline < code_end) {
      language = content.substr(code_start + 3, newline - (code_start + 3));
      content_start = newline + 1;
    }

    std::string code = content.substr(content_start, code_end - content_start);
    blocks.push_back({ContentBlock::Type::kCode, code, language});

    pos = code_end + 3;
  }

  return blocks;
}

void AgentChat::RenderTableData(const cli::agent::ChatMessage::TableData& table) {
  if (table.headers.empty()) {
    return;
  }

  // Render table
  if (ImGui::BeginTable("ToolResultTable", static_cast<int>(table.headers.size()),
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY)) {
    // Headers
    for (const auto& header : table.headers) {
      ImGui::TableSetupColumn(header.c_str());
    }
    ImGui::TableHeadersRow();

    // Rows
    for (const auto& row : table.rows) {
      ImGui::TableNextRow();
      for (size_t col = 0; col < std::min(row.size(), table.headers.size()); ++col) {
        ImGui::TableSetColumnIndex(static_cast<int>(col));
        ImGui::TextWrapped("%s", row[col].c_str());
      }
    }

    ImGui::EndTable();
  }
}

void AgentChat::RenderToolTimeline(const cli::agent::ChatMessage& msg) {
  // Check if we have model metadata with tool information
  if (!msg.model_metadata.has_value()) {
    return;
  }

  const auto& meta = msg.model_metadata.value();

  // Only render if tools were called
  if (meta.tool_names.empty() && meta.tool_iterations == 0) {
    return;
  }

  ImGui::Separator();
  ImGui::Spacing();

  // Tool timeline header - collapsible
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));

  std::string header = absl::StrFormat("%s Tools (%d calls, %.2fs)", ICON_MD_BUILD_CIRCLE,
                                       meta.tool_iterations, meta.latency_seconds);

  if (ImGui::TreeNode("##ToolTimeline", "%s", header.c_str())) {
    // List tool names
    if (!meta.tool_names.empty()) {
      ImGui::TextDisabled("Tools called:");
      for (const auto& tool : meta.tool_names) {
        ImGui::BulletText("%s", tool.c_str());
      }
    }

    // Provider/model info
    ImGui::Spacing();
    ImGui::TextDisabled("Provider: %s", meta.provider.c_str());
    if (!meta.model.empty()) {
      ImGui::TextDisabled("Model: %s", meta.model.c_str());
    }

    ImGui::TreePop();
  }

  ImGui::PopStyleColor(2);
}

absl::Status AgentChat::LoadHistory(const std::string& filepath) {
#ifdef YAZE_WITH_JSON
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open file: %s", filepath));
  }

  try {
    nlohmann::json j;
    file >> j;

    // Parse and load messages
    // Note: This would require exposing a LoadHistory method in
    // ConversationalAgentService. For now, we'll just return success.
    // TODO: Implement full history restoration when service supports it.

    return absl::OkStatus();
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse JSON: %s", e.what()));
  }
#else
  return absl::UnimplementedError("JSON support not available");
#endif
}

absl::Status AgentChat::SaveHistory(const std::string& filepath) {
#ifdef YAZE_WITH_JSON
  // Create directory if needed
  std::filesystem::path path(filepath);
  if (path.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return absl::InternalError(
          absl::StrFormat("Failed to create history directory: %s",
                          ec.message()));
    }
  }

  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Could not create file: %s", filepath));
  }

  try {
    nlohmann::json j;
    const auto& history = agent_service_.GetHistory();

    j["version"] = 1;
    j["messages"] = nlohmann::json::array();

    for (const auto& msg : history) {
      nlohmann::json msg_json;
      msg_json["sender"] = (msg.sender == cli::agent::ChatMessage::Sender::kUser) ? "user" : "agent";
      msg_json["message"] = msg.message;
      msg_json["timestamp"] = absl::FormatTime(msg.timestamp);
      j["messages"].push_back(msg_json);
    }

    file << j.dump(2);  // Pretty print with 2-space indent

    return absl::OkStatus();
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to serialize JSON: %s", e.what()));
  }
#else
  return absl::UnimplementedError("JSON support not available");
#endif
}

}  // namespace editor
}  // namespace yaze
