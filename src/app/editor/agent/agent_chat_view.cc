#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/agent/agent_chat_view.h"

#include <cmath>
#include <cstring>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

void RenderTable(const cli::agent::ChatMessage::TableData& table_data) {
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

AgentChatView::AgentChatView() { memset(input_buffer_, 0, sizeof(input_buffer_)); }

void AgentChatView::SetContext(AgentUIContext* context) { context_ = context; }

void AgentChatView::SetToastManager(ToastManager* toast_manager) {
  toast_manager_ = toast_manager;
}

void AgentChatView::SetChatCallbacks(const ChatCallbacks& callbacks) {
  chat_callbacks_ = callbacks;
}

void AgentChatView::SetProposalCallbacks(const ProposalCallbacks& callbacks) {
  proposal_callbacks_ = callbacks;
}

void AgentChatView::SetAgentService(
    cli::agent::ConversationalAgentService* service) {
  agent_service_ = service;
}

void AgentChatView::Draw(float available_height) {
  if (!context_) {
    ImGui::TextDisabled("No context set");
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  float input_height = compact_mode_ ? 60.0f : 80.0f;

  // Calculate history height
  float history_height = available_height > 0
                             ? available_height - input_height - 8.0f
                             : ImGui::GetContentRegionAvail().y - input_height;

  // History region
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_darker);
  ImGui::BeginChild("ChatHistory", ImVec2(0, history_height), false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  DrawHistory();

  // Smooth scroll-to-bottom animation
  if (scroll_to_bottom_) {
    scroll_target_ = ImGui::GetScrollMaxY();
    scroll_to_bottom_ = false;
  }

  if (scroll_target_ >= 0.0f) {
    float current = ImGui::GetScrollY();
    float target = scroll_target_;

    // Smooth interpolation (ease-out)
    float delta = target - current;
    float speed = 15.0f * ImGui::GetIO().DeltaTime;

    if (std::abs(delta) < 1.0f) {
      // Close enough, snap to target
      ImGui::SetScrollY(target);
      scroll_target_ = -1.0f;
    } else {
      // Smooth scroll
      float new_scroll = current + delta * std::min(speed, 1.0f);
      ImGui::SetScrollY(new_scroll);
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();

  // Input region
  DrawInputBox();
}

void AgentChatView::DrawHistory() {
  if (!agent_service_) {
    ImGui::TextDisabled("Agent service not connected");
    return;
  }

  const auto& history = agent_service_->GetHistory();

  if (history.empty()) {
    const auto& theme = AgentUI::GetTheme();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_secondary_color);
    ImGui::TextWrapped("Start a conversation by typing a message below.");
    ImGui::PopStyleColor();
    return;
  }

  int index = 0;
  for (const auto& msg : history) {
    RenderMessage(msg, index++);
  }

  // Show thinking indicator if waiting for response
  if (context_ && context_->chat_state().waiting_for_response) {
    RenderThinkingIndicator();
  }
}

void AgentChatView::DrawInputBox() {
  const auto& theme = AgentUI::GetTheme();
  auto& chat_state = context_ ? context_->chat_state() : *(ChatState*)nullptr;
  bool is_waiting = context_ && chat_state.waiting_for_response;

  ImGui::Spacing();

  // Input row
  float button_width = compact_mode_ ? 60.0f : 80.0f;
  float input_width =
      ImGui::GetContentRegionAvail().x - button_width - 8.0f;

  ImGui::PushItemWidth(input_width);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.panel_bg_color);

  ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
  if (is_waiting) {
    flags |= ImGuiInputTextFlags_ReadOnly;
  }

  bool enter_pressed =
      ImGui::InputText("##chat_input", input_buffer_, sizeof(input_buffer_),
                       flags);

  ImGui::PopStyleColor();
  ImGui::PopItemWidth();

  ImGui::SameLine();

  // Send button
  ImGui::BeginDisabled(is_waiting || strlen(input_buffer_) == 0);
  if (ImGui::Button(compact_mode_ ? ICON_MD_SEND : ICON_MD_SEND " Send",
                    ImVec2(button_width, 0)) ||
      enter_pressed) {
    if (strlen(input_buffer_) > 0) {
      SendMessage(input_buffer_);
      memset(input_buffer_, 0, sizeof(input_buffer_));
    }
  }
  ImGui::EndDisabled();

  // Hint text
  if (!compact_mode_) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_secondary_color);
    ImGui::TextUnformatted("Press Enter to send, Shift+Enter for newline");
    ImGui::PopStyleColor();
  }
}

