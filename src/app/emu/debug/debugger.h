#ifndef YAZE_APP_EMU_DEBUG_DEBUGGER_H_
#define YAZE_APP_EMU_DEBUG_DEBUGGER_H_

#include "app/emu/audio/apu.h"
#include "app/emu/cpu.h"
#include "app/emu/video/ppu.h"

namespace yaze {
namespace app {
namespace emu {

class Debugger {
 public:
  Debugger() = default;
  // Attach the debugger to the emulator
  // Debugger(CPU &cpu, PPU &ppu, APU &apu);

  // Set a breakpoint
  void SetBreakpoint(uint16_t address);

  // Remove a breakpoint
  void RemoveBreakpoint(uint16_t address);

  // Step through the code
  void Step();

  // Inspect memory
  uint8_t InspectMemory(uint16_t address);

  // Modify memory
  void ModifyMemory(uint16_t address, uint8_t value);

  // Inspect registers
  uint8_t InspectRegister(uint8_t reg);

  // Modify registers
  void ModifyRegister(uint8_t reg, uint8_t value);

  // Handle other debugger tasks
  // ...

 private:
  // References to the emulator's components
  // CPU &cpu;
  // PPU &ppu;
  // APU &apu;

  // Breakpoints, watchpoints, etc.
  // ...
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_DBG_H_