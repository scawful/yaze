#include "command_manager.h"

#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

// When the player presses Space, a popup will appear fixed to the bottom of the
// ImGui window with a list of the available key commands which can be used.
void CommandManager::ShowWhichKey() {
  if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space))) {
    ImGui::OpenPopup("WhichKey");
  }

  if (ImGui::BeginPopup("WhichKey")) {
    for (const auto& [shortcut, command] : commands_) {
      ImGui::Text("%s: %s", shortcut.c_str(),
                  command->GetDescription().c_str());
    }
    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
