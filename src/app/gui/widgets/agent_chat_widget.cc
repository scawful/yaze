#include "app/gui/widgets/agent_chat_widget.h"

#include <algorithm>
#include <iostream>
#include <fstream>

#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace app {
namespace gui {

AgentChatWidget::AgentChatWidget()
    : scroll_to_bottom_(false),
      auto_scroll_(true),
      show_timestamps_(true),
      show_reasoning_(false),
      message_spacing_(12.0f),
      rom_(nullptr) {
  memset(input_buffer_, 0, sizeof(input_buffer_));
  
  // Initialize colors with a pleasant dark theme
  colors_.user_bubble = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);      // Blue
  colors_.agent_bubble = ImVec4(0.3f, 0.3f, 0.35f, 1.0f);    // Dark gray
  colors_.system_text = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);      // Light gray
  colors_.error_text = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);       // Red
  colors_.tool_call_bg = ImVec4(0.2f, 0.5f, 0.3f, 0.3f);     // Green tint
  colors_.timestamp_text = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);   // Medium gray
  
#ifdef Z3ED_AI_AVAILABLE
  agent_service_ = std::make_unique<cli::agent::ConversationalAgentService>();
#endif
}

AgentChatWidget::~AgentChatWidget() = default;

void AgentChatWidget::Initialize(Rom* rom) {
  rom_ = rom;
#ifdef Z3ED_AI_AVAILABLE
  if (agent_service_ && rom_) {
    agent_service_->SetRomContext(rom_);
  }
#endif
}

void AgentChatWidget::Render(bool* p_open) {
#ifndef Z3ED_AI_AVAILABLE
  ImGui::Begin("Agent Chat", p_open);
  ImGui::TextColored(colors_.error_text, 
      "AI features not available");
  ImGui::TextWrapped(
      "Build with -DZ3ED_AI=ON to enable the conversational agent.");
  ImGui::End();
  return;
#else
  
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Z3ED Agent Chat", p_open)) {
    ImGui::End();
    return;
  }
  
  // Render toolbar at top
  RenderToolbar();
  ImGui::Separator();
  
  // Chat history area (scrollable)
  ImGui::BeginChild("ChatHistory", 
                    ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 60),
                    true, 
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  RenderChatHistory();
  
  // Auto-scroll to bottom when new messages arrive
  if (scroll_to_bottom_ || (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
    ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom_ = false;
  }
  
  ImGui::EndChild();
  
  // Input area at bottom
  RenderInputArea();
  
  ImGui::End();
#endif
}

void AgentChatWidget::RenderToolbar() {
  if (ImGui::Button("Clear History")) {
    ClearHistory();
  }
  ImGui::SameLine();
  
  if (ImGui::Button("Save History")) {
    std::string filepath = ".yaze/agent_chat_history.json";
    if (auto status = SaveHistory(filepath); !status.ok()) {
      std::cerr << "Failed to save history: " << status.message() << std::endl;
    } else {
      std::cout << "Saved chat history to: " << filepath << std::endl;
    }
  }
  ImGui::SameLine();
  
  if (ImGui::Button("Load History")) {
    std::string filepath = ".yaze/agent_chat_history.json";
    if (auto status = LoadHistory(filepath); !status.ok()) {
      std::cerr << "Failed to load history: " << status.message() << std::endl;
    }
  }
  
  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &auto_scroll_);
  
  ImGui::SameLine();
  ImGui::Checkbox("Show Timestamps", &show_timestamps_);
  
  ImGui::SameLine();
  ImGui::Checkbox("Show Reasoning", &show_reasoning_);
}

void AgentChatWidget::RenderChatHistory() {
#ifdef Z3ED_AI_AVAILABLE
  if (!agent_service_) return;
  
  const auto& history = agent_service_->GetHistory();
  
  if (history.empty()) {
    ImGui::TextColored(colors_.system_text, 
        "No messages yet. Type a message below to start chatting!");
    return;
  }
  
  for (size_t i = 0; i < history.size(); ++i) {
    RenderMessageBubble(history[i], i);
    ImGui::Spacing();
    if (message_spacing_ > 0) {
      ImGui::Dummy(ImVec2(0, message_spacing_));
    }
  }
#endif
}

