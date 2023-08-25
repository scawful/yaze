#ifndef YAZE_APP_EMU_APU_H_
#define YAZE_APP_EMU_APU_H_

#include <cstdint>

#include "app/emu/clock.h"
#include "app/emu/mem.h"
#include "app/emu/spc700.h"

namespace yaze {
namespace app {
namespace emu {

const int kApuClockSpeed = 1024000;  // 1.024 MHz
const int apuSampleRate = 32000;     // 32 KHz
const int apuClocksPerSample = 64;   // 64 clocks per sample

class APU : public Observer {
 public:
  // Initializes the APU with the necessary resources and dependencies
  APU(Memory &memory, VirtualAudioRAM &aram, VirtualClock &clock)
      : memory_(memory), clock_(clock), aram_(aram) {}

  void Init();

  // Resets the APU to its initial state
  void Reset();

  // Runs the APU for one frame
  void Update();

  void Notify(uint32_t address, uint8_t data) override {
    if (address >= 0x2140 && address <= 0x2143) {
      // Handle communication with the APU
    }
  }

  void UpdateClock(int delta_time) { clock_.UpdateClock(delta_time); }

  // Reads a byte from the specified APU register
  uint8_t ReadRegister(uint16_t address);

  // Writes a byte to the specified APU register
  void WriteRegister(uint16_t address, uint8_t value);

  // Returns the audio samples for the current frame
  const std::vector<int16_t> &GetAudioSamples() const;

 private:
  // Internal methods to handle APU operations and sound generation

  // Updates internal state based on APU register settings
  void UpdateChannelSettings();

  // Generates a sample for an audio channel
  int16_t GenerateSample(int channel);

  // Applies an envelope to an audio channel
  void ApplyEnvelope(int channel);

  // Handles DSP (Digital Signal Processor) memory reads and writes
  uint8_t ReadDSPMemory(uint16_t address);
  void WriteDSPMemory(uint16_t address, uint8_t value);

  // Member variables to store internal APU state and resources
  Memory &memory_;
  VirtualClock &clock_;
  VirtualAudioRAM &aram_;

  SPC700 spc700_{aram_};
  std::vector<int16_t> audioSamples_;
  // Other state variables (registers, counters, channel settings, etc.)
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif