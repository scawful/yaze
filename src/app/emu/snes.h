#ifndef YAZE_APP_EMU_SNES_H
#define YAZE_APP_EMU_SNES_H

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/cpu/clock.h"
#include "app/emu/cpu/cpu.h"
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

  // Step through a single instruction
  void StepRun();

  // Enable NMI Interrupts
  void EnableVBlankInterrupts();

  // Wait until the VBlank routine has been processed
  void WaitForVBlank();

  // NMI Interrupt Service Routine
  void NmiIsr();

  // VBlank routine
  void VBlankRoutine();

  // Boot the APU with the IPL ROM
  void BootApuWithIPL();
  void StartApuDataTransfer();

  // Controller input handling
  void HandleInput();

  // Save/Load game state
  void SaveState(const std::string& path);
  void LoadState(const std::string& path);

  bool running() const { return running_; }

  auto cpu() -> Cpu& { return cpu_; }
  auto ppu() -> video::Ppu& { return ppu_; }
  auto Memory() -> MemoryImpl* { return &memory_; }

  void SetCpuMode(int mode) { cpu_mode_ = mode; }
  Cpu::UpdateMode GetCpuMode() const {
    return static_cast<Cpu::UpdateMode>(cpu_mode_);
  }

  void SetupMemory(ROM& rom) {
    // Setup observers for the memory space
    memory_.AddObserver(&apu_);
    memory_.AddObserver(&ppu_);

    // Load the ROM into memory and set up the memory mapping
    rom_data = rom.vector();
    memory_.Initialize(rom_data);
  }

 private:
  void WriteToRegister(uint16_t address, uint8_t value) {
    memory_.WriteByte(address, value);
  }

  // Components of the SNES
  MemoryImpl memory_;
  ClockImpl clock_;
  audio::AudioRamImpl audio_ram_;

  Cpu cpu_{memory_, clock_};
  video::Ppu ppu_{memory_, clock_};
  audio::Apu apu_{memory_, audio_ram_, clock_};

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
  int cpu_mode_ = 0;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_SNES_H