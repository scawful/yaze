#include "app/core/emulator.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace core {

void Emulator::Run() {
  // Initialize the emulator if a ROM is loaded
  if (!snes_.running()) {
    if (rom()->isLoaded()) {
      snes_.Init(*rom());
      running_ = true;
    }
  }

  // Render the emulator output
  RenderNavBar();

  RenderEmulator();

  if (running_) {
    // Handle user input events
    HandleEvents();
    // Update the emulator state
    UpdateEmulator();
  }
}

void Emulator::RenderEmulator() {
  // Get the emulator output and render it to the child window
  // You can use the ImGui::Image function to display the emulator output as a
  // texture
  // ...
}

void Emulator::RenderNavBar() {
  MENU_BAR()

  if (ImGui::BeginMenu("Game")) {
    MENU_ITEM("Power Off") {}
    MENU_ITEM("Pause") {}
    MENU_ITEM("Reset") {}

    MENU_ITEM("Save State") {}
    MENU_ITEM("Load State") {}

    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Debug")) {
    ImGui::MenuItem("PPU Register Viewer", nullptr, &show_ppu_reg_viewer_);
    MENU_ITEM("Debugger") {}
    MENU_ITEM("Memory Viewer") {}
    MENU_ITEM("Tile Viewer") {}
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Options")) {
    MENU_ITEM("Audio") {}
    MENU_ITEM("Input") {}
    MENU_ITEM("Video") {}
    ImGui::EndMenu();
  }
  END_MENU_BAR()
}

void Emulator::HandleEvents() {
  // Handle user input events
  // ...
}

void Emulator::UpdateEmulator() {
  // Update the emulator state (CPU, PPU, APU, etc.)
  // ...
  snes_.Run();
}

}  // namespace core
}  // namespace app
}  // namespace yaze
