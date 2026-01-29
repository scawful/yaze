#include "app/editor/agent/agent_chat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_chat_history_codec.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/log.h"
#include "util/platform_paths.h"

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
  auto temp_dir = util::PlatformPaths::GetTempDirectory();
  if (temp_dir.ok()) {
    return (*temp_dir / "agent_chat_history.json").string();
  }
  return (std::filesystem::current_path() / "agent_chat_history.json").string();
}

std::optional<std::filesystem::path> ResolveAgentSessionsDir() {
  auto agent_dir = util::PlatformPaths::GetAppDataSubdirectory("agent");
  if (!agent_dir.ok()) {
    return std::nullopt;
  }
  return *agent_dir / "sessions";
}

absl::Time FileTimeToAbsl(std::filesystem::file_time_type value) {
  using FileClock = std::filesystem::file_time_type::clock;
  auto now_file = FileClock::now();
  auto now_sys = std::chrono::system_clock::now();
  auto converted = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      value - now_file + now_sys);
  return absl::FromChrono(converted);
}

std::string TrimTitle(const std::string& text) {
  std::string trimmed = std::string(absl::StripAsciiWhitespace(text));
  if (trimmed.empty()) {
    return trimmed;
  }
  constexpr size_t kMaxLen = 64;
  if (trimmed.size() > kMaxLen) {
    trimmed = trimmed.substr(0, kMaxLen - 3);
    trimmed.append("...");
  }
  return trimmed;
}

std::string BuildConversationTitle(const std::filesystem::path& path,
                                   const std::vector<cli::agent::ChatMessage>& history) {
  for (const auto& msg : history) {
    if (msg.sender == cli::agent::ChatMessage::Sender::kUser &&
        !msg.message.empty()) {
      std::string title = TrimTitle(msg.message);
      if (!title.empty()) {
        return title;
      }
    }
  }
  std::string fallback = path.stem().string();
  if (!fallback.empty()) {
    return fallback;
  }
  return "Untitled";
}

}  // namespace

AgentChat::AgentChat() {
  // Default initialization
}

void AgentChat::Initialize(ToastManager* toast_manager,
                           ProposalDrawer* proposal_drawer) {
  toast_manager_ = toast_manager;
  proposal_drawer_ = proposal_drawer;
  if (active_history_path_.empty()) {
    active_history_path_ = ResolveAgentChatHistoryPath();
  }
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
  if (message.empty())
    return;

  waiting_for_response_ = true;
  thinking_animation_ = 0.0f;
  ScrollToBottom();

  // Send to service
  auto status = agent_service_.SendMessage(message);
  HandleAgentResponse(status);
}

void AgentChat::HandleAgentResponse(
    const absl::StatusOr<cli::agent::ChatMessage>& response) {
  waiting_for_response_ = false;
  if (!response.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          "Agent Error: " + std::string(response.status().message()),
          ToastType::kError);
    }
    LOG_ERROR("AgentChat", "Agent Error: %s",
              response.status().ToString().c_str());
  } else {
    ScrollToBottom();
  }
}

void AgentChat::RefreshConversationList(bool force) {
  if (!AgentChatHistoryCodec::Available()) {
    conversations_.clear();
    return;
  }
  if (!force && last_conversation_refresh_ != absl::InfinitePast()) {
    absl::Duration since = absl::Now() - last_conversation_refresh_;
    if (since < absl::Seconds(5)) {
      return;
    }
  }

  if (active_history_path_.empty()) {
    active_history_path_ = ResolveAgentChatHistoryPath();
  }

  std::vector<std::filesystem::path> candidates;
  std::unordered_set<std::string> seen;

  if (!active_history_path_.empty()) {
    candidates.push_back(active_history_path_);
    seen.insert(active_history_path_.string());
  }

  if (auto sessions_dir = ResolveAgentSessionsDir()) {
    std::error_code ec;
    if (std::filesystem::exists(*sessions_dir, ec)) {
      for (const auto& entry :
           std::filesystem::directory_iterator(*sessions_dir, ec)) {
        if (ec) {
          break;
        }
        if (!entry.is_regular_file(ec)) {
          continue;
        }
        auto path = entry.path();
        if (path.extension() != ".json") {
          continue;
        }
        if (!absl::EndsWith(path.stem().string(), "_history")) {
          continue;
        }
        const std::string key = path.string();
        if (seen.insert(key).second) {
          candidates.push_back(std::move(path));
        }
      }
    }
  }

  conversations_.clear();
  for (const auto& path : candidates) {
    ConversationEntry entry;
    entry.path = path;
    entry.is_active = (!active_history_path_.empty() &&
                       path == active_history_path_);

    auto snapshot_or = AgentChatHistoryCodec::Load(path);
    if (snapshot_or.ok()) {
      const auto& snapshot = snapshot_or.value();
      entry.message_count = static_cast<int>(snapshot.history.size());
      entry.title = BuildConversationTitle(path, snapshot.history);
      if (!snapshot.history.empty()) {
        entry.last_updated = snapshot.history.back().timestamp;
      } else {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
          entry.last_updated = FileTimeToAbsl(std::filesystem::last_write_time(path, ec));
        }
      }
      if (entry.is_active && snapshot.history.empty()) {
        entry.title = "Current Session";
      }
    } else {
      entry.title = path.stem().string();
    }

    conversations_.push_back(std::move(entry));
  }

  std::sort(conversations_.begin(), conversations_.end(),
            [](const ConversationEntry& a, const ConversationEntry& b) {
              if (a.is_active != b.is_active) {
                return a.is_active;
              }
              return a.last_updated > b.last_updated;
            });

  last_conversation_refresh_ = absl::Now();
}

