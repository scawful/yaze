#include "app/editor/system/agent_chat_history_popup.h"

#include <cstring>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "imgui/imgui.h"
#include "app/gui/icons.h"
#include "app/editor/system/toast_manager.h"

namespace yaze {
namespace editor {

namespace {

// Theme-matched colors
const ImVec4 kUserColor = ImVec4(0.90f, 0.70f, 0.00f, 1.0f);  // Gold
const ImVec4 kAgentColor = ImVec4(0.40f, 0.76f, 0.64f, 1.0f);  // Teal
const ImVec4 kTimestampColor = ImVec4(0.6f, 0.6f, 0.6f, 0.9f);
const ImVec4 kAccentColor = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);  // Theme blue
const ImVec4 kBackgroundColor = ImVec4(0.10f, 0.10f, 0.13f, 0.98f);  // Darker
const ImVec4 kHeaderColor = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);

}  // namespace

AgentChatHistoryPopup::AgentChatHistoryPopup() {
  std::memset(input_buffer_, 0, sizeof(input_buffer_));
}

void AgentChatHistoryPopup::Draw() {
  if (!visible_) return;

  // Set drawer position on the LEFT side with beautiful styling
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(drawer_width_, io.DisplaySize.y), 
                           ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | 
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoTitleBar;

  // Theme-matched styling
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
  
  if (ImGui::Begin("##AgentChatPopup", &visible_, flags)) {
    // Animated header pulse
    header_pulse_ += io.DeltaTime * 2.0f;
    float pulse = 0.5f + 0.5f * sinf(header_pulse_);
    
    DrawHeader();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Message list with gradient background
    float list_height = io.DisplaySize.y - 280.0f;  // Reserve space for input and actions
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.08f, 0.95f));
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
  
  // Title with theme colors
  ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s AI Chat", ICON_MD_CHAT);
  
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130);
  
  // Compact mode toggle
  if (ImGui::SmallButton(compact_mode_ ? ICON_MD_UNFOLD_MORE : ICON_MD_UNFOLD_LESS)) {
    compact_mode_ = !compact_mode_;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(compact_mode_ ? "Expand view" : "Compact view");
  }
  
  ImGui::SameLine();
  
  // Full chat button
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.6f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentColor);
  if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW)) {
    if (open_chat_callback_) {
      open_chat_callback_();
    }
  }
  ImGui::PopStyleColor(2);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Open full chat window");
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
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.3f, 0.9f));
  
  float button_width = (ImGui::GetContentRegionAvail().x - 8) / 3.0f;
  
  // Multimodal snapshot button
  if (ImGui::Button(ICON_MD_CAMERA, ImVec2(button_width, 32))) {
    if (capture_snapshot_callback_) {
      capture_snapshot_callback_();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Capture screenshot for Gemini analysis");
  }
  
  ImGui::SameLine();
  
  // Filter button
  const char* filter_icon = ICON_MD_FILTER_LIST;
  if (ImGui::Button(filter_icon, ImVec2(button_width, 32))) {
    ImGui::OpenPopup("FilterPopup");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter messages");
  }
  
  // Filter popup
  if (ImGui::BeginPopup("FilterPopup")) {
    if (ImGui::Selectable("All Messages", message_filter_ == MessageFilter::kAll)) {
      message_filter_ = MessageFilter::kAll;
    }
    if (ImGui::Selectable("User Only", message_filter_ == MessageFilter::kUserOnly)) {
      message_filter_ = MessageFilter::kUserOnly;
    }
    if (ImGui::Selectable("Agent Only", message_filter_ == MessageFilter::kAgentOnly)) {
      message_filter_ = MessageFilter::kAgentOnly;
    }
    ImGui::EndPopup();
  }
  
  ImGui::SameLine();
  
  // Clear button
  if (ImGui::Button(ICON_MD_DELETE, ImVec2(button_width, 32))) {
    ClearHistory();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear popup view");
  }
  
  ImGui::PopStyleColor(2);
}

void AgentChatHistoryPopup::DrawInputSection() {
  ImGui::Separator();
  ImGui::Spacing();
  
  // Input field with beautiful styling
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.15f, 0.17f, 0.22f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.18f, 0.20f, 0.25f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
  
  bool send_message = false;
  ImGui::SetNextItemWidth(-1);
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
  
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
  
  // Send button with gradient
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kAccentColor.x, kAccentColor.y, kAccentColor.z, 0.7f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentColor);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.7f, 1.0f));
  
  if (ImGui::Button(absl::StrFormat("%s Send", ICON_MD_SEND).c_str(), ImVec2(-1, 32)) || send_message) {
    if (std::strlen(input_buffer_) > 0) {
      SendMessage(input_buffer_);
      std::memset(input_buffer_, 0, sizeof(input_buffer_));
    }
  }
  
  ImGui::PopStyleColor(3);
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Send message (Enter) • Ctrl+Enter for newline");
  }
  
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