void AgentChatView::RenderMessage(const cli::agent::ChatMessage& msg,
                                  int index) {
  // Skip internal messages
  if (msg.is_internal) {
    return;
  }

  ImGui::PushID(index);
  const auto& theme = AgentUI::GetTheme();

  const bool from_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);

  // Message bubble styling
  float window_width = ImGui::GetContentRegionAvail().x;
  float bubble_max_width = window_width * (compact_mode_ ? 0.95f : 0.80f);

  // Align user messages to right, agent to left
  if (from_user) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_width - bubble_max_width) - 20.0f);
  } else {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
  }

  // More distinct colors with higher opacity for better readability
  ImVec4 bg_color = from_user ? ImVec4(0.15f, 0.35f, 0.75f, 0.3f)
                              : ImVec4(0.25f, 0.35f, 0.25f, 0.3f);
  ImVec4 border_color = from_user ? ImVec4(0.3f, 0.5f, 0.9f, 0.6f)
                                  : ImVec4(0.4f, 0.6f, 0.4f, 0.6f);

  // Using Group + Rect approach for dynamic height bubbles
  ImGui::BeginGroup();

  // Header (Sender + Time)
  const ImVec4 header_color =
      from_user ? theme.user_message_color : theme.agent_message_color;
  
  // User friendly name
  std::string sender_name = from_user ? "You" : (msg.model_metadata.has_value() ? msg.model_metadata->model : "Agent");
  if (!from_user && sender_name.empty()) sender_name = "Agent";

  ImGui::TextColored(header_color, "%s", sender_name.c_str());
  
  ImGui::SameLine();
  ImGui::TextDisabled(
      " %s",
      absl::FormatTime("%H:%M", msg.timestamp, absl::LocalTimeZone()).c_str());

  // Copy button (small and subtle) - moved to right of header
  if (!compact_mode_) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY)) {
      std::string copy_text = msg.message;
      if (copy_text.empty() && msg.json_pretty.has_value()) {
        copy_text = *msg.json_pretty;
      }
      ImGui::SetClipboardText(copy_text.c_str());
      if (toast_manager_) {
        toast_manager_->Show("Copied", ToastType::kSuccess, 1.0f);
      }
    }
    ImGui::PopStyleColor();
  }

  // Separator for cleanliness
  ImGui::Spacing();

  // Content
  if (msg.table_data.has_value()) {
    RenderTable(*msg.table_data);
  } else if (msg.json_pretty.has_value()) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme.json_text_color);
    ImGui::TextDisabled(ICON_MD_DATA_OBJECT " (Structured Data)");
    ImGui::TextWrapped("%s", msg.json_pretty->c_str());
    ImGui::PopStyleColor();
  } else {
    // Parse message for code blocks
    auto blocks = ParseMessageContent(msg.message);
    int code_block_index = 0;
    for (const auto& block : blocks) {
      if (block.type == ContentBlock::Type::kCode) {
        RenderCodeBlock(block.content, block.language, index * 100 + code_block_index++);
      } else {
        ImGui::TextWrapped("%s", block.content.c_str());
      }
    }
  }

  // Proposal quick actions
  if (msg.proposal.has_value()) {
    ImGui::Spacing();
    RenderProposalQuickActions(msg, index);
  }

  ImGui::EndGroup();

  // Draw background rect with rounded corners
  ImVec2 p_min = ImGui::GetItemRectMin();
  ImVec2 p_max = ImGui::GetItemRectMax();
  p_min.x -= 12.0f;
  p_min.y -= 8.0f;
  p_max.x += 12.0f;
  p_max.y += 8.0f;

  ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max,
                                            ImGui::GetColorU32(bg_color), 12.0f);
  ImGui::GetWindowDrawList()->AddRect(p_min, p_max,
                                      ImGui::GetColorU32(border_color), 12.0f);

  // Extra spacing between messages
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::PopID();
}