void AgentChat::SelectConversation(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }
  active_history_path_ = path;
  auto status = LoadHistory(path.string());
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(std::string(status.message()), ToastType::kError,
                           3.0f);
    }
  } else {
    ScrollToBottom();
  }
  RefreshConversationList(true);
}

void AgentChat::Draw(float available_height) {
  if (!context_)
    return;

  RefreshConversationList(false);

  // Chat container
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

  const float content_width = ImGui::GetContentRegionAvail().x;
  const bool wide_layout = content_width >= 680.0f;

  // 0. Toolbar at top
  RenderToolbar(!wide_layout);
  ImGui::Separator();

  float content_height =
      available_height > 0 ? available_height : ImGui::GetContentRegionAvail().y;

  if (wide_layout) {
    const float sidebar_width =
        std::clamp(content_width * 0.28f, 220.0f, 320.0f);
    if (ImGui::BeginChild("##ChatSidebar", ImVec2(sidebar_width, content_height),
                          true, ImGuiWindowFlags_NoScrollbar)) {
      RenderConversationSidebar(content_height);
    }
    ImGui::EndChild();

    ImGui::SameLine();
  }

  if (ImGui::BeginChild("##ChatMain", ImVec2(0, content_height), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    const float input_height =
        ImGui::GetTextLineHeightWithSpacing() * 3.5f + 24.0f;
    const float history_height =
        std::max(140.0f, ImGui::GetContentRegionAvail().y - input_height);

    if (ImGui::BeginChild("##ChatHistory", ImVec2(0, history_height), true,
                          ImGuiWindowFlags_NoScrollbar)) {
      RenderHistory();
      if (scroll_to_bottom_ ||
          (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
      }
    }
    ImGui::EndChild();

    RenderInputBox(input_height);
  }
  ImGui::EndChild();

  ImGui::PopStyleVar();
}

void AgentChat::RenderToolbar(bool compact) {
  const auto& theme = AgentUI::GetTheme();

  ImGui::PushStyleColor(ImGuiCol_Button, theme.status_success);
  if (ImGui::Button(ICON_MD_ADD_COMMENT " New Chat")) {
    ClearHistory();
    active_history_path_ = ResolveAgentChatHistoryPath();
    RefreshConversationList(true);
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_DELETE_FOREVER " Clear")) {
    ClearHistory();
  }
  ImGui::SameLine();

  const bool history_available = AgentChatHistoryCodec::Available();
  ImGui::BeginDisabled(!history_available);
  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    const std::string filepath =
        active_history_path_.empty() ? ResolveAgentChatHistoryPath()
                                     : active_history_path_.string();
    if (auto status = SaveHistory(filepath); !status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show(
            "Failed to save history: " + std::string(status.message()),
            ToastType::kError);
      }
    } else if (toast_manager_) {
      toast_manager_->Show("Chat history saved", ToastType::kSuccess);
    }
    RefreshConversationList(true);
  }
  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load")) {
    const std::string filepath =
        active_history_path_.empty() ? ResolveAgentChatHistoryPath()
                                     : active_history_path_.string();
    if (auto status = LoadHistory(filepath); !status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show(
            "Failed to load history: " + std::string(status.message()),
            ToastType::kError);
      }
    } else if (toast_manager_) {
      toast_manager_->Show("Chat history loaded", ToastType::kSuccess);
    }
  }
  ImGui::EndDisabled();

  if (compact && !conversations_.empty()) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    const char* current_label = "Current Session";
    for (const auto& entry : conversations_) {
      if (entry.is_active) {
        current_label = entry.title.c_str();
        break;
      }
    }
    if (ImGui::BeginCombo("##conversation_combo", current_label)) {
      for (const auto& entry : conversations_) {
        bool selected = entry.is_active;
        if (ImGui::Selectable(entry.title.c_str(), selected)) {
          SelectConversation(entry.path);
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_TUNE)) {
    ImGui::OpenPopup("ChatOptions");
  }
  if (ImGui::BeginPopup("ChatOptions")) {
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    ImGui::Checkbox("Timestamps", &show_timestamps_);
    ImGui::Checkbox("Reasoning", &show_reasoning_);
    ImGui::EndPopup();
  }
}

