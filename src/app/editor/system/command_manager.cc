#include "command_manager.h"

#include <fstream>

#include "app/gui/input.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {


// When the player presses Space, a popup will appear fixed to the bottom of the
// ImGui window with a list of the available key commands which can be used.
void CommandManager::ShowWhichKey() {
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    ImGui::OpenPopup("WhichKey");
  }

  ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 100),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 100),
                           ImGuiCond_Always);
  if (ImGui::BeginPopup("WhichKey")) {
    // ESC to close the popup
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      ImGui::CloseCurrentPopup();
    }

    const ImVec4 colors[] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),  // Soft Red
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),  // Soft Green
        ImVec4(0.2f, 0.2f, 0.8f, 1.0f),  // Soft Blue
        ImVec4(0.8f, 0.8f, 0.2f, 1.0f),  // Soft Yellow
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f),  // Soft Magenta
        ImVec4(0.2f, 0.8f, 0.8f, 1.0f)   // Soft Cyan
    };
    const int numColors = sizeof(colors) / sizeof(colors[0]);
    int colorIndex = 0;

    if (ImGui::BeginTable("CommandsTable", commands_.size(),
                          ImGuiTableFlags_SizingStretchProp)) {
    for (const auto &[shortcut, group] : commands_) {
      ImGui::TableNextColumn();
      ImGui::TextColored(colors[colorIndex], "%c: %s",
                         group.main_command.mnemonic,
                         group.main_command.name.c_str());
      colorIndex = (colorIndex + 1) % numColors;
    }
      ImGui::EndTable();
    }
    ImGui::EndPopup();
  }
}

void CommandManager::SaveKeybindings(const std::string &filepath) {
  std::ofstream out(filepath);
  if (out.is_open()) {
    for (const auto &[shortcut, group] : commands_) {
      out << shortcut << " " << group.main_command.mnemonic << " "
          << group.main_command.name << " " << group.main_command.desc << "\n";
    }
    out.close();
  }
}

void CommandManager::LoadKeybindings(const std::string &filepath) {
  std::ifstream in(filepath);
  if (in.is_open()) {
    commands_.clear();
    std::string shortcut, name, desc;
    char mnemonic;
    while (in >> shortcut >> mnemonic >> name >> desc) {
      commands_[shortcut].main_command = {nullptr, mnemonic, name, desc};
    }
    in.close();
  }
}

}  // namespace editor
}  // namespace yaze
