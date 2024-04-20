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
class Apu : public Observer {
 public:
  Apu(MemoryImpl &memory, AudioRam &aram, Clock &clock)
      : aram_(aram), clock_(clock), memory_(memory) {}

  void Init();
  void Reset();
  void Update();
  void Notify(uint32_t address, uint16_t data) override;

  // Called upon a reset
  void Initialize() {
    spc700_.Reset();
    dsp_.Reset();
  }

  // Set Port 0 = $AA and Port 1 = $BB
  void SignalReady() {
    memory_.WriteByte(0x2140, READY_SIGNAL_0);
    memory_.WriteByte(0x2141, READY_SIGNAL_1);
  }

  void WriteToPort(uint8_t portNum, uint8_t value) {
    ports_[portNum] = value;
    switch (portNum) {
      case 0:
        memory_.WriteByte(0x2140, value);
        break;
      case 1:
        memory_.WriteByte(0x2141, value);
        break;
      case 2:
        memory_.WriteByte(0x2142, value);
        break;
      case 3:
        memory_.WriteByte(0x2143, value);
        break;
    }
  }

  void UpdateClock(int delta_time) { clock_.UpdateClock(delta_time); }

  auto dsp() -> Dsp & { return dsp_; }

 private:
  // Constants for communication
  static const uint8_t READY_SIGNAL_0 = 0xAA;
  static const uint8_t READY_SIGNAL_1 = 0xBB;
  static const uint8_t BEGIN_SIGNAL = 0xCC;

  // Port buffers (equivalent to $2140 to $2143 for the main CPU)
  uint8_t ports_[4] = {0};

  // Member variables to store internal APU state and resources
  AudioRam &aram_;
  Clock &clock_;
  MemoryImpl &memory_;

  Dsp dsp_;
  Spc700 spc700_{aram_};
};

}  // namespace audio
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif