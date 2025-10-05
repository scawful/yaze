#include "app/editor/system/agent_chat_history_popup.h"

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "imgui/imgui.h"
#include "app/gui/icons.h"
#include "app/editor/system/toast_manager.h"

namespace yaze {
namespace editor {

namespace {

const ImVec4 kUserColor = ImVec4(0.88f, 0.76f, 0.36f, 1.0f);
const ImVec4 kAgentColor = ImVec4(0.56f, 0.82f, 0.62f, 1.0f);
const ImVec4 kTimestampColor = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

}  // namespace

AgentChatHistoryPopup::AgentChatHistoryPopup() {}

void AgentChatHistoryPopup::Draw() {
  if (!visible_) return;

  // Set drawer position on the LEFT side
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(drawer_width_, io.DisplaySize.y), 
                           ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | 
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoCollapse;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.95f));
  
  if (ImGui::Begin(ICON_MD_CHAT " Chat History", &visible_, flags)) {
    // Header with controls
    if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Full Chat")) {
      if (open_chat_callback_) {
        open_chat_callback_();
      }
    }
    
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_REFRESH " Refresh")) {
      // Trigger external refresh through callback
      if (toast_manager_) {
        toast_manager_->Show("Refreshing chat history...", ToastType::kInfo, 1.5f);
      }
    }
    
    ImGui::Separator();
    
    // Filter dropdown
    const char* filter_labels[] = {"All Messages", "User Only", "Agent Only"};
    int current_filter = static_cast<int>(message_filter_);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##filter", &current_filter, filter_labels, 3)) {
      message_filter_ = static_cast<MessageFilter>(current_filter);
    }
    
    ImGui::Spacing();
    
    // Auto-scroll checkbox
    ImGui::Checkbox("Auto-scroll", &auto_scroll_);
    
    ImGui::Separator();
    
    // Message count indicator
    int visible_count = 0;
    for (const auto& msg : messages_) {
      if (msg.is_internal) continue;
      if (message_filter_ == MessageFilter::kUserOnly && 
          msg.sender != cli::agent::ChatMessage::Sender::kUser) continue;
      if (message_filter_ == MessageFilter::kAgentOnly && 
          msg.sender != cli::agent::ChatMessage::Sender::kAgent) continue;
      visible_count++;
    }
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                      "%d message%s", visible_count, visible_count == 1 ? "" : "s");
    
    ImGui::Separator();
    
    // Message list
    ImGui::BeginChild("MessageList", ImVec2(0, -45), true);
    DrawMessageList();
    
    if (needs_scroll_) {
      ImGui::SetScrollHereY(1.0f);
      needs_scroll_ = false;
    }
    
    ImGui::EndChild();
    
    // Action buttons at bottom
    ImGui::Separator();
    DrawActionButtons();
  }
  ImGui::End();
  
  ImGui::PopStyleColor();
}

void AgentChatHistoryPopup::DrawMessageList() {
  if (messages_.empty()) {
    ImGui::TextDisabled("No messages yet. Start a conversation in the chat window.");
    return;
  }
  
  // Calculate starting index for display limit
  int start_index = messages_.size() > display_limit_ ? 
                    messages_.size() - display_limit_ : 0;
  
  for (int i = start_index; i < messages_.size(); ++i) {
    const auto& msg = messages_[i];
    
    // Skip internal messages
    if (msg.is_internal) continue;
    
    // Apply filter
    if (message_filter_ == MessageFilter::kUserOnly && 
        msg.sender != cli::agent::ChatMessage::Sender::kUser) continue;
    if (message_filter_ == MessageFilter::kAgentOnly && 
        msg.sender != cli::agent::ChatMessage::Sender::kAgent) continue;
    
    DrawMessage(msg, i);
  }
}

void AgentChatHistoryPopup::DrawMessage(const cli::agent::ChatMessage& msg, int index) {
  ImGui::PushID(index);
  
  bool from_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);
  ImVec4 header_color = from_user ? kUserColor : kAgentColor;
  const char* sender_label = from_user ? ICON_MD_PERSON " You" : ICON_MD_SMART_TOY " Agent";
  
  // Message header with sender and timestamp
  ImGui::PushStyleColor(ImGuiCol_Text, header_color);
  ImGui::Text("%s", sender_label);
  ImGui::PopStyleColor();
  
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, kTimestampColor);
  ImGui::Text("%s", absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone()).c_str());
  ImGui::PopStyleColor();
  
  // Message content
  ImGui::Indent(10.0f);
  
  if (msg.table_data.has_value()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), ICON_MD_TABLE_CHART " [Table Data]");
  } else if (msg.json_pretty.has_value()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), ICON_MD_DATA_OBJECT " [Structured Response]");
  } else {
    // Truncate long messages
    std::string content = msg.message;
    if (content.length() > 200) {
      content = content.substr(0, 197) + "...";
    }
    ImGui::TextWrapped("%s", content.c_str());
  }
  
  // Show proposal indicator if present
  if (msg.proposal.has_value()) {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), 
                      ICON_MD_PREVIEW " Proposal: %s", msg.proposal->id.c_str());
  }
  
  ImGui::Unindent(10.0f);
  ImGui::Spacing();
  ImGui::Separator();
  
  ImGui::PopID();
}

void AgentChatHistoryPopup::DrawActionButtons() {
  if (ImGui::Button(ICON_MD_DELETE " Clear History", ImVec2(-1, 0))) {
    ClearHistory();
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear all messages from the popup view\n(Full history preserved in chat window)");
  }
}

void AgentChatHistoryPopup::UpdateHistory(const std::vector<cli::agent::ChatMessage>& history) {
  bool had_messages = !messages_.empty();
  int old_size = messages_.size();
  
  messages_ = history;
  
  // Auto-scroll if new messages arrived
  if (auto_scroll_ && messages_.size() > old_size) {
    needs_scroll_ = true;
  }
}

void AgentChatHistoryPopup::NotifyNewMessage() {
  if (auto_scroll_) {
    needs_scroll_ = true;
  }
  
  // Flash the window to draw attention
  if (toast_manager_ && !visible_) {
    toast_manager_->Show(ICON_MD_CHAT " New message received", ToastType::kInfo, 2.0f);
  }
}

void AgentChatHistoryPopup::ClearHistory() {
  messages_.clear();
  
  if (toast_manager_) {
    toast_manager_->Show("Chat history popup cleared", ToastType::kInfo, 2.0f);
  }
}

void AgentChatHistoryPopup::ExportHistory() {
  // TODO: Implement export functionality
  if (toast_manager_) {
    toast_manager_->Show("Export feature coming soon", ToastType::kInfo, 2.0f);
  }
}

void AgentChatHistoryPopup::ScrollToBottom() {
  needs_scroll_ = true;
}

}  // namespace editor
}  // namespace yaze
