#include "app/editor/agent/agent_chat_history_popup.h"

#include <cstring>
#include <set>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/system/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {

namespace {

std::string BuildProviderLabel(
    const std::optional<cli::agent::ChatMessage::ModelMetadata>& meta) {
  if (!meta.has_value()) {
    return "";
  }
  if (meta->model.empty()) {
    return meta->provider;
  }
  return absl::StrFormat("%s · %s", meta->provider, meta->model);
}

}  // namespace

AgentChatHistoryPopup::AgentChatHistoryPopup() {
  std::memset(input_buffer_, 0, sizeof(input_buffer_));
  std::memset(search_buffer_, 0, sizeof(search_buffer_));
  provider_filters_.push_back("All providers");
}

void AgentChatHistoryPopup::Draw() {
  if (!visible_) return;

  const auto& theme = AgentUI::GetTheme();

  // Animate retro effects
  ImGuiIO& io = ImGui::GetIO();
  pulse_animation_ += io.DeltaTime * 2.0f;
  scanline_offset_ += io.DeltaTime * 0.3f;
  if (scanline_offset_ > 1.0f) scanline_offset_ -= 1.0f;
  glitch_animation_ += io.DeltaTime * 5.0f;
  blink_counter_ = static_cast<int>(pulse_animation_ * 2.0f) % 2;

  // Set drawer position on the LEFT side (full height)
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(drawer_width_, io.DisplaySize.y),
                           ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoTitleBar;

  // Use current theme colors with slight glow
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

  // Pulsing border color
  float border_pulse = 0.7f + 0.3f * std::sin(pulse_animation_ * 0.5f);
  ImGui::PushStyleColor(
      ImGuiCol_Border,
      ImVec4(theme.provider_ollama.x * border_pulse,
             theme.provider_ollama.y * border_pulse,
             theme.provider_ollama.z * border_pulse + 0.2f, 0.8f));

  if (ImGui::Begin("##AgentChatPopup", &visible_, flags)) {
    DrawHeader();

    ImGui::Separator();
    ImGui::Spacing();

    // Calculate proper list height
    float list_height = ImGui::GetContentRegionAvail().y - 220.0f;

    // Dark terminal background
    ImVec4 terminal_bg = theme.code_bg_color;
    terminal_bg.x *= 0.9f;
    terminal_bg.y *= 0.9f;
    terminal_bg.z *= 0.95f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, terminal_bg);
    ImGui::BeginChild("MessageList", ImVec2(0, list_height), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Draw scanline effect
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    for (float y = 0; y < win_size.y; y += 3.0f) {
      float offset_y = y + scanline_offset_ * 3.0f;
      if (offset_y < win_size.y) {
        draw_list->AddLine(ImVec2(win_pos.x, win_pos.y + offset_y),
                           ImVec2(win_pos.x + win_size.x, win_pos.y + offset_y),
                           IM_COL32(0, 0, 0, 15));
      }
    }

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

  ImGui::PopStyleColor();  // Border color
  ImGui::PopStyleVar(2);
}

void AgentChatHistoryPopup::DrawMessageList() {
  if (messages_.empty()) {
    ImGui::TextDisabled(
        "No messages yet. Start a conversation in the chat window.");
    return;
  }

  // Calculate starting index for display limit
  int start_index =
      messages_.size() > display_limit_ ? messages_.size() - display_limit_ : 0;

  for (int i = start_index; i < messages_.size(); ++i) {
    const auto& msg = messages_[i];

    // Skip internal messages
    if (msg.is_internal) continue;

    if (!MessagePassesFilters(msg, i)) {
      continue;
    }

    DrawMessage(msg, i);
  }
}

void AgentChatHistoryPopup::DrawMessage(const cli::agent::ChatMessage& msg,
                                        int index) {
  ImGui::PushID(index);

  bool from_user = (msg.sender == cli::agent::ChatMessage::Sender::kUser);

  // Retro terminal colors
  ImVec4 header_color =
      from_user ? ImVec4(1.0f, 0.85f, 0.0f, 1.0f)  // Amber/Gold for user
                : ImVec4(0.0f, 1.0f, 0.7f, 1.0f);  // Cyan/Green for agent

  const char* sender_label = from_user ? "> USER:" : "> AGENT:";

  // Message header with terminal prefix
  ImGui::TextColored(header_color, "%s", sender_label);

  ImGui::SameLine();
  ImGui::TextColored(
      ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]",
      absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone())
          .c_str());
  if (msg.model_metadata.has_value()) {
    const auto& meta = *msg.model_metadata;
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[%s • %s]",
                       meta.provider.c_str(), meta.model.c_str());
  }

  bool is_pinned = pinned_messages_.find(index) != pinned_messages_.end();
  float pin_target =
      ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 24.0f;
  if (pin_target > ImGui::GetCursorPosX()) {
    ImGui::SameLine(pin_target);
  } else {
    ImGui::SameLine();
  }
  if (is_pinned) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.3f, 0.8f));
  }
  if (ImGui::SmallButton(ICON_MD_PUSH_PIN)) {
    TogglePin(index);
  }
  if (is_pinned) {
    ImGui::PopStyleColor();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(is_pinned ? "Unpin message" : "Pin message");
  }

  // Message content with terminal styling
  ImGui::Indent(15.0f);

  if (msg.table_data.has_value()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "  %s [Table Data]",
                       ICON_MD_TABLE_CHART);
  } else if (msg.json_pretty.has_value()) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f),
                       "  %s [Structured Response]", ICON_MD_DATA_OBJECT);
  } else {
    // Truncate long messages with ellipsis
    std::string content = msg.message;
    if (content.length() > 200) {
      content = content.substr(0, 197) + "...";
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::TextWrapped("  %s", content.c_str());
    ImGui::PopStyleColor();
  }

  // Show proposal indicator with pulse
  if (msg.proposal.has_value()) {
    float proposal_pulse = 0.7f + 0.3f * std::sin(pulse_animation_ * 2.0f);
    ImGui::TextColored(ImVec4(0.2f, proposal_pulse, 0.4f, 1.0f),
                       "  %s Proposal: [%s]", ICON_MD_PREVIEW,
                       msg.proposal->id.c_str());
  }

  if (msg.model_metadata.has_value()) {
    const auto& meta = *msg.model_metadata;
    ImGui::TextDisabled("  Latency: %.2fs | Tools: %d", meta.latency_seconds,
                        meta.tool_iterations);
    if (!meta.tool_names.empty()) {
      ImGui::TextDisabled("  Tool calls: %s",
                          absl::StrJoin(meta.tool_names, ", ").c_str());
    }
  }

  for (const auto& warning : msg.warnings) {
    ImGui::TextColored(ImVec4(0.95f, 0.6f, 0.2f, 1.0f), "  %s %s",
                       ICON_MD_WARNING, warning.c_str());
  }

  ImGui::Unindent(15.0f);
  ImGui::Spacing();

  // Retro separator line
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 line_start = ImGui::GetCursorScreenPos();
  float line_width = ImGui::GetContentRegionAvail().x;
  draw_list->AddLine(line_start,
                     ImVec2(line_start.x + line_width, line_start.y),
                     IM_COL32(60, 60, 70, 100), 1.0f);

  ImGui::Dummy(ImVec2(0, 2));

  ImGui::PopID();
}

void AgentChatHistoryPopup::DrawHeader() {
  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 header_start = ImGui::GetCursorScreenPos();
  ImVec2 header_size(ImGui::GetContentRegionAvail().x, 55);

  // Retro gradient with pulse
  float pulse = 0.5f + 0.5f * std::sin(pulse_animation_);
  ImVec4 bg_top = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
  ImVec4 bg_bottom = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
  bg_top.x += 0.1f * pulse;
  bg_top.y += 0.1f * pulse;
  bg_top.z += 0.15f * pulse;

  ImU32 color_top = ImGui::GetColorU32(bg_top);
  ImU32 color_bottom = ImGui::GetColorU32(bg_bottom);
  draw_list->AddRectFilledMultiColor(
      header_start,
      ImVec2(header_start.x + header_size.x, header_start.y + header_size.y),
      color_top, color_top, color_bottom, color_bottom);

  // Pulsing accent line with glow
  float line_pulse = 0.6f + 0.4f * std::sin(pulse_animation_ * 0.7f);
  ImU32 accent_color = IM_COL32(
      static_cast<int>(theme.provider_ollama.x * 255 * line_pulse),
      static_cast<int>(theme.provider_ollama.y * 255 * line_pulse),
      static_cast<int>(theme.provider_ollama.z * 255 * line_pulse + 50), 200);
  draw_list->AddLine(
      ImVec2(header_start.x, header_start.y + header_size.y),
      ImVec2(header_start.x + header_size.x, header_start.y + header_size.y),
      accent_color, 2.0f);

  ImGui::Dummy(ImVec2(0, 8));

  // Title with pulsing glow
  ImVec4 title_color =
      ImVec4(0.4f + 0.3f * pulse, 0.8f + 0.2f * pulse, 1.0f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, title_color);
  ImGui::Text("%s CHAT HISTORY", ICON_MD_CHAT);
  ImGui::PopStyleColor();

  ImGui::SameLine();
  ImGui::TextDisabled("[v0.4.x]");

  ImGui::Spacing();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
  if (ImGui::InputTextWithHint("##history_search", ICON_MD_SEARCH " Search...",
                               search_buffer_, sizeof(search_buffer_))) {
    needs_scroll_ = true;
  }

  if (provider_filters_.empty()) {
    provider_filters_.push_back("All providers");
    provider_filter_index_ = 0;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(150.0f);
  const char* provider_preview =
      provider_filters_[std::min<int>(
                            provider_filter_index_,
                            static_cast<int>(provider_filters_.size() - 1))]
          .c_str();
  if (ImGui::BeginCombo("##provider_filter", provider_preview)) {
    for (int i = 0; i < static_cast<int>(provider_filters_.size()); ++i) {
      bool selected = (provider_filter_index_ == i);
      if (ImGui::Selectable(provider_filters_[i].c_str(), selected)) {
        provider_filter_index_ = i;
        needs_scroll_ = true;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter messages by provider/model metadata");
  }

  ImGui::SameLine();
  if (ImGui::Checkbox(ICON_MD_PUSH_PIN "##pin_filter", &show_pinned_only_)) {
    needs_scroll_ = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Show pinned messages only");
  }

  // Buttons properly spaced from right edge
  ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                  75.0f);

  // Compact mode toggle with pulse
  bool should_highlight = (blink_counter_ == 0 && compact_mode_);
  if (should_highlight) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 0.7f));
  }
  if (ImGui::SmallButton(compact_mode_ ? ICON_MD_UNFOLD_MORE
                                       : ICON_MD_UNFOLD_LESS)) {
    compact_mode_ = !compact_mode_;
  }
  if (should_highlight) {
    ImGui::PopStyleColor();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(compact_mode_ ? "Expand view" : "Compact view");
  }

  ImGui::SameLine();

  // Full chat button
  if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW)) {
    if (open_chat_callback_) {
      open_chat_callback_();
      visible_ = false;
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

  // Message count with retro styling
  int visible_count = 0;
  for (int i = 0; i < static_cast<int>(messages_.size()); ++i) {
    if (messages_[i].is_internal) {
      continue;
    }
    if (MessagePassesFilters(messages_[i], i)) {
      ++visible_count;
    }
  }

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "> MESSAGES: [%d]",
                     visible_count);

  // Animated status indicator
  if (unread_count_ > 0) {
    ImGui::SameLine();
    float unread_pulse = 0.5f + 0.5f * std::sin(pulse_animation_ * 3.0f);
    ImGui::TextColored(ImVec4(1.0f, unread_pulse * 0.5f, 0.0f, 1.0f),
                       "%s %d NEW", ICON_MD_NOTIFICATION_IMPORTANT,
                       unread_count_);
  }

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
  const char* filter_icons[] = {ICON_MD_FILTER_LIST, ICON_MD_PERSON,
                                ICON_MD_SMART_TOY};
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
    if (ImGui::Selectable(ICON_MD_FILTER_LIST " All Messages",
                          message_filter_ == MessageFilter::kAll)) {
      message_filter_ = MessageFilter::kAll;
    }
    if (ImGui::Selectable(ICON_MD_PERSON " User Only",
                          message_filter_ == MessageFilter::kUserOnly)) {
      message_filter_ = MessageFilter::kUserOnly;
    }
    if (ImGui::Selectable(ICON_MD_SMART_TOY " Agent Only",
                          message_filter_ == MessageFilter::kAgentOnly)) {
      message_filter_ = MessageFilter::kAgentOnly;
    }
    ImGui::EndPopup();
  }

  ImGui::SameLine();

  // Save session button
  if (ImGui::Button(ICON_MD_SAVE, ImVec2(button_width, 30))) {
    if (toast_manager_) {
      toast_manager_->Show(ICON_MD_SAVE " Session auto-saved",
                           ToastType::kSuccess, 1.5f);
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

bool AgentChatHistoryPopup::MessagePassesFilters(
    const cli::agent::ChatMessage& msg, int index) const {
  if (message_filter_ == MessageFilter::kUserOnly &&
      msg.sender != cli::agent::ChatMessage::Sender::kUser) {
    return false;
  }
  if (message_filter_ == MessageFilter::kAgentOnly &&
      msg.sender != cli::agent::ChatMessage::Sender::kAgent) {
    return false;
  }
  if (show_pinned_only_ &&
      pinned_messages_.find(index) == pinned_messages_.end()) {
    return false;
  }
  if (provider_filter_index_ > 0 &&
      provider_filter_index_ < static_cast<int>(provider_filters_.size())) {
    std::string label = BuildProviderLabel(msg.model_metadata);
    if (label != provider_filters_[provider_filter_index_]) {
      return false;
    }
  }
  if (search_buffer_[0] != '\0') {
    std::string needle = absl::AsciiStrToLower(std::string(search_buffer_));
    auto contains = [&](const std::string& value) {
      return absl::StrContains(absl::AsciiStrToLower(value), needle);
    };
    bool matched = contains(msg.message);
    if (!matched && msg.json_pretty.has_value()) {
      matched = contains(*msg.json_pretty);
    }
    if (!matched && msg.proposal.has_value()) {
      matched = contains(msg.proposal->id);
    }
    if (!matched) {
      for (const auto& warning : msg.warnings) {
        if (contains(warning)) {
          matched = true;
          break;
        }
      }
    }
    if (!matched) {
      return false;
    }
  }
  return true;
}

void AgentChatHistoryPopup::RefreshProviderFilters() {
  std::set<std::string> unique_labels;
  for (const auto& msg : messages_) {
    std::string label = BuildProviderLabel(msg.model_metadata);
    if (!label.empty()) {
      unique_labels.insert(label);
    }
  }
  provider_filters_.clear();
  provider_filters_.push_back("All providers");
  provider_filters_.insert(provider_filters_.end(), unique_labels.begin(),
                           unique_labels.end());
  if (provider_filter_index_ >= static_cast<int>(provider_filters_.size())) {
    provider_filter_index_ = 0;
  }
}

void AgentChatHistoryPopup::TogglePin(int index) {
  if (pinned_messages_.find(index) != pinned_messages_.end()) {
    pinned_messages_.erase(index);
  } else {
    pinned_messages_.insert(index);
  }
}

void AgentChatHistoryPopup::DrawInputSection() {
  ImGui::Separator();
  ImGui::Spacing();

  // Input field using theme colors
  bool send_message = false;
  if (ImGui::InputTextMultiline("##popup_input", input_buffer_,
                                sizeof(input_buffer_), ImVec2(-1, 60),
                                ImGuiInputTextFlags_EnterReturnsTrue |
                                    ImGuiInputTextFlags_CtrlEnterForNewLine)) {
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
  if (ImGui::Button(absl::StrFormat("%s Send", ICON_MD_SEND).c_str(),
                    ImVec2(send_button_width, 32)) ||
      send_message) {
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
      toast_manager_->Show(ICON_MD_SEND " Message sent", ToastType::kSuccess,
                           1.5f);
    }

    // Auto-scroll to see response
    needs_scroll_ = true;
  }
}

void AgentChatHistoryPopup::UpdateHistory(
    const std::vector<cli::agent::ChatMessage>& history) {
  bool had_messages = !messages_.empty();
  int old_size = messages_.size();

  messages_ = history;

  std::unordered_set<int> updated_pins;
  for (int pin : pinned_messages_) {
    if (pin < static_cast<int>(messages_.size())) {
      updated_pins.insert(pin);
    }
  }
  pinned_messages_.swap(updated_pins);
  RefreshProviderFilters();

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
    toast_manager_->Show(ICON_MD_CHAT " New message received", ToastType::kInfo,
                         2.0f);
  }
}

void AgentChatHistoryPopup::ClearHistory() {
  messages_.clear();
  pinned_messages_.clear();
  provider_filters_.clear();
  provider_filters_.push_back("All providers");
  provider_filter_index_ = 0;

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

void AgentChatHistoryPopup::ScrollToBottom() { needs_scroll_ = true; }

}  // namespace editor
}  // namespace yaze
