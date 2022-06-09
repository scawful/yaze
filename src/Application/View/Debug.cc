#include "Debug.h"

namespace yaze {
namespace Application {
namespace View {

void Debug::UpdateScreen() const
{
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);


  if (!ImGui::Begin("Main", nullptr, flags)) {
    ImGui::End();
    return;
  }

  ImGui::ShowDemoWindow();


  ImGui::End();
}

}
}
}