#ifndef YAZE_APP_EMU_APU_H_
#define YAZE_APP_EMU_APU_H_

#include <array>
#include <cstdint>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {

// Forward declaration
namespace debug {
class ApuHandshakeTracker;
}

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
  Apu(MemoryImpl& memory) : memory_(memory) {}

  void Init();
  void Reset();

  void RunCycles(uint64_t cycles);
  
  void SaveState(std::ostream& stream);
  void LoadState(std::istream& stream);

  uint8_t SpcRead(uint16_t address);
  void SpcWrite(uint16_t address, uint8_t data);
  void SpcIdle(bool waiting);

  void Cycle();

  uint8_t Read(uint16_t address);
  void Write(uint16_t address, uint8_t data);

  auto dsp() -> Dsp& { return dsp_; }
  auto dsp() const -> const Dsp& { return dsp_; }
  auto spc700() -> Spc700& { return spc700_; }

  uint64_t GetCycles() const { return cycles_; }

  // Audio debugging
  void set_handshake_tracker(debug::ApuHandshakeTracker* tracker) {
    handshake_tracker_ = tracker;
  }
  uint8_t GetStatus() const { return ram[0x00]; }
  uint8_t GetControl() const { return ram[0x01]; }
  void GetSamples(int16_t* buffer, int count, bool loop = false) {
    dsp_.GetSamples(buffer, count, loop);
  }
  void WriteDma(uint16_t address, const uint8_t* data, int count) {
    for (int i = 0; i < count; i++) {
      ram[address + i] = data[i];
    }
  }

  /**
   * @brief Bootstrap SPC directly to driver code (bypasses IPL ROM handshake)
   *
   * This method allows direct control of the SPC700 by:
   * 1. Disabling the IPL ROM
   * 2. Setting PC to the driver entry point
   * 3. Initializing SPC state for driver execution
   *
   * Use this after uploading audio driver code via WriteDma() to bypass
   * the normal IPL ROM handshake protocol.
   *
   * @param entry_point The ARAM address where the driver code starts (typically $0800)
   */
  void BootstrapDirect(uint16_t entry_point);

  /**
   * @brief Check if SPC has completed IPL ROM boot and is running driver code
   * @return true if IPL ROM is disabled and SPC is executing from RAM
   */
  bool IsDriverRunning() const { return !rom_readable_; }

  /**
   * @brief Get timer state for debug UI
   * @param timer_index 0, 1, or 2
   */
  const Timer& GetTimer(int timer_index) const {
    if (timer_index < 0) timer_index = 0;
    if (timer_index > 2) timer_index = 2;
    return timer_[timer_index];
  }

  /**
   * @brief Write directly to DSP register
   * Used for direct instrument/note preview without going through driver
   */
  void WriteToDsp(uint8_t address, uint8_t value) {
    if (address < 0x80) {
      dsp_.Write(address, value);
    }
  }

  // Port buffers (equivalent to $2140 to $2143 for the main CPU)
  std::array<uint8_t, 6> in_ports_;  // includes 2 bytes of ram
  std::array<uint8_t, 4> out_ports_;
  std::vector<uint8_t> ram = std::vector<uint8_t>(0x10000, 0);

 private:
  bool rom_readable_ = false;

  uint8_t dsp_adr_ = 0;
  uint64_t cycles_ = 0;
  uint64_t last_master_cycles_ = 0;

  // IPL ROM transfer tracking for proper termination
  uint8_t transfer_size_ = 0;
  bool in_transfer_ = false;

  MemoryImpl& memory_;
  std::array<Timer, 3> timer_;

  // Audio debugging
  debug::ApuHandshakeTracker* handshake_tracker_ = nullptr;

  ApuCallbacks callbacks_ = {
      [&](uint16_t adr, uint8_t val) { SpcWrite(adr, val); },
      [&](uint16_t adr) { return SpcRead(adr); },
      [&](bool waiting) { SpcIdle(waiting); },
  };
  Dsp dsp_{ram};
  Spc700 spc700_{callbacks_};
};

}  // namespace emu
}  // namespace yaze

#endif
