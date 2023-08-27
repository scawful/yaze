#include <SDL_mixer.h>

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/clock.h"
#include "app/emu/cpu.h"
#include "app/emu/debug/debugger.h"
#include "app/emu/memory/dma.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

class SNES : public DMA {
 public:
  SNES() = default;
  ~SNES() = default;

  ROMInfo ReadRomHeader(uint32_t offset);

  // Initialization
  void Init(ROM& rom);

  // Main emulation loop
  void Run();

  // Enable NMI Interrupts
  void EnableVBlankInterrupts();

  // Wait until the VBlank routine has been processed
  void WaitForVBlank();

  // NMI Interrupt Service Routine
  void NmiIsr();

  // VBlank routine
  void VBlankRoutine();

  // Boot the APU with the IPL ROM
  void BootAPUWithIPL();

  // Controller input handling
  void HandleInput();

  // Save/Load game state
  void SaveState(const std::string& path);
  void LoadState(const std::string& path);

  // Debugger
  void Debug();
  void Breakpoint(uint16_t address);

  bool running() const { return running_; }

 private:
  void WriteToRegister(uint16_t address, uint8_t value) {
    memory_.WriteByte(address, value);
  }

  // Components of the SNES
  MemoryImpl memory_;
  ClockImpl clock_;
  AudioRamImpl audio_ram_;

  CPU cpu{memory_, clock_};
  PPU ppu{memory_, clock_};
  APU apu{memory_, audio_ram_, clock_};

  // Helper classes
  ROMInfo rom_info_;
  Debugger debugger;

  // Currently loaded ROM
  std::vector<uint8_t> rom_data;

  // Byte flag to indicate if the VBlank routine should be executed or not
  std::atomic<bool> v_blank_flag_;

  // 32-bit counter to track the number of NMI interrupts
  std::atomic<uint32_t> frame_counter_;

  // Other private member variables
  bool running_ = false;
  int scanline;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze