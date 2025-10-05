#include "app/editor/agent/agent_chat_history_popup.h"

#include <cstring>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {

AgentChatHistoryPopup::AgentChatHistoryPopup() {
  std::memset(input_buffer_, 0, sizeof(input_buffer_));
}

void AgentChatHistoryPopup::Draw() {
  if (!visible_) return;

  const auto& theme = AgentUI::GetTheme();
  
  // Set drawer position on the LEFT side (full height)
  ImGuiIO& io = ImGui::GetIO();
  
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(drawer_width_, io.DisplaySize.y), ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | 
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoTitleBar;

  // Use current theme colors
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
  
  if (ImGui::Begin("##AgentChatPopup", &visible_, flags)) {
    DrawHeader();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Calculate proper list height
    float list_height = ImGui::GetContentRegionAvail().y - 220.0f;
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.code_bg_color);
    ImGui::BeginChild("MessageList", ImVec2(0, list_height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawMessageList();
    
    if (needs_scroll_) {
      ImGui::SetScrollHereY(1.0f);
      needs_scroll_ = false;
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
    
    // Quick actions bar
    if (show_quick_actions_) {
      DrawQuickActions();
      ImGui::Spacing();
    }
    
    // Input section at bottom
    DrawInputSection();
  }
  ImGui::End();
  
  ImGui::PopStyleVar(2);
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
  
  // Use theme colors with slight tint
  ImVec4 text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
  ImVec4 header_color = from_user 
      ? ImVec4(text_color.x * 1.2f, text_color.y * 0.9f, text_color.z * 0.5f, 1.0f)  // Gold tint
      : ImVec4(text_color.x * 0.7f, text_color.y * 1.0f, text_color.z * 0.9f, 1.0f); // Teal tint
  
  const char* sender_label = from_user ? ICON_MD_PERSON " You" : ICON_MD_SMART_TOY " Agent";
  
  // Message header
  ImGui::TextColored(header_color, "%s", sender_label);
  
  ImGui::SameLine();
  ImGui::TextDisabled("%s", absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone()).c_str());
  
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

void AgentChatHistoryPopup::DrawHeader() {
  // Theme-matched header with subtle gradient
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 header_start = ImGui::GetCursorScreenPos();
  ImVec2 header_size(ImGui::GetContentRegionAvail().x, 55);
  
  // Subtle gradient matching theme
  ImU32 color_top = ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
  ImU32 color_bottom = ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
  draw_list->AddRectFilledMultiColor(
      header_start,
      ImVec2(header_start.x + header_size.x, header_start.y + header_size.y),
      color_top, color_top, color_bottom, color_bottom);
  
  // Thin accent line (no pulse - matches theme better)
  ImU32 accent_color = ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Separator));
  draw_list->AddLine(
      ImVec2(header_start.x, header_start.y + header_size.y),
      ImVec2(header_start.x + header_size.x, header_start.y + header_size.y),
      accent_color, 1.5f);
  
  ImGui::Dummy(ImVec2(0, 8));
  
  // Title and provider dropdown (like connection header)
  ImGui::Text(ICON_MD_CHAT);
  ImGui::SameLine();
  
  // Model dropdown (compact)
  ImGui::SetNextItemWidth(120);
  static int provider_idx = 0;
  const char* providers[] = {"Mock", "Ollama", "Gemini"};
  ImGui::Combo("##popup_provider", &provider_idx, providers, 3);
  
  // Buttons properly spaced from right edge
  ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 75.0f);
  
  // Compact mode toggle
  if (ImGui::SmallButton(compact_mode_ ? ICON_MD_UNFOLD_MORE : ICON_MD_UNFOLD_LESS)) {
    compact_mode_ = !compact_mode_;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(compact_mode_ ? "Expand view" : "Compact view");
  }
  
  ImGui::SameLine();
  
  // Full chat button (closes popup when opened)
  if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW)) {
    if (open_chat_callback_) {
      open_chat_callback_();
      visible_ = false;  // Close popup when opening main chat
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Open full chat");
  }
  
  ImGui::SameLine();
  
  // Close button
  if (ImGui::SmallButton(ICON_MD_CLOSE)) {
    visible_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Close (Ctrl+H)");
  }
  
  // Message count with badge
  int visible_count = 0;
  for (const auto& msg : messages_) {
    if (msg.is_internal) continue;
    if (message_filter_ == MessageFilter::kUserOnly && 
        msg.sender != cli::agent::ChatMessage::Sender::kUser) continue;
    if (message_filter_ == MessageFilter::kAgentOnly && 
        msg.sender != cli::agent::ChatMessage::Sender::kAgent) continue;
    visible_count++;
  }
  
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 0.9f), 
                    "%d message%s", visible_count, visible_count == 1 ? "" : "s");
  
  ImGui::Dummy(ImVec2(0, 5));
}