void AgentChat::RenderConversationSidebar(float height) {
  const auto& theme = AgentUI::GetTheme();

  if (!AgentChatHistoryCodec::Available()) {
    ImGui::TextDisabled("Chat history persistence unavailable.");
    ImGui::TextDisabled("Build with JSON support to enable sessions.");
    return;
  }

  if (context_) {
    const auto& config = context_->agent_config();
    ImGui::TextColored(theme.text_secondary_color, "Agent");
    if (panel_opener_) {
      if (ImGui::SmallButton(ICON_MD_SETTINGS " Config")) {
        panel_opener_("agent.configuration");
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_AUTO_FIX_HIGH " Builder")) {
        panel_opener_("agent.builder");
      }
    }
    ImGui::TextDisabled("Provider: %s",
                        config.ai_provider.empty() ? "mock"
                                                   : config.ai_provider.c_str());
    ImGui::TextDisabled("Model: %s",
                        config.ai_model.empty() ? "not set"
                                                : config.ai_model.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  ImGui::TextColored(theme.text_secondary_color, "Conversations");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    RefreshConversationList(true);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Refresh list");
  }

  ImGui::Spacing();
  ImGui::InputTextWithHint("##conversation_filter", "Search...",
                           conversation_filter_, sizeof(conversation_filter_));
  ImGui::Spacing();

  const float list_height = std::max(0.0f, height - 80.0f);
  if (ImGui::BeginChild("ConversationList", ImVec2(0, list_height), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    std::string filter = absl::AsciiStrToLower(conversation_filter_);
    if (conversations_.empty()) {
      ImGui::TextDisabled("No saved conversations yet.");
    } else {
      int index = 0;
      for (const auto& entry : conversations_) {
        std::string title_lower = absl::AsciiStrToLower(entry.title);
        if (!filter.empty() &&
            title_lower.find(filter) == std::string::npos) {
          continue;
        }

        ImGui::PushID(index++);
        ImGui::PushStyleColor(ImGuiCol_Header,
                              entry.is_active ? theme.status_active
                                              : theme.panel_bg_darker);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme.panel_bg_color);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme.status_active);
        if (ImGui::Selectable(entry.title.c_str(), entry.is_active,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          SelectConversation(entry.path);
        }
        ImGui::PopStyleColor(3);

        ImGui::TextDisabled("%d msg%s", entry.message_count,
                            entry.message_count == 1 ? "" : "s");
        if (entry.last_updated != absl::InfinitePast()) {
          ImGui::SameLine();
          ImGui::TextDisabled("%s",
                              absl::FormatTime("%b %d, %H:%M",
                                               entry.last_updated,
                                               absl::LocalTimeZone())
                                  .c_str());
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
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
  ImGui::SetCursorPosX(
      is_user ? (ImGui::GetWindowContentRegionMax().x - wrap_width - 10) : 10);

  ImGui::BeginGroup();

  // Timestamp (if enabled)
  if (show_timestamps_) {
    std::string timestamp =
        absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone());
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]",
                       timestamp.c_str());
    ImGui::SameLine();
  }

  // Name/Icon
  if (is_user) {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s You",
                       ICON_MD_PERSON);
  } else {
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s Agent",
                       ICON_MD_SMART_TOY);
  }

  // Message Bubble
  ImVec4 bg_col = is_user ? ImVec4(0.2f, 0.2f, 0.25f, 1.0f)
                          : ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_col);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

  std::string content_id = "msg_content_" + std::to_string(index);
  if (ImGui::BeginChild(content_id.c_str(), ImVec2(wrap_width, 0), true,
                        ImGuiWindowFlags_AlwaysUseWindowPadding)) {
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
  if (dots == 0)
    ImGui::Text(".");
  else if (dots == 1)
    ImGui::Text("..");
  else if (dots == 2)
    ImGui::Text("...");

  ImGui::Unindent(10);
}

