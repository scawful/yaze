#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <cstdint>
#include <vector>

#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace core {

class Emulator : public SharedROM {
 public:
  // Runs the emulator loop, including event handling and rendering
  void Run();

 private:
  // Renders the emulator output to an ImGui child window
  void RenderEmulator();

  // Draws the navigation bar with various controls
  void RenderNavBar();

  // Handles user input events
  void HandleEvents();

  // Updates the emulator state (CPU, PPU, APU, etc.)
  void UpdateEmulator();

  // Member variables to store internal state and resources
  emu::SNES snes_;

  bool running_ = false;
  bool show_ppu_reg_viewer_ = false;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H