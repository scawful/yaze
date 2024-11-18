#include "command_manager.h"

#include <fstream>

#include "app/editor/editor.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

ImGuiKey MapKeyToImGuiKey(char key) {
  switch (key) {
    case 'A':
      return ImGuiKey_A;
    case 'B':
      return ImGuiKey_B;
    case 'C':
      return ImGuiKey_C;
    case 'D':
      return ImGuiKey_D;
    case 'E':
      return ImGuiKey_E;
    case 'F':
      return ImGuiKey_F;
    case 'G':
      return ImGuiKey_G;
    case 'H':
      return ImGuiKey_H;
    case 'I':
      return ImGuiKey_I;
    case 'J':
      return ImGuiKey_J;
    case 'K':
      return ImGuiKey_K;
    case 'L':
      return ImGuiKey_L;
    case 'M':
      return ImGuiKey_M;
    case 'N':
      return ImGuiKey_N;
    case 'O':
      return ImGuiKey_O;
    case 'P':
      return ImGuiKey_P;
    case 'Q':
      return ImGuiKey_Q;
    case 'R':
      return ImGuiKey_R;
    case 'S':
      return ImGuiKey_S;
    case 'T':
      return ImGuiKey_T;
    case 'U':
      return ImGuiKey_U;
    case 'V':
      return ImGuiKey_V;
    case 'W':
      return ImGuiKey_W;
    case 'X':
      return ImGuiKey_X;
    case 'Y':
      return ImGuiKey_Y;
    case 'Z':
      return ImGuiKey_Z;
    case '/':
      return ImGuiKey_Slash;
    case '-':
      return ImGuiKey_Minus;
    default:
      return ImGuiKey_COUNT;
  }
}

// When the player presses Space, a popup will appear fixed to the bottom of the
// ImGui window with a list of the available key commands which can be used.
void CommandManager::ShowWhichKey(EditorLayoutParams *editor_layout) {
  if (commands_.empty()) {
    InitializeDefaults();
  }

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
      for (const auto &[shortcut, info] : commands_) {
        ImGui::TableNextColumn();
        ImGui::TextColored(colors[colorIndex], "%c: %s",
                           info.command_info.mnemonic,
                           info.command_info.name.c_str());
        colorIndex = (colorIndex + 1) % numColors;
      }
      ImGui::EndTable();
    }
    ImGui::EndPopup();
  }

  // if the user presses the shortcut key, execute the command
  // or if it is a subcommand prefix, wait for the next key
  // to determine the subcommand
  static bool subcommand_section = false;
  for (const auto &[shortcut, info] : commands_) {
    if (ImGui::IsKeyPressed(MapKeyToImGuiKey(shortcut[0]))) {
      info.command_info.command();
    }
  }
}

void CommandManager::InitializeDefaults() {}

void CommandManager::SaveKeybindings(const std::string &filepath) {
  std::ofstream out(filepath);
  if (out.is_open()) {
    for (const auto &[shortcut, info] : commands_) {
      out << shortcut << " " << info.command_info.mnemonic << " "
          << info.command_info.name << " " << info.command_info.desc << "\n";
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
      commands_[shortcut].command_info = {nullptr, mnemonic, name, desc};
    }
    in.close();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