void AgentChat::RenderInputBox(float height) {
  const auto& theme = AgentUI::GetTheme();
  if (ImGui::BeginChild("ChatInput", ImVec2(0, height), false,
                        ImGuiWindowFlags_NoScrollbar)) {
    ImGui::Separator();

    // Input flags
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CtrlEnterForNewLine;

    float button_row_height = ImGui::GetFrameHeightWithSpacing();
    float input_height =
        std::max(48.0f,
                 ImGui::GetContentRegionAvail().y - button_row_height - 6.0f);

    ImGui::PushItemWidth(-1);
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }

    bool submit = ImGui::InputTextMultiline(
        "##Input", input_buffer_, sizeof(input_buffer_),
        ImVec2(0, input_height), flags);

    bool clicked_send = false;
    ImGui::PushStyleColor(ImGuiCol_Button, theme.accent_color);
    clicked_send = ImGui::Button(ICON_MD_SEND " Send", ImVec2(90, 0));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DELETE_FOREVER " Clear")) {
      input_buffer_[0] = '\0';
    }

    if (submit || clicked_send) {
      std::string msg(input_buffer_);
      while (!msg.empty() &&
             std::isspace(static_cast<unsigned char>(msg.back()))) {
        msg.pop_back();
      }

      if (!msg.empty()) {
        SendMessage(msg);
        input_buffer_[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
      }
    }

    ImGui::PopItemWidth();
  }
  ImGui::EndChild();
}

void AgentChat::RenderProposalQuickActions(const cli::agent::ChatMessage& msg,
                                           int index) {
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

void AgentChat::RenderCodeBlock(const std::string& code,
                                const std::string& language, int msg_index) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
  if (ImGui::BeginChild(absl::StrCat("code_", msg_index).c_str(), ImVec2(0, 0),
                        true, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (!language.empty()) {
      ImGui::TextDisabled("%s", language.c_str());
      ImGui::SameLine();
    }
    if (ImGui::Button(ICON_MD_CONTENT_COPY)) {
      ImGui::SetClipboardText(code.c_str());
      if (toast_manager_)
        toast_manager_->Show("Code copied", ToastType::kSuccess);
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

std::vector<AgentChat::ContentBlock> AgentChat::ParseMessageContent(
    const std::string& content) {
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
      blocks.push_back({ContentBlock::Type::kText,
                        content.substr(pos, code_start - pos), ""});
    }

    size_t code_end = content.find("```", code_start + 3);
    if (code_end == std::string::npos) {
      // Malformed, treat as text
      blocks.push_back(
          {ContentBlock::Type::kText, content.substr(code_start), ""});
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

void AgentChat::RenderTableData(
    const cli::agent::ChatMessage::TableData& table) {
  if (table.headers.empty()) {
    return;
  }

  // Render table
  if (ImGui::BeginTable("ToolResultTable",
                        static_cast<int>(table.headers.size()),
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
      for (size_t col = 0; col < std::min(row.size(), table.headers.size());
           ++col) {
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
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                        ImVec4(0.2f, 0.2f, 0.25f, 1.0f));

  std::string header =
      absl::StrFormat("%s Tools (%d calls, %.2fs)", ICON_MD_BUILD_CIRCLE,
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
  auto snapshot_or = AgentChatHistoryCodec::Load(filepath);
  if (!snapshot_or.ok()) {
    return snapshot_or.status();
  }
  const auto& snapshot = snapshot_or.value();
  agent_service_.ReplaceHistory(snapshot.history);
  history_loaded_ = true;
  scroll_to_bottom_ = true;
  return absl::OkStatus();
#else
  return absl::UnimplementedError("JSON support not available");
#endif
}

absl::Status AgentChat::SaveHistory(const std::string& filepath) {
#ifdef YAZE_WITH_JSON
  AgentChatHistoryCodec::Snapshot snapshot;
  snapshot.history = agent_service_.GetHistory();

  std::filesystem::path path(filepath);
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return absl::InternalError(absl::StrFormat(
          "Failed to create history directory: %s", ec.message()));
    }
  }

  return AgentChatHistoryCodec::Save(path, snapshot);
#else
  return absl::UnimplementedError("JSON support not available");
#endif
}

}  // namespace editor
}  // namespace yaze
