#include "app/editor/system/agent_chat_widget.h"

#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

namespace {

const ImVec4 kUserColor = ImVec4(0.88f, 0.76f, 0.36f, 1.0f);
const ImVec4 kAgentColor = ImVec4(0.56f, 0.82f, 0.62f, 1.0f);
const ImVec4 kJsonTextColor = ImVec4(0.78f, 0.83f, 0.90f, 1.0f);

void RenderTable(const yaze::cli::agent::ChatMessage::TableData& table_data) {
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
}

void AgentChatWidget::SetRomContext(Rom* rom) {
  agent_service_.SetRomContext(rom);
}

void AgentChatWidget::Draw() {
  if (!active_) {
    return;
  }

  ImGui::Begin(title_.c_str(), &active_);

  // Display message history
  const auto& history = agent_service_.GetHistory();
  if (ImGui::BeginChild("History", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
                        false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_HorizontalScrollbar)) {
    for (size_t index = 0; index < history.size(); ++index) {
      const auto& msg = history[index];
      ImGui::PushID(static_cast<int>(index));

      const bool from_user =
          msg.sender == cli::agent::ChatMessage::Sender::kUser;
      const ImVec4 header_color = from_user ? kUserColor : kAgentColor;
      const char* header_label = from_user ? "You" : "Agent";

      ImGui::TextColored(header_color, "%s", header_label);

      ImGui::Indent();

      if (msg.json_pretty.has_value()) {
        if (ImGui::SmallButton("Copy JSON")) {
          ImGui::SetClipboardText(msg.json_pretty->c_str());
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

      ImGui::Unindent();
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::PopID();
    }

    if (history.size() > last_history_size_) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  ImGui::EndChild();
  last_history_size_ = history.size();

  // Display input text box
  if (ImGui::InputText("Input", input_buffer_, sizeof(input_buffer_),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    if (strlen(input_buffer_) > 0) {
      (void)agent_service_.SendMessage(input_buffer_);
      memset(input_buffer_, 0, sizeof(input_buffer_));
    }
    ImGui::SetKeyboardFocusHere(-1); // Refocus input
  }

  ImGui::End();
}

}  // namespace editor
}  // namespace yaze
