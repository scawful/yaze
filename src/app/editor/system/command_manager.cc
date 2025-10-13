#include "command_manager.h"

#include <fstream>

#include "app/gui/core/input.h"
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

// Enhanced hierarchical WhichKey with Spacemacs-style navigation
void CommandManager::ShowWhichKeyHierarchical() {
  // Activate on Space key
  if (ImGui::IsKeyPressed(ImGuiKey_Space) && current_prefix_.empty()) {
    whichkey_active_ = true;
    whichkey_timer_ = 0.0f;
    ImGui::OpenPopup("WhichKeyHierarchical");
  }

  // ESC to close or go back
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    if (!current_prefix_.empty()) {
      current_prefix_.clear();  // Go back to root
    } else {
      whichkey_active_ = false;
      ImGui::CloseCurrentPopup();
    }
  }

  // Position at bottom of screen
  ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 150),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 150),
                           ImGuiCond_Always);

  if (ImGui::BeginPopup("WhichKeyHierarchical")) {
    whichkey_active_ = true;

    // Update timer for auto-close
    whichkey_timer_ += ImGui::GetIO().DeltaTime;
    if (whichkey_timer_ > 5.0f) {  // Auto-close after 5 seconds
      whichkey_active_ = false;
      current_prefix_.clear();
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    // Show breadcrumb navigation
    if (!current_prefix_.empty()) {
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
                        "Space > %s", current_prefix_.c_str());
      ImGui::Separator();
    } else {
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Space > ...");
      ImGui::Separator();
    }

    // Color palette for visual grouping
    const ImVec4 colors[] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),  // Red - Window
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),  // Green - Buffer
        ImVec4(0.2f, 0.2f, 0.8f, 1.0f),  // Blue - File
        ImVec4(0.8f, 0.8f, 0.2f, 1.0f),  // Yellow - Session
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f),  // Magenta - Layout
        ImVec4(0.2f, 0.8f, 0.8f, 1.0f)   // Cyan - Theme
    };

    // Show commands based on current navigation level
    if (current_prefix_.empty()) {
      // Root level - show main groups
      if (ImGui::BeginTable("RootCommands", 6, ImGuiTableFlags_SizingStretchProp)) {
        int colorIndex = 0;
        for (const auto &[shortcut, group] : commands_) {
          ImGui::TableNextColumn();
          ImGui::TextColored(colors[colorIndex % 6],
                            "%c: %s",
                            group.main_command.mnemonic,
                            group.main_command.name.c_str());
          colorIndex++;
        }
        ImGui::EndTable();
      }
    } else {
      // Submenu level - show subcommands
      auto it = commands_.find(current_prefix_);
      if (it != commands_.end()) {
        const auto& group = it->second;
        if (!group.subcommands.empty()) {
          if (ImGui::BeginTable("Subcommands",
                               std::min(6, (int)group.subcommands.size()),
                               ImGuiTableFlags_SizingStretchProp)) {
            int colorIndex = 0;
            for (const auto& [key, cmd] : group.subcommands) {
              ImGui::TableNextColumn();
              ImGui::TextColored(colors[colorIndex % 6],
                                "%c: %s",
                                cmd.mnemonic,
                                cmd.name.c_str());
              colorIndex++;
            }
            ImGui::EndTable();
          }
        } else {
          ImGui::TextDisabled("No subcommands available");
        }
      }
    }

    ImGui::EndPopup();
  } else {
    whichkey_active_ = false;
    current_prefix_.clear();
  }
}

// Handle keyboard input for WhichKey navigation
void CommandManager::HandleWhichKeyInput() {
  if (!whichkey_active_) return;

  // Check for prefix keys (w, l, f, b, s, t, etc.)
  for (const auto& [shortcut, group] : commands_) {
    ImGuiKey key = gui::MapKeyToImGuiKey(group.main_command.mnemonic);
    if (key != ImGuiKey_COUNT && ImGui::IsKeyPressed(key)) {
      if (current_prefix_.empty()) {
        // Enter submenu
        current_prefix_ = shortcut;
        whichkey_timer_ = 0.0f;
        return;
      } else {
        // Execute subcommand
        auto it = commands_.find(current_prefix_);
        if (it != commands_.end()) {
          for (const auto& [subkey, cmd] : it->second.subcommands) {
            if (cmd.mnemonic == group.main_command.mnemonic) {
              if (cmd.command) {
                cmd.command();
              }
              whichkey_active_ = false;
              current_prefix_.clear();
              ImGui::CloseCurrentPopup();
              return;
            }
          }
        }
      }
    }
  }
}

}  // namespace editor
}  // namespace yaze
