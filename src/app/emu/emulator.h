#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <imgui/imgui.h>

#include <cstdint>
#include <vector>

#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace app {

/**
 * @namespace yaze::app::emu
 * @brief SNES Emulation and debugging tools.
 */
namespace emu {

/**
 * @class Emulator
 * @brief A class for emulating and debugging SNES games.
 */
class Emulator : public SharedRom {
 public:
  void Run();

 private:
  void RenderNavBar();
  void HandleEvents();

  void RenderEmulator();
  void RenderSnesPpu();
  void RenderBreakpointList();
  void RenderCpuState(Cpu& cpu);
  void RenderMemoryViewer();

  struct Bookmark {
    std::string name;
    uint64_t value;
  };
  std::vector<Bookmark> bookmarks;

  void RenderCpuInstructionLog(
      const std::vector<InstructionEntry>& instructionLog);

  SNES snes_;
  uint16_t manual_pc_ = 0;
  uint8_t manual_pb_ = 0;

  bool power_ = false;
  bool loading_ = false;
  bool running_ = false;
  bool step_ = true;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H