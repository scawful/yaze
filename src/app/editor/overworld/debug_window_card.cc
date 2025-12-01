#include "app/editor/overworld/debug_window_card.h"

#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

DebugWindowCard::DebugWindowCard() {}

void DebugWindowCard::Draw(bool* p_open) {
  if (ImGui::Begin("Debug Window", p_open)) {
    ImGui::Text("Debug Information");
    ImGui::Separator();
    ImGui::Text("Application Average: %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    
    // Add more debug info here as needed
  }
  ImGui::End();
}

}  // namespace yaze::editor
