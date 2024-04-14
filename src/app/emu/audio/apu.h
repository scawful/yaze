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

/**
 *
 * 64 kilobytes of RAM are mapped across the 16-bit memory space of the SPC-700.
 * Some regions of this space are overlaid with special hardware functions.
 *
 * Range 	      Note
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
 *
 */

const int kApuClockSpeed = 1024000;  // 1.024 MHz
const int apuSampleRate = 32000;     // 32 KHz
const int apuClocksPerSample = 64;   // 64 clocks per sample

class Apu : public Observer {
 public:
  Apu(MemoryImpl &memory, AudioRam &aram, Clock &clock)
      : aram_(aram), clock_(clock), memory_(memory) {}

  void Init();
  void Reset();
  void Update();
  void Notify(uint32_t address, uint8_t data) override;

  void ProcessSamples();
  uint8_t FetchSampleForVoice(uint8_t voice_num);
  uint16_t CalculateAddressForVoice(uint8_t voice_num);
  int16_t GetNextSample();

  // Called upon a reset
  void Initialize() {
    spc700_.Reset();
    dsp_.Reset();
    SignalReady();
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

  // Method to fetch a sample from AudioRam
  uint8_t FetchSampleFromRam(uint16_t address) const {
    return aram_.read(address);
  }

  // Method to push a processed sample to the audio buffer
  void PushToAudioBuffer(int16_t sample) { audio_samples_.push_back(sample); }

  // Returns the audio samples for the current frame
  const std::vector<int16_t> &GetAudioSamples() const;

 private:
  // Constants for communication
  static const uint8_t READY_SIGNAL_0 = 0xAA;
  static const uint8_t READY_SIGNAL_1 = 0xBB;
  static const uint8_t BEGIN_SIGNAL = 0xCC;

  // Port buffers (equivalent to $2140 to $2143 for the main CPU)
  uint8_t ports_[4] = {0};

  // Updates internal state based on APU register settings
  void UpdateChannelSettings();

  // Generates a sample for an audio channel
  int16_t GenerateSample(int channel);

  // Applies an envelope to an audio channel
  void ApplyEnvelope(int channel);

  // Handles DSP (Digital Signal Processor) memory reads and writes
  uint8_t ReadDspMemory(uint16_t address);
  void WriteDspMemory(uint16_t address, uint8_t value);

  // Member variables to store internal APU state and resources
  AudioRam &aram_;
  Clock &clock_;
  MemoryImpl &memory_;

  DigitalSignalProcessor dsp_;
  Spc700 spc700_{aram_};
  std::vector<int16_t> audio_samples_;

  std::function<void()> ready_callback_;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif