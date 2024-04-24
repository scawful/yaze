#ifndef YAZE_APP_EMU_APU_H_
#define YAZE_APP_EMU_APU_H_

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/cpu/clock.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {
namespace audio {

using namespace memory;

const int kApuClockSpeed = 1024000;  // 1.024 MHz
const int apuSampleRate = 32000;     // 32 KHz
const int apuClocksPerSample = 64;   // 64 clocks per sample

typedef struct Timer {
  uint8_t cycles;
  uint8_t divider;
  uint8_t target;
  uint8_t counter;
  bool enabled;
} Timer;

/**
 * @class Apu
 * @brief The Apu class represents the Audio Processing Unit (APU) of a system.
 *
 * The Apu class is responsible for generating audio samples and managing the
 * APU state. It interacts with the Memory, AudioRam, and Clock classes to
 * read/write data and update the clock. The class also implements the Observer
 * interface to receive notifications from the system.
 *
 * @par IPL ROM Info
 * 64 kilobytes of RAM are mapped across the 16-bit memory space of the SPC-700.
 * Some regions of this space are overlaid with special hardware functions.
 *
 * @par Range 	      Note
 * $0000-00EF 	Zero Page RAM
 * $00F0-00FF 	Sound CPU Registers
 * $0100-01FF 	Stack Page RAM
 * $0200-FFBF 	RAM
 * $FFC0-FFFF 	IPL ROM or RAM
 *
 * The region at $FFC0-FFFF will normally read from the 64-byte IPL ROM, but the
 * underlying RAM can always be written to, and the high bit of the Control
 * register $F1 can be cleared to unmap the IPL ROM and allow read access to
 * this RAM.
 */
class Apu {
 public:
  Apu(MemoryImpl &memory, Clock &clock) : clock_(clock), memory_(memory) {}

  void Init();
  void Reset();
  void Update();

  void RunCycles(uint64_t cycles);
  uint8_t SpcRead(uint16_t address);
  void SpcWrite(uint16_t address, uint8_t data);
  void SpcIdle(bool waiting);

  void Cycle();

  uint8_t Read(uint16_t address);
  void Write(uint16_t address, uint8_t data);

  void UpdateClock(int delta_time) { clock_.UpdateClock(delta_time); }

  auto dsp() -> Dsp & { return dsp_; }
  auto spc700() -> Spc700 & { return spc700_; }

  // Port buffers (equivalent to $2140 to $2143 for the main CPU)
  uint8_t in_ports_[6];  // includes 2 bytes of ram
  uint8_t out_ports_[4];

 private:
  Timer timer_[3];
  uint32_t cycles_;
  uint8_t dsp_adr_;
  bool rom_readable_ = false;
  std::vector<uint8_t> ram = std::vector<uint8_t>(0x10000, 0);

  // Member variables to store internal APU state and resources
  Clock &clock_;
  MemoryImpl &memory_;

  ApuCallbacks callbacks_ = {
      [&](uint16_t adr, uint8_t val) { SpcWrite(adr, val); },
      [&](uint16_t adr) { return SpcRead(adr); },
      [&](bool waiting) { SpcIdle(waiting); },
  };
  Dsp dsp_{ram};
  Spc700 spc700_{callbacks_};
};

}  // namespace audio
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif