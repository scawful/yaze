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
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

struct Input {
  uint8_t type;
  // latchline
  bool latchLine;
  // for controller
  uint16_t currentState;  // actual state
  uint16_t latchedState;
};

class SNES {
 public:
  SNES() = default;
  ~SNES() = default;

  // Initialization
  void Init(Rom& rom);
  void Reset(bool hard = false);

  // Emulation
  void RunFrame();
  void CatchUpApu();

  // Controller input handling
  void HandleInput();

  // Clock cycling and synchronization
  void RunCycle();
  void RunCycles(int cycles);
  void SyncCycles(bool start, int sync_cycles);

  uint8_t ReadBBus(uint8_t adr);
  uint8_t ReadReg(uint16_t adr);
  uint8_t Rread(uint32_t adr);
  uint8_t Read(uint32_t adr);

  void WriteBBus(uint8_t adr, uint8_t val);
  void WriteReg(uint16_t adr, uint8_t val);
  void Write(uint32_t adr, uint8_t val);

  int GetAccessTime(uint32_t adr);
  uint8_t CpuRead(uint32_t adr);
  void CpuWrite(uint32_t adr, uint8_t val);
  void CpuIdle(bool waiting);

  void SetSamples(int16_t* sample_data, int wanted_samples);
  void SetPixels(uint8_t* pixel_data);

  bool running() const { return running_; }
  auto cpu() -> Cpu& { return cpu_; }
  auto ppu() -> video::Ppu& { return ppu_; }
  auto apu() -> audio::Apu& { return apu_; }
  auto Memory() -> memory::MemoryImpl& { return memory_; }
  auto get_ram() -> uint8_t* { return ram; }

  auto mutable_cycles() -> uint64_t& { return cycles_; }

  std::vector<uint8_t> access_time;

  void build_accesstime( bool recalc) {
  int start = (recalc) ? 0x800000 : 0; // recalc only updates "fastMem" area
  access_time.resize(0x1000000);
  for (int i = start; i < 0x1000000; i++) {
    access_time[i] = GetAccessTime(i);
  }
}

 private:
  // Components of the SNES
  ClockImpl clock_;
  Debugger debugger;
  memory::RomInfo rom_info_;
  memory::MemoryImpl memory_;

  memory::CpuCallbacks cpu_callbacks_ = {
      [&](uint32_t adr) { return CpuRead(adr); },
      [&](uint32_t adr, uint8_t val) { CpuWrite(adr, val); },
      [&](bool waiting) { CpuIdle(waiting); },
  };
  Cpu cpu_{memory_, clock_, cpu_callbacks_};
  video::Ppu ppu_{memory_, clock_};
  audio::Apu apu_{memory_, clock_};

  // Currently loaded ROM
  std::vector<uint8_t> rom_data;

  // Emulation state
  bool running_ = false;

  // ram
  uint8_t ram[0x20000];
  uint32_t ram_adr_;

  // Frame timing
  uint32_t frames_ = 0;
  uint64_t cycles_ = 0;
  uint64_t sync_cycle_ = 0;
  double apu_catchup_cycles_;
  uint32_t nextHoriEvent;

  // Nmi / Irq
  bool h_irq_enabled_ = false;
  bool v_irq_enabled_ = false;
  bool nmi_enabled_ = false;
  uint16_t h_timer_ = 0;
  uint16_t v_timer_ = 0;
  bool in_nmi_ = false;
  bool irq_condition_ = false;
  bool in_irq_ = false;
  bool in_vblank_;

  // Multiplication / Division
  uint8_t multiply_a_;
  uint16_t multiply_result_;
  uint16_t divide_a_;
  uint16_t divide_result_;

  // Joypad State
  Input input1;
  Input input2;
  uint16_t port_auto_read_[4];  // as read by auto-joypad read
  bool auto_joy_read_ = false;
  uint16_t auto_joy_timer_ = 0;
  bool ppuLatch;

  bool fast_mem_ = false;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_SNES_H