void AgentChatView::RenderProposalQuickActions(
    const cli::agent::ChatMessage& msg, int index) {
  if (!msg.proposal.has_value()) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  const auto& proposal = *msg.proposal;

  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.proposal_panel_bg);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

  float panel_height = compact_mode_ ? ImGui::GetFrameHeight() * 2.0f
                                     : ImGui::GetFrameHeight() * 3.2f;
  ImGui::BeginChild(absl::StrFormat("proposal_panel_%d", index).c_str(),
                    ImVec2(0, panel_height), true, ImGuiWindowFlags_None);

  ImGui::TextColored(theme.proposal_accent, "%s Proposal %s", ICON_MD_PREVIEW,
                     proposal.id.c_str());

  if (!compact_mode_) {
    ImGui::Text("Changes: %d", proposal.change_count);
    ImGui::Text("Commands: %d", proposal.executed_commands);
  } else {
    ImGui::SameLine();
    ImGui::Text("(%d changes)", proposal.change_count);
  }

  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f);

  if (ImGui::SmallButton(ICON_MD_VISIBILITY " Review")) {
    if (proposal_callbacks_.focus_proposal) {
      proposal_callbacks_.focus_proposal(proposal.id);
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

void AgentChatView::RenderThinkingIndicator() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::Spacing();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

  ImGui::BeginGroup();

  // Smooth pulse animation
  float time = (float)ImGui::GetTime();
  thinking_pulse_ = 0.5f + 0.5f * sinf(time * 3.0f);

  // Animated pulsing dots
  ImGui::TextColored(theme.agent_message_color, ICON_MD_PSYCHOLOGY);
  ImGui::SameLine();

  // Three dots with staggered animations
  for (int i = 0; i < 3; ++i) {
    float dot_phase = time * 4.0f + i * 0.8f;
    float dot_alpha = 0.3f + 0.7f * (0.5f + 0.5f * sinf(dot_phase));
    ImVec4 dot_color = theme.agent_message_color;
    dot_color.w = dot_alpha;

    ImGui::SameLine(0, 2.0f);
    ImGui::TextColored(dot_color, ICON_MD_CIRCLE);
  }

  ImGui::SameLine();
  ImGui::TextColored(ImVec4(theme.text_secondary_color.x,
                            theme.text_secondary_color.y,
                            theme.text_secondary_color.z,
                            0.5f + 0.5f * thinking_pulse_),
                     " thinking...");

  ImGui::EndGroup();

  // Animated background with subtle pulse
  ImVec2 p_min = ImGui::GetItemRectMin();
  ImVec2 p_max = ImGui::GetItemRectMax();
  p_min.x -= 8;
  p_min.y -= 4;
  p_max.x += 8;
  p_max.y += 4;

  float bg_alpha = 0.15f + 0.05f * thinking_pulse_;
  ImGui::GetWindowDrawList()->AddRectFilled(
      p_min, p_max, ImGui::GetColorU32(ImVec4(0.3f, 0.4f, 0.5f, bg_alpha)), 8.0f);

  // Subtle border pulse
  ImGui::GetWindowDrawList()->AddRect(
      p_min, p_max,
      ImGui::GetColorU32(ImVec4(0.4f, 0.5f, 0.7f, 0.2f + 0.1f * thinking_pulse_)),
      8.0f);
}

void AgentChatView::SendMessage(const std::string& message) {
  if (!context_ || !agent_service_) {
    return;
  }

  auto& chat_state = context_->chat_state();
  chat_state.waiting_for_response = true;
  chat_state.pending_message = message;

  // Send through agent service
  auto response = agent_service_->SendMessage(message);
  HandleAgentResponse(response);

  chat_state.waiting_for_response = false;
  chat_state.pending_message.clear();

  // Persist history if callback is set
  if (chat_callbacks_.persist_history) {
    chat_callbacks_.persist_history();
  }

  scroll_to_bottom_ = true;
  context_->NotifyChanged();
}

void AgentChatView::HandleAgentResponse(
    const absl::StatusOr<cli::agent::ChatMessage>& response) {
  if (!response.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Agent error: %s", response.status().message()),
          ToastType::kError, 5.0f);
    }
    return;
  }

  const auto& message = response.value();
  int total = CountKnownProposals();
  if (message.metrics.has_value()) {
    total = std::max(total, message.metrics->total_proposals);
  }

  if (context_) {
    auto& proposal_state = context_->proposal_state();
    if (total > proposal_state.total_proposals) {
      // New proposal created
      if (toast_manager_ && message.proposal.has_value()) {
        const auto& proposal = *message.proposal;
        toast_manager_->Show(
            absl::StrFormat("%s Proposal %s ready (%d change%s)", ICON_MD_PREVIEW,
                            proposal.id, proposal.change_count,
                            proposal.change_count == 1 ? "" : "s"),
            ToastType::kSuccess, 5.5f);

        if (proposal_callbacks_.focus_proposal) {
          proposal_callbacks_.focus_proposal(proposal.id);
        }
      }
    }
    proposal_state.total_proposals = total;
  }
}

int AgentChatView::CountKnownProposals() const {
  if (!agent_service_) {
    return 0;
  }

  int total = 0;
  const auto& history = agent_service_->GetHistory();
  for (const auto& message : history) {
    if (message.metrics.has_value()) {
      total = std::max(total, message.metrics->total_proposals);
    } else if (message.proposal.has_value()) {
      ++total;
    }
  }
  return total;
}

std::vector<AgentChatView::ContentBlock> AgentChatView::ParseMessageContent(
    const std::string& content) {
  std::vector<ContentBlock> blocks;

  // Parse markdown-style code blocks: ```language\ncode\n```
  size_t pos = 0;
  while (pos < content.size()) {
    // Look for code block start
    size_t code_start = content.find("```", pos);
    if (code_start == std::string::npos) {
      // No more code blocks, add remaining text
      if (pos < content.size()) {
        ContentBlock block;
        block.type = ContentBlock::Type::kText;
        block.content = content.substr(pos);
        if (!block.content.empty()) {
          blocks.push_back(block);
        }
      }
      break;
    }

    // Add text before code block
    if (code_start > pos) {
      ContentBlock block;
      block.type = ContentBlock::Type::kText;
      block.content = content.substr(pos, code_start - pos);
      if (!block.content.empty()) {
        blocks.push_back(block);
      }
    }

    // Find language (until newline)
    size_t lang_start = code_start + 3;
    size_t lang_end = content.find('\n', lang_start);
    if (lang_end == std::string::npos) {
      lang_end = content.size();
    }

    std::string language = content.substr(lang_start, lang_end - lang_start);

    // Find code block end
    size_t code_content_start = lang_end + 1;
    size_t code_end = content.find("```", code_content_start);
    if (code_end == std::string::npos) {
      code_end = content.size();
    }

    // Extract code content
    ContentBlock block;
    block.type = ContentBlock::Type::kCode;
    block.language = language;
    block.content = content.substr(code_content_start, code_end - code_content_start);

    // Trim trailing newline if present
    if (!block.content.empty() && block.content.back() == '\n') {
      block.content.pop_back();
    }

    blocks.push_back(block);

    pos = (code_end != std::string::npos) ? code_end + 3 : content.size();
  }

  return blocks;
}

void AgentChatView::RenderCodeBlock(const std::string& code,
                                    const std::string& language,
                                    int msg_index) {
  const auto& theme = AgentUI::GetTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

  // Calculate height based on line count (max 10 visible lines)
  int line_count = 1;
  for (char c : code) {
    if (c == '\n') ++line_count;
  }
  float line_height = ImGui::GetTextLineHeightWithSpacing();
  float code_height = std::min(line_count, 10) * line_height + 8.0f;

  std::string child_id = absl::StrFormat("code_block_%d", msg_index);
  ImGui::BeginChild(child_id.c_str(), ImVec2(0, code_height + 24.0f), true,
                    ImGuiWindowFlags_None);

  // Header with language and copy button
  ImGui::PushStyleColor(ImGuiCol_Text, theme.text_secondary_color);
  if (!language.empty()) {
    ImGui::Text("%s %s", ICON_MD_CODE, language.c_str());
  } else {
    ImGui::Text("%s code", ICON_MD_CODE);
  }
  ImGui::PopStyleColor();

  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);

  // Copy button
  if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Copy")) {
    ImGui::SetClipboardText(code.c_str());
    if (toast_manager_) {
      toast_manager_->Show("Code copied!", ToastType::kSuccess, 1.5f);
    }
  }

  ImGui::Separator();

  // Code content with monospace-style rendering
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.85f, 1.0f));

  // Simple syntax highlighting hints based on language
  if (language == "cpp" || language == "c" || language == "cc" ||
      language == "h" || language == "hpp") {
    // C++ style - highlight keywords with different color
    ImGui::TextUnformatted(code.c_str());
  } else if (language == "json") {
    // JSON - could colorize keys/values
    ImGui::TextUnformatted(code.c_str());
  } else if (language == "asm" || language == "65816") {
    // Assembly - highlight opcodes
    ImGui::TextUnformatted(code.c_str());
  } else {
    // Default - plain text
    ImGui::TextUnformatted(code.c_str());
  }

  ImGui::PopStyleColor();

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

}  // namespace editor
}  // namespace yaze