void AgentChatHistoryPopup::DrawQuickActions() {
  // 4 buttons with narrower width
  float button_width = (ImGui::GetContentRegionAvail().x - 15) / 4.0f;
  
  // Multimodal snapshot button
  if (ImGui::Button(ICON_MD_CAMERA, ImVec2(button_width, 30))) {
    if (capture_snapshot_callback_) {
      capture_snapshot_callback_();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Capture screenshot");
  }
  
  ImGui::SameLine();
  
  // Filter button with icon indicator
  const char* filter_icons[] = {ICON_MD_FILTER_LIST, ICON_MD_PERSON, ICON_MD_SMART_TOY};
  int filter_idx = static_cast<int>(message_filter_);
  if (ImGui::Button(filter_icons[filter_idx], ImVec2(button_width, 30))) {
    ImGui::OpenPopup("FilterPopup");
  }
  if (ImGui::IsItemHovered()) {
    const char* filter_names[] = {"All", "User only", "Agent only"};
    ImGui::SetTooltip("Filter: %s", filter_names[filter_idx]);
  }
  
  // Filter popup
  if (ImGui::BeginPopup("FilterPopup")) {
    if (ImGui::Selectable(ICON_MD_FILTER_LIST " All Messages", message_filter_ == MessageFilter::kAll)) {
      message_filter_ = MessageFilter::kAll;
    }
    if (ImGui::Selectable(ICON_MD_PERSON " User Only", message_filter_ == MessageFilter::kUserOnly)) {
      message_filter_ = MessageFilter::kUserOnly;
    }
    if (ImGui::Selectable(ICON_MD_SMART_TOY " Agent Only", message_filter_ == MessageFilter::kAgentOnly)) {
      message_filter_ = MessageFilter::kAgentOnly;
    }
    ImGui::EndPopup();
  }
  
  ImGui::SameLine();
  
  // Save session button
  if (ImGui::Button(ICON_MD_SAVE, ImVec2(button_width, 30))) {
    if (toast_manager_) {
      toast_manager_->Show(ICON_MD_SAVE " Session auto-saved", ToastType::kSuccess, 1.5f);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Save chat session");
  }
  
  ImGui::SameLine();
  
  // Clear button
  if (ImGui::Button(ICON_MD_DELETE, ImVec2(button_width, 30))) {
    ClearHistory();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear popup view");
  }
}

void AgentChatHistoryPopup::DrawInputSection() {
  ImGui::Separator();
  ImGui::Spacing();
  
  // Input field using theme colors
  bool send_message = false;
  if (ImGui::InputTextMultiline("##popup_input", input_buffer_, sizeof(input_buffer_),
                                ImVec2(-1, 60),
                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine)) {
    send_message = true;
  }
  
  // Focus input on first show
  if (focus_input_) {
    ImGui::SetKeyboardFocusHere(-1);
    focus_input_ = false;
  }
  
  // Send button (proper width)
  ImGui::Spacing();
  float send_button_width = ImGui::GetContentRegionAvail().x;
  if (ImGui::Button(absl::StrFormat("%s Send", ICON_MD_SEND).c_str(), ImVec2(send_button_width, 32)) || send_message) {
    if (std::strlen(input_buffer_) > 0) {
      SendMessage(input_buffer_);
      std::memset(input_buffer_, 0, sizeof(input_buffer_));
    }
  }
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Send message (Enter) • Ctrl+Enter for newline");
  }
  
  // Info text
  ImGui::Spacing();
  ImGui::TextDisabled(ICON_MD_INFO " Enter: send • Ctrl+Enter: newline");
}

void AgentChatHistoryPopup::SendMessage(const std::string& message) {
  if (send_message_callback_) {
    send_message_callback_(message);
    
    if (toast_manager_) {
      toast_manager_->Show(ICON_MD_SEND " Message sent", ToastType::kSuccess, 1.5f);
    }
    
    // Auto-scroll to see response
    needs_scroll_ = true;
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