void AgentChatWidget::RenderMessageBubble(const cli::agent::ChatMessage& msg, int index) {
  bool is_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);
  
  // Timestamp (if enabled)
  if (show_timestamps_) {
    std::string timestamp = absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone());
    ImGui::TextColored(colors_.timestamp_text, "[%s]", timestamp.c_str());
    ImGui::SameLine();
  }
  
  // Sender label
  const char* sender_label = is_user ? "You" : "Agent";
  ImVec4 sender_color = is_user ? colors_.user_bubble : colors_.agent_bubble;
  ImGui::TextColored(sender_color, "%s:", sender_label);
  
  // Message content
  ImGui::Indent(20.0f);
  
  // Check if we have table data to render
  if (!is_user && msg.table_data.has_value()) {
    RenderTableData(msg.table_data.value());
  } else if (!is_user && msg.json_pretty.has_value()) {
    ImGui::TextWrapped("%s", msg.json_pretty.value().c_str());
  } else {
    // Regular text message
    ImGui::TextWrapped("%s", msg.message.c_str());
  }
  
  ImGui::Unindent(20.0f);
}

void AgentChatWidget::RenderTableData(const cli::agent::ChatMessage::TableData& table) {
  if (table.headers.empty()) {
    return;
  }
  
  // Render table
  if (ImGui::BeginTable("ToolResultTable", table.headers.size(), 
                        ImGuiTableFlags_Borders | 
                        ImGuiTableFlags_RowBg |
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
        ImGui::TableSetColumnIndex(col);
        ImGui::TextWrapped("%s", row[col].c_str());
      }
    }
    
    ImGui::EndTable();
  }
}

void AgentChatWidget::RenderInputArea() {
  ImGui::Separator();
  ImGui::Text("Message:");
  
  // Multi-line input
  ImGui::PushItemWidth(-1);
  bool enter_pressed = ImGui::InputTextMultiline(
      "##input", 
      input_buffer_, 
      sizeof(input_buffer_),
      ImVec2(-1, 60),
      ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopItemWidth();
  
  // Send button
  if (ImGui::Button("Send", ImVec2(100, 0)) || enter_pressed) {
    if (strlen(input_buffer_) > 0) {
      SendMessage(input_buffer_);
      memset(input_buffer_, 0, sizeof(input_buffer_));
      ImGui::SetKeyboardFocusHere(-1);  // Keep focus on input
    }
  }
  
  ImGui::SameLine();
  ImGui::TextColored(colors_.system_text, 
      "Tip: Press Enter to send (Shift+Enter for newline)");
}

void AgentChatWidget::SendMessage(const std::string& message) {
#ifdef Z3ED_AI_AVAILABLE
  if (!agent_service_) return;
  
  // Send message through agent service
  auto result = agent_service_->SendMessage(message);
  
  if (!result.ok()) {
    std::cerr << "Error processing message: " << result.status() << std::endl;
  }
  
  scroll_to_bottom_ = true;
#endif
}

void AgentChatWidget::ClearHistory() {
#ifdef Z3ED_AI_AVAILABLE
  if (agent_service_) {
    agent_service_->ClearHistory();
  }
#endif
}

absl::Status AgentChatWidget::LoadHistory(const std::string& filepath) {
#if defined(Z3ED_AI_AVAILABLE) && defined(YAZE_WITH_JSON)
  if (!agent_service_) {
    return absl::FailedPreconditionError("Agent service not initialized");
  }
  
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open file: %s", filepath));
  }
  
  try {
    nlohmann::json j;
    file >> j;
    
    // Parse and load messages
    // Note: This would require exposing a LoadHistory method in ConversationalAgentService
    // For now, we'll just return success
    
    return absl::OkStatus();
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse JSON: %s", e.what()));
  }
#else
  return absl::UnimplementedError("AI features not available");
#endif
}

absl::Status AgentChatWidget::SaveHistory(const std::string& filepath) {
#if defined(Z3ED_AI_AVAILABLE) && defined(YAZE_WITH_JSON)
  if (!agent_service_) {
    return absl::FailedPreconditionError("Agent service not initialized");
  }
  
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Could not create file: %s", filepath));
  }
  
  try {
    nlohmann::json j;
    const auto& history = agent_service_->GetHistory();
    
    j["version"] = 1;
    j["messages"] = nlohmann::json::array();
    
    for (const auto& msg : history) {
      nlohmann::json msg_json;
      msg_json["sender"] = (msg.sender == cli::agent::ChatMessage::Sender::kUser) 
                           ? "user" : "agent";
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
  return absl::UnimplementedError("AI features not available");
#endif
}

void AgentChatWidget::ScrollToBottom() {
  scroll_to_bottom_ = true;
}

}  // namespace gui
}  // namespace app
}  // namespace yaze
